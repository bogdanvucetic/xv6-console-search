#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H

#include <stdint.h>

typedef uint32_t  uint;
typedef uint16_t  ushort;
typedef uint8_t   uchar;
typedef uint64_t  uint64;

typedef uint64 segdesc;
typedef uint64 gatedesc;
typedef uint   pde_t;
typedef uint   pte_t;

#endif // KERNEL_TYPES_H
