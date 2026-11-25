import struct, sys

data = open('build/bios_kernel.elf', 'rb').read()
pe_off = struct.unpack_from('<I', data, 0x3C)[0]
num_sec = struct.unpack_from('<H', data, pe_off + 6)[0]
opt_sz = struct.unpack_from('<H', data, pe_off + 20)[0]
sec_off = pe_off + 24 + opt_sz

max_end = 0
sections = []
for i in range(num_sec):
    s = sec_off + i * 40
    name = data[s:s+8].rstrip(b'\x00').decode('ascii', errors='ignore')
    vsize, vaddr, raw_sz, raw_ptr = struct.unpack_from('<IIII', data, s + 8)
    if name in ('.text', '.rodata', '.rdata', '.rdata', '.data'):
        sections.append((name, vaddr, raw_sz, raw_ptr))
        if vaddr + raw_sz > max_end:
            max_end = vaddr + raw_sz

# Build flat binary starting from virtual offset 0x1000 (where .text begins)
base_vaddr = min(s[1] for s in sections)
flat = bytearray(max_end - base_vaddr)
for name, vaddr, raw_sz, raw_ptr in sections:
    off = vaddr - base_vaddr
    flat[off:off + raw_sz] = data[raw_ptr:raw_ptr + raw_sz]
    print(f"[PACK] {name}: vaddr=0x{vaddr:X} -> file_off=0x{off:X} ({raw_sz} bytes)")

open('build/bios_kernel.bin', 'wb').write(flat)
print(f"[PACK] Total flat kernel size: {len(flat)} bytes")


