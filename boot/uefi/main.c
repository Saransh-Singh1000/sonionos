#include <stdint.h>
#include "bootinfo.h"

typedef uint64_t UINTN; typedef uint64_t EFI_STATUS; typedef void* EFI_HANDLE; typedef uint16_t CHAR16;
typedef struct { uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8]; } EFI_GUID;
typedef struct { uint64_t Signature; uint32_t Revision; uint32_t HeaderSize; uint32_t CRC32; uint32_t Reserved; } EFI_TABLE_HEADER;
typedef struct { uint16_t ScanCode; CHAR16 UnicodeChar; } EFI_INPUT_KEY;
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL { void* Reset; EFI_STATUS (*ReadKeyStroke)(struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key); void* WaitForKey; } EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct { int32_t RelativeMovementX, RelativeMovementY, RelativeMovementZ; uint8_t LeftButton, RightButton; } EFI_SIMPLE_POINTER_STATE;
typedef struct { uint64_t ResolutionX, ResolutionY, ResolutionZ; uint8_t LeftButton, RightButton; } EFI_SIMPLE_POINTER_MODE;
typedef struct _EFI_SIMPLE_POINTER_PROTOCOL { EFI_STATUS (*Reset)(struct _EFI_SIMPLE_POINTER_PROTOCOL *This, uint8_t Ext); EFI_STATUS (*GetState)(struct _EFI_SIMPLE_POINTER_PROTOCOL *This, EFI_SIMPLE_POINTER_STATE *State); void* WaitForInput; EFI_SIMPLE_POINTER_MODE *Mode; } EFI_SIMPLE_POINTER_PROTOCOL;
typedef struct { uint32_t Version, HorizontalResolution, VerticalResolution, PixelFormat, PixelInformation[4], PixelsPerScanLine; } EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;
typedef struct { uint32_t MaxMode, Mode; EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info; UINTN SizeOfInfo; uint64_t FrameBufferBase; UINTN FrameBufferSize; } EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;
typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL { void* QueryMode; void* SetMode; void* Blt; EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode; } EFI_GRAPHICS_OUTPUT_PROTOCOL;
typedef struct { EFI_TABLE_HEADER Hdr; void *f1,*f2,*f3,*f4,*f5,*f6,*f7,*f8,*f9,*f10,*f11,*f12,*f13,*f14,*f15,*f16,*f17,*f18,*f19,*f20,*f21,*f22,*f23,*f24,*f25,*f26,*f27,*f28; EFI_STATUS (*Stall)(UINTN Microseconds); void *f30,*f31,*f32,*f33,*f34,*f35,*f36,*f37; EFI_STATUS (*LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface); } EFI_BOOT_SERVICES;
typedef struct { EFI_TABLE_HEADER Hdr; void *f1,*f2,*f3,*f4,*f5,*f6,*f7,*f8,*f9,*f10; void (*ResetSystem)(int ResetType, EFI_STATUS ResetStatus, UINTN DataSize, void *ResetData); } EFI_RUNTIME_SERVICES;
typedef struct { EFI_TABLE_HEADER Hdr; CHAR16 *FirmwareVendor; uint32_t FirmwareRevision; EFI_HANDLE ConsoleInHandle; EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn; EFI_HANDLE ConsoleOutHandle; void* ConOut; EFI_HANDLE StandardErrorHandle; void* StdErr; EFI_RUNTIME_SERVICES *RuntimeServices; EFI_BOOT_SERVICES *BootServices; } EFI_SYSTEM_TABLE;

static EFI_SYSTEM_TABLE *g_st = 0;
static EFI_SIMPLE_POINTER_PROTOCOL *g_mouse = 0;
static int cur_mx = 400, cur_my = 300, max_w = 800, max_h = 600;

void uefi_poll_input(InputState *state) {
    state->key_char = 0;
    state->scan_code = 0;
    state->left_click = 0;

    EFI_INPUT_KEY key;
    if (g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key) == 0) {
        state->key_char = (char)key.UnicodeChar;
        state->scan_code = key.ScanCode;
    }

    if (g_mouse) {
        EFI_SIMPLE_POINTER_STATE ms;
        if (g_mouse->GetState(g_mouse, &ms) == 0) {
            int rx = (g_mouse->Mode && g_mouse->Mode->ResolutionX > 0) ? (int)g_mouse->Mode->ResolutionX : 1;
            int ry = (g_mouse->Mode && g_mouse->Mode->ResolutionY > 0) ? (int)g_mouse->Mode->ResolutionY : 1;
            int dx = (rx > 100) ? (ms.RelativeMovementX / (rx / 16 + 1)) : (ms.RelativeMovementX * 8);
            int dy = (ry > 100) ? (ms.RelativeMovementY / (ry / 16 + 1)) : (ms.RelativeMovementY * 8);
            if (dx == 0 && ms.RelativeMovementX != 0) dx = (ms.RelativeMovementX > 0) ? 2 : -2;
            if (dy == 0 && ms.RelativeMovementY != 0) dy = (ms.RelativeMovementY > 0) ? 2 : -2;
            cur_mx += dx; cur_my += dy;
            if (cur_mx < 0) cur_mx = 0; if (cur_mx >= max_w) cur_mx = max_w - 1;
            if (cur_my < 0) cur_my = 0; if (cur_my >= max_h) cur_my = max_h - 1;
            state->left_click = ms.LeftButton;
        }
    }
    state->mouse_x = cur_mx;
    state->mouse_y = cur_my;
}

void uefi_sleep_ms(uint32_t ms) { g_st->BootServices->Stall((UINTN)ms * 1000); }
void uefi_reboot(void) { g_st->RuntimeServices->ResetSystem(0, 0, 0, 0); }
void uefi_shutdown(void) { g_st->RuntimeServices->ResetSystem(2, 0, 0, 0); }

EFI_STATUS efi_main(EFI_HANDLE img, EFI_SYSTEM_TABLE *st) {
    g_st = st;
    EFI_GUID gop_guid = {0x9042a9de, 0x23dc, 0x4a38, {0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
    EFI_GUID ptr_guid = {0x31878c87, 0x0b75, 0x11d5, {0x9a,0x4f,0x00,0x90,0x27,0x3f,0xc1,0x4d}};
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;

    st->BootServices->LocateProtocol(&gop_guid, 0, (void**)&gop);
    st->BootServices->LocateProtocol(&ptr_guid, 0, (void**)&g_mouse);
    if (g_mouse) g_mouse->Reset(g_mouse, 0);

    max_w = gop->Mode->Info->HorizontalResolution;
    max_h = gop->Mode->Info->VerticalResolution;
    cur_mx = max_w / 2; cur_my = max_h / 2;

    BootInfo info;
    info.framebuffer = (uint32_t*)gop->Mode->FrameBufferBase;
    info.width       = max_w;
    info.height      = max_h;
    info.pitch       = gop->Mode->Info->PixelsPerScanLine;
    info.poll_input  = uefi_poll_input;
    info.sleep_ms    = uefi_sleep_ms;
    info.reboot      = uefi_reboot;
    info.shutdown    = uefi_shutdown;

    // Hand over control to the shared sonionOS kernel!
    kernel_main(&info);
    return 0;
}
