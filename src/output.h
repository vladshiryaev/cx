#pragma once

#include <cstdlib>
#include <mutex>
#include "lists.h" 

void delayedError(const char* format, ...);
void delayedError(StringList&);
void delayedErrorFlush();
void printOutput(StringList&);

enum {
    logLevelError,
    logLevelInfo,
    logLevelDebug,
};

void say(int level, const char* format, ...);

extern int logLevel;

#define LOG(level, ...) do { \
    if ((level) <= logLevel) { \
        say(level, __VA_ARGS__); \
    } \
} while (0)

#define ERROR(...) LOG(logLevelError, __VA_ARGS__)
#define FAILURE(...) LOG(logLevelError, __VA_ARGS__)
#define PANIC(...) do { FAILURE(__VA_ARGS__); delayedErrorFlush(); _Exit(1); } while (0)
#define INFO(...) LOG(logLevelInfo, __VA_ARGS__)
#define TRACE(...) LOG(logLevelDebug, __VA_ARGS__)

extern const char* em;
extern const char* noem;
