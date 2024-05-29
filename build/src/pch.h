#if !defined(_PCH_H_)
#define _PCH_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <new.h>

#define ZeroThat(x) memset(x, 0, sizeof(*x))
#define ZeroThis() memset(this, 0, sizeof(*x))

#define ALIGN(x, y) ((x + (y - 1)) & ~(y - 1))
#define ALIGN8(x) ALIGN(x, 8)
#define ALIGN16(x) ALIGN(x, 16)

#define KILOBYTES(x) (x * 1024)
#define MEGABYTES(x) (x * 1024 * 1024)
#define GIGABYTES(x) (x * 1024 * 1024 * 1024)

#if defined(_MSC_VER)
#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)
#else
#endif

#define LSTR(x) L ## x
#if !defined(XBR_FORCE_MBC)
#	define STR(x) LSTR(x)
//	For Windows
#	define UNICODE
#	define _UNICODE
//	~~~~~~~~~~~
typedef const wchar_t *str_t;
#else
#	define STR(x) x
#	define MBCS
typedef const char *str_t;
#endif

typedef unsigned int uint;
typedef unsigned char uchar;

typedef long long s64;
typedef long s32;
typedef short s16;
typedef char s8;
typedef unsigned long long u64;
typedef unsigned long u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef float f32;
typedef double f64;

typedef s64 b64;
typedef s32 b32;
typedef s16 b16;
typedef s8 b8;

#endif // _PCH_H_