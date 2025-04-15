#include "time.h"

#include "string.h"
#include "../drivers/tty.h"

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

calendar_t time_calendar(uint32_t epoch)
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

    calendar_t new;
    new.year = year;
    new.month = month;
    new.day = days;

    new.hour = hours;
    new.minute = minutes;
    new.second = seconds;

    return new;
}

char *convert_time(uint32_t epoch, char *buf)
{
    calendar_t time = time_calendar(epoch);
    sprintf(buf, "%4d-%2d-%2d %2d:%2d:%2d", time.year, time.month, time.day, time.hour, time.minute, time.second);
    return buf;
}

uint32_t make_time(uint16_t year, uint8_t month, uint8_t days, uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    uint32_t epoch = 0;

    while (year > 1970)
    {
        days += days_for_year(year);
        year--;
    }
    printf("%d %d\n", year, days);

    return 10;
}