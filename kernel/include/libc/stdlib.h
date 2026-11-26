#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>
#include <stdint.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX     0x7FFFFFFF

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;

void   mem_init(uintptr_t heap_start, size_t heap_size);
void  *malloc(size_t size);
void  *calloc(size_t num, size_t size);
void  *realloc(void *ptr, size_t new_size);
void   free(void *ptr);
size_t mem_get_used(void);
size_t mem_get_free(void);

int                atoi(const char *s);
long               atol(const char *s);
long long          atoll(const char *s);
double             atof(const char *s);
long               strtol(const char *nptr, char **endptr, int base);
unsigned long      strtoul(const char *nptr, char **endptr, int base);
long long          strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
double             strtod(const char *nptr, char **endptr);
float              strtof(const char *nptr, char **endptr);

int       rand(void);
void      srand(unsigned int seed);
void      qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void     *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
int       abs(int j);
long      labs(long j);
long long llabs(long long j);
div_t     div(int numer, int denom);
ldiv_t    ldiv(long numer, long denom);
lldiv_t   lldiv(long long numer, long long denom);

void      abort(void);
void      exit(int status);
int       atexit(void (*func)(void));
char     *getenv(const char *name);
int       system(const char *command);

#endif
