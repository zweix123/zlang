#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token previous; // current
    Token current;  // 超尾
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    // 优先级从低到高, C的enum可以隐式转换成整数, 直接比较符号就是优先级的比较
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // ||
    PREC_AND,        // &&
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence; // 优先级
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool isCaptured; // 是否被底层代码块捕获成闭包中的值
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,    // type_function
    TYPE_METHOD,      // type_method
    TYPE_SCRIPT,      // type_script
    TYPE_INITIALIZER, // 类的初始化器, 如果说method是特殊的function,
                      // 那么它就是特殊的method
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;
    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool hasSuperclass;
} ClassCompiler;

Parser parser;
// 下面两个指针的作用是, 通过全局变量避免修改每个函数的接口
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;
Chunk* compilingChunk;

static Chunk* currentChunk() {
    return &current->function->chunk;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;

    compiler->function = newFunction();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name =
            copyString(parser.previous.start, parser.previous.length);
    }
    // 编译模拟栈的0索引由块自己使用
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    local->name.start = "";
    local->name.length = 0;

    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

// ===

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // nothing, 占位
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) { // errorAtPrevious
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;
    for (;;) {
        // 这里会不断的寻找正确的Token，错误的Token不会被保存
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

// ===

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static void emitReturn() {
    if (current->type == TYPE_INITIALIZER) {
        emitBytes(OP_GET_LOCAL, 0);
    } else {
        emitByte(OP_NIL);
    }
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) { error("Too much code to jump over."); }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

// ===

static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(
            currentChunk(),
            function->name != NULL ? function->name->chars : "<script>");
    }
#endif
    current =
        current->enclosing; // 这里, 直接就把当前层的compiler结构体变量丢弃了,
                            // 因为我们绑定的是C语言下的局部变量,
                            // 由C语言的栈管理, 其实有一个隐含问题,
                            // 如果代码有恶意大量嵌套可能导致C爆栈
    return function;
}

// ===

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;
    while (current->localCount > 0
           && current->locals[current->localCount - 1].depth
                  > current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else { // 如果没有被捕获, 就普通的弹出
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

// ===

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    // 严格的说这里经过了三次拷贝
    local->depth = -1; // 调用这个函数的是declareVariable,
                       // 当时只声明而未定义,
                       // 以flag -1标记
    local->isCaptured = false;
}

static void declareVariable() {
    if (current->scopeDepth == 0) return; // 这是针对局部变量的, 全局变量不需要
    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) { break; }
        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }
    addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0; // 局部变量不需要名字所在的常量表索引

    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    if (current->scopeDepth == 0) return; // 全局名称不在这里管理
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) { // 局部变量不需要全局变量相关的声明指令
        markInitialized(); // 定义了, 要标记下
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void expression();
static void statement();
static void declaration();
static void block();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void dot(bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList();
        emitBytes(OP_INVOKE, name);
        emitByte(argCount);

    } else {
        emitBytes(OP_GET_PROPERTY, name);
    }
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;
    for (int i = 0; i < upvalueCount; i++) {
        // 如果之前处理过, 不同再处理
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index
            && upvalue->isLocal == isLocal) { // 索引唯一
            return i;
        }
    }
    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }
    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    // 这里的返回值左右上值相关的get set参数,
    // 让解释器直接使用参数这个索引找到对应的栈索引
    if (compiler->enclosing == NULL) return -1;

    int local =
        resolveLocal(compiler->enclosing, name); // 去上一层找它的局部变量
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name); // 递归向上
    if (upvalue != -1) { return addUpvalue(compiler, (uint8_t)upvalue, false); }

    return -1;
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static Token syntheticToken(const char* text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

static void this_(bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'this' outside of a class.");
        return;
    }
    variable(false);
}

static void super_(bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'super' outside of a class.");
    } else if (!currentClass->hasSuperclass) {
        error("Can't use 'super' in a class with no superclass.");
    }
    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(&parser.previous);
    namedVariable(syntheticToken("this"), false);
    namedVariable(syntheticToken("super"), false);
    emitBytes(OP_GET_SUPER, name);
}

static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
    emitConstant(OBJ_VAL(
        copyString(parser.previous.start + 1, parser.previous.length - 2)));
    // 在这个函数里的调用流程比较复杂
    // 1. 创建一个新字符数组拷贝字符串(字符串脱离原本的C编译器的管理)
    // 2. 创建一个大小从满足的堆内存块
    //    1. 强制转换成Obj, 设置type
    //    2. 强制转换会ObjString, 设置其他属性(上面的字符串放入)
    // 返回最后的ObjString
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        default: return;
    }
}

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return;
    }
}

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
        case TOKEN_GREATER: emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS: emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS: emitByte(OP_ADD); break;
        case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
        default: return; // Unreachable.
    }
}

static void add_(bool canAssign) {
    int endJump =
        emitJump(OP_JUMP_IF_FALSE); // 当&&被运行时, 作为一个中缀,
                                    // 左边已经知道了, 如果是假则跳过右边
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}

static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump); // 左侧值为假, 则跳过"跳过全部的语句" -> 不跳过
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

// ===

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},        // left_paren
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},           // right_paren
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},            // left_brace
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},           // right_brace
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},                 // comma
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},                    // dot
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},              // minus
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},                // plus
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},             // semicolon
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},             // slash
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},              // star
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},                 // bang !
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},      // bang_equal
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},                 // equal
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},     // equal_equal
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},       // greater
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON}, // greater_equal
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},          // less
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},    // less_equal
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},        // identifier
    [TOKEN_STRING] = {string, NULL, PREC_NONE},              // string
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},              // number
    [TOKEN_AND] = {NULL, add_, PREC_AND},                    // and
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},                 // class
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},                  // else
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},              // false
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},                   // for
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},                   // fun
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},                    // if
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},                // nil
    [TOKEN_OR] = {NULL, or_, PREC_OR},                       // or
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},                 // print
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},                // return
    [TOKEN_SUPER] = {super_, NULL, PREC_NONE},               // super
    [TOKEN_THIS] = {this_, NULL, PREC_NONE},                 // this
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},               // true
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},                   // var
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},                 // while
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},                 // error
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE}};                  // eof

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void parsePrecedence(Precedence precedence) {
    // 该函数实现从当前Token开始，去parse比precedence更高优先级的表达式
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);

        if (canAssign && match(TOKEN_EQUAL)) {
            error("Invalid assignment target.");
        }
    }
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name.");
    // 先解析将变量名放到常量表中

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
    // 然后再将"定义全局变量"的指令字节码写入chunk
}

static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    // op_closure是一个不定参数的指令
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void method() {
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(&parser.previous);
    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4
        && memcmp(parser.previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(type);
    emitBytes(OP_METHOD, constant);
}

static void classDeclaration() {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;

    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();

    emitBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    if (match(TOKEN_LESS)) {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(false);
        if (identifiersEqual(&className, &parser.previous)) {
            error("A class can't inherit from itself.");
        }
        beginScope();
        addLocal(syntheticToken("super"));
        defineVariable(0);
        namedVariable(className, false);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false);

    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) { method(); }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(OP_POP);

    if (classCompiler.hasSuperclass) { endScope(); }

    currentClass = currentClass->enclosing;
}

static void funDeclaration() {
    // 和下面的变量声明很像
    uint8_t global = parseVariable("Expect function name.");
    markInitialized(); // 但是这里和变量很不一样, 声明之后就定义了,
                       // 这在递归中是很有必要的
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void forStatement() {
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    if (match(TOKEN_SEMICOLON)) {
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        expressionStatement();
    }

    int loopStart = currentChunk()->count;

    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition.
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        // 对于增量语句怎么做呢? 这是循环体还不知道

        int bodyJump = emitJump(OP_JUMP); // 首先要跳一下
                                          // 把这一块都跳出去
                                          // 因为最开始这里不用运行
        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart); // 然后(实际上这里已经循环一次了), 会跳转到循环开头
        loopStart =
            incrementStart; // 然后修改整体循环的距离,
                            // 这样整体的循环是跳转到这里 自己再跳转到循环开头
                            // 从而实现将一个再开始编译的代码放在后面编译的代码后面
        patchJump(bodyJump);
    }

    statement();
    emitLoop(loopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP); // Condition.
    }

    endScope();
}

static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // 为了pop condition, 这句话在编译器一定被执行,
                      // 但是在虚拟中不一定, 因为上面那个指令会跳转,
                      // 从而把这个pop跳过去
    statement();
    int elseJump = emitJump(OP_JUMP);
    // 下面两个先后顺序不关键, 因为回填不影响新加入的指令
    patchJump(thenJump); // backpathcing回填
    emitByte(OP_POP);    // 同上
    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump); // 回填, 同上
}

static void whileStatement() {
    int loopStart = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) { declaration(); }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }
    if (match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void statement() {
    if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN: return;

            default:;
        }
        advance();
    }
}

static void declaration() {
    if (match(TOKEN_CLASS)) {
        classDeclaration();
    } else if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }
    if (parser.panicMode) synchronize();
}

ObjFunction* compile(const char* source) {
    initScanner(source);

    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF)) { declaration(); }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}