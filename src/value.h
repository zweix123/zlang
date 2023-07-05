#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

// tagged union
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

typedef struct {
    ValueType type;

    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// check
#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value)    ((value).type == VAL_OBJ)
// union -> C
#define AS_OBJ(value)    ((value).as.obj)
#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
// C -> union  // 要求C值正确(但不检测)
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

bool valuesEqual(Value a, Value b);

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void freeValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);

void printValue(Value value);

#endif