#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    vm.bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) { // GC什么就是收缩空间嘛, 如果没有这个限制,
                             // 就在这里dead loop了
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif
        if (vm.bytesAllocated > vm.nextGC) { collectGarbage(); }
    }
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }
    void* result = realloc(pointer, newSize); // 通过标准库实现
    if (result == NULL) exit(1);
    return result;
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif
    switch (object->type) {
        case OBJ_NATIVE: FREE(ObjNative, object); break;
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            // 这里有两个部分的释放
            // 因为字符串里指向的字符串是创建的
            // 这个字符串示例本身也是先创建空间再构造的
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            // name是ObjString类型的, 在其内部有嵌入式链表, 会自动管理析构
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_CLOSURE: {
            // 依然没有管理底层的ObjFunction, 它有自己的析构管理
            // 而且语义上也是合理的, 因为闭包不应该拥有函数,
            // 一个函数可以被多个闭包捕获
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(ObjUpvalue, object);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* zlass = (ObjClass*)object;
            freeTable(&zlass->methods);
            FREE(ObjClass, object);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            freeTable(&instance->fields);
            FREE(ObjInstance, object);
            break;
        }
        case OBJ_BOUND_METHOD: FREE(ObjBoundMethod, object); break;
        default: break;
    }
}

void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
    free(vm.grayStack);
}

void markObject(Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif
    object->isMarked = true;

    // 下面的栈的扩充使用系统调用, 而不是我们自己封装的,
    // 因为我们不希望这部分被GC
    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack =
            (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
        if (vm.grayStack == NULL)
            exit(1); // 如果创建失败, 则寄,
                     // 当然可以在程序运行之初就为例创建一个内存池
    }

    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
    for (int i = 0; i < array->count; i++) { markValue(array->values[i]); }
}

static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif
    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
            markArray(&function->chunk.constants);
            break;
        }
        case OBJ_UPVALUE: markValue(((ObjUpvalue*)object)->closed); break;
        case OBJ_CLASS: {
            ObjClass* zlass = (ObjClass*)object;
            markObject((Obj*)zlass->name);
            markTable(&zlass->methods);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            markObject((Obj*)instance->klass);
            markTable(&instance->fields);
            break;
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            markValue(bound->receiver);
            markObject((Obj*)bound->method);
            break;
        }
        case OBJ_NATIVE:
        case OBJ_STRING: break;
    }
}

static void markRoots() {
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }
    markTable(&vm.globals);
    for (int i = 0; i < vm.frameCount; i++) {
        markObject((Obj*)vm.frames[i].closure);
    }
    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL;
         upvalue = upvalue->next) {
        markObject((Obj*)upvalue);
    }
    markCompilerRoots();
    markObject((Obj*)vm.initString);
}

static void traceReferences() {
    while (vm.grayCount > 0) {
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

static void sweep() {
    Obj* previous = NULL;
    Obj* object = vm.objects;
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false; // 从黑色变成白色
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else { // 链表首
                vm.objects = object;
            }

            freeObject(unreached);
        }
    }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
#endif

    markRoots();
    traceReferences();
    // 对驻留字符串的特殊处理: 将其从HashSet中删除
    tableRemoveWhite(&vm.strings);
    sweep();
    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf(
        "   collected %zu bytes (from %zu to %zu) next at %zu\n",
        before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
    printf("-- gc end\n");
#endif
}