#ifndef _SETJMP_H
#define _SETJMP_H

#include <stdint.h>

typedef uint64_t jmp_buf[10];

int  setjmp(jmp_buf env);
__attribute__((noreturn)) void longjmp(jmp_buf env, int val);

#endif

