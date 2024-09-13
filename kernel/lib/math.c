#include "math.h"

uint32_t pow(int16_t x, uint16_t y)
{
    uint32_t temp;
    if (y == 0)
        return 1;

    temp = pow(x, y / 2);
    
    if ((y % 2) == 0)
        return temp * temp;
    else
        return x * temp * temp;
}