#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code =
            GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines =
            GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int addConstant(Chunk* Chunk, Value value) {
    // 这里, 这个Value可能是在整个obj链上的, 但是此时它在C栈上,
    // 即Lox中它是一个孤岛, 而下面有vector的add, 可能触发gc,
    // 此时gc会清楚这个Value, 所有这里将其放到虚拟机栈上使之被引用不会被清除.
    push(value);
    writeValueArray(&Chunk->constants, value);
    pop();
    return Chunk->constants.count - 1;
}