#include <time.h>
#include <stdio.h>
#include <string.h>

static inline void cmos_outb(uint16_t p, uint8_t v) {
    __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(p));
}
static inline uint8_t cmos_inb(uint16_t p) {
    uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(p)); return r;
}

static uint8_t rtc_read(uint8_t reg) {
    cmos_outb(0x70, reg);
    return cmos_inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t b) {
    return (uint8_t)((b & 0x0F) + ((b >> 4) * 10));
}

static int is_leap(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static const int mdays[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

clock_t clock(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t tsc = ((uint64_t)hi << 32) | lo;
    return (clock_t)(tsc / 2500000ULL);
}

double difftime(time_t t1, time_t t0) { return (double)(t1 - t0); }

time_t mktime(struct tm *tm) {
    if (!tm) return (time_t)-1;
    int year = tm->tm_year + 1900;
    int mon  = tm->tm_mon;
    while (mon < 0)  { mon += 12; year--; }
    while (mon >= 12){ mon -= 12; year++; }

    int64_t days = 0;
    for (int y = 1970; y < year; y++) days += is_leap(y) ? 366 : 365;
    for (int y = year; y < 1970; y++) days -= is_leap(y) ? 366 : 365;

    int yday = 0;
    for (int m = 0; m < mon; m++) {
        int d = mdays[m] + ((m == 1 && is_leap(year)) ? 1 : 0);
        yday += d;
    }
    yday += (tm->tm_mday - 1);
    days += yday;

    tm->tm_yday = yday;
    int wday = (int)((days + 4) % 7);
    if (wday < 0) wday += 7;
    tm->tm_wday = wday;

    return (time_t)(days * 86400LL + tm->tm_hour * 3600LL + tm->tm_min * 60LL + tm->tm_sec);
}

time_t time(time_t *timer) {
    while (rtc_read(0x0A) & 0x80) __asm__ volatile ("pause");
    uint8_t sec  = rtc_read(0x00);
    uint8_t min  = rtc_read(0x02);
    uint8_t hour = rtc_read(0x04);
    uint8_t day  = rtc_read(0x07);
    uint8_t mon  = rtc_read(0x08);
    uint8_t year = rtc_read(0x09);
    uint8_t regb = rtc_read(0x0B);

    if (!(regb & 0x04)) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = bcd_to_bin(hour & 0x7F) | (hour & 0x80);
        day  = bcd_to_bin(day);
        mon  = bcd_to_bin(mon);
        year = bcd_to_bin(year);
    }
    if (!(regb & 0x02) && (hour & 0x80)) {
        hour = (uint8_t)(((hour & 0x7F) + 12) % 24);
    }

    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_sec  = sec;
    t.tm_min  = min;
    t.tm_hour = hour;
    t.tm_mday = day ? day : 1;
    t.tm_mon  = mon ? (mon - 1) : 0;
    t.tm_year = 100 + year;

    time_t epoch = mktime(&t);
    if (timer) *timer = epoch;
    return epoch;
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

