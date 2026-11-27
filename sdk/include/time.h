#ifdef __cplusplus
extern "C" {
#endif
#ifndef _TIME_H
#define _TIME_H

#include <stddef.h>
#include <stdint.h>

typedef int64_t clock_t;
typedef int64_t time_t;

#define CLOCKS_PER_SEC 1000L

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

clock_t    clock(void);
time_t     time(time_t *timer);
double     difftime(time_t time1, time_t time0);
time_t     mktime(struct tm *timeptr);
struct tm *gmtime(const time_t *timer);
struct tm *localtime(const time_t *timer);
char      *asctime(const struct tm *timeptr);
char      *ctime(const time_t *timer);
size_t     strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr);

#endif

#ifdef __cplusplus
}
#endif
