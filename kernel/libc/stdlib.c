#include "stdlib.h"
#include "ctype.h"
#include "string.h"
#include "errno.h"

static unsigned long long rng_state = 0x5DEECE66DULL;

void srand(unsigned int seed) {
    rng_state = (unsigned long long)seed ^ 0x5DEECE66DULL;
}

int rand(void) {
    rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (int)((rng_state >> 33) & RAND_MAX);
}

static int char_to_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    while (isspace((uint8_t)*s)) s++;

    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16; s += 2;
    } else if (base == 0) {
        base = (*s == '0') ? 8 : 10;
    }

    unsigned long long acc = 0;
    int any = 0, d;
    while ((d = char_to_digit(*s)) >= 0 && d < base) {
        acc = acc * (unsigned long long)base + (unsigned long long)d;
        any = 1;
        s++;
    }

    if (endptr) *endptr = (char *)(any ? s : nptr);
    return neg ? (unsigned long long)(-(long long)acc) : acc;
}

long long strtoll(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    while (isspace((uint8_t)*s)) s++;
    int neg = (*s == '-');
    unsigned long long val = strtoull(nptr, endptr, base);
    return neg ? -(long long)val : (long long)val;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    return (unsigned long)strtoull(nptr, endptr, base);
}

long strtol(const char *nptr, char **endptr, int base) {
    return (long)strtoll(nptr, endptr, base);
}

int atoi(const char *s) { return (int)strtoll(s, NULL, 10); }
long atol(const char *s) { return (long)strtoll(s, NULL, 10); }
long long atoll(const char *s) { return strtoll(s, NULL, 10); }

double strtod(const char *nptr, char **endptr) {
    const char *s = nptr;
    while (isspace((uint8_t)*s)) s++;

    double sign = 1.0;
    if (*s == '-') { sign = -1.0; s++; }
    else if (*s == '+') { s++; }

    double val = 0.0;
    int any = 0;
    while (isdigit((uint8_t)*s)) {
        val = val * 10.0 + (*s - '0');
        any = 1;
        s++;
    }

    if (*s == '.') {
        s++;
        double frac = 0.1;
        while (isdigit((uint8_t)*s)) {
            val += (*s - '0') * frac;
            frac *= 0.1;
            any = 1;
            s++;
        }
    }

    if (any && (*s == 'e' || *s == 'E')) {
        s++;
        int exp_sign = 1;
        if (*s == '-') { exp_sign = -1; s++; }
        else if (*s == '+') { s++; }
        int exp = 0;
        while (isdigit((uint8_t)*s)) {
            exp = exp * 10 + (*s - '0');
            s++;
        }
        double p10 = 1.0;
        while (exp--) p10 *= 10.0;
        val = (exp_sign > 0) ? (val * p10) : (val / p10);
    }

    if (endptr) *endptr = (char *)(any ? s : nptr);
    return sign * val;
}

float strtof(const char *nptr, char **endptr) { return (float)strtod(nptr, endptr); }
double atof(const char *s) { return strtod(s, NULL); }

static void swap_bytes(uint8_t *a, uint8_t *b, size_t size) {
    while (size--) {
        uint8_t tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (!base || nmemb < 2 || size == 0) return;
    uint8_t *arr = (uint8_t *)base;

    // Shell/Comb-assisted In-Place Sort (zero recursion stack overflow risk in Ring 0)
    size_t gap = nmemb;
    int swapped = 1;
    while (gap > 1 || swapped) {
        gap = (gap * 10) / 13;
        if (gap < 1) gap = 1;
        swapped = 0;
        for (size_t i = 0; i + gap < nmemb; i++) {
            uint8_t *a = arr + i * size;
            uint8_t *b = arr + (i + gap) * size;
            if (compar(a, b) > 0) {
                swap_bytes(a, b, size);
                swapped = 1;
            }
        }
    }
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    const uint8_t *arr = (const uint8_t *)base;
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const void *elem = arr + mid * size;
        int cmp = compar(key, elem);
        if (cmp == 0) return (void *)elem;
        if (cmp < 0) hi = mid;
        else lo = mid + 1;
    }
    return NULL;
}

int abs(int j) { return j < 0 ? -j : j; }
long labs(long j) { return j < 0 ? -j : j; }
long long llabs(long long j) { return j < 0 ? -j : j; }

div_t div(int numer, int denom) { div_t r = { numer / denom, numer % denom }; return r; }
ldiv_t ldiv(long numer, long denom) { ldiv_t r = { numer / denom, numer % denom }; return r; }
lldiv_t lldiv(long long numer, long long denom) { lldiv_t r = { numer / denom, numer % denom }; return r; }

#define MAX_ATEXIT 32
static void (*atexit_funcs[32])(void);
static int atexit_count = 0;

int atexit(void (*func)(void)) {
    if (atexit_count >= MAX_ATEXIT) return -1;
    atexit_funcs[atexit_count++] = func;
    return 0;
}

void exit(int status) {
    (void)status;
    while (atexit_count > 0) {
        atexit_funcs[--atexit_count]();
    }
    while (1) __asm__ volatile ("hlt");
}

void abort(void) {
    while (1) __asm__ volatile ("hlt");
}

char *getenv(const char *name) { (void)name; return NULL; }
int system(const char *command) { (void)command; return -1; }


