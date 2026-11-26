#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include "console.h"
#include "blockdev.h"
#include "fs/fat32.h"

static BootInfo *s_info = 0;
static char cwd[256] = "/";

static void resolve_path(const char *in, char *out, size_t out_sz) {
    char tmp[256];
    if (!in || !*in) { strncpy(out, cwd, out_sz - 1); out[out_sz - 1] = 0; return; }
    if (in[0] == '/' || in[0] == '\\') {
        strncpy(tmp, in, sizeof(tmp) - 1);
    } else if (strcmp(cwd, "/") == 0) {
        snprintf(tmp, sizeof(tmp), "/%s", in);
    } else {
        snprintf(tmp, sizeof(tmp), "%s/%s", cwd, in);
    }
    tmp[255] = 0;

    char *parts[32];
    int depth = 0;
    char *p = tmp;
    while (*p) {
        while (*p == '/' || *p == '\\') *p++ = 0;
        if (!*p) break;
        char *tok = p;
        while (*p && *p != '/' && *p != '\\') p++;
        if (*p) *p++ = 0;
        if (strcmp(tok, ".") == 0) continue;
        if (strcmp(tok, "..") == 0) { if (depth > 0) depth--; continue; }
        if (depth < 32) parts[depth++] = tok;
    }

    if (depth == 0) { strncpy(out, "/", out_sz); return; }
    out[0] = 0;
    for (int i = 0; i < depth; i++) {
        strncat(out, "/", out_sz - strlen(out) - 1);
        strncat(out, parts[i], out_sz - strlen(out) - 1);
    }
}

static void cmd_pwd(void) { printf("%s\n", cwd); }

static void cmd_cd(const char *arg) {
    if (!fat32_is_mounted()) { printf("cd: filesystem not mounted\n"); return; }
    char target[256];
    resolve_path((arg && *arg) ? arg : "/", target, sizeof(target));
    if (strcmp(target, "/") == 0) { strcpy(cwd, "/"); return; }
    Fat32Dir *d = fat32_opendir(target);
    if (!d) { printf("cd: %s: No such directory\n", arg); return; }
    fat32_closedir(d);
    strcpy(cwd, target);
}

static void cmd_ls(const char *arg) {
    if (!fat32_is_mounted()) { printf("ls: filesystem not mounted\n"); return; }
    char target[256];
    resolve_path(arg, target, sizeof(target));
    Fat32Dir *d = fat32_opendir(target);
    if (!d) { printf("ls: cannot access '%s': No such file or directory\n", arg ? arg : cwd); return; }
    Fat32DirEntry ent;
    while (fat32_readdir(d, &ent)) {
        if (ent.is_dir) {
            console_set_color(0x0038BDF8, 0x00090D16);
            printf("drwxr-xr-x  %10s  %s/\n", "-", ent.name);
        } else {
            console_set_color(0x00E2E8F0, 0x00090D16);
            printf("-rw-r--r--  %10u  %s\n", ent.size, ent.name);
        }
    }
    console_set_color(0x00E2E8F0, 0x00090D16);
    fat32_closedir(d);
}

static void cmd_mkdir(const char *arg) {
    if (!arg || !*arg) { printf("mkdir: missing operand\n"); return; }
    char target[256];
    resolve_path(arg, target, sizeof(target));
    if (fat32_mkdir(target) != 0) printf("mkdir: cannot create directory '%s'\n", arg);
}

static void cmd_rmdir(const char *arg) {
    if (!arg || !*arg) { printf("rmdir: missing operand\n"); return; }
    char target[256];
    resolve_path(arg, target, sizeof(target));
    if (remove(target) != 0) printf("rmdir: failed to remove '%s'\n", arg);
}

static void print_tree(const char *path, int depth) {
    Fat32Dir *d = fat32_opendir(path);
    if (!d) return;
    Fat32DirEntry ent;
    while (fat32_readdir(d, &ent)) {
        if (strcmp(ent.name, ".") == 0 || strcmp(ent.name, "..") == 0) continue;
        for (int i = 0; i < depth; i++) printf("  ");
        printf("|-- %s%s\n", ent.name, ent.is_dir ? "/" : "");
        if (ent.is_dir && depth < 6) {
            char sub[256];
            if (strcmp(path, "/") == 0) snprintf(sub, sizeof(sub), "/%s", ent.name);
            else snprintf(sub, sizeof(sub), "%s/%s", path, ent.name);
            print_tree(sub, depth + 1);
        }
    }
    fat32_closedir(d);
}
static void cmd_touch(const char *arg) {
    if (!arg || !*arg) { printf("touch: missing file operand\n"); return; }
    char target[256];
    resolve_path(arg, target, sizeof(target));
    FILE *fp = fopen(target, "a");
    if (!fp) { perror("touch"); return; }
    fclose(fp);
}

static void cmd_cat(const char *arg) {
    if (!arg || !*arg) { printf("cat: missing file operand\n"); return; }
    char target[256];
    resolve_path(arg, target, sizeof(target));
    FILE *fp = fopen(target, "r");
    if (!fp) { printf("cat: %s: No such file or directory\n", arg); return; }
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) printf("%s", buf);
    fclose(fp);
}

static void cmd_head_tail(const char *arg, int is_tail) {
    if (!arg || !*arg) { printf("%s: missing file operand\n", is_tail ? "tail" : "head"); return; }
    char target[256];
    resolve_path(arg, target, sizeof(target));
    FILE *fp = fopen(target, "r");
    if (!fp) { printf("%s: cannot open '%s'\n", is_tail ? "tail" : "head", arg); return; }
    char line[256];
    if (!is_tail) {
        int n = 0;
        while (n < 10 && fgets(line, sizeof(line), fp)) { printf("%s", line); n++; }
    } else {
        static char ring[10][256];
        int total = 0;
        while (fgets(ring[total % 10], sizeof(ring[0]), fp)) total++;
        int start = (total > 10) ? (total - 10) : 0;
        for (int i = start; i < total; i++) printf("%s", ring[i % 10]);
    }
    fclose(fp);
}

static int copy_file(const char *src_arg, const char *dst_arg) {
    char src[256], dst[256];
    resolve_path(src_arg, src, sizeof(src));
    resolve_path(dst_arg, dst, sizeof(dst));
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    uint8_t buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in); fclose(out);
    return 0;
}

static void cmd_wc(const char *arg) {
    if (!arg || !*arg) { printf("wc: missing file operand\n"); return; }
    char target[256];
    resolve_path(arg, target, sizeof(target));
    FILE *fp = fopen(target, "rb");
    if (!fp) { printf("wc: %s: No such file\n", arg); return; }
    int c, lines = 0, words = 0, bytes = 0, in_word = 0;
    while ((c = fgetc(fp)) != EOF) {
        bytes++;
        if (c == '\n') lines++;
        if (isspace(c)) in_word = 0;
        else if (!in_word) { in_word = 1; words++; }
    }
    fclose(fp);
    printf(" %d %d %d %s\n", lines, words, bytes, arg);
}

static void cmd_grep(const char *args) {
    char pat[64], fname[192];
    if (!args || sscanf(args, "%63s %191s", pat, fname) != 2) {
        printf("Usage: grep <pattern> <file>\n"); return;
    }
    char target[256];
    resolve_path(fname, target, sizeof(target));
    FILE *fp = fopen(target, "r");
    if (!fp) { printf("grep: %s: No such file\n", fname); return; }
    char line[256];
    int lno = 1;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, pat)) printf("%d:%s", lno, line);
        lno++;
    }
    fclose(fp);
}

static void cmd_hexdump(const char *arg) {
    if (!arg || !*arg) { printf("hexdump: missing file operand\n"); return; }
    char target[256];
    resolve_path(arg, target, sizeof(target));
    FILE *fp = fopen(target, "rb");
    if (!fp) { printf("hexdump: %s: No such file\n", arg); return; }
    uint8_t row[16];
    size_t off = 0, n;
    while ((n = fread(row, 1, 16, fp)) > 0 && off < 256) {
        printf("%04X: ", (unsigned int)off);
        for (size_t i = 0; i < 16; i++) {
            if (i < n) printf("%02X ", row[i]);
            else printf("   ");
        }
        printf(" ");
        for (size_t i = 0; i < n; i++) {
            putchar((row[i] >= 32 && row[i] <= 126) ? row[i] : '.');
        }
        putchar('\n');
        off += n;
    }
    fclose(fp);
}

static void cmd_echo(const char *args) {
    if (!args) { putchar('\n'); return; }
    const char *redir = strstr(args, ">>");
    int append = 1;
    if (!redir) { redir = strchr(args, '>'); append = 0; }
    if (!redir) { printf("%s\n", args); return; }

    char text[256];
    size_t tlen = (size_t)(redir - args);
    while (tlen > 0 && args[tlen - 1] == ' ') tlen--;
    if (tlen >= sizeof(text)) tlen = sizeof(text) - 1;
    memcpy(text, args, tlen);
    text[tlen] = 0;

    const char *fn = redir + (append ? 2 : 1);
    while (*fn == ' ') fn++;
    if (!*fn) { printf("echo: missing redirect file\n"); return; }

    char target[256];
    resolve_path(fn, target, sizeof(target));
    FILE *fp = fopen(target, append ? "a" : "w");
    if (!fp) { perror("echo"); return; }
    fprintf(fp, "%s\n", text);
    fclose(fp);
}

static void hw_reboot(void) {
    __asm__ volatile ("cli");
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x06), "Nd"((uint16_t)0xCF9));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
    struct __attribute__((packed)) { uint16_t limit; uint64_t base; } null_idtr = { 0, 0 };
    __asm__ volatile ("lidt %0; int $3" : : "m"(null_idtr));
    while (1) __asm__ volatile ("hlt");
}

void shell_init(BootInfo *info) {
    s_info = info;
    strcpy(cwd, "/");
}

void shell_exec(const char *cmdline) {
    while (*cmdline == ' ') cmdline++;
    if (!*cmdline) return;

    char cmd[32];
    int i = 0;
    while (cmdline[i] && !isspace((unsigned char)cmdline[i]) && i < 31) {
        cmd[i] = cmdline[i]; i++;
    }
    cmd[i] = 0;
    const char *args = cmdline + i;
    while (*args == ' ') args++;

    if (strcmp(cmd, "help") == 0) {
        printf("Linux Shell Commands:\n");
        printf("  ls [dir]        cd <dir>        pwd             tree [dir]\n");
        printf("  mkdir <dir>     rmdir <dir>     touch <file>    rm <file>\n");
        printf("  cat <file>      head <file>     tail <file>     wc <file>\n");
        printf("  cp <src> <dst>  mv <src> <dst>  grep <pat> <f>  hexdump <f>\n");
        printf("  echo <txt> [>|>> file]          clear           uname [-a]\n");
        printf("  date            uptime          df              lsblk\n");
        printf("  reboot          poweroff\n");
    } else if (strcmp(cmd, "pwd") == 0) {
        cmd_pwd();
    } else if (strcmp(cmd, "cd") == 0) {
        cmd_cd(args);
    } else if (strcmp(cmd, "ls") == 0) {
        cmd_ls(args);
    } else if (strcmp(cmd, "mkdir") == 0) {
        cmd_mkdir(args);
    } else if (strcmp(cmd, "rmdir") == 0) {
        cmd_rmdir(args);
    } else if (strcmp(cmd, "tree") == 0) {
        char target[256];
        resolve_path(args, target, sizeof(target));
        printf("%s\n", target);
        print_tree(target, 0);
    } else if (strcmp(cmd, "touch") == 0) {
        cmd_touch(args);
    } else if (strcmp(cmd, "cat") == 0) {
        cmd_cat(args);
    } else if (strcmp(cmd, "head") == 0) {
        cmd_head_tail(args, 0);
    } else if (strcmp(cmd, "tail") == 0) {
        cmd_head_tail(args, 1);
    } else if (strcmp(cmd, "cp") == 0) {
        char a[128], b[128];
        if (sscanf(args, "%127s %127s", a, b) == 2) {
            if (copy_file(a, b) != 0) printf("cp: failed to copy '%s'\n", a);
        } else printf("Usage: cp <src> <dst>\n");
    } else if (strcmp(cmd, "mv") == 0) {
        char a[128], b[128];
        if (sscanf(args, "%127s %127s", a, b) == 2) {
            char src[256]; resolve_path(a, src, sizeof(src));
            if (copy_file(a, b) == 0) remove(src);
            else printf("mv: failed to move '%s'\n", a);
        } else printf("Usage: mv <src> <dst>\n");
    } else if (strcmp(cmd, "rm") == 0) {
        if (!*args) printf("rm: missing operand\n");
        else {
            char target[256]; resolve_path(args, target, sizeof(target));
            if (remove(target) != 0) printf("rm: cannot remove '%s'\n", args);
        }
    } else if (strcmp(cmd, "wc") == 0) {
        cmd_wc(args);
    } else if (strcmp(cmd, "grep") == 0) {
        cmd_grep(args);
    } else if (strcmp(cmd, "hexdump") == 0) {
        cmd_hexdump(args);
    } else if (strcmp(cmd, "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(cmd, "clear") == 0) {
        console_clear();
    } else if (strcmp(cmd, "uname") == 0) {
        if (strcmp(args, "-a") == 0)
            printf("sonionOS sonion-pc 0.1.0-x86_64 #1 SMP x86_64 GNU/sonionOS\n");
        else
            printf("sonionOS\n");
    } else if (strcmp(cmd, "date") == 0) {
        time_t now = time(0);
        printf("%s", ctime(&now));
    } else if (strcmp(cmd, "uptime") == 0) {
        long long sec = (long long)clock() / 1000;
        printf("up %lld min, %lld sec\n", sec / 60, sec % 60);
    } else if (strcmp(cmd, "df") == 0) {
        uint64_t mb = (block_get_sector_count() * 512ULL) / (1024ULL * 1024ULL);
        printf("Filesystem     Type     Size   Mounted on\n");
        printf("/dev/sda1      %-6s %5lluM   /\n", fat32_get_type_name(), (unsigned long long)mb);
    } else if (strcmp(cmd, "lsblk") == 0) {
        uint64_t mb = (block_get_sector_count() * 512ULL) / (1024ULL * 1024ULL);
        printf("NAME   DRIVER       SECTORS      SIZE\n");
        printf("sda    %-10s %10llu   %lluM\n", block_get_driver_name(),
               (unsigned long long)block_get_sector_count(), (unsigned long long)mb);
    } else if (strcmp(cmd, "reboot") == 0) {
        printf("Rebooting sonionOS...\\n");
        hw_reboot();
    } else if (strcmp(cmd, "poweroff") == 0 || strcmp(cmd, "shutdown") == 0) {
        if (s_info && s_info->shutdown) s_info->shutdown();
    } else {
        printf("%s: command not found\n", cmd);
    }
}


const char *shell_get_cwd(void) { return cwd; }


