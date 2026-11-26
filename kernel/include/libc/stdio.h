#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define EOF          (-1)
#define SEEK_SET     0
#define SEEK_CUR     1
#define SEEK_END     2

#define BUFSIZ       512
#define FILENAME_MAX 260
#define FOPEN_MAX    32
#define L_tmpnam     32

#define _IOFBF       0
#define _IOLBF       1
#define _IONBF       2

typedef int64_t fpos_t;

typedef struct {
    int   in_use;
    int   kind;        /* 0 = console, 1 = FAT32 file */
    void *handle;      /* Fat32File* when kind == 1 */
    int   eof;
    int   err;
    int   unget_ch;    /* -1 if empty */
    int   readable;
    int   writable;
} FILE;

extern FILE __sonion_stdin;
extern FILE __sonion_stdout;
extern FILE __sonion_stderr;

#define stdin  (&__sonion_stdin)
#define stdout (&__sonion_stdout)
#define stderr (&__sonion_stderr)

/* Formatted Output */
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsprintf(char *str, const char *fmt, va_list ap);
int sprintf(char *str, const char *fmt, ...);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int fprintf(FILE *stream, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int printf(const char *fmt, ...);

/* Formatted Input */
int vsscanf(const char *str, const char *fmt, va_list ap);
int sscanf(const char *str, const char *fmt, ...);
int vfscanf(FILE *stream, const char *fmt, va_list ap);
int fscanf(FILE *stream, const char *fmt, ...);
int vscanf(const char *fmt, va_list ap);
int scanf(const char *fmt, ...);

/* File & Stream Control */
FILE *fopen(const char *filename, const char *mode);
FILE *freopen(const char *filename, const char *mode, FILE *stream);
int   fclose(FILE *stream);
int   fflush(FILE *stream);
void  setbuf(FILE *stream, char *buf);
int   setvbuf(FILE *stream, char *buf, int mode, size_t size);
int   remove(const char *filename);
int   rename(const char *oldname, const char *newname);
FILE *tmpfile(void);
char *tmpnam(char *s);

/* Direct Block I/O */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/* Character & Line I/O */
int   fgetc(FILE *stream);
int   getc(FILE *stream);
int   getchar(void);
char *fgets(char *s, int n, FILE *stream);
int   fputc(int c, FILE *stream);
int   putc(int c, FILE *stream);
int   putchar(int c);
int   fputs(const char *s, FILE *stream);
int   puts(const char *s);
int   ungetc(int c, FILE *stream);

/* File Positioning */
int     fseek(FILE *stream, long offset, int whence);
long    ftell(FILE *stream);
void    rewind(FILE *stream);
int     fgetpos(FILE *stream, fpos_t *pos);
int     fsetpos(FILE *stream, const fpos_t *pos);

/* Error Handling */
void clearerr(FILE *stream);
int  feof(FILE *stream);
int  ferror(FILE *stream);
void perror(const char *s);

#endif

