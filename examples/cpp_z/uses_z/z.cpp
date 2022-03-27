#include "z.h"
#include "zlib.h"
#include <cstring>

bool z() {
    z_stream strm;
    std::memset(&strm, 0, sizeof(z_stream));
    int ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    return ret == Z_OK;
}

