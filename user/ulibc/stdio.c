#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include "../usyscall.h"


FILE __sonion_stdin  = { 1, 0, 0, 0, 0, -1, 1, 0 };
FILE __sonion_stdout = { 1, 0, 0, 0, 0, -1, 0, 1 };
FILE __sonion_stderr = { 1, 0, 0, 0, 0, -1, 0, 1 };

static FILE file_table[FOPEN_MAX];

typedef struct {
    char  *buf;
    size_t cap;
    size_t pos;
} FmtOut;

static void fmt_putc(FmtOut *o, char c) {
    if (o->buf && o->pos + 1 < o->cap) {
        o->buf[o->pos] = c;
    }
    o->pos++;
}

static void fmt_pad(FmtOut *o, char ch, int count) {
    for (int i = 0; i < count; i++) fmt_putc(o, ch);
}

static void fmt_uint(FmtOut *o, uint64_t val, int base, int upper,
                     int width, int prec, int flags, char sign_ch) {
    char tmp[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int len = 0;

    if (val == 0 && prec != 0) {
        tmp[len++] = '0';
    } else {
        while (val > 0) {
            tmp[len++] = digits[val % (uint64_t)base];
            val /= (uint64_t)base;
        }
    }

    int zeros = (prec > len) ? (prec - len) : 0;
    int prefix_len = (sign_ch ? 1 : 0);
    if ((flags & 8) && base == 16) prefix_len += 2;
    if ((flags & 8) && base == 8 && zeros == 0 && len > 0) prefix_len += 1;

    int total = prefix_len + zeros + len;
    int pad = (width > total) ? (width - total) : 0;

    if (!(flags & 1) && !(flags & 4)) fmt_pad(o, ' ', pad);
    if (sign_ch) fmt_putc(o, sign_ch);
    if ((flags & 8) && base == 16) {
        fmt_putc(o, '0');
        fmt_putc(o, upper ? 'X' : 'x');
    } else if ((flags & 8) && base == 8 && zeros == 0 && len > 0) {
        fmt_putc(o, '0');
    }
    if (!(flags & 1) && (flags & 4)) fmt_pad(o, '0', pad);
    fmt_pad(o, '0', zeros);
    while (len > 0) fmt_putc(o, tmp[--len]);
    if (flags & 1) fmt_pad(o, ' ', pad);
}

static void fmt_float(FmtOut *o, double val, int width, int prec, int flags, int is_g) {
    if (prec < 0) prec = 6;
    if (is_g && prec == 0) prec = 1;
    if (prec > 9) prec = 9;
    char sign_ch = 0;
    if (val < 0.0) { sign_ch = '-'; val = -val; }
    else if (flags & 2) sign_ch = '+';
    else if (flags & 16) sign_ch = ' ';

    if (isnan(val) || isinf(val)) {
        const char *s = isnan(val) ? "nan" : "inf";
        if (sign_ch) fmt_putc(o, sign_ch);
        while (*s) fmt_putc(o, *s++);
        return;
    }

    double scale = 1.0;
    for (int i = 0; i < prec; i++) scale *= 10.0;
    uint64_t scaled = (uint64_t)(val * scale + 0.5);
    uint64_t iscale = (uint64_t)scale;
    uint64_t ipart  = scaled / iscale;
    uint64_t fpart  = scaled % iscale;

    char ibuf[24];
    int ilen = 0;
    if (ipart == 0) ibuf[ilen++] = '0';
    else while (ipart > 0) { ibuf[ilen++] = (char)('0' + (ipart % 10)); ipart /= 10; }

    int total = (sign_ch ? 1 : 0) + ilen + (prec > 0 ? 1 + prec : 0);
    int pad = (width > total) ? (width - total) : 0;
    if (!(flags & 1) && !(flags & 4)) fmt_pad(o, ' ', pad);
    if (sign_ch) fmt_putc(o, sign_ch);
    if (!(flags & 1) && (flags & 4)) fmt_pad(o, '0', pad);
    while (ilen > 0) fmt_putc(o, ibuf[--ilen]);

    if (prec > 0) {
        char fbuf[16];
        for (int i = prec - 1; i >= 0; i--) {
            fbuf[i] = (char)('0' + (fpart % 10));
            fpart /= 10;
        }
        int show = prec;
        if (is_g && !(flags & 8)) {
            while (show > 0 && fbuf[show - 1] == '0') show--;
        }
        if (show > 0) {
            fmt_putc(o, '.');
            for (int i = 0; i < show; i++) fmt_putc(o, fbuf[i]);
        }
    }
    if (flags & 1) fmt_pad(o, ' ', pad);
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    FmtOut o = { str, size, 0 };
    if (!fmt) return 0;

    while (*fmt) {
        if (*fmt != '%') { fmt_putc(&o, *fmt++); continue; }
        fmt++;
        if (*fmt == '%') { fmt_putc(&o, *fmt++); continue; }

        int flags = 0; /* 1:'-', 2:'+', 4:'0', 8:'#', 16:' ' */
        while (*fmt == '-' || *fmt == '+' || *fmt == '0' || *fmt == '#' || *fmt == ' ') {
            if (*fmt == '-') flags |= 1;
            else if (*fmt == '+') flags |= 2;
            else if (*fmt == '0') flags |= 4;
            else if (*fmt == '#') flags |= 8;
            else if (*fmt == ' ') flags |= 16;
            fmt++;
        }

        int width = 0;
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; if (width < 0) { flags |= 1; width = -width; } }
        else while (isdigit((unsigned char)*fmt)) width = width * 10 + (*fmt++ - '0');

        int prec = -1;
        if (*fmt == '.') {
            fmt++; prec = 0;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else while (isdigit((unsigned char)*fmt)) prec = prec * 10 + (*fmt++ - '0');
        }

        int len_mod = 0; /* 1:l, 2:ll, 3:h, 4:hh */
        if (*fmt == 'l') { fmt++; len_mod = 1; if (*fmt == 'l') { fmt++; len_mod = 2; } }
        else if (*fmt == 'h') { fmt++; len_mod = 3; if (*fmt == 'h') { fmt++; len_mod = 4; } }
        else if (*fmt == 'z' || *fmt == 't' || *fmt == 'j') { fmt++; len_mod = 2; }

        char spec = *fmt ? *fmt++ : 0;
        if (spec == 'd' || spec == 'i') {
            int64_t v = (len_mod == 2) ? va_arg(ap, long long) :
                        (len_mod == 1) ? va_arg(ap, long) : va_arg(ap, int);
            char sc = 0; uint64_t uv = (uint64_t)v;
            if (v < 0) { sc = '-'; uv = (uint64_t)(-(v + 1)) + 1ULL; }
            else if (flags & 2) sc = '+';
            else if (flags & 16) sc = ' ';
            fmt_uint(&o, uv, 10, 0, width, prec, flags, sc);
        } else if (spec == 'u' || spec == 'x' || spec == 'X' || spec == 'o') {
            uint64_t uv = (len_mod == 2) ? va_arg(ap, unsigned long long) :
                          (len_mod == 1) ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            int base = (spec == 'o') ? 8 : ((spec == 'u') ? 10 : 16);
            fmt_uint(&o, uv, base, (spec == 'X'), width, prec, flags, 0);
        } else if (spec == 'p') {
            uintptr_t ptr = (uintptr_t)va_arg(ap, void *);
            fmt_uint(&o, (uint64_t)ptr, 16, 0, width, prec, flags | 8, 0);
        } else if (spec == 's') {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            int slen = (int)strlen(s);
            if (prec >= 0 && slen > prec) slen = prec;
            int pad = (width > slen) ? (width - slen) : 0;
            if (!(flags & 1)) fmt_pad(&o, ' ', pad);
            for (int i = 0; i < slen; i++) fmt_putc(&o, s[i]);
            if (flags & 1) fmt_pad(&o, ' ', pad);
        } else if (spec == 'c') {
            char ch = (char)va_arg(ap, int);
            int pad = (width > 1) ? (width - 1) : 0;
            if (!(flags & 1)) fmt_pad(&o, ' ', pad);
            fmt_putc(&o, ch);
            if (flags & 1) fmt_pad(&o, ' ', pad);
        } else if (spec == 'f' || spec == 'F' || spec == 'g' || spec == 'G' || spec == 'e' || spec == 'E') {
            double dv = va_arg(ap, double);
            fmt_float(&o, dv, width, prec, flags, (spec == 'g' || spec == 'G'));
        } else if (spec == 'n') {
            int *np = va_arg(ap, int *);
            if (np) *np = (int)o.pos;
        }
    }

    if (str && size > 0) {
        size_t end = (o.pos < size) ? o.pos : (size - 1);
        str[end] = 0;
    }
    return (int)o.pos;
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(str, size, fmt, ap);
    va_end(ap); return r;
}

int vsprintf(char *str, const char *fmt, va_list ap) {
    return vsnprintf(str, (size_t)-1, fmt, ap);
}

int sprintf(char *str, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(str, (size_t)-1, fmt, ap);
    va_end(ap); return r;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap) {
    char tmp[1024];
    int len = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    if (len <= 0 || !stream) return len;
    size_t out_len = (len < (int)sizeof(tmp)) ? (size_t)len : (sizeof(tmp) - 1);
    fwrite(tmp, 1, out_len, stream);
    return len;
}

int fprintf(FILE *stream, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vfprintf(stream, fmt, ap);
    va_end(ap); return r;
}

int vprintf(const char *fmt, va_list ap) { return vfprintf(stdout, fmt, ap); }

int printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vfprintf(stdout, fmt, ap);
    va_end(ap); return r;
}
int vsscanf(const char *str, const char *fmt, va_list ap) {
    if (!str || !fmt) return EOF;
    const char *s = str;
    int assigned = 0;

    while (*fmt) {
        if (isspace((unsigned char)*fmt)) {
            while (isspace((unsigned char)*fmt)) fmt++;
            while (isspace((unsigned char)*s)) s++;
            continue;
        }
        if (*fmt != '%') {
            if (*s != *fmt) break;
            s++; fmt++; continue;
        }
        fmt++;
        if (*fmt == '%') { if (*s != '%') break; s++; fmt++; continue; }

        int suppress = 0;
        if (*fmt == '*') { suppress = 1; fmt++; }

        int width = 0;
        while (isdigit((unsigned char)*fmt)) width = width * 10 + (*fmt++ - '0');

        int is_long = 0;
        if (*fmt == 'l') { is_long = 1; fmt++; if (*fmt == 'l') { is_long = 2; fmt++; } }
        else if (*fmt == 'h') { fmt++; if (*fmt == 'h') fmt++; }

        char spec = *fmt ? *fmt++ : 0;
        if (spec != 'c' && spec != 'n') {
            while (isspace((unsigned char)*s)) s++;
        }
        if (!*s && spec != 'n') return assigned ? assigned : EOF;

        if (spec == 'd' || spec == 'i' || spec == 'u' ||
            spec == 'x' || spec == 'X' || spec == 'o') {
            char num[64];
            int n = 0, maxw = (width > 0 && width < 63) ? width : 63;
            if ((spec == 'd' || spec == 'i') && (*s == '-' || *s == '+') && n < maxw)
                num[n++] = *s++;
            if (spec == 'x' || spec == 'X' || spec == 'i') {
                if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && n + 2 <= maxw) {
                    num[n++] = *s++; num[n++] = *s++;
                }
            }
            while (*s && n < maxw) {
                char c = *s;
                int ok = (spec == 'o') ? (c >= '0' && c <= '7') :
                         (spec == 'x' || spec == 'X' || spec == 'i') ? isxdigit((unsigned char)c) :
                         isdigit((unsigned char)c);
                if (!ok) break;
                num[n++] = *s++;
            }
            num[n] = 0;
            if (n == 0 || (n == 1 && (num[0] == '-' || num[0] == '+'))) break;
            int base = (spec == 'o') ? 8 : ((spec == 'x' || spec == 'X') ? 16 : ((spec == 'i') ? 0 : 10));
            if (!suppress) {
                if (spec == 'd' || spec == 'i') {
                    long long v = strtoll(num, 0, base);
                    if (is_long == 2) *va_arg(ap, long long *) = v;
                    else if (is_long == 1) *va_arg(ap, long *) = (long)v;
                    else *va_arg(ap, int *) = (int)v;
                } else {
                    unsigned long long uv = strtoull(num, 0, base);
                    if (is_long == 2) *va_arg(ap, unsigned long long *) = uv;
                    else if (is_long == 1) *va_arg(ap, unsigned long *) = (unsigned long)uv;
                    else *va_arg(ap, unsigned int *) = (unsigned int)uv;
                }
                assigned++;
            }
        } else if (spec == 'f' || spec == 'F' || spec == 'g' || spec == 'e') {
            char *endp = 0;
            double dv = strtod(s, &endp);
            if (endp == s) break;
            s = endp;
            if (!suppress) {
                if (is_long >= 1) *va_arg(ap, double *) = dv;
                else *va_arg(ap, float *) = (float)dv;
                assigned++;
            }
        } else if (spec == 's') {
            char *dst = suppress ? 0 : va_arg(ap, char *);
            int cnt = 0;
            while (*s && !isspace((unsigned char)*s) && (!width || cnt < width)) {
                if (dst) dst[cnt] = *s;
                s++; cnt++;
            }
            if (cnt == 0) break;
            if (dst) dst[cnt] = 0;
            if (!suppress) assigned++;
        } else if (spec == 'c') {
            char *dst = suppress ? 0 : va_arg(ap, char *);
            int cnt = width ? width : 1;
            for (int i = 0; i < cnt; i++) {
                if (!*s) return assigned ? assigned : EOF;
                if (dst) dst[i] = *s;
                s++;
            }
            if (!suppress) assigned++;
        } else if (spec == '[') {
            int invert = 0;
            if (*fmt == '^') { invert = 1; fmt++; }
            const char *set_start = fmt;
            if (*fmt == ']') fmt++;
            while (*fmt && *fmt != ']') fmt++;
            const char *set_end = fmt;
            if (*fmt == ']') fmt++;
            char *dst = suppress ? 0 : va_arg(ap, char *);
            int cnt = 0;
            while (*s && (!width || cnt < width)) {
                int in_set = 0;
                for (const char *p = set_start; p < set_end; p++) {
                    if (*p == *s) { in_set = 1; break; }
                }
                if (invert ? in_set : !in_set) break;
                if (dst) dst[cnt] = *s;
                s++; cnt++;
            }
            if (cnt == 0) break;
            if (dst) dst[cnt] = 0;
            if (!suppress) assigned++;
        } else if (spec == 'n') {
            if (!suppress) *va_arg(ap, int *) = (int)(s - str);
        } else {
            break;
        }
    }
    return assigned;
}

int sscanf(const char *str, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsscanf(str, fmt, ap);
    va_end(ap); return r;
}

int vfscanf(FILE *stream, const char *fmt, va_list ap) {
    char line[512];
    if (!fgets(line, sizeof(line), stream)) return EOF;
    return vsscanf(line, fmt, ap);
}

int fscanf(FILE *stream, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vfscanf(stream, fmt, ap);
    va_end(ap); return r;
}

int vscanf(const char *fmt, va_list ap) { return vfscanf(stdin, fmt, ap); }

int scanf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vfscanf(stdin, fmt, ap);
    va_end(ap); return r;
}

static void u_putc_con(char c) { sys_write(&c, 1); }

FILE *fopen(const char *filename, const char *mode) {
    if (!filename || !mode) { errno = EINVAL; return 0; }
    int fd = sys_open(filename, mode);
    if (fd < 0) { errno = ENOENT; return 0; }
    for (int i = 0; i < FOPEN_MAX; i++) {
        if (!file_table[i].in_use) {
            FILE *f = &file_table[i];
            memset(f, 0, sizeof(FILE));
            f->in_use   = 1;
            f->kind     = 1;
            f->handle   = (void *)(intptr_t)(fd + 1);
            f->unget_ch = -1;
            f->readable = (strchr(mode, 'r') != 0 || strchr(mode, '+') != 0);
            f->writable = (strchr(mode, 'w') != 0 || strchr(mode, 'a') != 0 || strchr(mode, '+') != 0);
            return f;
        }
    }
    sys_close(fd);
    errno = EMFILE;
    return 0;
}

FILE *freopen(const char *filename, const char *mode, FILE *stream) {
    if (!stream) return fopen(filename, mode);
    if (stream->kind == 1 && stream->handle) sys_close((int)(intptr_t)stream->handle - 1);
    int fd = sys_open(filename, mode);
    if (fd < 0) { stream->in_use = 0; return 0; }
    stream->in_use = 1; stream->kind = 1; stream->handle = (void *)(intptr_t)(fd + 1);
    stream->eof = 0; stream->err = 0; stream->unget_ch = -1;
    return stream;
}

int fclose(FILE *stream) {
    if (!stream || !stream->in_use) return EOF;
    if (stream->kind == 1 && stream->handle) {
        sys_close((int)(intptr_t)stream->handle - 1);
        stream->handle = 0;
        stream->in_use = 0;
    }
    return 0;
}

int fflush(FILE *stream) { (void)stream; return 0; }
void setbuf(FILE *stream, char *buf) { (void)stream; (void)buf; }
int setvbuf(FILE *stream, char *buf, int mode, size_t size) { (void)stream; (void)buf; (void)mode; (void)size; return 0; }
int remove(const char *filename) { return sys_remove(filename); }
int rename(const char *o, const char *n) { (void)o; (void)n; errno = ENOSYS; return -1; }
FILE *tmpfile(void) { return fopen("/TMPFILE.BIN", "w+b"); }

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !stream->in_use || !ptr || !size || !nmemb) return 0;
    uint8_t *dst = (uint8_t *)ptr;
    size_t want = size * nmemb, extra = 0;
    if (stream->kind == 0) {
        while (extra < want) {
            int ch = fgetc(stream);
            if (ch == EOF) break;
            dst[extra++] = (uint8_t)ch;
        }
        return extra / size;
    }
    if (stream->unget_ch >= 0 && want > 0) {
        *dst++ = (uint8_t)stream->unget_ch;
        stream->unget_ch = -1;
        want--; extra = 1;
    }
    int fd = (int)(intptr_t)stream->handle - 1;
    int64_t r = sys_read(fd, dst, want);
    if (r < 0) r = 0;
    if ((size_t)r + extra < size * nmemb) stream->eof = 1;
    return ((size_t)r + extra) / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !stream->in_use || !ptr || !size || !nmemb) return 0;
    if (stream->kind == 0) { sys_write((const char *)ptr, size * nmemb); return nmemb; }
    int fd = (int)(intptr_t)stream->handle - 1;
    int64_t w = sys_fwrite(fd, ptr, size * nmemb);
    return (w > 0) ? ((size_t)w / size) : 0;
}

int fgetc(FILE *stream) {
    if (!stream || !stream->in_use) return EOF;
    if (stream->unget_ch >= 0) { int c = stream->unget_ch; stream->unget_ch = -1; return c; }
    if (stream->kind == 0) {
        static char in_buf[512];
        static int in_len = 0, in_pos = 0;
        while (in_pos >= in_len) {
            in_len = 0; in_pos = 0;
            while (1) {
                int c = sys_getchar(1);
                if (c < 0) { if (in_len == 0) { stream->eof = 1; return EOF; } break; }
                if (c == '\r' || c == '\n') { u_putc_con('\n'); if (in_len < 511) in_buf[in_len++] = '\n'; break; }
                if (c == '\b' || c == 127) { if (in_len > 0) { in_len--; u_putc_con('\b'); } continue; }
                if (c >= 32 && c <= 126 && in_len < 510) { in_buf[in_len++] = (char)c; u_putc_con((char)c); }
            }
        }
        return (unsigned char)in_buf[in_pos++];
    }
    uint8_t b = 0;
    int fd = (int)(intptr_t)stream->handle - 1;
    if (sys_read(fd, &b, 1) != 1) { stream->eof = 1; return EOF; }
    return (int)b;
}

int getc(FILE *stream) { return fgetc(stream); }
int getchar(void) { return fgetc(stdin); }

char *fgets(char *s, int n, FILE *stream) {
    if (!s || n <= 0 || !stream) return 0;
    int i = 0;
    while (i + 1 < n) {
        int c = fgetc(stream);
        if (c == EOF) break;
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    if (i == 0) return 0;
    s[i] = 0;
    return s;
}

int fputc(int c, FILE *stream) {
    if (!stream || !stream->in_use) return EOF;
    uint8_t b = (uint8_t)c;
    if (stream->kind == 0) { u_putc_con((char)b); return (int)b; }
    int fd = (int)(intptr_t)stream->handle - 1;
    return (sys_fwrite(fd, &b, 1) == 1) ? (int)b : EOF;
}

int putc(int c, FILE *stream) { return fputc(c, stream); }
int putchar(int c) { return fputc(c, stdout); }
int fputs(const char *s, FILE *stream) { if (!s || !stream) return EOF; size_t l = strlen(s); return (fwrite(s, 1, l, stream) == l) ? 0 : EOF; }
int puts(const char *s) { if (fputs(s, stdout) == EOF) return EOF; return fputc('\n', stdout); }
int ungetc(int c, FILE *stream) { if (!stream || c == EOF || stream->unget_ch >= 0) return EOF; stream->unget_ch = (unsigned char)c; stream->eof = 0; return stream->unget_ch; }
int fseek(FILE *stream, long offset, int whence) { if (!stream || !stream->in_use || stream->kind != 1) return -1; stream->unget_ch = -1; stream->eof = 0; return sys_seek((int)(intptr_t)stream->handle - 1, (int64_t)offset, whence); }
long ftell(FILE *stream) { if (!stream || !stream->in_use || stream->kind != 1) return -1L; int64_t p = sys_tell((int)(intptr_t)stream->handle - 1); if (p >= 0 && stream->unget_ch >= 0) p--; return (long)p; }
void rewind(FILE *stream) { if (stream) { fseek(stream, 0L, SEEK_SET); stream->err = 0; } }
int fgetpos(FILE *stream, fpos_t *pos) { if (!pos) return -1; long p = ftell(stream); if (p < 0) return -1; *pos = (fpos_t)p; return 0; }
int fsetpos(FILE *stream, const fpos_t *pos) { return pos ? fseek(stream, (long)(*pos), SEEK_SET) : -1; }
void clearerr(FILE *stream) { if (stream) { stream->eof = 0; stream->err = 0; } }
int feof(FILE *stream) { return stream ? stream->eof : 0; }
int ferror(FILE *stream) { return stream ? stream->err : 0; }
void perror(const char *s) { if (s && *s) fprintf(stderr, "%s: ", s); fprintf(stderr, "%s\n", strerror(errno)); }
char *tmpnam(char *s) { static char b[32]; static int n = 0; char *p = s ? s : b; snprintf(p, 32, "/TMP_%d.TMP", n++); return p; }


