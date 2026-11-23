// Audio stub for the ESP32-S3 port.
// Audio is deferred until the I2S hardware is validated; this header only
// provides the SfxSpec parameter type so the game's sound-effect tables and
// call sites compile unchanged.
#ifndef AUDIO_STUB_H
#define AUDIO_STUB_H

#include <stdint.h>
#include <stddef.h>

// Synthesis parameters for one sound effect; layout identical to the RP audio.
struct SfxSpec {
    uint32_t freq;         // start frequency Hz
    uint32_t duration_ms;  // sustain duration
    uint32_t attack_ms;
    uint32_t decay_ms;
    uint8_t  sustain;      // 0..100
    uint32_t release_ms;
    int32_t  bend;         // frequency slide Hz (0 = none)
    uint32_t bend_ms;
    uint8_t  noise;        // 0..100 mixed noise amount
    uint8_t  volume;       // 0..100
};

#endif
