import os, sys, subprocess

CANDIDATES = [
    os.path.join("assets", "logo.png"),
    os.path.join("assets", "logo.jpg"),
    os.path.join("assets", "logo.bmp"),
    os.path.join("assets", "logo.tga"),
]
OUT_HDR = os.path.join("kernel", "include", "boot_logo_data.h")
TITLE_PNG = os.path.join("build", "boot_title.png")

src = next((p for p in CANDIDATES if os.path.exists(p)), None)
if not src:
    print("[ERROR] Place your logo at assets\\logo.png first!")
    sys.exit(1)

os.makedirs("build", exist_ok=True)
if os.name == "nt":
    ps = (
        'Add-Type -AssemblyName System.Drawing; '
        '$bmp = New-Object System.Drawing.Bitmap(260, 56, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb); '
        '$g = [System.Drawing.Graphics]::FromImage($bmp); '
        '$g.Clear([System.Drawing.Color]::Transparent); '
        '$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit; '
        '$font = New-Object System.Drawing.Font("Segoe UI", 24, [System.Drawing.FontStyle]::Bold); '
        '$brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 248, 250, 252)); '
        '$sf = New-Object System.Drawing.StringFormat; '
        '$sf.Alignment = [System.Drawing.StringAlignment]::Center; '
        '$sf.LineAlignment = [System.Drawing.StringAlignment]::Center; '
        '$rect = New-Object System.Drawing.RectangleF(0, 0, 260, 56); '
        '$g.DrawString("sonionOS", $font, $brush, $rect, $sf); '
        '$bmp.Save("build\\boot_title.png", [System.Drawing.Imaging.ImageFormat]::Png); '
        '$g.Dispose(); $bmp.Dispose()'
    )
    subprocess.run(["powershell", "-NoProfile", "-Command", ps], check=True)

with open(src, "rb") as f:
    logo_bytes = f.read()
title_bytes = open(TITLE_PNG, "rb").read() if os.path.exists(TITLE_PNG) else b""

def write_arr(out, name, data):
    out.write(f"static const uint8_t {name}[{len(data)}] = {{\n")
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i:i+16])
        out.write(f"    {chunk},\n")
    out.write("};\n")

os.makedirs(os.path.dirname(OUT_HDR), exist_ok=True)
with open(OUT_HDR, "w", encoding="ascii") as out:
    out.write("#ifndef BOOT_LOGO_DATA_H\n#define BOOT_LOGO_DATA_H\n#include <stdint.h>\n")
    write_arr(out, "boot_logo_data", logo_bytes)
    if title_bytes:
        write_arr(out, "boot_title_data", title_bytes)
    out.write("#endif\n")

print(f"[SPLASH] Embedded {src} + smooth anti-aliased sonionOS wordmark -> {OUT_HDR}")
