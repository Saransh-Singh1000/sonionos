#ifndef _STDDEF_H
#define _STDDEF_H

typedef unsigned long long size_t;
typedef long long ptrdiff_t;
#ifndef __cplusplus
typedef unsigned short wchar_t;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif

