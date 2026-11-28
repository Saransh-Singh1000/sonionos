#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "term.h"
#include "desktop.h"

#define TERM_LINES 64
#define TERM_COLS  112

static char lines[TERM_LINES][TERM_COLS];
static int  line_cnt = 0;
static char in_buf[TERM_COLS];
static int  in_len = 0;
static char cwd[64] = "/USER";
static uint32_t frame_ticks = 0;

static const uint8_t font5x7[64][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40}
};

static void term_push_line(const char *s) {
    if (line_cnt >= TERM_LINES) {
        memmove(lines[0], lines[1], (TERM_LINES - 1) * TERM_COLS);
        line_cnt = TERM_LINES - 1;
    }
    strncpy(lines[line_cnt], s, TERM_COLS - 1);
    lines[line_cnt][TERM_COLS - 1] = 0;
    line_cnt++;
}

static void term_printf(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    char *p = buf;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl) { *nl = 0; term_push_line(p); p = nl + 1; }
        else { term_push_line(p); break; }
    }
}

static void draw_glyph(uint32_t *fb, int sw, int sh, int x, int y, char c, uint32_t col,
                       int cx, int cy, int cw, int ch) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c < 32 || c > 95) c = '?';
    const uint8_t *g = font5x7[(int)c - 32];
    for (int col_i = 0; col_i < 5; col_i++) {
        uint8_t bits = g[col_i];
        for (int row_i = 0; row_i < 7; row_i++) {
            if (bits & (1U << row_i)) {
                int px = x + col_i, py = y + row_i;
                if (px >= cx && px < cx + cw && py >= cy && py < cy + ch &&
                    px >= 0 && px < sw && py >= 0 && py < sh) {
                    fb[py * sw + px] = col;
                }
            }
        }
    }
}

static void draw_str(uint32_t *fb, int sw, int sh, int x, int y, const char *s, uint32_t col,
                     int cx, int cy, int cw, int ch) {
    while (*s) {
        draw_glyph(fb, sw, sh, x, y, *s++, col, cx, cy, cw, ch);
        x += 6;
    }
}

void term_draw(uint32_t *backbuf, int sw, int sh) {
    if (!backbuf || !desktop_console_visible()) return;
    int cx = 0, cy = 0, cw = 0, ch = 0;
    desktop_console_bounds(&cx, &cy, &cw, &ch);
    if (cw <= 16 || ch <= 16) return;

    frame_ticks++;
    int row_h = 11;
    int max_rows = ch / row_h;
    if (max_rows < 2) return;

    int show_history = max_rows - 1;
    int start = (line_cnt > show_history) ? (line_cnt - show_history) : 0;
    int y = cy + 2;

    for (int i = start; i < line_cnt; i++) {
        draw_str(backbuf, sw, sh, cx + 2, y, lines[i], 0x00CBD5E1, cx, cy, cw, ch);
        y += row_h;
    }

    char prompt[TERM_COLS + 64];
    snprintf(prompt, sizeof(prompt), "root@sonionOS:%s# %s", cwd, in_buf);
    draw_str(backbuf, sw, sh, cx + 2, y, prompt, 0x0038BDF8, cx, cy, cw, ch);

    if ((frame_ticks / 15) & 1) {
        int cur_x = cx + 2 + (int)strlen(prompt) * 6;
        draw_glyph(backbuf, sw, sh, cur_x, y, '_', 0x0022C55E, cx, cy, cw, ch);
    }
}

static void term_resolve(const char *arg, char *out, size_t max_out) {
    if (!arg || !*arg) { strncpy(out, cwd, max_out - 1); out[max_out - 1] = 0; return; }
    if (arg[0] == '/') strncpy(out, arg, max_out - 1);
    else if (strcmp(cwd, "/") == 0) snprintf(out, max_out, "/%s", arg);
    else snprintf(out, max_out, "%s/%s", cwd, arg);
    out[max_out - 1] = 0;
}

static void term_exec_cmd(const char *raw) {
    char cmd_buf[TERM_COLS];
    strncpy(cmd_buf, raw, sizeof(cmd_buf) - 1);
    cmd_buf[sizeof(cmd_buf) - 1] = 0;

    char *p = cmd_buf;
    while (*p == ' ') p++;
    if (!*p) return;

    char *sp = strchr(p, ' ');
    char *arg = "";
    if (sp) { *sp = 0; arg = sp + 1; while (*arg == ' ') arg++; }

    if (strcmp(p, "help") == 0) {
        term_printf("Commands: help, clear, ls, cd <dir>, cat <file>, ps, kmem, cpl, audio, <app.son>, exit, reboot");
    } else if (strcmp(p, "clear") == 0) {
        line_cnt = 0;
    } else if (strcmp(p, "cpl") == 0) {
        term_printf("[USER] Ring 3 Desktop Terminal Active (CPL = %u)", get_current_cpl());
    } else if (strcmp(p, "audio") == 0) {
        FILE *f = fopen("/dev/dsp", "rb");
        if (f) {
            char nm[64] = {0};
            fread(nm, 1, sizeof(nm) - 1, f);
            fclose(f);
            term_printf("[/dev/dsp] %s", nm);
        } else term_printf("[/dev/dsp] unavailable");
    } else if (strcmp(p, "ps") == 0) {
        UserTaskInfo list[8];
        int n = sys_ps(list, 8);
        for (int i = 0; i < n; i++) {
            term_printf("PID %d | CPL %d | %u KB | %s", list[i].pid, list[i].cpl, list[i].mem_kb, list[i].name);
        }
    } else if (strcmp(p, "kmem") == 0) {
        uint64_t st[7] = {0};
        if (sys_kmem(st) == 0) {
            term_printf("Heap: %llu KB used / %llu KB total (%llu KB free)",
                        st[1] / 1024ULL, st[0] / 1024ULL, st[2] / 1024ULL);
        }
    } else if (strcmp(p, "cd") == 0) {
        char path[256];
        term_resolve(*arg ? arg : "/USER", path, sizeof(path));
        int dd = sys_opendir(path);
        if (dd >= 0) { sys_closedir(dd); strncpy(cwd, path, sizeof(cwd) - 1); cwd[sizeof(cwd) - 1] = 0; }
        else term_printf("cd: %s: No such directory", path);
    } else if (strcmp(p, "ls") == 0 || strcmp(p, "dir") == 0) {
        char path[256];
        term_resolve(arg, path, sizeof(path));
        int dd = sys_opendir(path);
        if (dd < 0) { term_printf("ls: cannot open %s", path); return; }
        UserDirEntry ent;
        char row[TERM_COLS] = {0};
        while (sys_readdir(dd, &ent) > 0) {
            char item[64];
            snprintf(item, sizeof(item), ent.is_dir ? "[%s] " : "%s ", ent.name);
            if (strlen(row) + strlen(item) >= TERM_COLS - 2) { term_push_line(row); row[0] = 0; }
            strcat(row, item);
        }
        if (row[0]) term_push_line(row);
        sys_closedir(dd);
    } else if (strcmp(p, "cat") == 0) {
        if (!*arg) { term_printf("Usage: cat <file>"); return; }
        char path[256];
        term_resolve(arg, path, sizeof(path));
        FILE *f = fopen(path, "rb");
        if (!f) { term_printf("cat: cannot open %s", path); return; }
        char buf[512];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = 0;
        fclose(f);
        term_printf("%s", buf);
    } else if (strcmp(p, "reboot") == 0) {
        sys_reboot();
    } else if (strcmp(p, "poweroff") == 0 || strcmp(p, "shutdown") == 0) {
        sys_poweroff();
    } else {
        const char *prog = (strcmp(p, "exec") == 0 && *arg) ? arg : p;
        char path[256];
        term_resolve(prog, path, sizeof(path));
        int rc = sys_exec(path);
        if (rc < 0 && prog[0] != '/') {
            char upath[256];
            snprintf(upath, sizeof(upath), "/USER/%s", prog);
            rc = sys_exec(upath);
        }
        if (rc >= 0) term_printf("[Child exited: %d]", rc);
        else term_printf("%s: command not found", p);
    }
}

void term_init(void) {
    line_cnt = 0;
    in_len = 0;
    in_buf[0] = 0;
    term_printf("[sonionOS] Ring 3 Windowed Terminal Ready (CPL = %u)", get_current_cpl());
    term_printf("Type 'help' for commands, 'PLAY.SON' for audio, or ESC to exit GUI.");
}

void term_on_key(const SonionKeyEvent *ev, int *out_exit) {
    if (!ev || !ev->pressed) return;
    if (ev->scancode == 0x01) { if (out_exit) *out_exit = 1; return; }
    if (ev->ascii == 12) { line_cnt = 0; return; }
    if (ev->ascii == 3) { term_printf("root@sonionOS:%s# %s^C", cwd, in_buf); in_len = 0; in_buf[0] = 0; return; }

    if (ev->ascii == '\n' || ev->ascii == '\r') {
        term_printf("root@sonionOS:%s# %s", cwd, in_buf);
        char exec_copy[TERM_COLS];
        strcpy(exec_copy, in_buf);
        in_len = 0;
        in_buf[0] = 0;
        if (strcmp(exec_copy, "exit") == 0) { if (out_exit) *out_exit = 1; return; }
        term_exec_cmd(exec_copy);
    } else if (ev->ascii == '\b' || ev->ascii == 127) {
        if (in_len > 0) in_buf[--in_len] = 0;
    } else if (ev->ascii >= 32 && ev->ascii <= 126 && in_len + 1 < TERM_COLS) {
        in_buf[in_len++] = ev->ascii;
        in_buf[in_len] = 0;
    }
}


