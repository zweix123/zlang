#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
// check
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
// Value -> 具体的Object
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_UPVALUE(value)      ((ObjUpvalue*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))

// #define AS_CSTRING(value)  (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj* next; // intrusive list侵入式列表,
                      // 用来保证虚拟机可以找到每个堆内存的对象
};

typedef struct {
    Obj obj;
    int arity;        // 参数数量
    int upvalueCount; // 当前逻辑块的上值数量
    Chunk chunk;      // 函数逻辑字节码
    ObjString* name;  // 函数名称
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

struct ObjString {
    // 一个ObjString结构体变量指针可以安全转换成Obj类型指针,
    // 然后正常访问type属性, 而实际上反向转换也是可以的(当然安全性由用户保证)
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct ObjUpvalue {
    Obj obj;
    Value* location; // 指针(各个上值的底层Value离散在堆中)
    Value closed;    // 指针指向的位置的Value
    struct ObjUpvalue* next; //
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function; // 此时ObjFunction更像是对底层函数的封装
    ObjUpvalue** upvalues; // ObjUpvalue* upvalues[], 因为上值存储在栈中,
                           // 肯定用指针, 但是C不允许定义name[]
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    ObjString* name;
    Table methods;
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass* klass;
    Table fields;
} ObjInstance;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);

ObjClosure* newClosure(ObjFunction* function);
ObjUpvalue* newUpvalue(Value* slot);

ObjClass* newClass(ObjString* name);
ObjInstance* newInstance(ObjClass* klass);
ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);

void printObject(Value value);

static bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif