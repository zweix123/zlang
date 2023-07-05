#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"
#include "bytecode.h"

// 字节码存储
typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    // 以上三个实现一个vector存储字节码
    int* lines;
    // 这个数组维护上面字节码数组对应索引的字节码在源代码中的行号
    ValueArray constants; // 常量池, 也是一个vector
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);

#endif