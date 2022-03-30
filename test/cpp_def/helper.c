#include <stdlib.h>

// FLAG is only defined for C++ (cxx_options)
void helper(void) {
    #ifdef FLAG
        abort();
    #endif
}
