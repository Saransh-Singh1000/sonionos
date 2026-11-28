#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WIDTH 4096
#define MAX_HEIGHT 2160
#define MAX_BYTES (16u * 1024u * 1024u)

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "-o") != 0) {
        printf("Sonion Image Converter v0.1\nUsage: %s input.jpg -o output.BIN\n", argv[0]);
        return 1;
    }

    int width, height, channels;
    unsigned char *rgba = stbi_load(argv[1], &width, &height, &channels, 4);

    if (!rgba) {
        fprintf(stderr, "Image error: %s\n", stbi_failure_reason());
        return 2;
    }

    uint64_t count = (uint64_t)width * height;
    uint64_t output_size = 8 + count * 4;

    if (width <= 0 || height <= 0 || width > MAX_WIDTH ||
        height > MAX_HEIGHT || output_size > MAX_BYTES) {
        fprintf(stderr, "Error: Image exceeds SonionOS limits.\n");
        stbi_image_free(rgba);
        return 3;
    }

    uint32_t *pixels = malloc((size_t)count * sizeof(uint32_t));
    if (!pixels) {
        fprintf(stderr, "Error: Out of memory.\n");
        stbi_image_free(rgba);
        return 4;
    }

    for (uint64_t i = 0; i < count; i++) {
        uint32_t r = rgba[i * 4];
        uint32_t g = rgba[i * 4 + 1];
        uint32_t b = rgba[i * 4 + 2];
        pixels[i] = (r << 16) | (g << 8) | b;
    }

    FILE *out = fopen(argv[3], "wb");
    if (!out) {
        perror("Output file");
        free(pixels);
        stbi_image_free(rgba);
        return 5;
    }

    uint32_t header[2] = {(uint32_t)width, (uint32_t)height};
    int ok = fwrite(header, 1, 8, out) == 8 &&
             fwrite(pixels, 4, (size_t)count, out) == count;

    if (fclose(out) != 0) ok = 0;

    free(pixels);
    stbi_image_free(rgba);

    if (!ok) {
        fprintf(stderr, "Error writing output.\n");
        remove(argv[3]);
        return 6;
    }

    printf("Conversion successful!\nResolution: %d x %d\nOutput: %s\n",
           width, height, argv[3]);
    return 0;
}
