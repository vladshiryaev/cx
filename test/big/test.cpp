#include "1/1/h11.h"
#include "1/2/h12.h"
#include "2/1/h21.h"
#include "2/2/h22.h"
#include "3/1/h31.h"
#include "3/2/h32.h"


bool test() {
    return
        f11() + f12() +
        f21() + f22() +
        f31() + f32()
        ==
        11 + 12 +
        21 + 22 +
        31 + 32;
}
