#include <time.h>
#include <stdio.h>
#include <string.h>
#include "../usyscall.h"

static int is_leap(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static const int mdays[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

clock_t clock(void) { return (clock_t)sys_clock(); }
double difftime(time_t t1, time_t t0) { return (double)(t1 - t0); }

time_t mktime(struct tm *tm) {
    if (!tm) return (time_t)-1;
    int year = tm->tm_year + 1900, mon = tm->tm_mon;
    while (mon < 0) { mon += 12; year--; }
    while (mon >= 12) { mon -= 12; year++; }
    int64_t days = 0;
    for (int y = 1970; y < year; y++) days += is_leap(y) ? 366 : 365;
    for (int y = year; y < 1970; y++) days -= is_leap(y) ? 366 : 365;
    int yday = 0;
    for (int m = 0; m < mon; m++) yday += mdays[m] + ((m == 1 && is_leap(year)) ? 1 : 0);
    yday += (tm->tm_mday - 1);
    days += yday;
    tm->tm_yday = yday;
    int wday = (int)((days + 4) % 7);
    tm->tm_wday = (wday < 0) ? (wday + 7) : wday;
    return (time_t)(days * 86400LL + tm->tm_hour * 3600LL + tm->tm_min * 60LL + tm->tm_sec);
}

time_t time(time_t *timer) {
    time_t ep = (time_t)sys_time();
    if (timer) *timer = ep;
    return ep;
}
static struct tm static_tm;

struct tm *gmtime(const time_t *timer) {
    if (!timer) return 0;
    int64_t t = *timer;
    int64_t days = t / 86400LL;
    int64_t rem  = t % 86400LL;
    if (rem < 0) { rem += 86400LL; days--; }

    static_tm.tm_hour = (int)(rem / 3600);
    static_tm.tm_min  = (int)((rem % 3600) / 60);
    static_tm.tm_sec  = (int)(rem % 60);

    int wday = (int)((days + 4) % 7);
    if (wday < 0) wday += 7;
    static_tm.tm_wday = wday;

    int year = 1970;
    while (days >= (is_leap(year) ? 366 : 365)) {
        days -= is_leap(year) ? 366 : 365;
        year++;
    }
    while (days < 0) {
        year--;
        days += is_leap(year) ? 366 : 365;
    }
    static_tm.tm_year = year - 1900;
    static_tm.tm_yday = (int)days;

    int mon = 0;
    while (mon < 11) {
        int d = mdays[mon] + ((mon == 1 && is_leap(year)) ? 1 : 0);
        if (days < d) break;
        days -= d;
        mon++;
    }
    static_tm.tm_mon  = mon;
    static_tm.tm_mday = (int)days + 1;
    static_tm.tm_isdst = 0;
    return &static_tm;
}

struct tm *localtime(const time_t *timer) { return gmtime(timer); }

char *asctime(const struct tm *tm) {
    static const char *wname[7]  = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
    static const char *mname[12] = { "Jan","Feb","Mar","Apr","May","Jun",
                                       "Jul","Aug","Sep","Oct","Nov","Dec" };
    static char buf[32];
    if (!tm) return 0;
    int w = (tm->tm_wday >= 0 && tm->tm_wday < 7) ? tm->tm_wday : 0;
    int m = (tm->tm_mon >= 0 && tm->tm_mon < 12) ? tm->tm_mon : 0;
    snprintf(buf, sizeof(buf), "%s %s %2d %02d:%02d:%02d %d\n",
             wname[w], mname[m], tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
    return buf;
}

char *ctime(const time_t *timer) { return asctime(localtime(timer)); }

size_t strftime(char *s, size_t maxsize, const char *fmt, const struct tm *tm) {
    if (!s || maxsize == 0 || !fmt || !tm) return 0;
    size_t pos = 0;
    while (*fmt && pos + 1 < maxsize) {
        if (*fmt != '%') { s[pos++] = *fmt++; continue; }
        fmt++;
        char tmp[32]; tmp[0] = 0;
        if (*fmt == 'Y') snprintf(tmp, sizeof(tmp), "%04d", tm->tm_year + 1900);
        else if (*fmt == 'm') snprintf(tmp, sizeof(tmp), "%02d", tm->tm_mon + 1);
        else if (*fmt == 'd') snprintf(tmp, sizeof(tmp), "%02d", tm->tm_mday);
        else if (*fmt == 'H') snprintf(tmp, sizeof(tmp), "%02d", tm->tm_hour);
        else if (*fmt == 'M') snprintf(tmp, sizeof(tmp), "%02d", tm->tm_min);
        else if (*fmt == 'S') snprintf(tmp, sizeof(tmp), "%02d", tm->tm_sec);
        else if (*fmt == '%') { tmp[0] = '%'; tmp[1] = 0; }
        if (*fmt) fmt++;
        for (int i = 0; tmp[i] && pos + 1 < maxsize; i++) s[pos++] = tmp[i];
    }
    s[pos] = 0;
    return pos;
}



