#include "string.h"

#include "../hw/mem.h"
#include "../hw/cpu/system.h"

void *memset(void *ptr, int value, size_t num)
{
    int d0;
    int d1;

    asm volatile(
        "rep stosb"
        : "=&c"(d0), "=&D"(d1)
        : "a"((uint8_t)value), "1"(ptr), "0"(num)
        : "memory");

    return ptr;
}

void *memcpy(void *to, void *from, size_t n)
{
    size_t i = 0;
    void *ret = to;

    for (i = 0; i < n / 8; i++)
    {
        *(uint64_t *)to = *(uint64_t *)from;

        from += 8;
        to += 8;
    }

    n -= i * 8;

    for (i = 0; i < n / 4; i++)
    {
        *(uint32_t *)to = *(uint32_t *)from;

        from += 4;
        to += 4;
    }

    n -= i * 4;

    for (i = 0; i < n / 2; i++)
    {
        *(uint16_t *)to = *(uint16_t *)from;

        from += 2;
        to += 2;
    }

    n -= i * 2;

    uint8_t *c_src = (uint8_t *)from;
    uint8_t *c_dest = (uint8_t *)to;

    while (n--)
    {
        *c_dest++ = *c_src++;
    }

    return ret;
}

void swapptr(char *a, char *b)
{
    if (!a || !b)
        return;

    char temp = *(a);
    *(a) = *(b);
    *(b) = temp;
}

bool char_is_small_letter(char c)
{
    return (c >= 'a' && c <= 'z');
}

bool char_is_number(char c)
{
    return (c >= '0' && c <= '9');
}

int strlen(const char *s)
{
    int i = 0;
    while (s[i] != '\0')
    {
        ++i;
    }
    return i;
}

void strlower(char *s)
{
    for (int i = 0; s[i]; i++)
    {
        s[i] = s[i] >= 'A' && s[i] <= 'Z' ? s[i] | 0x60 : s[i];
    }
}

void strappend(char *s, char n)
{
    int len = strlen(s);
    s[len] = n;
    s[len + 1] = '\0';
}

int strcmp(char s1[], char s2[])
{
    int i;
    for (i = 0; s1[i] == s2[i]; i++)
    {
        if (s1[i] == '\0')
            return 0;
    }
    return s1[i] - s2[i];
}

void strreverse(char *str, int length)
{
    int start = 0;
    int end = length - 1;
    while (start < end)
    {
        swapptr((str + start), (str + end));
        start++;
        end--;
    }
}

char *itoa(uint32_t num, char *buf, uint8_t base)
{
    size_t i = 0;

    if (num == 0)
    {
        buf[i++] = '0';
        buf[i] = '\0';
        return buf;
    }

    uint32_t rem = 0;
    while (num != 0)
    {
        rem = num % base;
        buf[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        num = num / base;
    }

    buf[i] = '\0';
    strreverse(buf, i);

    return buf;
}

char *strpadstart(char *buf, uint16_t desired_length, char padding)
{
    int16_t length_change = desired_length - strlen(buf);
    char *ret;
    int i;

    if (length_change < 0)
    {
        ret = malloc(strlen(buf) + 1);
        if (!ret)
            panic("malloc failed (str_padstart 1)");

        for (i = 0; i < strlen(buf); i++)
            ret[i] = buf[i];
    }
    else
    {
        ret = malloc((desired_length * sizeof(char)) + 1);
        if (!ret)
            panic("malloc failed (str_padstart 2)");

        for (i = 0; i < length_change; i++)
            ret[i] = padding;
        for (; i < desired_length; i++)
            ret[i] = buf[i - length_change];
        ret[desired_length] = '\0';
    }

    mfree(buf);
    return ret;
}

char *human_readable_time(uint64_t s, char *buf, int bufsize)
{
    size_t loc = 0;
    memset(buf, 0, bufsize);

    // days
    if (s > 86399)
    {
        itoa(s / 86400, buf + loc, 10);
        strappend(buf, 'd');
        strappend(buf, ' ');
        loc = strlen(buf);
        s %= 86400;
    }

    // hours
    if (s > 3599)
    {
        itoa(s / 3600, buf + loc, 10);
        strappend(buf, 'h');
        strappend(buf, ' ');
        loc = strlen(buf);
        s %= 3600;
    }

    // minutes
    if (s > 59)
    {
        itoa(s / 60, buf + loc, 10);
        strappend(buf, 'm');
        strappend(buf, ' ');
        loc = strlen(buf);
        s %= 60;
    }

    // seconds
    if (s > 0)
    {
        itoa(s, buf + loc, 10);
        strappend(buf, 's');
        strappend(buf, ' ');
        loc = strlen(buf);
    }

    if (buf[loc] == ' ')
        buf[loc] = 0;
    return buf;
}

char *human_readable_size(uint64_t size, char *buf, int bufsize)
{
    memset(buf, 0, bufsize);
    if (size > ((uint64_t)1024 * 1024 * 1024 * 10))
    {
        // gb
        itoa(size / 1024 / 1024 / 1024, buf, 10);
        strappend(buf, 'g');
    }
    else if (size > (1024 * 1024 * 10))
    {
        // mb
        itoa(size / 1024 / 1024, buf, 10);
        strappend(buf, 'm');
    }
    else if (size > (1024) * 10)
    {
        // kb
        itoa(size / 1024, buf, 10);
        strappend(buf, 'k');
    }
    else
    {
        // b
        itoa(size, buf, 10);
    }
    strappend(buf, 'b');

    return buf;
}

size_t sputs(char *dst, char *s)
{
    int i;

    for (i = 0; i < strlen(s); i++)
        *((char *)(dst + i)) = s[i];

    return i;
}

size_t sputc(char *dst, char c)
{
    char s[2];
    s[0] = c;
    s[1] = 0;

    return sputs(dst, s);
}

size_t sprintf(char *dst, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    char buf[32];
    size_t char_printed = 0;
    for (int i = 0; i < strlen(fmt); i++)
    {
        if (fmt[i] == '%')
        {
            i++;
            switch (fmt[i])
            {
            case 'd':
                int32_t a = va_arg(ap, int32_t);
                if (a < 0)
                {
                    sputc(dst + char_printed, '-');
                    a *= -1;
                    char_printed += 1;
                }
                char_printed += sputs(dst + char_printed, itoa(a, buf, 10));
                break;
            case 'u':
                char_printed += sputs(dst + char_printed, itoa(va_arg(ap, uint32_t), buf, 10));
                break;
            case 'o':
                char_printed += sputs(dst + char_printed, itoa(va_arg(ap, uint32_t), buf, 8));
                break;
            case 'x':
                char_printed += sputs(dst + char_printed, itoa(va_arg(ap, uint32_t), buf, 16));
                break;
            case 'b':
                char_printed += sputs(dst + char_printed, itoa(va_arg(ap, uint32_t), buf, 2));
                break;
            case 'c':
                char_printed += sputc(dst + char_printed, va_arg(ap, int));
                break;
            case 's':
                char_printed += sputs(dst + char_printed, va_arg(ap, char *));
                break;
            case 'p':
                char_printed += sputs(dst + char_printed, itoa((uintptr_t)va_arg(ap, void *), buf, 16));
                break;
            case 'n':
                *va_arg(ap, int32_t *) = char_printed;
                break;
            case '%':
                char_printed += sputc(dst + char_printed, '%');
                break;
            case 'm':
                uint8_t *mac = va_arg(ap, uint8_t *);
                char_printed += sprintf(dst + char_printed, "%x:%x:%x:%x:%x:%x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                break;
            case 'i':
                uint32_t ip = va_arg(ap, uint32_t);
                char_printed += sprintf(dst + char_printed, "%d.%d.%d.%d", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip >> 0) & 0xFF);
                break;
            case 'f':
                human_readable_size(va_arg(ap, uint32_t), buf, 32);
                char_printed += sputs(dst + char_printed, buf);
                break;
            case 't':
                human_readable_time(va_arg(ap, uint32_t), buf, 32);
                char_printed += sputs(dst + char_printed, buf);
                break;
            default:
                va_arg(ap, void *);
                char_printed += sputs(dst + char_printed, "(?)");
            }
        }
        else
        {
            char_printed += sputc(dst + char_printed, fmt[i]);
        }
    }

    dst[char_printed] = 0;

    va_end(ap);
    return char_printed;
}
