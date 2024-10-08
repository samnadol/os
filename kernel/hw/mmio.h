#ifndef HW_MMIO_H
#define HW_MMIO_H

#include <stdint.h>

uint8_t mmio_read8(uint64_t *p_address);
uint16_t mmio_read16(uint64_t *p_address);
uint32_t mmio_read32(uint64_t *p_address);
uint64_t mmio_read64(uint64_t *p_address);

void mmio_write8(uint64_t *p_address, uint8_t p_value);
void mmio_write16(uint64_t *p_address, uint16_t p_value);
void mmio_write32(uint64_t *p_address, uint32_t p_value);
void mmio_write64(uint64_t *p_address, uint64_t p_value);

#endif