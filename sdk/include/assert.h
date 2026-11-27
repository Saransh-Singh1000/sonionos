#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) do { if (!(expr)) { while (1) __asm__ volatile ("hlt"); } } while (0)
#endif

#endif
