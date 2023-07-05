#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "native.h"
#include "vm.h"

// 内置函数不需要管理Lox虚拟机的栈, 直接在C语义下执行逻辑即可

static Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value showNative(int argCount, Value* args) {
    printf("show(");
    for (int i = 0; i < argCount; i++) {
        printValue(args[i]);
        printf(i == argCount - 1 ? ")\n" : ", ");
    }
    return NUMBER_VAL(argCount);
}

static Value exitNative(int argCount, Value* args) {
    exit(0);
    return *args;
}

// ===

void defineNative(const char* name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

//

static Value appendNative(int argCount, Value* args) {
    // Append a value to the end of a list increasing the list's length by 1
    if (argCount != 2 || !IS_LIST(args[0])) {
        // Handle error
    }
    ObjList* list = AS_LIST(args[0]);
    Value item = args[1];
    appendToList(list, item);
    return NIL_VAL;
}

static Value deleteNative(int argCount, Value* args) {
    // Delete an item from a list at the given index.
    if (argCount != 2 || !IS_LIST(args[0]) || !IS_NUMBER(args[1])) {
        // Handle error
    }

    ObjList* list = AS_LIST(args[0]);
    int index = AS_NUMBER(args[1]);

    if (!isValidListIndex(list, index)) {
        // Handle error
    }

    deleteFromList(list, index);
    return NIL_VAL;
}

//

void nativeRegister() {
    defineNative("clock", clockNative);
    defineNative("show", showNative);
    defineNative("exit", exitNative);

    defineNative("append", appendNative);
    defineNative("delete", deleteNative);
}