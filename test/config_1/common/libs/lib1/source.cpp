#include "header.h"

#include <cstdlib>
#include <cstring>

void test(const char* arg) {
    // Common config.
    #if !defined(foo) || !defined(bar)
        exit(1); 
    #endif

    if (strcmp(arg, "release") == 0) {
        // If invoked with "release" arg, expect to be compiled for "relese" configuration.
        #if !defined(NDEBUG) || !defined(__RELEASE) || defined(__DEBUG)
            exit(1); 
        #endif
    }
    else if (strcmp(arg, "debug") == 0) {
        // If invoked with "debug" arg, expect to be compiled for "debug" configuration.
        #if defined(NDEBUG) || !defined(__DEBUG) || defined(__RELEASE)
            exit(1); 
        #endif
    }
    else {
        exit(1); 
    }
}

