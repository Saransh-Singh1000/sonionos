#include <stdint.h>
#include "bootinfo.h"

typedef uint64_t UINTN; typedef uint64_t EFI_STATUS; typedef void* EFI_HANDLE; typedef uint16_t CHAR16;
typedef struct { uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8]; } EFI_GUID;
typedef struct { uint64_t Signature; uint32_t Revision; uint32_t HeaderSize; uint32_t CRC32; uint32_t Reserved; } EFI_TABLE_HEADER;
typedef struct { uint16_t ScanCode; CHAR16 UnicodeChar; } EFI_INPUT_KEY;
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL { void* Reset; EFI_STATUS (__attribute__((ms_abi)) *ReadKeyStroke)(struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key); void* WaitForKey; } EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct { int32_t RelativeMovementX, RelativeMovementY, RelativeMovementZ; uint8_t LeftButton, RightButton; } EFI_SIMPLE_POINTER_STATE;
typedef struct { uint64_t ResolutionX, ResolutionY, ResolutionZ; uint8_t LeftButton, RightButton; } EFI_SIMPLE_POINTER_MODE;
typedef struct _EFI_SIMPLE_POINTER_PROTOCOL { EFI_STATUS (*Reset)(struct _EFI_SIMPLE_POINTER_PROTOCOL *This, uint8_t Ext); EFI_STATUS (*GetState)(struct _EFI_SIMPLE_POINTER_PROTOCOL *This, EFI_SIMPLE_POINTER_STATE *State); void* WaitForInput; EFI_SIMPLE_POINTER_MODE *Mode; } EFI_SIMPLE_POINTER_PROTOCOL;
typedef struct { uint32_t Version, HorizontalResolution, VerticalResolution, PixelFormat, PixelInformation[4], PixelsPerScanLine; } EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;
typedef struct { uint32_t MaxMode, Mode; EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info; UINTN SizeOfInfo; uint64_t FrameBufferBase; UINTN FrameBufferSize; } EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;
typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL { void* QueryMode; void* SetMode; void* Blt; EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode; } EFI_GRAPHICS_OUTPUT_PROTOCOL;
typedef struct { EFI_TABLE_HEADER Hdr; void *f1,*f2,*f3,*f4,*f5,*f6,*f7,*f8,*f9,*f10,*f11,*f12,*f13,*f14,*f15,*f16,*f17; EFI_STATUS (*HandleProtocol)(EFI_HANDLE H, EFI_GUID *P, void **I); void *f19,*f20,*f21,*f22,*f23,*f24,*f25,*f26,*f27,*f28; EFI_STATUS (__attribute__((ms_abi)) *Stall)(UINTN Microseconds); void *f30; EFI_STATUS (*ConnectController)(EFI_HANDLE C, EFI_HANDLE *D, void *R, uint8_t Rec); void *f32,*f33,*f34,*f35; EFI_STATUS (*LocateHandleBuffer)(UINTN T, EFI_GUID *P, void *K, UINTN *N, EFI_HANDLE **B); void *f37; EFI_STATUS (__attribute__((ms_abi)) *LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface); } EFI_BOOT_SERVICES;
typedef struct { EFI_TABLE_HEADER Hdr; void *f1,*f2,*f3,*f4,*f5,*f6,*f7,*f8,*f9,*f10; void (__attribute__((ms_abi)) *ResetSystem)(int ResetType, EFI_STATUS ResetStatus, UINTN DataSize, void *ResetData); } EFI_RUNTIME_SERVICES;
typedef struct { EFI_GUID VendorGuid; void *VendorTable; } EFI_CONFIGURATION_TABLE;
typedef struct { EFI_TABLE_HEADER Hdr; CHAR16 *FirmwareVendor; uint32_t FirmwareRevision; EFI_HANDLE ConsoleInHandle; EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn; EFI_HANDLE ConsoleOutHandle; void* ConOut; EFI_HANDLE StandardErrorHandle; void* StdErr; EFI_RUNTIME_SERVICES *RuntimeServices; EFI_BOOT_SERVICES *BootServices; UINTN NumberOfTableEntries; EFI_CONFIGURATION_TABLE *ConfigurationTable; } EFI_SYSTEM_TABLE;

static EFI_SYSTEM_TABLE *g_st = 0;
static EFI_SIMPLE_POINTER_PROTOCOL *g_mice[8];
static int g_num_mice = 0;

typedef struct { uint64_t CurrentX, CurrentY, CurrentZ; uint32_t ActiveButtons; } EFI_ABSOLUTE_POINTER_STATE;
typedef struct { uint64_t AbsoluteMinX, AbsoluteMinY, AbsoluteMinZ, AbsoluteMaxX, AbsoluteMaxY, AbsoluteMaxZ, Attributes; } EFI_ABSOLUTE_POINTER_MODE;
typedef struct _EFI_ABSOLUTE_POINTER_PROTOCOL { EFI_STATUS (*Reset)(struct _EFI_ABSOLUTE_POINTER_PROTOCOL *This, uint8_t Ext); EFI_STATUS (*GetState)(struct _EFI_ABSOLUTE_POINTER_PROTOCOL *This, EFI_ABSOLUTE_POINTER_STATE *State); void* WaitForInput; EFI_ABSOLUTE_POINTER_MODE *Mode; } EFI_ABSOLUTE_POINTER_PROTOCOL;
static EFI_ABSOLUTE_POINTER_PROTOCOL *g_abs_mice[8];
static int g_num_abs = 0;
uint32_t g_mouse_dbg = 0;
static int cur_mx = 400, cur_my = 300, max_w = 800, max_h = 600;

static uint8_t uefi_mbtn = 0, uefi_mcycle = 0;
static uint8_t uefi_mpkt[4];

static inline uint8_t u_inb(uint16_t p) {
    uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(p)); return r;
}
static inline void u_outb(uint16_t p, uint8_t v) {
    __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(p));
}
static void u_ps2_wait_w(void) { for (int i = 0; i < 100000; i++) if (!(u_inb(0x64) & 2)) return; }
static void u_ps2_wait_r(void) { for (int i = 0; i < 100000; i++) if (u_inb(0x64) & 1) return; }
static uint8_t uefi_pkt_len = 3;
static int8_t uefi_mz = 0;
static void u_ps2_mouse_init(void) {
    u_ps2_wait_w(); u_outb(0x64, 0xA8);
    u_ps2_wait_w(); u_outb(0x64, 0x20);
    u_ps2_wait_r(); uint8_t st = (u_inb(0x60) | 2) & ~0x20;
    u_ps2_wait_w(); u_outb(0x64, 0x60);
    u_ps2_wait_w(); u_outb(0x60, st);
    u_ps2_wait_w(); u_outb(0x64, 0xD4);
    u_ps2_wait_w(); u_outb(0x60, 0xF6);
    u_ps2_wait_r(); (void)u_inb(0x60);
    uint8_t rates[3] = { 200, 100, 80 };
    for (int r = 0; r < 3; r++) {
        u_ps2_wait_w(); u_outb(0x64, 0xD4); u_ps2_wait_w(); u_outb(0x60, 0xF3); u_ps2_wait_r(); (void)u_inb(0x60);
        u_ps2_wait_w(); u_outb(0x64, 0xD4); u_ps2_wait_w(); u_outb(0x60, rates[r]); u_ps2_wait_r(); (void)u_inb(0x60);
    }
    u_ps2_wait_w(); u_outb(0x64, 0xD4); u_ps2_wait_w(); u_outb(0x60, 0xF2); u_ps2_wait_r(); (void)u_inb(0x60);
    u_ps2_wait_r();
    uefi_pkt_len = (u_inb(0x60) == 3) ? 4 : 3;
    u_ps2_wait_w(); u_outb(0x64, 0xD4);
    u_ps2_wait_w(); u_outb(0x60, 0xF4);
    u_ps2_wait_r(); (void)u_inb(0x60);
}
void uefi_poll_input(InputState *state) {
    state->key_char = 0;
    state->scan_code = 0;
    state->left_click = 0;
    state->scroll_z = 0;
    uefi_mz = 0;

    while (u_inb(0x64) & 1) {
        uint8_t status = u_inb(0x64);
        if (!(status & 0x20)) break; /* leave keyboard byte for UEFI ConIn! */
        uint8_t data = u_inb(0x60);
        if (uefi_mcycle == 0 && !(data & 0x08)) continue;
        uefi_mpkt[uefi_mcycle++] = data;
        if (uefi_mcycle == uefi_pkt_len) {
            uefi_mcycle = 0;
            uefi_mbtn = uefi_mpkt[0] & 0x07;
            int32_t dx = (int32_t)uefi_mpkt[1] - ((uefi_mpkt[0] & 0x10) ? 256 : 0);
            int32_t dy = (int32_t)uefi_mpkt[2] - ((uefi_mpkt[0] & 0x20) ? 256 : 0);
            cur_mx += dx; cur_my -= dy;
            if (cur_mx < 0) cur_mx = 0; if (cur_mx >= max_w) cur_mx = max_w - 1;
            if (cur_my < 0) cur_my = 0; if (cur_my >= max_h) cur_my = max_h - 1;
            if (uefi_pkt_len == 4) { int8_t dz = (int8_t)(uefi_mpkt[3] & 0x0F); if (dz & 0x08) dz |= (int8_t)0xF0; uefi_mz += dz; }
        }
    }
    state->mouse_btns = uefi_mbtn & 0x07;
    if (uefi_mbtn & 1) state->left_click = 1;
    state->scroll_z = uefi_mz;
    EFI_INPUT_KEY key;
    if (g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key) == 0) {
        state->key_char = (char)key.UnicodeChar;
        state->scan_code = key.ScanCode;
    }

    for (int i = 0; i < g_num_abs; i++) {
        EFI_ABSOLUTE_POINTER_PROTOCOL *ap = g_abs_mice[i];
        EFI_ABSOLUTE_POINTER_STATE as;
        EFI_STATUS st_a = (ap && ap->Mode) ? ap->GetState(ap, &as) : 99; g_mouse_dbg = ((uint32_t)g_num_abs << 24) | ((uint32_t)g_num_mice << 16) | ((uint32_t)st_a & 0xFFFFU); if (st_a == 0) {
            uint64_t rx = ap->Mode->AbsoluteMaxX - ap->Mode->AbsoluteMinX;
            uint64_t ry = ap->Mode->AbsoluteMaxY - ap->Mode->AbsoluteMinY;
            if (rx > 0) cur_mx = (int)(((as.CurrentX - ap->Mode->AbsoluteMinX) * (uint64_t)max_w) / rx);
            if (ry > 0) cur_my = (int)(((as.CurrentY - ap->Mode->AbsoluteMinY) * (uint64_t)max_h) / ry);
            if (cur_mx < 0) cur_mx = 0; if (cur_mx >= max_w) cur_mx = max_w - 1;
            if (cur_my < 0) cur_my = 0; if (cur_my >= max_h) cur_my = max_h - 1;
            if (uefi_pkt_len == 4) { int8_t dz = (int8_t)(uefi_mpkt[3] & 0x0F); if (dz & 0x08) dz |= (int8_t)0xF0; uefi_mz += dz; }
            if (as.ActiveButtons & 1) state->left_click = 1;
        }
    }
    for (int i = 0; i < g_num_mice; i++) {
        EFI_SIMPLE_POINTER_PROTOCOL *mp = g_mice[i];
        EFI_SIMPLE_POINTER_STATE ms;
        EFI_STATUS st_m = mp ? mp->GetState(mp, &ms) : 99; if (g_num_abs == 0) g_mouse_dbg = ((uint32_t)g_num_mice << 16) | ((uint32_t)st_m & 0xFFFFU); if (st_m == 0) {
            int rx = (mp->Mode && mp->Mode->ResolutionX > 0) ? (int)mp->Mode->ResolutionX : 1;
            int ry = (mp->Mode && mp->Mode->ResolutionY > 0) ? (int)mp->Mode->ResolutionY : 1;
            int dx = (rx > 100) ? (ms.RelativeMovementX / (rx / 16 + 1)) : (ms.RelativeMovementX * 8);
            int dy = (ry > 100) ? (ms.RelativeMovementY / (ry / 16 + 1)) : (ms.RelativeMovementY * 8);
            if (dx == 0 && ms.RelativeMovementX != 0) dx = (ms.RelativeMovementX > 0) ? 2 : -2;
            if (dy == 0 && ms.RelativeMovementY != 0) dy = (ms.RelativeMovementY > 0) ? 2 : -2;
            cur_mx += dx; cur_my += dy;
            if (cur_mx < 0) cur_mx = 0; if (cur_mx >= max_w) cur_mx = max_w - 1;
            if (cur_my < 0) cur_my = 0; if (cur_my >= max_h) cur_my = max_h - 1;
            if (uefi_pkt_len == 4) { int8_t dz = (int8_t)(uefi_mpkt[3] & 0x0F); if (dz & 0x08) dz |= (int8_t)0xF0; uefi_mz += dz; }
            if (ms.LeftButton) state->left_click = 1;
        }
    }
    state->mouse_btns = uefi_mbtn & 0x07;
    if (uefi_mbtn & 1) state->left_click = 1;
    state->scroll_z = uefi_mz;
    state->mouse_x = cur_mx;
    state->mouse_y = cur_my;
}

void uefi_sleep_ms(uint32_t ms) { g_st->BootServices->Stall((UINTN)ms * 1000); }
void uefi_reboot(void) { g_st->RuntimeServices->ResetSystem(0, 0, 0, 0); }
void uefi_shutdown(void) { g_st->RuntimeServices->ResetSystem(2, 0, 0, 0); }


static int uefi_guid_equal(const EFI_GUID *a, const EFI_GUID *b) {
    if (a->Data1 != b->Data1 ||
        a->Data2 != b->Data2 ||
        a->Data3 != b->Data3)
        return 0;

    for (unsigned i = 0; i < 8; i++)
        if (a->Data4[i] != b->Data4[i]) return 0;

    return 1;
}

static uint64_t uefi_find_acpi_rsdp(EFI_SYSTEM_TABLE *st) {
    static const EFI_GUID acpi20 = {
        0x8868e871, 0xe4f1, 0x11d3,
        {0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}
    };
    static const EFI_GUID acpi10 = {
        0xeb9d2d30, 0x2d88, 0x11d3,
        {0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}
    };

    uint64_t fallback = 0;
    if (!st || !st->ConfigurationTable) return 0;

    for (UINTN i = 0; i < st->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *entry = &st->ConfigurationTable[i];
        if (!entry->VendorTable) continue;

        if (uefi_guid_equal(&entry->VendorGuid, &acpi20))
            return (uint64_t)(uintptr_t)entry->VendorTable;

        if (uefi_guid_equal(&entry->VendorGuid, &acpi10))
            fallback = (uint64_t)(uintptr_t)entry->VendorTable;
    }

    return fallback;
}

EFI_STATUS __attribute__((ms_abi)) efi_main(EFI_HANDLE img, EFI_SYSTEM_TABLE *st) {
    g_st = st;
    EFI_GUID gop_guid = {0x9042a9de, 0x23dc, 0x4a38, {0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
    EFI_GUID ptr_guid = {0x31878c87, 0x0b75, 0x11d5, {0x9a,0x4f,0x00,0x90,0x27,0x3f,0xc1,0x4d}};
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;

    st->BootServices->LocateProtocol(&gop_guid, 0, (void**)&gop);
    UINTN nh = 0; EFI_HANDLE *hb = 0;
    if (st->BootServices->LocateHandleBuffer(0, 0, 0, &nh, &hb) == 0 && hb) {
        for (UINTN i = 0; i < nh; i++) st->BootServices->ConnectController(hb[i], 0, 0, 1);
    }
    nh = 0; hb = 0;
    EFI_GUID abs_guid = {0x8D59D32B, 0xC655, 0x4AE9, {0x9B,0x15,0xF2,0x59,0x04,0x99,0x2A,0x43}};
    if (st->BootServices->LocateHandleBuffer(2, &abs_guid, 0, &nh, &hb) == 0 && hb) {
        for (UINTN i = 0; i < nh && g_num_abs < 8; i++) {
            EFI_ABSOLUTE_POINTER_PROTOCOL *ap = 0;
            if (st->BootServices->HandleProtocol(hb[i], &abs_guid, (void**)&ap) == 0 && ap) {
                ap->Reset(ap, 0); g_abs_mice[g_num_abs++] = ap;
            }
        }
    }
    nh = 0; hb = 0;
    if (st->BootServices->LocateHandleBuffer(2, &ptr_guid, 0, &nh, &hb) == 0 && hb) {
        for (UINTN i = 0; i < nh && g_num_mice < 8; i++) {
            EFI_SIMPLE_POINTER_PROTOCOL *mp = 0;
            if (st->BootServices->HandleProtocol(hb[i], &ptr_guid, (void**)&mp) == 0 && mp) {
                mp->Reset(mp, 0); g_mice[g_num_mice++] = mp;
            }
        }
    }

    u_ps2_mouse_init();
    max_w = gop->Mode->Info->HorizontalResolution;
    max_h = gop->Mode->Info->VerticalResolution;
    cur_mx = max_w / 2; cur_my = max_h / 2;

    BootInfo info;
    info.framebuffer = (uint32_t*)gop->Mode->FrameBufferBase;
    info.width       = max_w;
    info.height      = max_h;
    info.pitch       = gop->Mode->Info->PixelsPerScanLine;
    info.acpi_rsdp    = uefi_find_acpi_rsdp(st);
    info.poll_input  = uefi_poll_input;
    info.sleep_ms    = uefi_sleep_ms;
    info.reboot      = uefi_reboot;
    info.shutdown    = uefi_shutdown;

    // Hand over control to the shared sonionOS kernel!
    kernel_main(&info);
    return 0;
}














