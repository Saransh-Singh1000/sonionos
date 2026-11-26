from pathlib import Path

def replace_once(path, old, new):
    p = Path(path)
    s = p.read_text()
    if new in s:
        print(f"[OK] Already patched: {path}")
        return
    if old not in s:
        raise SystemExit(f"[STOP] Expected code not found in {path}")
    p.write_text(s.replace(old, new, 1))
    print(f"[PATCHED] {path}")

files = {
    "kernel/include/desktop.h": r"""#ifndef SONION_DESKTOP_H
#define SONION_DESKTOP_H

#include <stdint.h>
#include "bootinfo.h"

void desktop_init(BootInfo *info, uint32_t *backbuf);

#endif
""",

    "kernel/desktop.c": r"""#include "desktop.h"
#include "gfx.h"

void desktop_init(BootInfo *info, uint32_t *backbuf) {
    if (!info || !backbuf || !info->width || !info->height)
        return;

    gfx_init(info, backbuf);

    /* Base wallpaper */
    gfx_fill(0x000B1220);

    uint32_t w = info->width;
    uint32_t h = info->height;

    /* Abstract wallpaper geometry */
    gfx_fill_rect((int)(w * 3 / 5), 60,
                  (int)(w / 3), (int)(h / 3),
                  0x0015223A);

    gfx_fill_rect((int)(w * 2 / 3), 90,
                  (int)(w / 4), (int)(h / 4),
                  0x001B2C48);

    /* Top status bar */
    gfx_fill_rect(0, 0, (int)w, 34, 0x00101829);
    gfx_fill_rect(0, 33, (int)w, 1, 0x0038BDF8);

    /* Desktop icon placeholders */
    int ix = 28;
    int iy = 64;

    for (int i = 0; i < 3; i++) {
        uint32_t colors[] = {
            0x0038BDF8,
            0x0022C55E,
            0x00A78BFA
        };

        gfx_fill_rect(ix, iy + i * 100, 48, 48, colors[i]);
        gfx_rect(ix, iy + i * 100, 48, 48, 0x00E2E8F0);
        gfx_fill_rect(ix + 12, iy + i * 100 + 12,
                      24, 24, 0x00E2E8F0);
    }

    /* Bottom application dock */
    int dock_w = (w > 440) ? 400 : (int)w - 32;
    if (dock_w < 0) dock_w = 0;

    int dock_x = ((int)w - dock_w) / 2;
    int dock_y = (h > 100) ? (int)h - 76 : 0;

    gfx_fill_rect(dock_x, dock_y, dock_w, 56, 0x00182235);
    gfx_rect(dock_x, dock_y, dock_w, 56, 0x00475B78);

    /* Dock application icons */
    for (int i = 0; i < 5; i++) {
        int x = dock_x + 14 + i * 72;
        if (x + 40 >= dock_x + dock_w) break;

        uint32_t colors[] = {
            0x0038BDF8,
            0x0022C55E,
            0x00F59E0B,
            0x00A78BFA,
            0x00EF4444
        };

        gfx_fill_rect(x, dock_y + 8, 40, 40, colors[i]);
        gfx_rect(x, dock_y + 8, 40, 40, 0x00E2E8F0);
    }
}
"""
}

for name, content in files.items():
    p = Path(name)
    p.parent.mkdir(parents=True, exist_ok=True)

    if p.exists():
        raise SystemExit(f"[STOP] {name} already exists; refusing overwrite")

    p.write_text(content)
    print(f"[CREATED] {name}")

# Repair the accidental literal backslash-n in console.c
p = Path("kernel/console.c")
s = p.read_text()
bad = r"/* Graphics layer owns the background; console draws text on top. */\n"
good = "/* Graphics layer owns the background; console draws text on top. */"

if bad in s:
    p.write_text(s.replace(bad, good, 1))
    print("[FIXED] console.c literal newline")
elif good in s:
    print("[OK] console.c comment")
else:
    raise SystemExit("[STOP] Console background comment not found")

# Kernel integration
replace_once(
    "kernel/kernel.c",
    '#include "console.h"',
    '#include "console.h"\n#include "desktop.h"'
)

replace_once(
    "kernel/kernel.c",
    '    console_init(info, backbuf, 2);',
    '    desktop_init(info, backbuf);\n'
    '    console_init(info, backbuf, 2);'
)

# Build integration
replace_once(
    "build.bat",
    "kernel\\kernel.c kernel\\gfx.c",
    "kernel\\kernel.c kernel\\gfx.c kernel\\desktop.c"
)

replace_once(
    "build.bat",
    "build\\kernel.o build\\gfx.o",
    "build\\kernel.o build\\gfx.o build\\desktop.o"
)

print("\n[DONE] Desktop integration prepared.")
