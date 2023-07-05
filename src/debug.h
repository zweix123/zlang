#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name); // 反汇编所有指令
int disassembleInstruction(Chunk* chunk, int offset);  // 反汇编一个指令

#endif