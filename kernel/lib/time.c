#include "time.h"

#include "string.h"

inline bool is_leap_year(uint16_t year)
{
    return (year % 400 == 0) || ((year % 4) == 0 && (year % 100 != 0));
}

inline uint16_t days_for_year(uint16_t year)
{
    return is_leap_year(year) ? 366 : 365;
}

uint8_t days_for_month(uint16_t year, uint8_t month)
{
    switch (month)
    {
    case 2:
        return is_leap_year(year) ? 29 : 28;
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    default:
        return 0;
    }
}

char *convert_time(uint32_t epoch, char *buf)
{
    uint32_t days = epoch / (60 * 60 * 24);
    epoch %= (60 * 60 * 24);

    uint16_t year = 1970;
    while (days > days_for_year(year))
    {
        days -= days_for_year(year);
        year++;
    }

    uint8_t month = 1;
    while (days > days_for_month(year, month))
    {
        days -= days_for_month(year, month);
        month++;
    }

    days++;

    uint8_t hours = 0, minutes = 0, seconds = 0;
    hours = epoch / 3600;
    epoch %= 3600;
    minutes = epoch / 60;
    epoch  %= 60;
    seconds = epoch;

    sprintf(buf, "%d-%d-%d %d:%d:%d", year, month, days, hours, minutes, seconds);
    return buf;
}