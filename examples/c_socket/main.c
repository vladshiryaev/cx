#include <stdio.h>
#include <sys/socket.h>

int main() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s >= 0) {
        printf("OK\n");
    }
}

