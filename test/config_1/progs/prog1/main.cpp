#include "common/libs/lib1/header.h"

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    // Common config.
    #if !defined(foo) || !defined(bar)
        return 1; 
    #endif

    if (argc < 2) {
        return 1;
    }

    if (strcmp(argv[1], "release") == 0) {
        // If invoked with "release" arg, expect to be compiled for "relese" configuration.
        #if !defined(NDEBUG) || !defined(__RELEASE) || defined(__DEBUG)
            return 1;
        #endif
    }
    else if (strcmp(argv[1], "debug") == 0) {
        // If invoked with "debug" arg, expect to be compiled for "debug" configuration.
        #if defined(NDEBUG) || !defined(__DEBUG) || defined(__RELEASE)
            return 1;
        #endif
    }
    else {
        printf("Please run with 'debug' or 'release'.\n");
        return 1;
    }

    test(argv[1]); // Do similar checks in a sub-unit.

    // Everything as expected.
    printf("OK\n");
}

