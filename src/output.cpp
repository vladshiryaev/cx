#include "output.h"

#include <cstdio>
#include <cstdarg> 
#include <unistd.h>

int logLevel = logLevelInfo;

static std::mutex outputMutex;


const char* emColor   = "\x1b[1m";
const char* noemColor = "\x1b[22m";

static const char* prefixColor[3] = { 
    "\x1b[31m*** ERROR: ",
    "",
    "\x1b[2m",
};

static const char* suffixColor[3] = { 
    "\x1b[22;0m",
    "",
    "\x1b[0m",
};


const char* em   = "";
const char* noem = "";

static const char* prefix[3] = { "", "", "", };

static const char* suffix[3] = { "", "", "", };


void say(int level, const char* format, ...) {
    std::lock_guard<std::mutex> lock(outputMutex);
    fprintf(stderr, "%s", prefix[level]);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "%s\n", suffix[level]);
}


static Blob output;


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
        memcpy(p, i->string, i->length);
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
        fprintf(stderr, "%s\n", i->string);
    }
}


bool colorEnabled = false;


void setColor(ColorMode mode) {
    switch (mode) {
        case colorNever: colorEnabled = false; break;
        case colorAlways: colorEnabled = true; break;
        case colorAuto: colorEnabled = isatty(fileno(stderr)); break;
    }
    if (colorEnabled) {
        em = emColor;
        noem = noemColor;
        prefix[logLevelError] = prefixColor[logLevelError];
        prefix[logLevelInfo ] = prefixColor[logLevelInfo ];
        prefix[logLevelDebug] = prefixColor[logLevelDebug];
        suffix[logLevelError] = prefixColor[logLevelError];
        suffix[logLevelInfo ] = prefixColor[logLevelInfo ];
        suffix[logLevelDebug] = prefixColor[logLevelDebug];
    }
    else {
        em = "";
        noem = "";
        prefix[logLevelError] = "";
        prefix[logLevelInfo ] = "";
        prefix[logLevelDebug] = "";
        suffix[logLevelError] = "";
        suffix[logLevelInfo ] = "";
        suffix[logLevelDebug] = "";
    }
}


struct Init {
    Init() {
        setColor(colorAuto);
    }
};

Init init;

