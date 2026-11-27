#include "audio.h"
#include "drivers/pci.h"
#include <string.h>

static inline void outb(uint16_t p, uint8_t v)  { __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p)           { uint8_t v; __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile ("outw %0, %1" : : "a"(v), "Nd"(p)); }
static inline uint16_t inw(uint16_t p)          { uint16_t v; __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outl(uint16_t p, uint32_t v) { __asm__ volatile ("outl %0, %1" : : "a"(v), "Nd"(p)); }
static inline uint32_t inl(uint16_t p)          { uint32_t v; __asm__ volatile ("inl %1, %0" : "=a"(v) : "Nd"(p)); return v; }

static void io_delay(int n) { while (n-- > 0) outb(0x80, 0); }

static int active_backend = AUDIO_BACKEND_PCSPK;

/* ==================== 1. PC SPEAKER (PIT CH 2) ==================== */
void speaker_beep(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz < 20 || freq_hz > 20000) {
        outb(0x61, inb(0x61) & 0xFC);
        return;
    }
    uint32_t div = 1193180U / freq_hz;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));
    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3) outb(0x61, tmp | 3);
    if (duration_ms > 0) {
        io_delay((int)(duration_ms * 800));
        outb(0x61, inb(0x61) & 0xFC);
    }
}

/* ==================== 2. SOUND BLASTER 16 (ISA DMA 5) ==================== */
#define SB16_BUF_BYTES 32768
static uint8_t sb16_buf[SB16_BUF_BYTES] __attribute__((aligned(4096)));

static void sb16_dsp_write(uint8_t val) {
    for (int i = 0; i < 10000; i++) if (!(inb(0x22C) & 0x80)) break;
    outb(0x22C, val);
}

static int sb16_init(void) {
    outb(0x226, 1); io_delay(20); outb(0x226, 0);
    int ok = 0;
    for (int i = 0; i < 1000; i++) {
        if ((inb(0x22E) & 0x80) && inb(0x22A) == 0xAA) { ok = 1; break; }
    }
    if (!ok) return 0;
    uint32_t phys = (uint32_t)(uintptr_t)sb16_buf;
    if (phys >= 0x01000000U) return 0; /* ISA 24-bit DMA limit: < 16MB */
    sb16_dsp_write(0xD1); /* Speaker ON */
    return 1;
}

static int sb16_write_pcm(const void *pcm, size_t bytes) {
    const uint8_t *src = (const uint8_t *)pcm;
    size_t sent = 0;
    while (sent < bytes) {
        size_t chunk = bytes - sent;
        if (chunk > SB16_BUF_BYTES) chunk = SB16_BUF_BYTES;
        memcpy(sb16_buf, src + sent, chunk);
        uint32_t phys = (uint32_t)(uintptr_t)sb16_buf;
        uint32_t words = (uint32_t)(chunk / 2) - 1;
        outb(0xD4, 0x05); /* Mask 16-bit DMA channel 5 */
        outb(0xD8, 0x00); /* Clear flip-flop */
        outb(0xD6, 0x49); /* Single-cycle read, ch 5 */
        outb(0x8B, (uint8_t)(phys >> 16));
        outb(0xC4, (uint8_t)((phys >> 1) & 0xFF));
        outb(0xC4, (uint8_t)((phys >> 9) & 0xFF));
        outb(0xC6, (uint8_t)(words & 0xFF));
        outb(0xC6, (uint8_t)((words >> 8) & 0xFF));
        outb(0xD4, 0x01); /* Unmask DMA ch 5 */
        sb16_dsp_write(0x41); sb16_dsp_write(0xBB); sb16_dsp_write(0x80); /* 48000 Hz */
        sb16_dsp_write(0xB0); sb16_dsp_write(0x30); /* 16-bit signed stereo */
        sb16_dsp_write((uint8_t)(words & 0xFF));
        sb16_dsp_write((uint8_t)((words >> 8) & 0xFF));
        io_delay((int)(chunk / 16));
        (void)inb(0x22F); /* Ack 16-bit IRQ */
        sent += chunk;
    }
    return (int)sent;
}

/* ==================== 3. INTEL AC'97 (PCI 0x04:0x01) ==================== */
typedef struct __attribute__((packed)) {
    uint32_t addr;
    uint16_t samples;
    uint16_t flags;
} Ac97BdlEntry;

#define AC97_BDL_COUNT 32
#define AC97_BUF_BYTES 4096
static Ac97BdlEntry ac97_bdl[AC97_BDL_COUNT] __attribute__((aligned(4096)));
static uint8_t      ac97_buf[AC97_BDL_COUNT][AC97_BUF_BYTES] __attribute__((aligned(4096)));
static uint16_t     ac97_nambar = 0, ac97_nabmbar = 0;
static uint8_t      ac97_tail = 0;

static int ac97_init(void) {
    PciDevice dev;
    if (!pci_find_class(0x04, 0x01, -1, &dev)) return 0;
    uint16_t cmd = pci_read16(dev.bus, dev.slot, dev.func, 0x04);
    pci_write16(dev.bus, dev.slot, dev.func, 0x04, cmd | 0x05);
    ac97_nambar  = (uint16_t)(pci_read32(dev.bus, dev.slot, dev.func, 0x10) & ~0x3U);
    ac97_nabmbar = (uint16_t)(pci_read32(dev.bus, dev.slot, dev.func, 0x14) & ~0x3U);
    if (!ac97_nambar || !ac97_nabmbar) return 0;
    outl(ac97_nabmbar + 0x2C, 0x02); io_delay(200);
    outw(ac97_nambar + 0x00, 0x0000);
    outw(ac97_nambar + 0x02, 0x0000);
    outw(ac97_nambar + 0x18, 0x0808);
    outw(ac97_nambar + 0x2A, 0x0001);
    outw(ac97_nambar + 0x2C, 48000);
    memset(ac97_buf, 0, sizeof(ac97_buf));
    for (int i = 0; i < AC97_BDL_COUNT; i++) {
        ac97_bdl[i].addr    = (uint32_t)(uintptr_t)ac97_buf[i];
        ac97_bdl[i].samples = AC97_BUF_BYTES / 2;
        ac97_bdl[i].flags   = 0;
    }
    outb(ac97_nabmbar + 0x1B, 0x02); io_delay(50);
    outl(ac97_nabmbar + 0x10, (uint32_t)(uintptr_t)ac97_bdl);
    ac97_tail = 0;
    return 1;
}

static int ac97_write_pcm(const void *pcm, size_t bytes) {
    const uint8_t *src = (const uint8_t *)pcm;
    size_t sent = 0;
    while (sent < bytes) {
        size_t chunk = bytes - sent;
        if (chunk > AC97_BUF_BYTES) chunk = AC97_BUF_BYTES;
        memset(ac97_buf[ac97_tail], 0, AC97_BUF_BYTES);
        memcpy(ac97_buf[ac97_tail], src + sent, chunk);
        outb(ac97_nabmbar + 0x15, ac97_tail);
        if (!(inb(ac97_nabmbar + 0x1B) & 0x01)) outb(ac97_nabmbar + 0x1B, 0x01);
        ac97_tail = (uint8_t)((ac97_tail + 1) % AC97_BDL_COUNT);
        sent += chunk;
    }
    return (int)sent;
}

/* ==================== 4. INTEL HD AUDIO (HDA MMIO) ==================== */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t len;
    uint32_t ioc;
} HdaBdlEntry;

#define HDA_BUF_BYTES 16384
static HdaBdlEntry hda_bdl[2] __attribute__((aligned(4096)));
static uint8_t     hda_buf[2][HDA_BUF_BYTES] __attribute__((aligned(4096)));
static volatile uint8_t *hda_mmio = 0;
static uint32_t hda_osd_off = 0x100;
static int hda_ping = 0;

static uint32_t hda_verb(uint8_t codec, uint8_t nid, uint16_t verb, uint16_t payload) {
    if (!hda_mmio) return 0;
    volatile uint32_t *icoi = (volatile uint32_t *)(hda_mmio + 0x60);
    volatile uint32_t *icii = (volatile uint32_t *)(hda_mmio + 0x64);
    volatile uint16_t *icis = (volatile uint16_t *)(hda_mmio + 0x68);
    uint32_t cmd = ((uint32_t)(codec & 0xF) << 28) | ((uint32_t)nid << 20) |
                   ((uint32_t)(verb & 0xFFF) << 8) | (payload & 0xFF);
    if (verb & 0xF00) cmd = ((uint32_t)(codec & 0xF) << 28) | ((uint32_t)nid << 20) |
                            ((uint32_t)(verb & 0xF) << 16) | (payload & 0xFFFF);
    for (int i = 0; i < 10000; i++) if (!(*icis & 1U)) break;
    *icis = 0x02;
    *icoi = cmd;
    *icis = 0x01;
    for (int i = 0; i < 10000; i++) if (*icis & 2U) return *icii;
    return 0;
}

static int hda_init(void) {
    PciDevice dev;
    if (!pci_find_class(0x04, 0x03, -1, &dev)) return 0;
    pci_enable_bus_master_mmio(&dev);
    if (!dev.bar0) return 0;
    hda_mmio = (volatile uint8_t *)(uintptr_t)dev.bar0;

    volatile uint32_t *gctl = (volatile uint32_t *)(hda_mmio + 0x08);
    *gctl &= ~1U; io_delay(50);
    *gctl |= 1U;
    int ready = 0;
    for (int i = 0; i < 20000; i++) { if (*gctl & 1U) { ready = 1; break; } }
    if (!ready) return 0;
    io_delay(200);

    uint16_t gcap = *(volatile uint16_t *)(hda_mmio + 0x00);
    uint8_t iss = (uint8_t)((gcap >> 8) & 0x0F);
    hda_osd_off = 0x80U + (uint32_t)iss * 0x20U;

    for (uint8_t nid = 1; nid <= 5; nid++) {
        hda_verb(0, nid, 0x705, 0x00);
        hda_verb(0, nid, 0x706, 0x10);
        hda_verb(0, nid, 0x707, 0x40);
        hda_verb(0, nid, 0x3A0, 0x7F);
        hda_verb(0, nid, 0x200, 0x0011);
    }

    memset(hda_buf, 0, sizeof(hda_buf));
    for (int i = 0; i < 2; i++) {
        hda_bdl[i].addr = (uint64_t)(uintptr_t)hda_buf[i];
        hda_bdl[i].len  = HDA_BUF_BYTES;
        hda_bdl[i].ioc  = 0;
    }

    volatile uint8_t *osd = hda_mmio + hda_osd_off;
    *(volatile uint32_t *)(osd + 0x00) = 0x00000001U;
    io_delay(50);
    *(volatile uint32_t *)(osd + 0x00) = 0x00000000U;
    io_delay(50);
    *(volatile uint32_t *)(osd + 0x08) = HDA_BUF_BYTES * 2;
    *(volatile uint16_t *)(osd + 0x0C) = 1;
    *(volatile uint16_t *)(osd + 0x12) = 0x0011;
    *(volatile uint32_t *)(osd + 0x18) = (uint32_t)(uintptr_t)hda_bdl;
    *(volatile uint32_t *)(osd + 0x1C) = (uint32_t)((uint64_t)(uintptr_t)hda_bdl >> 32);
    *(volatile uint32_t *)(osd + 0x00) = 0x00100002U;
    return 1;
}

static int hda_write_pcm(const void *pcm, size_t bytes) {
    const uint8_t *src = (const uint8_t *)pcm;
    size_t sent = 0;
    while (sent < bytes) {
        size_t chunk = bytes - sent;
        if (chunk > HDA_BUF_BYTES) chunk = HDA_BUF_BYTES;
        memset(hda_buf[hda_ping], 0, HDA_BUF_BYTES);
        memcpy(hda_buf[hda_ping], src + sent, chunk);
        hda_ping ^= 1;
        sent += chunk;
    }
    return (int)sent;
}

/* ==================== UNIFIED AUDIO DISPATCHER ==================== */
void audio_init(void) {
    if (ac97_init())      active_backend = AUDIO_BACKEND_AC97;
    else if (hda_init())  active_backend = AUDIO_BACKEND_HDA;
    else if (sb16_init()) active_backend = AUDIO_BACKEND_SB16;
    else                  active_backend = AUDIO_BACKEND_PCSPK;
}

int audio_backend(void) { return active_backend; }

const char *audio_backend_name(void) {
    switch (active_backend) {
        case AUDIO_BACKEND_AC97: return "Intel AC'97 (16-bit 48kHz Stereo DMA)";
        case AUDIO_BACKEND_HDA:  return "Intel HD Audio (HDA MMIO DMA)";
        case AUDIO_BACKEND_SB16: return "Sound Blaster 16 (16-bit ISA DMA)";
        default:                 return "PC Speaker (PIT Channel 2)";
    }
}

int audio_write_pcm(const void *pcm, size_t bytes) {
    if (!pcm || bytes == 0) return 0;
    if (active_backend == AUDIO_BACKEND_AC97) return ac97_write_pcm(pcm, bytes);
    if (active_backend == AUDIO_BACKEND_HDA)  return hda_write_pcm(pcm, bytes);
    if (active_backend == AUDIO_BACKEND_SB16) return sb16_write_pcm(pcm, bytes);

    const int16_t *s16 = (const int16_t *)pcm;
    int16_t amp = s16[0] < 0 ? -s16[0] : s16[0];
    if (amp > 500) speaker_beep(440 + (uint32_t)(amp >> 6), 5);
    return (int)bytes;
}

