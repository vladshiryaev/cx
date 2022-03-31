#pragma once

// A chunk of memory.
class Blob {
public:
    char* data;
    int size;
    int allocated;
    Blob(int n = 1024);
    ~Blob() { delete[] data; }
    Blob(const Blob&);
    Blob& operator=(const Blob&);
    void clear() { size = 0; }
    char* growTo(int n, bool retainData = true);
    char* growBy(int n) { return growTo(size + n, true); }
    void add(const void* data, int size);
    void add(const char* data);
    bool save(const char* path);
    bool load(const char* path);
};

bool save(const char* path, const void*, int size);
bool load(const char* path, void*, int size);
