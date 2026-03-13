#ifndef KERNEL_PRINTF_H
#define KERNEL_PRINTF_H

#include <stdarg.h>

typedef void (*fnprintf_putc)(int c);

// Common code for all printf implementations.
void fnvprintf(fnprintf_putc putc, const char* fmt, va_list args);

#endif // KERNEL_PRINTF_H
