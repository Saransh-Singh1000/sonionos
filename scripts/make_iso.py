import os, struct

SECTOR_SIZE = 512
ISO_BLOCK   = 2048

def pad_to(data, align):
    rem = len(data) % align
    return data if rem == 0 else data + b"\x00" * (align - rem)

def make_short_name(name):
    parts = name.upper().split(".")
    base = "".join(c for c in parts[0] if c.isalnum() or c in "_-~")[:8].ljust(8, " ")
    ext  = "".join(c for c in (parts[1] if len(parts) > 1 else "") if c.isalnum())[:3].ljust(3, " ")
    return (base + ext).encode("ascii")

def build_fat16_esp(disk_dir, out_esp):
    total_sectors = 32768 # 16 MB
    rsvd_sectors  = 4
    num_fats      = 2
    fat_sectors   = 128
    root_entries  = 512
    root_sectors  = (root_entries * 32) // 512
    spc           = 4
    data_start    = rsvd_sectors + num_fats * fat_sectors + root_sectors

    img = bytearray(total_sectors * SECTOR_SIZE)
    fat = [0] * (fat_sectors * 256)
    fat[0] = 0xFFF8
    fat[1] = 0xFFFF
    next_clus = 2

    vbr = bytearray(512)
    vbr[0:3]   = b"\xEB\x3C\x90"
    vbr[3:11]  = b"SONIONOS"
    struct.pack_into("<H", vbr, 11, 512)
    vbr[13]   = spc
    struct.pack_into("<H", vbr, 14, rsvd_sectors)
    vbr[16]   = num_fats
    struct.pack_into("<H", vbr, 17, root_entries)
    struct.pack_into("<H", vbr, 19, total_sectors)
    vbr[21]   = 0xF8
    struct.pack_into("<H", vbr, 22, fat_sectors)
    struct.pack_into("<H", vbr, 24, 63)
    struct.pack_into("<H", vbr, 26, 255)
    vbr[36]   = 0x80
    vbr[38]   = 0x29
    struct.pack_into("<I", vbr, 39, 0x20261127)
    vbr[43:54] = b"SONIONOS   "
    vbr[54:62] = b"FAT16   "
    vbr[510:512] = b"\x55\xAA"
    img[0:512] = vbr

    def alloc_chain(blob):
        nonlocal next_clus
        cbytes = spc * 512
        nclus = max(1, (len(blob) + cbytes - 1) // cbytes)
        first = next_clus
        for i in range(nclus):
            c = next_clus
            next_clus += 1
            fat[c] = 0xFFFF if i == nclus - 1 else (c + 1)
            chunk = blob[i * cbytes : (i + 1) * cbytes]
            off = (data_start + (c - 2) * spc) * 512
            img[off : off + len(chunk)] = chunk
        return first

    def pack_dir(host_path, parent_clus=0):
        entries = bytearray()
        if parent_clus != 0:
            dot = bytearray(32); dot[0:11] = b".          "; dot[11] = 0x10
            ddot = bytearray(32); ddot[0:11] = b"..         "; ddot[11] = 0x10
            entries += dot + ddot
        for item in sorted(os.listdir(host_path)):
            if item.lower() == "nvvars": continue
            full = os.path.join(host_path, item)
            ent = bytearray(32)
            ent[0:11] = make_short_name(item)
            if os.path.isdir(full):
                ent[11] = 0x10
                sub_bytes = pack_dir(full, 1)
                c = alloc_chain(sub_bytes)
                struct.pack_into("<H", ent, 26, c)
            else:
                ent[11] = 0x20
                with open(full, "rb") as f: data = f.read()
                c = alloc_chain(data) if len(data) > 0 else 0
                struct.pack_into("<H", ent, 26, c)
                struct.pack_into("<I", ent, 28, len(data))
            entries += ent
        return bytes(entries)

    root_bytes = pack_dir(disk_dir, 0)
    root_off = (rsvd_sectors + num_fats * fat_sectors) * 512
    img[root_off : root_off + len(root_bytes)] = root_bytes

    fat_raw = struct.pack("<" + "H" * len(fat), *fat)
    for f_idx in range(num_fats):
        foff = (rsvd_sectors + f_idx * fat_sectors) * 512
        img[foff : foff + len(fat_raw)] = fat_raw

    with open(out_esp, "wb") as f: f.write(img)
    return bytes(img)

def build_hybrid_iso(bios_img_path, esp_bytes, out_iso):
    with open(bios_img_path, "rb") as f: bios_bytes = pad_to(f.read(), ISO_BLOCK)
    esp_padded = pad_to(esp_bytes, ISO_BLOCK)

    bios_lba = 21
    bios_blocks = len(bios_bytes) // ISO_BLOCK
    esp_lba = bios_lba + bios_blocks
    esp_blocks = len(esp_padded) // ISO_BLOCK
    total_blocks = esp_lba + esp_blocks

    iso = bytearray(total_blocks * ISO_BLOCK)

    mbr = bytearray(512)
    part1 = bytearray(16)
    part1[0] = 0x80
    part1[4] = 0x06
    struct.pack_into("<I", part1, 8, esp_lba * 4)
    struct.pack_into("<I", part1, 12, esp_blocks * 4)
    mbr[446:462] = part1
    mbr[510:512] = b"\x55\xAA"
    iso[0:512] = mbr

    pvd = bytearray(ISO_BLOCK)
    pvd[0] = 1; pvd[1:6] = b"CD001"; pvd[6] = 1
    pvd[8:40] = b"SONIONOS".ljust(32, b" ")
    pvd[40:72] = b"SONIONOS_HYBRID".ljust(32, b" ")
    struct.pack_into("<I", pvd, 80, total_blocks); struct.pack_into(">I", pvd, 84, total_blocks)
    struct.pack_into("<H", pvd, 120, 1); struct.pack_into(">H", pvd, 122, 1)
    struct.pack_into("<H", pvd, 124, 1); struct.pack_into(">H", pvd, 126, 1)
    struct.pack_into("<H", pvd, 128, ISO_BLOCK); struct.pack_into(">H", pvd, 130, ISO_BLOCK)
    pvd[156] = 34
    struct.pack_into("<I", pvd, 158, 20); struct.pack_into(">I", pvd, 162, 20)
    struct.pack_into("<I", pvd, 166, ISO_BLOCK); struct.pack_into(">I", pvd, 170, ISO_BLOCK)
    pvd[181] = 2; pvd[188] = 1; pvd[189] = 0
    iso[16 * ISO_BLOCK : 17 * ISO_BLOCK] = pvd

    br = bytearray(ISO_BLOCK)
    br[0] = 0; br[1:6] = b"CD001"; br[6] = 1
    br[7:39] = b"EL TORITO SPECIFICATION".ljust(32, b"\x00")
    struct.pack_into("<I", br, 71, 19)
    iso[17 * ISO_BLOCK : 18 * ISO_BLOCK] = br

    term = bytearray(ISO_BLOCK)
    term[0] = 255; term[1:6] = b"CD001"; term[6] = 1
    iso[18 * ISO_BLOCK : 19 * ISO_BLOCK] = term

    cat = bytearray(ISO_BLOCK)
    val = bytearray(32)
    val[0] = 1; val[1] = 0
    val[4:28] = b"SONIONOS".ljust(24, b"\x00")
    val[30] = 0x55; val[31] = 0xAA
    csum = (0x10000 - (sum(struct.unpack("<16H", val)) & 0xFFFF)) & 0xFFFF
    struct.pack_into("<H", val, 28, csum)
    cat[0:32] = val

    bios_ent = bytearray(32)
    bios_ent[0] = 0x88; bios_ent[1] = 0x00
    struct.pack_into("<H", bios_ent, 2, 0x07C0); struct.pack_into("<H", bios_ent, 6, 1)
    struct.pack_into("<I", bios_ent, 8, bios_lba)
    cat[32:64] = bios_ent

    sec_hdr = bytearray(32)
    sec_hdr[0] = 0x91; sec_hdr[1] = 0xEF
    struct.pack_into("<H", sec_hdr, 2, 1)
    cat[64:96] = sec_hdr

    uefi_ent = bytearray(32)
    uefi_ent[0] = 0x88; uefi_ent[1] = 0x00
    struct.pack_into("<H", uefi_ent, 6, 0)
    struct.pack_into("<I", uefi_ent, 8, esp_lba)
    cat[96:128] = uefi_ent
    iso[19 * ISO_BLOCK : 20 * ISO_BLOCK] = cat

    iso[bios_lba * ISO_BLOCK : (bios_lba + bios_blocks) * ISO_BLOCK] = bios_bytes
    iso[esp_lba * ISO_BLOCK : (esp_lba + esp_blocks) * ISO_BLOCK] = esp_padded

    with open(out_iso, "wb") as f: f.write(iso)
    print(f"[ISO] Built hybrid UEFI+BIOS image: {out_iso} ({len(iso) // 1024} KB)")

if __name__ == "__main__":
    esp = build_fat16_esp("disk", "build/esp.img")
    build_hybrid_iso("build/sonionos_bios.img", esp, "build/sonionOS.iso")
