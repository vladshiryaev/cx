#include <stdio.h>
#include <string.h>

int main() {
    #if defined(FLAG) && defined(FLAG2)
        if (strcmp(FLAG, "   ") == 0 && FLAG2 == ' ') {
            printf("OK\n");
        }
    #endif
}

