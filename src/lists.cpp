#include "lists.h"
#include "output.h"

#include <cstring> 
#include <cstdio> 


// Putting trivial wrappers here, don't want them to be inlined.

void StringList::add(const char* data, int length) {
    allocate(data, length);
}


void StringList::add(const char* data) {
    add(data, strlen(data));
}


void FileStateList::add(uint64_t tag, const char* name, int length) {
    Entry* p = allocate(name, length);
    p->tag = tag;
}


void FileStateList::add(uint64_t tag, const char* name) {
    add(tag, name, strlen(name));
}


bool FileStateDict::add(uint64_t tag, const char* name, int length, Entry*& entry) {
    if (insert(name, length, entry)) {
        entry->tag = tag;
        return true;
    }
    return false;
}


bool FileStateDict::add(uint64_t tag, const char* name, Entry*& entry) {
    return add(tag, name, strlen(name), entry);
}


FileStateDict::Entry* FileStateDict::find(const char* name) const {
    return find(name, strlen(name));
}


bool StringDict::add(const char* name, int length, Entry*& entry) {
    return insert(name, length, entry);
}


bool StringDict::add(const char* name, Entry*& entry) {
    return add(name, strlen(name), entry);
}


StringDict::Entry* StringDict::find(const char* name) const {
    return find(name, strlen(name));
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
        h = h * 3 + hash(i->string, i->length);
    }
    return h;
}


uint32_t getStringListHash(const StringDict& list) {
    uint32_t h = 0;
    for (StringDict::Iterator i(list); i; i.next()) {
        h = h * 3 + hash(i->string, i->length);
    }
    return h;
}

