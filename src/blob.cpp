#include "blob.h"
#include "output.h"

#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


Blob::Blob(const Blob& other): data(nullptr), size(0), allocated(0) {
    data = new char[other.allocated]; 
    allocated = other.allocated;
    size = other.size;
}


Blob& Blob::operator=(const Blob& other) {
    delete[] data;
    data = nullptr;
    size = allocated = 0;
    data = new char[other.allocated]; 
    allocated = other.allocated;
    size = other.size;
    memcpy(data, other.data, other.size);
    return *this;
}


char* Blob::growTo(int n, bool retain) {
    int oldSize = size;
    if (allocated < n) {
        int newAlloc = allocated * 2;
        if (newAlloc < n) {
            newAlloc = (n + 4095) & ~4095;
        }
        char* p = new char[newAlloc];
        if (retain) {
            memcpy(p, data, size);
        }
        delete[] data;
        data = p;
        allocated = newAlloc;
    }
    size = n;
    return data + oldSize;
}


void Blob::add(const void* data, int len) {
    memcpy(growBy(len), data, len);
}


void Blob::add(const char* data) {
    add(data, strlen(data));
}


bool Blob::save(const char* path) {
    return ::save(path, data, size);
}


bool Blob::load(const char* path) {
    bool ok = false;
    struct stat s;
    if (stat(path, &s) == 0) {
       int fileSize = s.st_size;
       growTo((fileSize + 256) & ~63);
       ok = ::load(path, data, fileSize);
       size = fileSize;
       data[size] = 0;
    }
    if (!ok) {
        size = 0;
    }
    return ok;
}


bool load(const char* path, void* data, int size) {
    bool ok = false;
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        while (size) {
            int n = read(fd, data, size);
            if (n <= 0) {
                break;
            }
            size -= n;
            data = (char*)data + n;
        }
        close(fd);
        ok = size == 0;
    }
    return ok;
}


bool save(const char* path, const void* data, int size) {
    bool ok = false;
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd >= 0) {
        while (size) {
            int n = write(fd, data, size);
            if (n <= 0) {
                break;
            }
            size -= n;
            data = (const char*)data + n;
        }
        close(fd);
        ok = size == 0;
    }
    return ok;
}

