#include "hash.h"
#include <cstring>

// Not married to it...
uint32_t hash(const char* p, int length) {
    uint32_t h = length;
    for (int i = 0; i < length; i++) {
        h = h * 101 + p[i];
    }
    return h * 0x9e3779b9; // N high bits must be suitable for hashing.
}


uint32_t hash(const char* p) {
    return hash(p, strlen(p));
}
