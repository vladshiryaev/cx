#include "mul.h"
#include "lib_add/add.h"

int mul(int a, int b) {
    int sum = 0;
    for (int i = 1; i <= a; i++) {
        sum = add(sum, b);
    }
    return sum;
}

void circular_dependency() {}
