#include "inet.h"
#include "../bits.h"

uint64_t htonll(uint64_t hostlonglong)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return switch_endian_64(hostlonglong);
#else
    return hostlonglong;
#endif
}

uint32_t htonl(uint32_t hostlong)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return switch_endian_32(hostlong);
#else
    return hostlong;
#endif
}

uint16_t htons(uint16_t hostshort)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return switch_endian_16(hostshort);
#else
    return hostshort;
#endif
}

uint64_t ntohll(uint64_t netlonglong)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return switch_endian_64(netlonglong);
#else
    return netlonglong;
#endif
}

uint32_t ntohl(uint32_t netlong)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return switch_endian_32(netlong);
#else
    return netlong;
#endif
}

uint16_t ntohs(uint16_t netshort)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return switch_endian_16(netshort);
#else
    return netshort;
#endif
}
