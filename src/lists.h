#pragma once

#include "blob.h"
#include "hash.h"

#include <cstdint> 
#include <cstring> 


// The point:
// 1. A structure like FileStateList is loaded from a file in a single read() call.
// 2. It is usable immediately, without a special building/population step.
// Checking dependencies of an artifact when nothing has really changed is probably
// the most critical step perf-wise.
// 3. Memory is contigous. Iteration is in insertion order. Insertions don't invalidate
// iterators. There is no deletion.


// A string of variable size + some data of fixed size.

template <class EntryType>
class StringContainerBase {
public:
    using Entry = EntryType;
    StringContainerBase(int h): headerSize(h) { blob.growTo(h); }
    int getCount() const { return count; }
    void clear() { blob.size = headerSize; count = 0; }
    int isEmpty() const { return count == 0; }
    const Entry* get(int offset) const { return (Entry*)(blob.data + offset); }
    bool load(const char* path) { return blob.load(path); }
    bool save(const char* path) { return blob.save(path); }
protected:    
    int headerSize; // For file headers.
    Blob blob;
    int count = 0;
    static int payloadSize(int length) { return sizeof(Entry) + length + 1; }
    static int recordSize(int length) { return (payloadSize(length) + Entry::alignment - 1) & ~(Entry::alignment - 1); }
    static int padding(int length) { int n = payloadSize(length) & (Entry::alignment - 1); return n ? (Entry::alignment - n) : 0; }
    class Iterator {
    public:
        using Container = StringContainerBase<EntryType>;
        Iterator(const Container& container): blob(&container.blob), pos(container.headerSize) { validate(); }
        Entry* operator->() const { return (Entry*)(blob->data + pos); }
        operator int() const { return pos; }
        operator bool() const { return pos >= 0; }
        void next() {
            if (pos >= 0) {
                Entry* entry = (Entry*)(blob->data + pos);
                pos += Container::recordSize(entry->length);
                validate();
            } 
        }
    private:
        const Blob* blob;
        int pos;
        void validate() {
            if (pos + sizeof(Entry) < blob->size) {
                Entry* entry = (Entry*)(blob->data + pos);
                if (pos + Container::recordSize(entry->length) <= blob->size) {
                    return;
                }
            }
            pos = -1;
        }
    };
    Entry* allocate(const char* data, int length) {
        Entry* p = (Entry*)blob.growBy(recordSize(length));
        p->length = length;
        memcpy(p->string, data, length);
        memset(p->string + length, 0, 1 + padding(length));
        count++;
        return p;
    }
};

// The same with index (hash table).

template <class EntryType>
class IndexedStringContainerBase: public StringContainerBase<EntryType> {
public:
    using Entry = typename StringContainerBase<EntryType>::Entry;
    using Iterator = typename StringContainerBase<EntryType>::Iterator;
    IndexedStringContainerBase(int headerSize = 0): StringContainerBase<EntryType>(headerSize) {
        hashTableSizePower = 6;
        hashTableSize = 1 << hashTableSizePower;
        hashTable = allocHashTable(hashTableSize);    
    }
    ~IndexedStringContainerBase() {
        delete[] hashTable;
    }
    void clear() {
        StringContainerBase<EntryType>::clear();
        initHashTable(hashTable, hashTableSize);
    }
    Entry* find(const char* name, int length) const {
        uint32_t h = hash(name, length);
        int slot = h >> (32 - hashTableSizePower);
        for (int offset = hashTable[slot]; offset != -1; ) {
            Entry* e = (Entry*)(this->blob.data + offset);
            if (e->length == length && memcmp(name, e->string, length) == 0) {
                return e;
            }
            offset = e->next;
        }
        return nullptr;
    } 
protected:
    int* hashTable;
    int hashTableSizePower;
    int hashTableSize;
    static int recordSize(int length) { return StringContainerBase<EntryType>::recordSize(length); }
    static int padding(int length) { return StringContainerBase<EntryType>::padding(length); }
    static int* allocHashTable(int n) {
        int* p = new int[n];
        initHashTable(p, n);
        return p;
    }
    static void initHashTable(int* p, int n) {
        for (int i = 0; i < n; i++) {
            p[i] = -1;
        }
    }
    void rehash() {
        int* ht = hashTable;
        const char* start = this->blob.data;
        for (Iterator i(*this); i; i.next()) {
            int h = i->hash >> (32 - hashTableSizePower);
            int old = ht[h];
            ht[h] = i;
            i->next = old;
        }
    }
    bool insert(const char* name, int length, Entry*& entry) {
        uint32_t h = hash(name, length);
        int slot = h >> (32 - hashTableSizePower);
        int head = hashTable[slot];
        for (int offset = head; offset != -1; ) {
            Entry* e = (Entry*)(this->blob.data + offset);
            if (e->length == length && memcmp(name, e->string, length) == 0) {
                entry = e;
                return false;
            }
            offset = e->next;
        }
        Entry* e = (Entry*)(this->blob.growBy(recordSize(length)));
        e->length = length;
        e->hash = h;
        e->next = head;
        hashTable[slot] = (char*)e - this->blob.data;
        memcpy(e->string, name, length);
        memset(e->string + length, 0, 1 + padding(length));
        entry = e;
        if (++this->count > hashTableSize / 2) {
            int* newHashTable = allocHashTable(hashTableSize * 2);
            hashTableSizePower++;
            hashTableSize *= 2;
            delete[] hashTable;
            hashTable = newHashTable;
            rehash();
        }
        return true;
    }
        
};


// A simple grow-only list of strings, contiguous in memory.

struct __attribute__((__packed__)) StringListEntry {
    static constexpr int alignment = 2; // Must be a power of 2.
    short length;
    char string[];
};

class StringList: public StringContainerBase<StringListEntry> {
public:
    StringList(int headerSize = 0): StringContainerBase<StringListEntry>(headerSize) {}
    void add(const char*);
    void add(const char*, int length);
    using Iterator = StringContainerBase<StringListEntry>::Iterator;
};


// Strings with 64-bit tags.

struct __attribute__((__packed__)) FileStateListEntry {
    static constexpr int alignment = 8;
    uint64_t tag;
    // Repeating common/expected fields may be avoided, but it
    // may give some freedom with packaging, so doing it.
    short length;
    char string[];
};


// File paths with tags.

class FileStateList: public StringContainerBase<FileStateListEntry> {
public:
    FileStateList(int headerSize = 0): StringContainerBase<FileStateListEntry>(headerSize) {}
    void add(uint64_t tag, const char* name);
    void add(uint64_t tag, const char* name, int length);
    using Iterator = StringContainerBase<FileStateListEntry>::Iterator;
};


// Like above but indexed.

struct __attribute__((__packed__)) FileStateDictEntry {
    static constexpr int alignment = 8;
    uint64_t tag;
    uint32_t hash; // Raw full-sized hash. Never recalculated (we just use more bits as table grows).
    int next; // In collision list, or -1.
    short length;
    char string[];
};

class FileStateDict: public IndexedStringContainerBase<FileStateDictEntry> {
public:
    FileStateDict(): IndexedStringContainerBase<FileStateDictEntry>() {}
    bool add(uint64_t tag, const char* name, Entry*&);
    bool add(uint64_t tag, const char* name, int length, Entry*&);
    Entry* put(uint64_t tag, const char* name) { Entry* e; add(tag, name, e); e->tag = tag; return e; }
    Entry* put(uint64_t tag, const char* name, int length) { Entry* e; add(tag, name, length, e); e->tag = tag; return e; }
    Entry* put(const char* name) { return put(0, name); }
    Entry* put(const char* name, int length) { return put(0, name, length); }
    Entry* find(const char* name) const;
    Entry* find(const char* name, int length) const { return IndexedStringContainerBase<FileStateDictEntry>::find(name, length); }
};


// A simple set of strings.

struct __attribute__((__packed__)) StringDictEntry {
    static constexpr int alignment = 4;
    uint32_t hash;
    int next;
    short length;
    char string[];
};

class StringDict: public IndexedStringContainerBase<StringDictEntry> {
public:
    StringDict(): IndexedStringContainerBase<StringDictEntry>() {}
    bool add(const char* name, Entry*&);
    bool add(const char* name, int length, Entry*&);
    Entry* find(const char* name) const;
    Entry* find(const char* name, int length) const { return IndexedStringContainerBase<StringDictEntry>::find(name, length); }
};


// A simple dependency file without an explicit list of dependencies (just their combined "tag").
// (they are implicitly defined by something else, like a list of source files in unit directory)

struct DepsHeader {  // 32 bytes.
    static constexpr uint32_t magicValue = 0x000055FF;
    uint32_t magic;
    uint32_t toolTag; // Like compiler type/version.
    uint32_t optTag; // Command arguments.
    uint8_t flags;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
    uint64_t inputsTag; // All inputs combined.
    uint64_t zero64;
    void clear() {
        memset(this, 0, sizeof(*this));
        magic = magicValue;
    }
    DepsHeader() { clear(); }
    bool isValid() const { return magic == magicValue; }
    bool load(const char* path);
    bool save(const char* path);
};


class Dependencies: public FileStateList {
public:
    Dependencies(): FileStateList(sizeof(DepsHeader)) { getHeader().clear(); }
    DepsHeader& getHeader() const { return *(DepsHeader*)blob.data; }
    bool load(const char* path);
    bool save(const char* path);
};


uint32_t getStringListHash(const StringList&);
uint32_t getStringListHash(const StringDict&);

