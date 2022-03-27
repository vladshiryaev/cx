#pragma once

#include <cstdint> 
#include <cstring> 
#include "blob.h"

class StringList {
public:
    struct __attribute__((__packed__)) Entry {
        short length;
        char data[1];
    };
    StringList(int headerSize = 0);
    void clear() { blob.size = headerSize; count = 0; }
    void add(const char*);
    void add(const char*, int length);
    int getCount() const { return count; }
    int isEmpty() const { return count == 0; }
    const Entry* getString(int offset) const { return (Entry*)(blob.data + offset); }
    bool load(const char* path) { return blob.load(path); }
    bool save(const char* path) { return blob.save(path); }
    class Iterator {
    public:
        Iterator(const StringList& list): blob(&list.blob), pos(list.headerSize) { validate(); }
        Entry* operator->() const { return (Entry*)(blob->data + pos); }
        operator bool() const { return pos >= 0; }
        void next() {
            if (pos >= 0) {
                Entry* entry = (Entry*)(blob->data + pos);
                pos += recordSize(entry->length);
                validate();
            } 
        }
    private:
        const Blob* blob;
        int pos;
        void validate() {
            if (pos + entryFixedSize < blob->size) {
                Entry* entry = (Entry*)(blob->data + pos);
                if (pos + recordSize(entry->length) <= blob->size) {
                    return;
                }
            }
            pos = -1;
        }
    };
private:
    int headerSize;
    Blob blob;
    int count = 0;
    static constexpr int entryFixedSize = sizeof(short);
    static int payloadSize(int length) { return entryFixedSize + length + 1; }
    static int recordSize(int length) { return (payloadSize(length) + 1) & ~1; }
    static int padding(int length) { int n = payloadSize(length) & 1; return n ? (2 - n) : 0; }
};


class FileStateList {
public:
    struct __attribute__((__packed__)) Entry {
        uint64_t tag;
        short length;
        char name[1];
    };
    FileStateList(int headerSize = 0);
    void clear() { blob.size = headerSize; count = 0; }
    void add(uint64_t tag, const char* name);
    void add(uint64_t tag, const char* name, int length);
    int getCount() const { return count; }
    int isEmpty() const { return count == 0; }
    bool load(const char* path) { return blob.load(path); }
    bool save(const char* path) { return blob.save(path); }
    class Iterator {
    public:
        Iterator(const FileStateList& list): blob(&list.blob), pos(list.headerSize) { validate(); }
        Entry* operator->() const { return (Entry*)(blob->data + pos); }
        operator bool() const { return pos >= 0; }
        void next() {
            if (pos >= 0) {
                Entry* entry = (Entry*)(blob->data + pos);
                pos += recordSize(entry->length);
                validate();
            } 
        }
    private:
        const Blob* blob;
        int pos;
        void validate() {
            if (pos + entryFixedSize < blob->size) {
                Entry* entry = (Entry*)(blob->data + pos);
                if (pos + recordSize(entry->length) <= blob->size) {
                    return;
                }
            }
            pos = -1;
        }
    };
protected:
    int headerSize;
    Blob blob;
    int count = 0;
    static constexpr int entryFixedSize = sizeof(uint64_t) + sizeof(short);
    static int payloadSize(int length) { return entryFixedSize + length + 1; }
    static int recordSize(int length) { return (payloadSize(length) + 7) & ~7; }
    static int padding(int length) { int n = payloadSize(length) & 7; return n ? (8 - n) : 0; }
};


class FileStateDict {
public:
    struct __attribute__((__packed__)) Entry {
        uint64_t tag;
        uint32_t hash; // Raw full-size hash.
        int next; // In collision list, or -1.
        uint16_t length;
        char name[1];
    };
    FileStateDict();
    ~FileStateDict();
    void clear();
    bool add(uint64_t tag, const char* name, Entry*&);
    bool add(uint64_t tag, const char* name, int length, Entry*&);
    Entry* put(uint64_t tag, const char* name) { Entry* e; if (!add(tag, name, e)) e->tag = tag; return e; }
    Entry* put(uint64_t tag, const char* name, int length) { Entry* e; if (!add(tag, name, length, e)) e->tag = tag; return e; }
    Entry* put(const char* name) { return put(0, name); }
    Entry* put(const char* name, int length) { return put(0, name, length); }
    Entry* find(const char* name) const;
    Entry* find(const char* name, int length) const;
    int getCount() const { return count; }
    int isEmpty() const { return count == 0; }
    bool load(const char* path) { return blob.load(path); }
    bool save(const char* path) { return blob.save(path); }
    class Iterator {
    public:
        Iterator(const FileStateDict& list): blob(&list.blob), pos(0) { validate(); }
        Entry* operator->() const { return (Entry*)(blob->data + pos); }
        operator bool() const { return pos >= 0; }
        void next() {
            if (pos >= 0) {
                Entry* entry = (Entry*)(blob->data + pos);
                pos += recordSize(entry->length);
                validate();
            } 
        }
    private:
        friend class FileStateDict;
        const Blob* blob;
        int pos;
        void validate() {
            if (pos + entryFixedSize < blob->size) {
                Entry* entry = (Entry*)(blob->data + pos);
                if (pos + recordSize(entry->length) <= blob->size) {
                    return;
                }
            }
            pos = -1;
        }
    };
private:
    Blob blob;
    int* hashTable;
    int hashTableSizePower;
    int hashTableSize;
    int count;
    static constexpr int entryFixedSize = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(int) + sizeof(uint16_t);
    static int payloadSize(int length) { return entryFixedSize + length + 1; }
    static int recordSize(int length) { return (payloadSize(length) + 7) & ~7; }
    static int padding(int length) { int n = payloadSize(length) & 7; return n ? (8 - n) : 0; }
    static int* allocHashTable(int n);
    static void initHashTable(int*, int n);
    void rehash();
};


using StringDict = FileStateDict;


struct DepsHeader {  // 32 bytes.
    static constexpr uint32_t magicValue = 0x000055FF;
    uint32_t magic;
    uint32_t toolTag;
    uint32_t optTag;
    uint8_t flags;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
    uint64_t inputsTag;
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

