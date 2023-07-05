#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;
    object->next = vm.objects;
    vm.objects = object;
#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif
    return object;
}

ObjFunction* newFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative* newNative(NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    // GC有关, 原因见chunk.c addConstant()注释
    push(OBJ_VAL(string));
    tableSet(&vm.strings, string, NIL_VAL);
    pop();
    return string;
}

static uint32_t hashString(const char* key, int length) { // FNV-1a
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString* takeString(char* chars, int length) {
    // 虚拟机内部使用
    // 将C下的字符串转换成lox的ObjString
    // 该字符串可以直接用(C语义下的)
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }
    return allocateString(chars, length, hash);
}

ObjString* copyString(const char* chars, int length) {
    // 编译器使用的
    // 将C接受的字符串放在虚拟机中
    // 需要深拷贝, 因为不知道虚拟机在使用时这个字符串在C的状态
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;
    // 如果这个字符串已经被虚拟机驻留过, 说明已经被接管过, 不用再拷贝独立的了
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

ObjClosure* newClosure(ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) { upvalues[i] = NULL; }
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjUpvalue* newUpvalue(Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

ObjClass* newClass(ObjString* name) {
    ObjClass* zlass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    zlass->name = name;
    initTable(&zlass->methods);
    return zlass;
}

ObjInstance* newInstance(ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

// ===

void printFunction(ObjFunction* function) {
    if (function->name == NULL) { // 给调试用的
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

void printNative(void*) {
    printf("<native fn>");
}

void printObjString(ObjString* objstring) {
    printf("%s", objstring->chars);
}

void printObjClosure(ObjClosure* closure) {
    printFunction(closure->function);
}

void printObjUpvalue(void*) {
    printf("upvalue");
}

void printObjClass(ObjClass* zlass) {
    printf("%s", zlass->name->chars);
}

void printObjInstance(ObjInstance* instance) {
    printf("%s instance", instance->klass->name->chars);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_FUNCTION: printFunction(AS_FUNCTION(value)); break;
        case OBJ_NATIVE: printNative(AS_NATIVE(value)); break;
        case OBJ_STRING: printObjString(AS_STRING(value)); break;
        case OBJ_CLOSURE: printObjClosure(AS_CLOSURE(value)); break;
        case OBJ_UPVALUE: printObjUpvalue(AS_CLOSURE(value)); break;
        case OBJ_CLASS: printObjClass(AS_CLASS(value)); break;
        case OBJ_INSTANCE: printObjInstance(AS_INSTANCE(value)); break;
        case OBJ_BOUND_METHOD:
            printFunction(AS_BOUND_METHOD(value)->method->function);
            break;
        default: break;
    }
}
