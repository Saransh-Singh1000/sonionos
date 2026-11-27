#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "usyscall.h"
#include "lua_runner.h"

static char cwd[256] = "/";

static void resolve_path(const char *in, char *out, size_t out_sz) {
    char tmp[256];
    if (!in || !*in) {
        strncpy(out, cwd, out_sz - 1);
        out[out_sz - 1] = 0;
        return;
    }
    if (in[0] == '/' || in[0] == '\\') strncpy(tmp, in, sizeof(tmp) - 1);
    else if (strcmp(cwd, "/") == 0) snprintf(tmp, sizeof(tmp), "/%s", in);
    else snprintf(tmp, sizeof(tmp), "%s/%s", cwd, in);
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

static void cmd_ls(const char *arg) {
    char target[256];
    resolve_path(arg, target, sizeof(target));
    int dd = sys_opendir(target);
    if (dd < 0) { printf("ls: cannot access '%s'\n", arg ? arg : cwd); return; }
    UserDirEntry ent;
    while (sys_readdir(dd, &ent)) {
        if (ent.is_dir) printf("drwxr-xr-x  %10s  %s/\n", "-", ent.name);
        else printf("-rw-r--r--  %10u  %s\n", ent.size, ent.name);
    }
    sys_closedir(dd);
}

static void print_tree(const char *path, int depth) {
    int dd = sys_opendir(path);
    if (dd < 0) return;
    UserDirEntry ent;
    while (sys_readdir(dd, &ent)) {
        if (!strcmp(ent.name, ".") || !strcmp(ent.name, "..")) continue;
        for (int i = 0; i < depth; i++) printf("  ");
        printf("|-- %s%s\n", ent.name, ent.is_dir ? "/" : "");
        if (ent.is_dir && depth < 6) {
            char sub[256];
            if (!strcmp(path, "/")) snprintf(sub, sizeof(sub), "/%s", ent.name);
            else snprintf(sub, sizeof(sub), "%s/%s", path, ent.name);
            print_tree(sub, depth + 1);
        }
    }
    sys_closedir(dd);
}

static int copy_file(const char *a, const char *b) {
    char src[256], dst[256];
    resolve_path(a, src, sizeof(src));
    resolve_path(b, dst, sizeof(dst));
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    uint8_t buf[512]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in); fclose(out);
    return 0;
}
static void shell_exec(char *cmdline) {
    size_t len = strlen(cmdline);
    while (len > 0 && (cmdline[len - 1] == '\n' || cmdline[len - 1] == '\r')) {
        cmdline[--len] = 0;
    }
    while (*cmdline == ' ') cmdline++;
    if (!*cmdline) return;

    char cmd[32]; int i = 0;
    while (cmdline[i] && !isspace((unsigned char)cmdline[i]) && i < 31) {
        cmd[i] = cmdline[i]; i++;
    }
    cmd[i] = 0;
    const char *args = cmdline + i;
    while (*args == ' ') args++;

    if (!strcmp(cmd, "help")) {
        printf("User-Mode (Ring 3) Linux Shell Commands:\n");
        printf("  ls [dir]        cd <dir>        pwd             tree\n");
        printf("  mkdir <dir>     rmdir <dir>     touch <file>    rm <file>\n");
        printf("  cat <file>      cp <src> <dst>  mv <src> <dst>  wc <file>\n");
        printf("  echo <txt> [>|>> file]          clear           uname\n");
        printf("  date            uptime          df              cpl\n");
        printf("  lua <file.lua>  exec <app.bin>  reboot          poweroff\n");
    } else if (!strcmp(cmd, "cpl")) {
        uint16_t cpl = get_current_cpl();
        printf("Current CPU Privilege Level: Ring %u (CPL = %u)\n", cpl, cpl);
    } else if (!strcmp(cmd, "pwd")) {
        printf("%s\n", cwd);
    } else if (!strcmp(cmd, "cd")) {
        char target[256];
        resolve_path(*args ? args : "/", target, sizeof(target));
        if (!strcmp(target, "/")) { strcpy(cwd, "/"); return; }
        int dd = sys_opendir(target);
        if (dd < 0) { printf("cd: %s: No such directory\n", args); return; }
        sys_closedir(dd);
        strcpy(cwd, target);
    } else if (!strcmp(cmd, "ls")) {
        cmd_ls(args);
    } else if (!strcmp(cmd, "tree")) {
        char target[256]; resolve_path(args, target, sizeof(target));
        printf("%s\n", target); print_tree(target, 0);
    } else if (!strcmp(cmd, "mkdir")) {
        if (!*args) printf("mkdir: missing operand\n");
        else {
            char t[256]; resolve_path(args, t, sizeof(t));
            if (sys_mkdir(t) != 0) printf("mkdir: failed\n");
        }
    } else if (!strcmp(cmd, "rmdir") || !strcmp(cmd, "rm")) {
        if (!*args) printf("%s: missing operand\n", cmd);
        else {
            char t[256]; resolve_path(args, t, sizeof(t));
            if (remove(t) != 0) printf("%s: failed\n", cmd);
        }
    } else if (!strcmp(cmd, "touch")) {
        if (!*args) printf("touch: missing operand\n");
        else {
            char t[256]; resolve_path(args, t, sizeof(t));
            FILE *f = fopen(t, "a");
            if (f) fclose(f); else perror("touch");
        }
    } else if (!strcmp(cmd, "cat")) {
        if (!*args) printf("cat: missing operand\n");
        else {
            char t[256]; resolve_path(args, t, sizeof(t));
            FILE *f = fopen(t, "r");
            if (!f) { printf("cat: %s: No such file\n", args); return; }
            char b[256]; while (fgets(b, sizeof(b), f)) printf("%s", b);
            fclose(f);
        }
    } else if (!strcmp(cmd, "cp")) {
        char a[128], b[128];
        if (sscanf(args, "%127s %127s", a, b) == 2) {
            if (copy_file(a, b) != 0) printf("cp: failed\n");
        } else printf("Usage: cp <src> <dst>\n");
    } else if (!strcmp(cmd, "mv")) {
        char a[128], b[128];
        if (sscanf(args, "%127s %127s", a, b) == 2) {
            char s[256]; resolve_path(a, s, sizeof(s));
            if (copy_file(a, b) == 0) remove(s); else printf("mv: failed\n");
        } else printf("Usage: mv <src> <dst>\n");
    } else if (!strcmp(cmd, "wc")) {
        char t[256]; resolve_path(args, t, sizeof(t));
        FILE *f = fopen(t, "rb");
        if (!f) { printf("wc: cannot open '%s'\n", args); return; }
        int c, l = 0, w = 0, by = 0, iw = 0;
        while ((c = fgetc(f)) != EOF) {
            by++; if (c == '\n') l++;
            if (isspace(c)) iw = 0; else if (!iw) { iw = 1; w++; }
        }
        fclose(f); printf(" %d %d %d %s\n", l, w, by, args);
    } else if (!strcmp(cmd, "echo")) {
        const char *r = strstr(args, ">>"); int app = 1;
        if (!r) { r = strchr(args, '>'); app = 0; }
        if (!r) { printf("%s\n", args); return; }
        char txt[256]; size_t tl = (size_t)(r - args);
        while (tl > 0 && args[tl - 1] == ' ') tl--;
        if (tl >= sizeof(txt)) tl = sizeof(txt) - 1;
        memcpy(txt, args, tl); txt[tl] = 0;
        const char *fn = r + (app ? 2 : 1); while (*fn == ' ') fn++;
        char t[256]; resolve_path(fn, t, sizeof(t));
        FILE *f = fopen(t, app ? "a" : "w");
        if (f) { fprintf(f, "%s\n", txt); fclose(f); } else perror("echo");
    } else if (!strcmp(cmd, "clear")) {
        sys_clear();
    } else if (!strcmp(cmd, "uname")) {
        printf(!strcmp(args, "-a") ? "sonionOS 0.1.0-x86_64 UserMode\n"
                                   : "sonionOS\n");
    } else if (!strcmp(cmd, "date")) {
        time_t now = time(0); printf("%s", ctime(&now));
    } else if (!strcmp(cmd, "uptime")) {
        long long s = (long long)clock() / 1000;
        printf("up %lld min, %lld sec\n", s / 60, s % 60);
    } else if (!strcmp(cmd, "df")) {
        uint64_t di[8]; sys_disk_info(di);
        uint64_t mb = (di[0] * 512ULL) / (1024ULL * 1024ULL);
        printf("Driver: %s | FS: %s (%s) | Size: %llu MB\n",
               (char *)(di + 2), di[1] ? "MOUNTED" : "NONE",
               (char *)(di + 4), (unsigned long long)mb);
    } else if (!strcmp(cmd, "lua")) {
        if (!*args) printf("Usage: lua <script.lua>\n");
        else {
            char t[256]; resolve_path(args, t, sizeof(t));
            lua_run_file(t);
        }
    } else if (!strcmp(cmd, "exec")) {
        if (!*args) printf("Usage: exec <program.bin>\n");
        else {
            char t[256]; resolve_path(args, t, sizeof(t));
            int rc = sys_exec(t);
            printf("[Child Ring 3 exited: %d]\n", rc);
        }
    } else if (!strcmp(cmd, "reboot")) {
        printf("Rebooting...\n"); sys_reboot();
    } else if (!strcmp(cmd, "poweroff") || !strcmp(cmd, "shutdown")) {
        sys_poweroff();
    } else {
        printf("%s: command not found\n", cmd);
    }
}

__attribute__((section(".text._start")))
void _start(void) {
    uint64_t di[8];
    sys_disk_info(di);
    printf("[USER] Ring 3 Shell + Lua 5.5.1 Active (CPL = %u)\n",
           get_current_cpl());
    printf("[DISK] %s | [FS] %s (%s)\n", (char *)(di + 2),
           di[1] ? "MOUNTED" : "NONE", (char *)(di + 4));
    printf("Type 'help' for commands ('cpl', 'lua hello.lua').\n\n");

    char line[256];
    while (1) {
        printf("root@sonionOS:%s# ", cwd);
        if (!fgets(line, sizeof(line), stdin)) break;
        shell_exec(line);
    }
    sys_exit(0);
}


