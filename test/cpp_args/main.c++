#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc == 2 && strcmp(argv[1], "OK") == 0) {
        printf("OK\n");
    }
}

