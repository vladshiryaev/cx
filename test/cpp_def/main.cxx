#include <stdio.h>

extern "C" void helper();

int main() {

    helper();

    #ifdef FLAG
        printf("OK\n");
    #endif
}

