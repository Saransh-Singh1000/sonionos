#ifndef SONION_IMG_H
#define SONION_IMG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width;
    int height;
    uint32_t *pixels; /* 0xAARRGGBB */
} Image;

int  img_decode(const uint8_t *buf, size_t len, Image *out);
#ifndef SONION_KERNEL
int  img_load(const char *path, Image *out);
#endif
void img_blit(uint32_t *dst, int dst_w, int dst_h,
              const Image *src, int dx, int dy, int dw, int dh);
void img_free(Image *img);

#ifdef __cplusplus
}
#endif

#endif
