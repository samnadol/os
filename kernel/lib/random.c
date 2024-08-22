#include "random.h"

uint32_t next = 1;

// generates random number between 0 and RAND_MAX
uint32_t rand(uint32_t RAND_MAX)
{
    next = next * 1103515245 + 12345;
    return (uint32_t)(next / 65536) % RAND_MAX;
}