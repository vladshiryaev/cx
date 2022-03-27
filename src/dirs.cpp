#include "dirs.h"
#include "output.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <cstdlib>
#include <cstring>


static int checkPathLength(int len) {
    if (len > maxPath - 16) {
        PANIC("Path is too long for this program");
    }
    return len;
}


bool directoryExists(const char* path) {
    struct stat s;
    return stat(path, &s) == 0 && S_ISDIR(s.st_mode); 
}

bool fileExists(const char* path) {
    struct stat s;
    return stat(path, &s) == 0 && S_ISREG(s.st_mode); 
}

bool makeDirectory(const char* path) {
    return mkdir(path, 0777) == 0;
}

bool deleteFile(const char* path) {
    return remove(path) == 0;
}

char* catPath(const char* dir, const char* name, char* path) {
    return catPath(dir, strlen(dir), name, strlen(name), path);
}

char* catPath(const char* dir, int dirLength, const char* name, char* path) {
    return catPath(dir, dirLength, name, strlen(name), path);
}

char* catPath(const char* dir, int dirLength, const char* name, int nameLength, char* path) {
    checkPathLength(dirLength + nameLength);
    if (dirLength) {
        memcpy(path, dir, dirLength);
        if (path[dirLength - 1] != '/') {
            path[dirLength] = '/';
            dirLength++;
        }
    }
    if (nameLength) {
        memcpy(path + dirLength, name, nameLength);
    }
    path[dirLength + nameLength] = 0;
    return path;
}


char* addSuffix(const char* source, const char* suffix, char* path) {
    int len = checkPathLength(strlen(source));
    memmove(path, source, len);
    strcpy(path + len, suffix);
    return path;
}


bool isAbsPath(const char* path) {
    return path[0] == '/';
}


char* rebasePath(const char* baseDir, const char* relPath, char* path) {
    char temp[maxPath];
    if (isAbsPath(relPath)) {
        strcpy(temp, relPath);
    }
    else {
        catPath(baseDir, strlen(baseDir), relPath, temp);
    }
    return normalizePath(temp, path);
}


char* stripBasePath(const char* baseDir, const char* srcPath, char* path) {
    int baseLen = strlen(baseDir);
    if (baseLen) {
        int srcLen = strlen(srcPath);
        if (baseLen <= srcLen && memcmp(baseDir, srcPath, baseLen) == 0) {
            if (baseLen == srcLen) {
                path[0] = 0;
                return path;
            }
            else if (baseDir[baseLen - 1] == '/') {
                strcpy(path, srcPath + baseLen);
                return path;
            }
            else if (srcPath[baseLen] == '/') {
                strcpy(path, srcPath + baseLen + 1);
                return path;
            }
        }
    }
    strcpy(path, srcPath);
    return path;
}



const char* getSuffix(const char* path) {
    const char* lastDot = nullptr;
    const char* lastSlash = nullptr;
    for ( ; *path; path++) {
        char c = *path;
        if (c == '.') {
            lastDot = path;
        }
        else if (c == '/') {
            lastSlash = path;
        }
    }
    if (lastDot && (!lastSlash || lastDot > lastSlash)) {
        return lastDot;
    }
    return path;
}


bool splitPath(const char* path, char* dir, char* name) {
    const char* lastSlash = nullptr;
    for (const char* p = path; *p; p++) {
        if (*p == '/') {
            lastSlash = p;
        }
    }
    if (!lastSlash) {
        return false;
    }
    int dirLen = checkPathLength(lastSlash + 1 - path);
    memmove(dir, path, dirLen);
    dir[dirLen] = 0;
    strcpy(name, lastSlash + 1);
    return true;
}


int getDirectoryPartLength(const char* path) {
    const char* lastSlash = nullptr;
    for (const char* p = path; *p; p++) {
        if (*p == '/') {
            lastSlash = p;
        }
    }
    return lastSlash ? (lastSlash + 1 - path) : 0;
}


const char* getDirectory(const char* path, char* dir) {
    int len = getDirectoryPartLength(path);
    memmove(dir, path, len);
    dir[len] = 0;
    return dir;
}


char* normalizePath(const char* in, char* out) {
    const int stackSize = maxPath / 2 + 1;
    short stack[stackSize];
    int top = 0;
    int i = 0;
    int j = 0;
    for (;;) {
        int start = i;
        while (in[i] && in[i] != '/') i++;
        int len = i - start;
        if (in[start] == '.') {
            if (len == 1) {
                goto next;
            }
            else if (len == 2 && in[start + 1] == '.' && top) {
                j = stack[--top];
                goto next;
            }
        }
        if (len || j == 0) {
            stack[top++] = j;
            memcpy(out + j, in + start, len + 1);
            j += len + 1;
        }
    next:
        if (in[i] == 0) {
            out[j] = 0;
            return out;
        }
        i++;
    }
}


void Directory::close() {
    if (dir) {
        closedir((DIR*)dir);
        dir = nullptr;
    }
}


bool Directory::open(const char* dirPath) {
    close();
    dir = opendir(dirPath);
    if (!dir) {
        path[0] = 0;
        pathLength = 0;
        return false;
    }
    strncpy(path, dirPath, maxPath - 1);
    path[maxPath - 1] = 0;
    pathLength = strlen(path);
    return true;
}


static uint64_t makeFileTag(const struct stat* s) {
    uint64_t tag = s->st_size + (uint64_t(s->st_mtime) << 32);
    return tag >= 256 ? tag : (tag + 256); // Small values are reserved. E.g. 0 may mean file does not exist.
}


uint64_t makeFileTag(const char* path) {
    struct stat s;
    return stat(path, &s) == 0 ? makeFileTag(&s) : 0; 
}


bool Directory::read(Entry& entry, bool full) {
    if (!dir) {
       return false;  
    }
    for (;;) {
        struct dirent* e = readdir((DIR*)dir);
        if (!e) {
            return false;
        }
        if (e->d_name[0] == '.') {
            continue;
        }
        switch (e->d_type) {
            case DT_DIR: entry.type = typeDirectory; break;
            case DT_REG: entry.type = typeFile; break;
            case DT_LNK: entry.type = typeLink; break;
            default: continue; // entry.type = typeLink; break;
        }
        entry.name = e->d_name;
        entry.size = 0;
        entry.time = 0;
        entry.tag = 0;
        if (full) {
            char fullName[maxPath];
            catPath(path, entry.name, fullName);
            struct stat s;
            if (stat(fullName, &s) == 0) {
                entry.size = s.st_size;
                entry.time = s.st_mtime;
                entry.tag = makeFileTag(&s);
            }
        }
        return true;
    }
}



bool changeDirectory(const char* dir) {
    return chdir(dir) == 0;
}


char* getCurrentDirectory(char* dir) {
    if (!getcwd(dir, maxPath - 2)) {
        PANIC("Path is too long");
    }
    int len = strlen(dir);
    if (len && dir[len - 1] != '/') {
        dir[len] = '/';
        dir[len + 1] = 0;
    }
    return dir;
}


bool setVariable(const char* name, const char* value) {
    return setenv(name, value, 1) == 0;
}


char* getVariable(const char* name) {
    return getenv(name);
}

