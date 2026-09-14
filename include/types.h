#ifndef PW_TYPES_H
#define PW_TYPES_H

#include <limits.h>

/* Scalar types for firmware arithmetic and stored records. */
#if defined(PW_RENESAS_H8) || defined(__H8__) || defined(__HITACHI__)
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned long u32;
typedef signed long s32;
#else
#include <stdint.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
#endif

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;

/* Native arithmetic type. Its width and promotions follow the compiler's int.
 */
typedef unsigned int uint;

/* C90 compile-time checks enforce the required storage widths. */
typedef char ByteMustBe8Bits[CHAR_BIT == 8 ? 1 : -1];
typedef char U8MustBe8Bits[sizeof(u8) * CHAR_BIT == 8 ? 1 : -1];
typedef char S8MustBe8Bits[sizeof(s8) * CHAR_BIT == 8 ? 1 : -1];
typedef char U16MustBe16Bits[sizeof(u16) * CHAR_BIT == 16 ? 1 : -1];
typedef char S16MustBe16Bits[sizeof(s16) * CHAR_BIT == 16 ? 1 : -1];
typedef char U32MustBe32Bits[sizeof(u32) * CHAR_BIT == 32 ? 1 : -1];
typedef char S32MustBe32Bits[sizeof(s32) * CHAR_BIT == 32 ? 1 : -1];

typedef int Bool;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#endif
