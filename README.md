# zlang

+ Reference
  + 脱胎于[lox](https://github.com/munificent/craftinginterpreters)
  + 动态数组内置类型借鉴[lox list](https://calebschoepp.com/blog/2020/adding-a-list-data-type-to-lox/)

## Quickly Start

### Compile

+ Pre: gcc, make.

项目目录下运行
```
make
```

### Use

+ 注释仅支持单行注释, 使用`//`
+ 数据类型
  + `nil`
  + 布尔类型:`true` and `false`
    >`false` and `nill` is false, else is true.

  + 双精度浮点数
  + 字符串: 使用双引号`"`

+ 表达式
  + 加减乘除取负
    + 加号的操作数为字符串则表意为拼接
  + 比较运算符(不支持隐式转换)
  + 逻辑运算符: `!`、`and`、`or`

  >优先级和结合性同C

+ 语句
  + end with `;`
  + 代码块使用`{}`, 同C

+ 变量: 动态类型
  + 关键字`var`相当于声明

+ 控制流: 支持`if`、`while`、`for`, 规则同C

+ 函数:
  + 定义使用关键字`fun`, 其余同C
  + `return`语句, 默认返回`nil`
  + 闭包, 在zlang中闭包是一等公民

+ 类: 
  + 定义使用关键字`class`, 类中方法声明不用使用关键字`fun`
  + 属性
    + 可直接向实例中添加属性
    + 类内部通过`this`访问

  + 构造函数`init`
  + 继承使用关键字`<`, 比如`class Derived < Base {}`
    + 通过关键字`super`访问父类

### Develop

## Gammer

```
# 语法
program        → declaration* EOF ;  # zlang是一系列"声明", 
# 声明
declaration    → classDecl
               | funDecl
               | varDecl
               | statement ;         # 有一个"特殊"的声明是语句

classDecl      → "class" IDENTIFIER ( "<" IDENTIFIER )?
                 "{" function* "}" ;
funDecl        → "fun" function ;
varDecl        → "var" IDENTIFIER ( "=" expression )? ";" ;
# 语句: 声明会产生绑定, 语句则不会
statement      → exprStmt
               | forStmt
               | ifStmt
               | printStmt
               | returnStmt
               | whileStmt
               | block ;

exprStmt       → expression ";" ;
forStmt        → "for" "(" ( varDecl | exprStmt | ";" )
                           expression? ";"
                           expression? ")" statement ;
ifStmt         → "if" "(" expression ")" statement
                 ( "else" statement )? ;
printStmt      → "print" expression ";" ;
returnStmt     → "return" expression? ";" ;
whileStmt      → "while" "(" expression ")" statement ;
block          → "{" declaration* "}" ;

function       → IDENTIFIER "(" parameters? ")" block ;  # 实用规则
parameters     → IDENTIFIER ( "," IDENTIFIER )* ;        # 实用规则
arguments      → expression ( "," expression )* ;        # 实用规则
# 表达式
expression     → assignment ;

assignment     → ( call "." )? IDENTIFIER ( "[" logic_or "]" )* "=" assignment
               | logic_or ;

logic_or       → logic_and ( "or" logic_and )* ;
logic_and      → equality ( "and" equality )* ;
equality       → comparison ( ( "!=" | "==" ) comparison )* ;
comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
term           → factor ( ( "-" | "+" ) factor )* ;
factor         → unary ( ( "/" | "*" ) unary )* ;

unary          → ( "!" | "-" ) unary | call ;
call           → subscript ( "(" arguments? ")" | "." IDENTIFIER )* ;
subscript      → primary ( "[" logic_or "]" )* ;
primary        → "true" | "false" | "nil" | "this"
               | NUMBER | STRING | IDENTIFIER | "(" expression ")"
               | "super" "." IDENTIFIER | "[" list_display? "]" ;
list_display   → logic_or ( "," logic_or )* ( "," )? ;
# 词法(非递归->正则)
NUMBER         → DIGIT+ ( "." DIGIT+ )? ;
STRING         → "\"" <any char except "\"">* "\"" ;
IDENTIFIER     → ALPHA ( ALPHA | DIGIT )* ;
ALPHA          → "a" ... "z" | "A" ... "Z" | "_" ;
DIGIT          → "0" ... "9" ;
```


### TODO

+ 添加`switch`
  ```
  switchStmt     → "switch" "(" expression ")"
                  "{" switchCase* defaultCase? "}" ;
  switchCase     → "case" expression ":" statement* ;
  defaultCase    → "default" ":" statement* ;
  ```
