#ifndef BITS_H
#define BITS_H

#include <stdint.h>

uint16_t switch_endian_16(uint16_t val);
uint32_t switch_endian_32(uint32_t val);
uint64_t switch_endian_64(uint64_t val);

#define GET_BYTE(reg, byte) ((reg >> byte) & 0xFF)

#define GET_BIT(reg, bit) (((reg) >> (bit)) & 1)
#define SET_BIT(reg, bit) ((reg) | (1 << bit))

#define DISABLE_BIT(reg, bit) ((reg) & ~(1 << bit))
#define ENABLE_BIT(reg, bit) ((reg) | (1 << bit))
#define TOGGLE_BIT(reg, bit) ((reg) ^ (1 << bit))
#define ISSET_BIT(reg, bit) (((reg) & (1 << bit)) != 0)

#define DISABLE_BIT_INT(reg, bit) ((reg) & ~(bit))
#define ENABLE_BIT_INT(reg, bit) ((reg) | (bit))
#define TOGGLE_BIT_INT(reg, bit) ((reg) ^ (bit))
#define ISSET_BIT_INT(reg, bit) (((reg) & (bit)) != 0)

#endif