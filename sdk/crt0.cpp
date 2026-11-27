#include <stddef.h>
#include "usyscall.h"

extern "C" void *malloc(size_t size);
extern "C" void free(void *ptr);
extern "C" int main(void);

void *operator new(size_t n) { return malloc(n); }
void *operator new[](size_t n) { return malloc(n); }
void operator delete(void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }
extern "C" void __cxa_pure_virtual(void) { while (1); } extern "C" void __main(void) {}

extern "C" __attribute__((section(".text._start")))
void _start(void) { sys_exit(main()); }


