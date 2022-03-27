#include "lists.h"
#include "hash.h"
#include "output.h"
#include <cstring> 
#include <cstdio> 


StringList::StringList(int h): headerSize(h) {
    blob.growTo(headerSize);
}


void StringList::add(const char* data, int length) {
    Entry* p = (Entry*)blob.growBy(recordSize(length));
    p->length = length;
    memcpy(p->data, data, length);
    memset(p->data + length, 0, 1 + padding(length));
    count++;
}


void StringList::add(const char* data) {
    add(data, strlen(data));
}


FileStateList::FileStateList(int h): headerSize(h) {
    blob.growTo(headerSize);
}


void FileStateList::add(uint64_t tag, const char* name, int length) {
    Entry* p = (Entry*)blob.growBy(recordSize(length));
    p->tag = tag;
    p->length = length;
    memcpy(p->name, name, length);
    memset(p->name + length, 0, 1 + padding(length));
    count++;
}


void FileStateList::add(uint64_t tag, const char* name) {
    add(tag, name, strlen(name));
}


void FileStateDict::initHashTable(int* p, int n) {
    for (int i = 0; i < n; i++) {
        p[i] = -1;
    }
}


FileStateDict::FileStateDict() {
    hashTableSizePower = 6;
    hashTableSize = 1 << hashTableSizePower;
    hashTable = allocHashTable(hashTableSize);
    count = 0;
}


FileStateDict::~FileStateDict() {
    delete[] hashTable;
}


int* FileStateDict::allocHashTable(int n) {
    int* p = new int[n];
    initHashTable(p, n);
    return p;
}


void FileStateDict::clear() {
    blob.clear();
    count = 0;
    initHashTable(hashTable, hashTableSize);
}


void FileStateDict::rehash() {
    int* ht = hashTable;
    const char* start = blob.data;
    for (Iterator i(*this); i; i.next()) {
        int h = i->hash >> (32 - hashTableSizePower);
        int old = ht[h];
        ht[h] = i.pos;
        i->next = old;
    }
}


bool FileStateDict::add(uint64_t tag, const char* name, int length, Entry*& entry) {
    uint32_t h = hash(name, length);
    int slot = h >> (32 - hashTableSizePower);
    int head = hashTable[slot];
    for (int offset = head; offset != -1; ) {
        Entry* e = (Entry*)(blob.data + offset);
        if (e->length == length && memcmp(name, e->name, length) == 0) {
            entry = e;
            return false;
        }
        offset = e->next;
    }
    Entry* e = (Entry*)(blob.growBy(recordSize(length)));
    e->tag = tag;
    e->length = length;
    e->hash = h;
    e->next = head;
    hashTable[slot] = (char*)e - blob.data;
    memcpy(e->name, name, length);
    memset(e->name + length, 0, 1 + padding(length));
    entry = e;
    if (++count > hashTableSize / 2) {
        int* newHashTable = allocHashTable(hashTableSize * 2);
        hashTableSizePower++;
        hashTableSize *= 2;
        delete[] hashTable;
        hashTable = newHashTable;
        rehash();
    }
    return true;
}


bool FileStateDict::add(uint64_t tag, const char* name, Entry*& entry) {
    return add(tag, name, strlen(name), entry);
}


FileStateDict::Entry* FileStateDict::find(const char* name) const {
    return find(name, strlen(name));
}


FileStateDict::Entry* FileStateDict::find(const char* name, int length) const {
    uint32_t h = hash(name, length);
    int slot = h >> (32 - hashTableSizePower);
    for (int offset = hashTable[slot]; offset != -1; ) {
        Entry* e = (Entry*)(blob.data + offset);
        if (e->length == length && memcmp(name, e->name, length) == 0) {
            return e;
        }
        offset = e->next;
    }
    return nullptr;
}


bool Dependencies::load(const char* path) {
    return
        FileStateList::load(path) &&
        blob.size >= sizeof(DepsHeader) &&
        getHeader().isValid();
}


bool Dependencies::save(const char* path) {
    DepsHeader& header = getHeader();
    header.magic = DepsHeader::magicValue;
    header.zero64 = 0;
    return FileStateList::save(path);
}


bool DepsHeader::load(const char* path) {
    return ::load(path, this, sizeof(*this)) && isValid();
}


bool DepsHeader::save(const char* path) {
    magic = magicValue;
    zero64 = 0;
    return ::save(path, this, sizeof(*this));
}


uint32_t getStringListHash(const StringList& list) {
    uint32_t h = 0;
    for (StringList::Iterator i(list); i; i.next()) {
        h = h * 3 + hash(i->data, i->length);
    }
    return h;
}

uint32_t getStringListHash(const StringDict& list) {
    uint32_t h = 0;
    for (StringDict::Iterator i(list); i; i.next()) {
        h = h * 3 + hash(i->name, i->length);
    }
    return h;
}

