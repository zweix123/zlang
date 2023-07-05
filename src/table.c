#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

void copyTable(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) { tableSet(to, entry->key, entry->value); }
    }
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;
    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            // 键为NULL, 空
            if (IS_NIL(entry->value)) {
                // 值为NIL, 真的空桶
                return tombstone != NULL ? tombstone : entry;
            } else {
                // 值不为NIL(那就是BOOL-true), 墓碑
                if (tombstone == NULL) tombstone = entry;
            }
            // 为什么不选择遇到墓碑就直接返回呢?
            // 因为这个键是在哈希表上顺序存储, 而墓碑会造成hile,
            // 如果不继续走会让我们以为该key不存在, 实际上在后面
            // 这样才是真的找到最后都没有是真的没有, 返回墓碑供使用
            // 但是这有个锅, 有没有可能整个哈希表都是被使用的桶和墓碑?
            // (这样不会造成扩容)
            // 这个问题要这样看
            // 1. 在加入键值对时检测桶是否是真的空, 真的空才增加
            // 2. 在删除键值对时并没有减少count
            // 所以墓碑被视为满桶了, 会触发扩容
            // 而在扩容转移时会舍弃所有墓碑
            // 优雅
        } else if (entry->key == key) {
            return entry;
        }
        index = (index + 1) % capacity;
    }
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(Table* table, int capacity) {
    // create and init
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
        // 初始化为键为NILL，值为NIL_VAL
    }
    // copy: 重走find路
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;
        Entry* dest = findEntry(entries, capacity, entry->key); // !!!
        dest->key = entry->key;
        table->count++;
        dest->value = entry->value;
    }
    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->value)) table->count++;
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
    // use tombstones
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    // 墓碑键为NULL，但值为BOOL_VAL(true)

    return true;
}

void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) { tableSet(to, entry->key, entry->value); }
    }
}

ObjString*
tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;
    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) return NULL; // 真的空桶
            // 这里还有一个分支是墓碑(虽然虚拟机不会删除), 冷处理了
        } else if (
            entry->key->length == length
            && entry->key->hash == hash // 先比较len和hash, 更快
            && memcmp(entry->key->chars, chars, length) == 0) {
            return entry->key;
        }
        index = (index + 1) % table->capacity;
    }
}

void tableRemoveWhite(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

void markTable(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject((Obj*)entry->key);
        markValue(entry->value);
    }
}