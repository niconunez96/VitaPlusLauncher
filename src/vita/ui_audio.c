#include <psp2/audioout.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ui_audio.h"

#define VP_UI_AUDIO_SAMPLE_RATE 48000
#define VP_UI_AUDIO_BUFFER_SAMPLES 1024

typedef struct {
    const char *path;
} VpUiAudioAsset;

typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool running;
    bool pending;
    VpUiSound sound;
} VpUiAudioPlayer;

/* UI sounds are serialized on a dedicated thread because sceAudioOutOutput blocks
 * while a buffer is consumed. Keeping only the newest pending cue prevents rapid
 * directional input from accumulating stale navigation clicks. */
static VpUiAudioPlayer player = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .condition = PTHREAD_COND_INITIALIZER
};

static const VpUiAudioAsset assets[] = {
    [VP_UI_SOUND_NAVIGATION] = {"app0:assets/audio/01-navegacion-alternativa.wav"},
    [VP_UI_SOUND_TAB] = {"app0:assets/audio/02-cambio-tab.wav"},
    [VP_UI_SOUND_ENTER] = {"app0:assets/audio/03-entrar.wav"},
    [VP_UI_SOUND_BACK] = {"app0:assets/audio/04-volver-salir.wav"},
    [VP_UI_SOUND_DOWNLOAD_START] = {"app0:assets/audio/05-iniciar-descarga.wav"},
    [VP_UI_SOUND_DOWNLOAD_COMPLETE] = {"app0:assets/audio/06-descarga-completa.wav"}
};

static unsigned int read_u32_le(const unsigned char *bytes) {
    return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8) |
           ((unsigned int)bytes[2] << 16) | ((unsigned int)bytes[3] << 24);
}

static unsigned int read_u16_le(const unsigned char *bytes) {
    return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

static bool seek_pcm_data(FILE *file) {
    unsigned char riff[12];
    bool saw_format = false;
    if (fread(riff, 1, sizeof(riff), file) != sizeof(riff) ||
        memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) return false;
    for (;;) {
        unsigned char chunk[8];
        unsigned int size;
        if (fread(chunk, 1, sizeof(chunk), file) != sizeof(chunk)) return false;
        size = read_u32_le(chunk + 4);
        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char format[16];
            if (size < sizeof(format) || fread(format, 1, sizeof(format), file) != sizeof(format) ||
                read_u16_le(format) != 1 || read_u16_le(format + 2) != 1 ||
                read_u32_le(format + 4) != VP_UI_AUDIO_SAMPLE_RATE || read_u16_le(format + 14) != 16)
                return false;
            if (size > sizeof(format) && fseek(file, (long)(size - sizeof(format)), SEEK_CUR) != 0)
                return false;
            saw_format = true;
        } else if (memcmp(chunk, "data", 4) == 0) {
            return saw_format;
        } else if (fseek(file, (long)size, SEEK_CUR) != 0) {
            return false;
        }
        if ((size & 1U) != 0U && fseek(file, 1, SEEK_CUR) != 0) return false;
    }
}

static void play_sound(int port, VpUiSound sound) {
    FILE *file;
    int16_t samples[VP_UI_AUDIO_BUFFER_SAMPLES];
    if ((unsigned int)sound >= sizeof(assets) / sizeof(assets[0])) return;
    file = fopen(assets[sound].path, "rb");
    if (file == NULL || !seek_pcm_data(file)) {
        if (file != NULL) fclose(file);
        return;
    }
    for (;;) {
        size_t count = fread(samples, sizeof(samples[0]), VP_UI_AUDIO_BUFFER_SAMPLES, file);
        if (count == 0) break;
        if (count < VP_UI_AUDIO_BUFFER_SAMPLES)
            memset(samples + count, 0, (VP_UI_AUDIO_BUFFER_SAMPLES - count) * sizeof(samples[0]));
        if (sceAudioOutOutput(port, samples) < 0) break;
    }
    fclose(file);
}

static void *audio_thread(void *context) {
    VpUiAudioPlayer *audio = context;
    int port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, VP_UI_AUDIO_BUFFER_SAMPLES,
                                   VP_UI_AUDIO_SAMPLE_RATE, SCE_AUDIO_OUT_MODE_MONO);
    if (port >= 0) {
        int volume = SCE_AUDIO_OUT_MAX_VOL / 2;
        (void)sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH, &volume);
    }
    for (;;) {
        VpUiSound sound;
        pthread_mutex_lock(&audio->mutex);
        while (audio->running && !audio->pending)
            (void)pthread_cond_wait(&audio->condition, &audio->mutex);
        if (!audio->running) {
            pthread_mutex_unlock(&audio->mutex);
            break;
        }
        sound = audio->sound;
        audio->pending = false;
        pthread_mutex_unlock(&audio->mutex);
        if (port >= 0) play_sound(port, sound);
    }
    if (port >= 0) (void)sceAudioOutReleasePort(port);
    return NULL;
}

bool vp_ui_audio_init(void) {
    int result;
    pthread_mutex_lock(&player.mutex);
    if (player.running) {
        pthread_mutex_unlock(&player.mutex);
        return true;
    }
    player.pending = false;
    player.running = true;
    result = pthread_create(&player.thread, NULL, audio_thread, &player);
    if (result != 0) player.running = false;
    pthread_mutex_unlock(&player.mutex);
    return result == 0;
}

void vp_ui_audio_play(VpUiSound sound) {
    pthread_mutex_lock(&player.mutex);
    if (player.running) {
        player.sound = sound;
        player.pending = true;
        (void)pthread_cond_signal(&player.condition);
    }
    pthread_mutex_unlock(&player.mutex);
}

void vp_ui_audio_shutdown(void) {
    bool join = false;
    pthread_mutex_lock(&player.mutex);
    if (player.running) {
        player.running = false;
        (void)pthread_cond_signal(&player.condition);
        join = true;
    }
    pthread_mutex_unlock(&player.mutex);
    if (join) (void)pthread_join(player.thread, NULL);
}
