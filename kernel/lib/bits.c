#include "bits.h"

inline uint16_t switch_endian_16(uint16_t val) {
    return (val << 8)
        | (val >> 8);
}

inline uint32_t switch_endian_32(uint32_t val) {
    return (val << 24)
        | ((val << 8) & 0x00FF0000)
        | ((val >> 8) & 0x0000FF00)
        | (val >> 24);
}

inline uint64_t switch_endian_64(uint64_t val) {
    return (val << 56)
        | ((val << 40) & 0x00FF000000000000)
        | ((val << 24) & 0x0000FF0000000000)
        | ((val << 8) & 0x000000FF00000000)
        | ((val >> 8) & 0x00000000FF000000)
        | ((val >> 24) & 0x0000000000FF0000)
        | ((val >> 40) & 0x000000000000FF00)
        | (val >> 56);
}