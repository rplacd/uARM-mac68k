#ifndef _TYPES_H_
#define _TYPES_H_

#include "compiler_hacks.h"
#include <stdint.h>


typedef uint32_t UInt32;
typedef int32_t Int32;
typedef uint16_t UInt16;
typedef int16_t Int16;
typedef uint8_t UInt8;
typedef int8_t Int8;
typedef unsigned char Err;
typedef unsigned char Boolean;

#define true	1
#define false	0

#define TYPE_CHECK ((sizeof(UInt32) == 4) && (sizeof(UInt16) == 2) && (sizeof(UInt8) == 1))

#define errNone		0x00
#define errInternal	0x01

/* runtime stuffs */
void err_str(const char* str);
void err_hex(UInt32 val);
void err_dec(UInt32 val);
void __mem_zero(void* mem, UInt16 len);
UInt32 rtcCurTime(void);
void* emu_alloc(UInt32 size);
void emu_free(void* ptr);
void __mem_copy(void* d, const void* s, UInt32 sz);

#endif

