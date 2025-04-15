#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;

    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} calendar_t;

calendar_t time_calendar(uint32_t epoch);
char *convert_time(uint32_t epoch, char *buf);
uint32_t make_time(uint16_t year, uint8_t month, uint8_t days, uint8_t hours, uint8_t minutes, uint8_t seconds);