#ifndef CANDLE_PLUGINS_AUDIO_H
#define CANDLE_PLUGINS_AUDIO_H

#include "../candle_runtime.h"

#ifdef CANDLE_AUDIO_IMPL
#define MINIAUDIO_IMPLEMENTATION
#endif
#include "../vendor/miniaudio/miniaudio.h"

typedef struct {
    ma_engine engine;
    int initialized;
} CandleAudio;

static CandleAudio g_candle_audio = {0};

static inline candle_bool audio_init(void) {
    if (g_candle_audio.initialized) return 1;
    ma_result r = ma_engine_init(NULL, &g_candle_audio.engine);
    if (r != MA_SUCCESS) return 0;
    g_candle_audio.initialized = 1;
    return 1;
}

static inline void audio_shutdown(void) {
    if (!g_candle_audio.initialized) return;
    ma_engine_uninit(&g_candle_audio.engine);
    g_candle_audio.initialized = 0;
}

static inline candle_bool audio_play(candle_string path) {
    if (!g_candle_audio.initialized && !audio_init()) return 0;
    return ma_engine_play_sound(&g_candle_audio.engine, path, NULL) == MA_SUCCESS;
}

static inline void audio_set_volume(candle_double v) {
    if (g_candle_audio.initialized)
        ma_engine_set_volume(&g_candle_audio.engine, (float)v);
}

#endif
