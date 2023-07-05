#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"

#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * (count))
#define FREE(type, pointer)   reallocate(pointer, sizeof(type), 0)
// vector的扩张规则
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)
// 扩展vector(自动内容转移)
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(                                \
        pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))
// 释放vector的空间
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
/*
| oldSize |       newSize        |        Operation                        |
|---------|----------------------|-----------------------------------------|
|   0     |      Non‑zero        | Allocate new block. 分配新块              |
|Non‑zero |         0            | Free allocation. 释放已分配内存            |
|Non‑zero | Smaller than oldSize | Shrink existing allocation. 收缩已分配内存 |
|Non‑zero | Larger than oldSize  | Grow existing allocation. 增加已分配内存   |
*/
void freeObjects();

void markObject(Obj* object);
void markValue(Value value);
void collectGarbage();

#endif