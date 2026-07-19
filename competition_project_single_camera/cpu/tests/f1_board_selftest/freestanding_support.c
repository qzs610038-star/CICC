#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

void *memset(void *destination, int value, size_t count)
{
    unsigned char *out = (unsigned char *)destination;
    while (count-- != 0u) *out++ = (unsigned char)value;
    return destination;
}

void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *out = (unsigned char *)destination;
    const unsigned char *in = (const unsigned char *)source;
    while (count-- != 0u) *out++ = *in++;
    return destination;
}

static void append_char(char *out, size_t size, size_t *position, char value)
{
    if (*position + 1u < size) out[*position] = value;
    (*position)++;
}

static void append_text(char *out, size_t size, size_t *position, const char *text)
{
    if (text == 0) text = "(null)";
    while (*text != '\0') append_char(out, size, position, *text++);
}

static void append_uint(char *out, size_t size, size_t *position, unsigned long value)
{
    char digits[10];
    unsigned count = 0u;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && count < sizeof(digits));
    while (count != 0u) append_char(out, size, position, digits[--count]);
}

int snprintf(char *out, size_t size, const char *format, ...)
{
    va_list args;
    size_t position = 0u;
    va_start(args, format);
    while (*format != '\0') {
        int long_value = 0;
        if (*format != '%') {
            append_char(out, size, &position, *format++);
            continue;
        }
        format++;
        if (*format == 'l') { long_value = 1; format++; }
        if (*format == 'u') append_uint(out, size, &position,
            long_value ? va_arg(args, unsigned long) : (unsigned long)va_arg(args, unsigned int));
        else if (*format == 's') append_text(out, size, &position, va_arg(args, const char *));
        else if (*format == '%') append_char(out, size, &position, '%');
        else append_char(out, size, &position, '?');
        if (*format != '\0') format++;
    }
    va_end(args);
    if (size != 0u) out[position < size ? position : size - 1u] = '\0';
    return (int)position;
}
