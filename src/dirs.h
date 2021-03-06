#pragma once

#include <cstdint>
#include <ctime>

// Keep it simple.
const int maxPath = 1024;

// A 'tag' is a shallow fingerprint of a file, like a (mostly lossless)
// combination of timestamp and size. So like Make, except we don't care about
// ordering of time, and the tags of inputs must be saved at the moment of
// building a derivative.
uint64_t makeFileTag(const char* path);


// For scanning directories.
class Directory {
public:
    enum Type {
        typeFile,
        typeDirectory,
        typeLink,
        typeOther
    };
    struct Entry {
        Type type;
        const char* name;
        uint64_t size;
        time_t time;
        uint64_t tag;
    };
    Directory() {}
    Directory(const char* path): Directory() { open(path); }
    ~Directory() { close(); }
    operator bool() const { return dir != nullptr; }
    bool open(const char* path);
    void close();
    bool read(Entry&, bool full = true);

private:
    void* dir = nullptr;
    char path[maxPath];
    int pathLength;
};

// Some path utilities.

bool directoryExists(const char*);
bool fileExists(const char*);
bool makeDirectory(const char*);
bool deleteFile(const char*);
bool setVariable(const char*, const char*);
char* getVariable(const char*);

char* catPath(const char* dir, const char* name, char* path);
char* catPath(const char* dir, int dirLength, const char* name, char* path);
char* catPath(const char* dir, int dirLength, const char* name, int nameLength, char* path);
char* addSuffix(const char* source, const char* suffix, char* path);
bool isAbsPath(const char* path);
char* rebasePath(const char* baseDir, const char* relPath, char* path);
char* stripBasePath(const char* baseDir, const char* srcPath, char* path);

const char* getSuffix(const char* path);
bool splitPath(const char* path, char* dir, char* name);
int getDirectoryPartLength(const char* path);
const char* getDirectory(const char* path, char* dir);
char* normalizePath(const char* in, char* out);
char* getParentDirectory(const char* in, char* out);

bool changeDirectory(const char*);
char* getCurrentDirectory(char*);


