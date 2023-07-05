#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
    ObjString* key;
    Value value;
} Entry;

// 初始化为key指向NULL, value为NIL
// 墓碑是key为NULL, value是BOOL的true

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
void copyTable(Table* from, Table* to); // copy

bool tableGet(Table* table, ObjString* key, Value* value); // 返回值通过value
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);

void tableAddAll(Table* from, Table* to);

ObjString*
tableFindString(Table* table, const char* chars, int length, uint32_t hash);

void tableRemoveWhite(Table* table);
void markTable(Table* table);

#endif