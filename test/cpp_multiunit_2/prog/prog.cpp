#include <cstdio>
#include "lib_add/add.h" 
#include "lib_sub/sub.h" 
#include "lib_mul/mul.h" 



int main() {
    if (sub(add(1, 2), 1) != 2) {
        return 1;
    }
    if (mul(2, 3) != 6) {
        return 1;
    }
    printf("OK\n");
}


