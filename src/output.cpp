#include "output.h"

#include <cstdio>
#include <cstdarg> 

int logLevel = logLevelInfo;

std::mutex outputMutex;

const char* em   = "\x1b[1m";
const char* noem = "\x1b[22m";

const char* prefix[3] = { 
    "\x1b[31m*** ERROR: ",
    "",
    "\x1b[2m",
};

const char* suffix[3] = { 
    "\x1b[22;0m",
    "",
    "\x1b[0m",
};

void say(int level, const char* format, ...) {
    std::lock_guard<std::mutex> lock(outputMutex);
    fprintf(stderr, "%s", prefix[level]);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "%s\n", suffix[level]);
}


Blob output;


void delayedError(const char* format, ...) {
    std::lock_guard<std::mutex> lock(outputMutex);
    char* buf = new char[8 * 1024];
    char* p = buf;
    p += sprintf(p, "%s", prefix[logLevelError]);
    va_list args;
    va_start(args, format);
    p += vsprintf(p, format, args);
    va_end(args);
    p += sprintf(p, "%s\n", suffix[logLevelError]);
    int len = p - buf;
    p = output.growBy(len);
    memcpy(p, buf, len);
    delete[] buf;
}

void delayedError(StringList& list) {
    std::lock_guard<std::mutex> lock(outputMutex);
    for (StringList::Iterator i(list); i && output.size < 1024 * 1024; i.next()) {
        char* p = output.growBy(i->length + 1);
        memcpy(p, i->data, i->length);
        p[i->length] = '\n';
    }
}

void delayedErrorFlush() {
    std::lock_guard<std::mutex> lock(outputMutex);
    fwrite(output.data, output.size, 1, stderr);
    output.clear();
}


void printOutput(StringList& list) {
    std::lock_guard<std::mutex> lock(outputMutex);
    for (StringList::Iterator i(list); i; i.next()) {
        fprintf(stderr, "%s\n", i->data);
    }
}

