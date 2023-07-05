#ifndef clox_scanner_h
#define clox_scanner_h

typedef enum {
    NONE,
    // Single-character tokens. 单字符词法
    TOKEN_LEFT_PAREN,  // token_left_paren,
    TOKEN_RIGHT_PAREN, // token_right_paren,
    TOKEN_LEFT_BRACE,  // token_left_brace,
    TOKEN_RIGHT_BRACE, // token_right_brace,
    TOKEN_COMMA,       // token_comma,
    TOKEN_DOT,         // token_dot,
    TOKEN_MINUS,       // token_minus,
    TOKEN_PLUS,        // token_plus,
    TOKEN_SEMICOLON,   // token_semicolon,
    TOKEN_SLASH,       // token_slash,
    TOKEN_STAR,        // token_star,
    // One or two character tokens
    TOKEN_BANG,          // token_bang,
    TOKEN_BANG_EQUAL,    // token_bang_equal,
    TOKEN_EQUAL,         // token_equal,
    TOKEN_EQUAL_EQUAL,   // token_equal_equal,
    TOKEN_GREATER,       // token_greater,
    TOKEN_GREATER_EQUAL, // token_greater_equal,
    TOKEN_LESS,          // token_less,
    TOKEN_LESS_EQUAL,    // token_less_equal,
    // Literals. 字面量
    TOKEN_IDENTIFIER, // token_identifier,
    TOKEN_STRING,     // token_string,
    TOKEN_NUMBER,     // token_number,
    // Keywords. 关键字
    TOKEN_AND,    // token_and,
    TOKEN_CLASS,  // token_class,
    TOKEN_ELSE,   // token_else,
    TOKEN_FALSE,  // token_false,
    TOKEN_FOR,    // token_for,
    TOKEN_FUN,    // token_fun,
    TOKEN_IF,     // token_if,
    TOKEN_NIL,    // token_nil,
    TOKEN_OR,     // token_or,
    TOKEN_PRINT,  // token_print,
    TOKEN_RETURN, // token_return,
    TOKEN_SUPER,  // token_super,
    TOKEN_THIS,   // token_this,
    TOKEN_TRUE,   // token_true,
    TOKEN_VAR,    // token_var,
    TOKEN_WHILE,  // token_while,
    //
    TOKEN_ERROR, // token_error,
    TOKEN_EOF    // token_eof
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    // 每个token的词素是这样被组织的, 一个字符串的起始指针和长度
    // 因为C中的所有字符串都是常量, 且内容相同的字符串都是同一个内存的
    int line;
} Token;

void initScanner(const char* source);
Token scanToken();

#endif