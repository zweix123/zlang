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

void nativeRegister() {
    defineNative("clock", clockNative);
    defineNative("show", showNative);
    defineNative("exit", exitNative);
}
