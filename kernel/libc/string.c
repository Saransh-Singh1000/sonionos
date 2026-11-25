#include "string.h"
#include "ctype.h"
#include "errno.h"

#undef errno
int errno = 0;

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n >= 8) {
        *(uint64_t *)d = *(const uint64_t *)s;
        d += 8; s += 8; n -= 8;
    }
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) return memcpy(dst, src, n);
    if (d > s) {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

void *memset(void *dst, int val, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    uint8_t v8 = (uint8_t)val;
    uint64_t v64 = (uint64_t)v8 * 0x0101010101010101ULL;
    while (n >= 8) {
        *(uint64_t *)d = v64;
        d += 8; n -= 8;
    }
    while (n--) *d++ = v8;
    return dst;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;
    while (n--) {
        if (*a != *b) return (int)*a - (int)*b;
        a++; b++;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const uint8_t *p = (const uint8_t *)s;
    while (n--) {
        if (*p == (uint8_t)c) return (void *)p;
        p++;
    }
    return NULL;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

size_t strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && s[len]) len++;
    return len;
}

char *strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return ret;
}

char *strncat(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (*dst) dst++;
    while (n-- && *src) *dst++ = *src++;
    *dst = '\0';
    return ret;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return (int)(uint8_t)*s1 - (int)(uint8_t)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    return n == 0 ? 0 : (int)(uint8_t)*s1 - (int)(uint8_t)*s2;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && (tolower((uint8_t)*s1) == tolower((uint8_t)*s2))) { s1++; s2++; }
    return tolower((uint8_t)*s1) - tolower((uint8_t)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (tolower((uint8_t)*s1) == tolower((uint8_t)*s2))) { s1++; s2++; n--; }
    return n == 0 ? 0 : tolower((uint8_t)*s1) - tolower((uint8_t)*s2);
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && (*h == *n)) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    while (*s && strchr(accept, *s++)) count++;
    return count;
}

size_t strcspn(const char *s, const char *reject) {
    size_t count = 0;
    while (*s && !strchr(reject, *s++)) count++;
    return count;
}

char *strpbrk(const char *s, const char *accept) {
    while (*s) {
        if (strchr(accept, *s)) return (char *)s;
        s++;
    }
    return NULL;
}

char *strtok(char *str, const char *delim) {
    static char *saved = NULL;
    if (str) saved = str;
    if (!saved) return NULL;
    saved += strspn(saved, delim);
    if (!*saved) { saved = NULL; return NULL; }
    char *token = saved;
    saved += strcspn(saved, delim);
    if (*saved) *saved++ = '\0';
    else saved = NULL;
    return token;
}

char *strerror(int errnum) {
    switch (errnum) {
        case 0:            return "Success";
        case EPERM:        return "Operation not permitted";
        case ENOENT:       return "No such file or directory";
        case ESRCH:        return "No such process";
        case EINTR:        return "Interrupted system call";
        case EIO:          return "Input/output error";
        case ENXIO:        return "No such device or address";
        case E2BIG:        return "Argument list too long";
        case ENOEXEC:      return "Exec format error";
        case EBADF:        return "Bad file descriptor";
        case ECHILD:       return "No child processes";
        case EAGAIN:       return "Resource temporarily unavailable";
        case ENOMEM:       return "Cannot allocate memory";
        case EACCES:       return "Permission denied";
        case EFAULT:       return "Bad address";
        case EBUSY:        return "Device or resource busy";
        case EEXIST:       return "File exists";
        case EXDEV:        return "Invalid cross-device link";
        case ENODEV:       return "No such device";
        case ENOTDIR:      return "Not a directory";
        case EISDIR:       return "Is a directory";
        case EINVAL:       return "Invalid argument";
        case ENFILE:       return "Too many open files in system";
        case EMFILE:       return "Too many open files";
        case ENOTTY:       return "Inappropriate ioctl for device";
        case EFBIG:        return "File too large";
        case ENOSPC:       return "No space left on device";
        case ESPIPE:       return "Illegal seek";
        case EROFS:        return "Read-only file system";
        case EMLINK:       return "Too many links";
        case EPIPE:        return "Broken pipe";
        case EDOM:         return "Numerical argument out of domain";
        case ERANGE:       return "Numerical result out of range";
        case EDEADLK:      return "Resource deadlock avoided";
        case ENAMETOOLONG: return "File name too long";
        case ENOSYS:       return "Function not implemented";
        case ENOTEMPTY:    return "Directory not empty";
        case EILSEQ:       return "Invalid or incomplete multibyte or wide character";
        default:           return "Unknown error";
    }
}

