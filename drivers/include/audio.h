#ifndef SONION_AUDIO_H
#define SONION_AUDIO_H

#include <stdint.h>
#include <stddef.h>

enum {
    AUDIO_BACKEND_NONE  = 0,
    AUDIO_BACKEND_PCSPK = 1,
    AUDIO_BACKEND_SB16  = 2,
    AUDIO_BACKEND_AC97  = 3,
    AUDIO_BACKEND_HDA   = 4
};

void        audio_init(void);
int         audio_backend(void);
const char *audio_backend_name(void);
void        speaker_beep(uint32_t freq_hz, uint32_t duration_ms);
int         audio_write_pcm(const void *pcm_16bit_stereo_48k, size_t bytes);

#endif
