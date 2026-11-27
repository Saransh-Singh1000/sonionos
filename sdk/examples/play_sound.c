#include <stdio.h>
#include <stdint.h>

static int16_t pcm[48000]; /* 0.5 sec 48kHz stereo PCM */

int main(void) {
    FILE *dsp = fopen("/dev/dsp", "r+b");
    if (!dsp) { printf("[AUDIO] Failed to open /dev/dsp\n"); return 1; }

    char name[64] = {0};
    fread(name, 1, sizeof(name) - 1, dsp);
    printf("[/dev/dsp] Backend: %s\n", name);

    int freqs[3] = { 523, 659, 784 };
    for (int n = 0; n < 3; n++) {
        int period = 48000 / freqs[n];
        for (int i = 0; i < 24000; i++) {
            int16_t v = ((i % period) < (period / 2)) ? 6000 : -6000;
            pcm[i * 2]     = v;
            pcm[i * 2 + 1] = v;
        }
        fwrite(pcm, 1, sizeof(pcm), dsp);
    }
    fclose(dsp);
    printf("[/dev/dsp] Startup chord complete!\n");
    return 0;
}
