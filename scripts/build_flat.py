import struct

data = open('build/bios_kernel.elf', 'rb').read()
(pe_off,) = struct.unpack_from('<I', data, 0x3C)
(num_sec,) = struct.unpack_from('<H', data, pe_off + 6)
(opt_sz,) = struct.unpack_from('<H', data, pe_off + 20)
sec_off = pe_off + 24 + opt_sz

max_end = 0
sections = []
for i in range(num_sec):
    s = sec_off + i * 40
    name = data[s:s+8].rstrip(b'\x00').decode('ascii', errors='ignore')
    vsize, vaddr, raw_sz, raw_ptr = struct.unpack_from('<IIII', data, s + 8)
    if name in ('.text', '.rodata', '.rdata', '.data'):
        sections.append((name, vaddr, raw_sz, raw_ptr))
        if vaddr + raw_sz > max_end:
            max_end = vaddr + raw_sz

base_vaddr = min(vaddr for _, vaddr, _, _ in sections)
flat = bytearray(max_end - base_vaddr)
for name, vaddr, raw_sz, raw_ptr in sections:
    off = vaddr - base_vaddr
    flat[off:off + raw_sz] = data[raw_ptr:raw_ptr + raw_sz]

open('build/bios_kernel.bin', 'wb').write(flat)
print(f"[PACK] Built flat kernel ({len(flat)} bytes)")

import os, struct
ksize = os.path.getsize("build/bios_kernel.bin")
chunks = max(1, (ksize + 32767) // 32768)
with open("build/bios_boot.bin", "r+b") as bf:
    bf.seek(2)
    bf.write(struct.pack("<H", chunks))
print(f"[PATCH] Bootloader configured for {chunks} x 32KB chunks ({chunks * 32} KB)")
