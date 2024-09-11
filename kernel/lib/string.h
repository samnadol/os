#ifndef LIB_STR_H
#define LIB_STR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

int strlen(const char *s);
int strcmp(char *s1, char *s2);
int strncmp(char *s1, char *s2, size_t n);

size_t strfindstr(char *s, char *delim);
size_t strfindchar(char *s, char delim);

//  returns the a new string truncated to length n
char *strcut(char *s, size_t n); 
// replaces all occurances of needle in os with replacement
char *strrep(char *os, char *needle, char *replacement);
// adds padding to buf until it is of length desired_length
char *strpadstart(char *buf, uint16_t desired_length, char padding);

// modifies string s to be all lowercase
void strlower(char *s);
// modifies string s to be in reverse order
void strreverse(char *str, int length);
// modifies string s by adding char n to end. string s MUST be long enough to hold the extra char
void strappend(char *s, char n);

// true if char is a lowercase letter, else false
bool char_is_small_letter(char c);
// true if char is numeric, else false
bool char_is_number(char c);

// converts num to a string, using base base and outputting into buf. buf must be big enough to hold the result
char *itoa(uint32_t num, char *buf, uint8_t base);
// converts size into a human readable size (eg 1G, 32M), placing the result in to buf. will not fill characters past bufsize
char *human_readable_size(uint64_t size, char *buf, int bufsize);

void *memset(void *ptr, int value, size_t num);
void *memcpy(void *to, void *from, size_t n);

size_t sprintf(char *dst, const char *fmt, ...);

#endif