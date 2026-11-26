#ifndef _SETJMP_H
#define _SETJMP_H

#include <stdint.h>

typedef intptr_t jmp_buf;

#define setjmp(env)       __builtin_setjmp(env)
#define longjmp(env, val) __builtin_longjmp((env), (val) ? (val) : 1)

#endif
