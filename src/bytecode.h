#ifndef clox_bytecode_h
#define clox_bytecode_h

// 虚拟机字节码
typedef enum {
    OP_CONSTANT, // op arg: 从常量池中取出索引为arg的常量放入栈中
    OP_CLOSURE, // 同上, 但是取出的ObjString是闭包名
    OP_NIL,     // op: 将NIL放入栈中
    OP_TRUE,    // 类似上
    OP_FALSE,   // 类似上
    OP_POP,     // 弹出栈顶
    // (全局)变量名存在常量池中,
    // 下面三个命令的arg即为其索引,
    // 其值存储在global哈希表中
    OP_DEFINE_GLOBAL, // op arg, 取出栈顶元素作为变量名的value放入global
    OP_GET_GLOBAL, // op arg, 取出变量名的value放入栈中
    OP_SET_GLOBAL, // op arg, 将栈顶元素作为变量名在global中的value
    // 而局部变量直接存在栈中, 下面的arg是栈的地址
    OP_GET_LOCAL,   // op arg, 取出栈arg位置的值入栈
    OP_SET_LOCAL,   // op_arg, 直接修改arg位置的值
    OP_GET_UPVALUE, // 同上, 参数为upvalue在上值列表的索引
    OP_SET_UPVALUE,
    OP_EQUAL, // equal: 取出栈顶两个元素进行比较, 并将结构压入栈中
    OP_GREATER,       // greater >:
    OP_LESS,          // less <:
    OP_ADD,           // add +:
    OP_SUBTRACT,      // subtract 中缀-:
    OP_MULTIPLY,      // multiply *:
    OP_DIVIDE,        // divide /:
    OP_NOT,           // not !:
    OP_NEGATE,        // negate 前缀-
    OP_PRINT,         // op, "取出"栈顶元素并打印
    OP_JUMP,          // jump
    OP_JUMP_IF_FALSE, // 条件jump
    OP_CALL,          // 函数调用
    OP_LOOP,          //
    OP_CLOSE_UPVALUE, // 对于函数中被其他闭包捕获的变量的处理
    OP_RETURN,        // return
    OP_CLASS,
    OP_GET_PROPERTY, // class get
    OP_SET_PROPERTY, // class set
    OP_METHOD,
    OP_INVOKE, // 针对直接调用方法, 两个参数,
               // 属性名在常量表的索引和传递给方法的参数数量
    OP_INHERIT,   // 继承
    OP_GET_SUPER, // 超类访问

    OP_BUILD_LIST,
    OP_INDEX_SUBSCR,
    OP_STORE_SUBSCR,
} OpCode; // operation code

// 并没有<=、>=、!=
//  因为 !(a < b) -> (a >= b)
//      !(a > b) -> (a <= b)
//      !(a == b) -> (a != b)
//  有一个例外, 就是数字Nan, 在C中它和任何数字比较都是false

#endif