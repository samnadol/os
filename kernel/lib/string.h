#ifndef LIB_STR_H
#define LIB_STR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

int strcmp(char s1[], char s2[]);
void strreverse(char *str, int length);
void strappend(char *s, char n);
int strlen(const char *s);
void strlower(char *s);
char *strpadstart(char *buf, uint16_t desired_length, char padding);

bool char_is_small_letter(char c);
bool char_is_number(char c);

char *itoa(uint32_t num, char *buf, uint8_t base);
char *human_readable_size(uint64_t size, char *buf, int bufsize);

void *memset(void *ptr, int value, size_t num);
void *memcpy(void *to, void *from, size_t n);

size_t sprintf(char *dst, const char *fmt, ...);

#endif