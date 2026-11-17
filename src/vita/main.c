#include <psp2/ctrl.h>
#include <psp2/ime_dialog.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <vita2d.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vita_backend.h"
#include "vitaplus.h"

#define RGBA(r, g, b, a) ((unsigned int)((r) | ((g) << 8) | ((b) << 16) | ((a) << 24)))
#define VP_CONFIG_PATH "ux0:data/vitaplus/config.ini"

static VpRommConfig romm_config;
static VpHomebrewConfig homebrew_config;
static VpLaunchConfig launch_config;
static VpApp app;
static char active_theme[VP_TEXT] = "default";
static char romm_pair_code[VP_TEXT];
static int settings_section;
static int settings_field;
static bool settings_detail_active;
static bool settings_ime_active;
static SceWChar16 settings_ime_buffer[VP_URL];

typedef enum {
    VP_JOB_NONE,
    VP_JOB_SYNC_PROVIDERS,
    VP_JOB_PAIR_AND_SYNC,
    VP_JOB_DOWNLOAD_ROM,
    VP_JOB_INSTALL_HOMEBREW,
    VP_JOB_FETCH_COVER
} VpJobKind;

typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    bool active;
    bool completed;
    bool succeeded;
    VpJobKind kind;
    VpRommConfig romm;
    VpHomebrewConfig homebrew;
    VpItem item;
    VpCatalog catalog;
    char destination[VP_PATH];
    char partial[VP_PATH + 8];
    char pair_code[VP_TEXT];
    char error[VP_TEXT];
} VpBackgroundJob;

/* The core fetch buffer and backend diagnostics are deliberately single-owner. One worker keeps
 * all network/disk work off the UI thread without making those shared resources concurrent. */
static VpBackgroundJob background_job = {
    .mutex = PTHREAD_MUTEX_INITIALIZER
};
#define VP_COVER_SLOTS 8
static VpPlatformGroup platform_groups[VP_MAX_PLATFORMS];
static size_t visible_indexes[VP_MAX_ITEMS];
static size_t platform_group_count;
static size_t visible_count_cache;
static VpLibraryLevel cached_library_level;
static char cached_platform[VP_TEXT];
static bool catalog_view_cache_dirty = true;
static char unavailable_cover_ids[VP_COVER_SLOTS][VP_TEXT];

static bool cover_is_unavailable(const char *id) {
    size_t index;
    for (index = 0; index < VP_COVER_SLOTS; index++) {
        if (strcmp(unavailable_cover_ids[index], id) == 0) return true;
    }
    return false;
}

static void mark_cover_unavailable(const char *id) {
    size_t index;
    for (index = 0; index < VP_COVER_SLOTS; index++) {
        if (unavailable_cover_ids[index][0] == '\0') {
            (void)snprintf(unavailable_cover_ids[index], sizeof(unavailable_cover_ids[index]), "%s", id);
            return;
        }
    }
}

static void trim(char *value) {
    size_t length;
    char *start = value;
    while (*start == ' ' || *start == '\t') start++;
    if (start != value) memmove(value, start, strlen(start) + 1);
    length = strlen(value);
    while (length > 0 && (value[length - 1] == '\r' || value[length - 1] == '\n' ||
                          value[length - 1] == ' ' || value[length - 1] == '\t')) value[--length] = '\0';
}

static unsigned int theme_color(const char *hex, unsigned int fallback) {
    unsigned int red, green, blue;
    if (hex == NULL || sscanf(hex, "#%02x%02x%02x", &red, &green, &blue) != 3) return fallback;
    return RGBA(red, green, blue, 255);
}

static void load_theme_file(VpApp *app, const char *path) {
    FILE *file = fopen(path, "r");
    char text[2048];
    size_t read_count;
    if (file == NULL) return;
    read_count = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    text[read_count] = '\0';
    (void)vp_theme_load(&app->theme, text);
    app->active_view = app->theme.view;
}

static void load_providers(VpApp *app) {
    FILE *file = fopen(VP_CONFIG_PATH, "r");
    char line[512];
    load_theme_file(app, "app0:assets/default/theme.ini");
    if (file == NULL) return;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *equals = strchr(line, '=');
        if (equals == NULL || line[0] == '#') continue;
        *equals++ = '\0';
        trim(line);
        trim(equals);
        if (strcmp(line, "romm_url") == 0) (void)snprintf(romm_config.base_url, sizeof(romm_config.base_url), "%s", equals);
        else if (strcmp(line, "romm_token") == 0) (void)snprintf(romm_config.token, sizeof(romm_config.token), "%s", equals);
        else if (strcmp(line, "homebrew_catalog") == 0)
            (void)snprintf(homebrew_config.catalog_url, sizeof(homebrew_config.catalog_url), "%s", equals);
        else if (strcmp(line, "active_theme") == 0)
            (void)snprintf(active_theme, sizeof(active_theme), "%s", equals);
        else if (strncmp(line, "launch_", 7) == 0)
            (void)vp_launch_config_set(&launch_config, line + 7, equals);
    }
    fclose(file);
    if (strcmp(active_theme, "default") != 0) {
        char path[VP_PATH];
        (void)snprintf(path, sizeof(path), "ux0:data/vitaplus/themes/%s/theme.ini", active_theme);
        load_theme_file(app, path);
    }
}

static bool save_romm_config(void) {
    FILE *file;
    size_t index;
    (void)sceIoMkdir("ux0:data", 0777);
    (void)sceIoMkdir("ux0:data/vitaplus", 0777);
    file = fopen(VP_CONFIG_PATH, "w");
    if (file == NULL) return false;
    (void)fprintf(file, "romm_url=%s\nromm_token=%s\nhomebrew_catalog=%s\nactive_theme=%s\n",
                  romm_config.base_url, romm_config.token, homebrew_config.catalog_url, active_theme);
    for (index = 0; index < launch_config.count; index++) {
        const VpLaunchProfile *profile = &launch_config.profiles[index];
        (void)fprintf(file, "launch_%s=%s:%s\n", profile->platform, profile->titleid,
                      profile->core);
    }
    return fclose(file) == 0;
}

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    fclose(file);
    return true;
}

static void refresh_downloaded_items(VpApp *app) {
    size_t index;
    if (app == NULL) return;
    for (index = 0; index < app->catalog.count; index++) {
        VpItem *item = &app->catalog.items[index];
        char path[VP_PATH];
        if (item->source != VP_SOURCE_ROMM) continue;
        if (item->multi_file ||
            !vp_item_download_path(item, "ux0:data/vitaplus/downloads", path, sizeof(path)) ||
            !file_exists(path)) {
            item->installed = false;
            item->launch_target[0] = '\0';
            continue;
        }
        item->installed = true;
        (void)snprintf(item->launch_target, sizeof(item->launch_target), "%s", path);
    }
}

static void apply_provider_catalog(VpApp *app, const VpCatalog *catalog) {
    size_t index = 0;
    if (app == NULL || catalog == NULL) return;
    while (index < app->catalog.count) {
        if (app->catalog.items[index].source == VP_SOURCE_ROMM ||
            app->catalog.items[index].source == VP_SOURCE_HOMEBREW) {
            app->catalog.items[index] = app->catalog.items[app->catalog.count - 1];
            app->catalog.count--;
        } else {
            index++;
        }
    }
    for (index = 0; index < catalog->count; index++)
        (void)vp_catalog_add(&app->catalog, &catalog->items[index]);
    app->library_level = VP_LEVEL_PLATFORMS;
    app->active_platform[0] = '\0';
    app->selected_platform = 0;
    app->selected_item = 0;
    refresh_downloaded_items(app);
    catalog_view_cache_dirty = true;
}

static void *background_job_run(void *context) {
    VpBackgroundJob *job = context;
    VpJobKind kind;
    VpRommConfig config;
    VpHomebrewConfig homebrew;
    VpItem item;
    char pair_code[VP_TEXT];
    char destination[VP_PATH];
    char partial[VP_PATH + 8];
    bool succeeded = false;
    bool romm_requested = false;
    bool homebrew_requested = false;
    char error[VP_TEXT];

    pthread_mutex_lock(&job->mutex);
    kind = job->kind;
    config = job->romm;
    homebrew = job->homebrew;
    item = job->item;
    (void)snprintf(destination, sizeof(destination), "%s", job->destination);
    (void)snprintf(partial, sizeof(partial), "%s", job->partial);
    (void)snprintf(pair_code, sizeof(pair_code), "%s", job->pair_code);
    pthread_mutex_unlock(&job->mutex);

    if (kind == VP_JOB_SYNC_PROVIDERS || kind == VP_JOB_PAIR_AND_SYNC) {
        VpFetchBackend fetch_backend = {vp_vita_fetch, NULL};
        vp_catalog_init(&job->catalog);
        if (kind == VP_JOB_PAIR_AND_SYNC) {
            VpRommPairBackend pair_backend = {vp_vita_exchange_pair_code, NULL};
            succeeded = vp_romm_exchange_pair_code(&config, pair_code, pair_backend);
            if (!succeeded) goto finish;
        }
        if (config.base_url[0] != '\0' && config.token[0] != '\0') {
            romm_requested = true;
            succeeded = vp_romm_sync(&job->catalog, &config, fetch_backend);
        }
        if (homebrew.catalog_url[0] != '\0') {
            homebrew_requested = true;
            succeeded = vp_homebrew_sync(&job->catalog, &homebrew, fetch_backend) &&
                        (!romm_requested || succeeded);
        }
        if (!romm_requested && !homebrew_requested) succeeded = false;
    } else {
        VpTransferBackend transfer_backend = {vp_vita_transfer, NULL};
        const char *target = kind == VP_JOB_DOWNLOAD_ROM ? partial : destination;
        if (kind == VP_JOB_INSTALL_HOMEBREW)
            succeeded = vp_install_homebrew(&item, target, transfer_backend);
        else if (kind == VP_JOB_FETCH_COVER)
            succeeded = vp_cache_cover(&item, &config, target, transfer_backend);
        else
            succeeded = vp_queue_download(&item, &config, target, transfer_backend);
        if (succeeded && kind == VP_JOB_DOWNLOAD_ROM) {
            succeeded = sceIoRename(partial, destination) >= 0;
            if (!succeeded) (void)sceIoRemove(partial);
        }
    }
finish:
    (void)snprintf(error, sizeof(error), "%s", succeeded ? "" : vp_vita_last_error());

    pthread_mutex_lock(&job->mutex);
    if ((kind == VP_JOB_SYNC_PROVIDERS || kind == VP_JOB_PAIR_AND_SYNC) && succeeded) {
        job->romm = config;
    }
    job->succeeded = succeeded;
    (void)snprintf(job->error, sizeof(job->error), "%s", error);
    job->completed = true;
    pthread_mutex_unlock(&job->mutex);
    return NULL;
}

static bool background_job_start(VpJobKind kind, const VpItem *item, const char *destination,
                                 const char *partial) {
    int result;
    pthread_mutex_lock(&background_job.mutex);
    if (background_job.active) {
        pthread_mutex_unlock(&background_job.mutex);
        return false;
    }
    background_job.kind = kind;
    background_job.completed = false;
    background_job.succeeded = false;
    background_job.error[0] = '\0';
    background_job.romm = romm_config;
    background_job.homebrew = homebrew_config;
    (void)snprintf(background_job.pair_code, sizeof(background_job.pair_code), "%s", romm_pair_code);
    memset(&background_job.item, 0, sizeof(background_job.item));
    if (item != NULL) background_job.item = *item;
    (void)snprintf(background_job.destination, sizeof(background_job.destination), "%s",
                   destination != NULL ? destination : "");
    (void)snprintf(background_job.partial, sizeof(background_job.partial), "%s",
                   partial != NULL ? partial : "");
    background_job.active = true;
    result = pthread_create(&background_job.thread, NULL, background_job_run, &background_job);
    if (result != 0) background_job.active = false;
    pthread_mutex_unlock(&background_job.mutex);
    return result == 0;
}

static void background_job_poll(VpApp *app) {
    VpJobKind kind;
    VpItem item;
    VpRommConfig config;
    char destination[VP_PATH];
    char error[VP_TEXT];
    bool succeeded;

    pthread_mutex_lock(&background_job.mutex);
    if (!background_job.active || !background_job.completed) {
        pthread_mutex_unlock(&background_job.mutex);
        return;
    }
    kind = background_job.kind;
    succeeded = background_job.succeeded;
    item = background_job.item;
    config = background_job.romm;
    (void)snprintf(destination, sizeof(destination), "%s", background_job.destination);
    (void)snprintf(error, sizeof(error), "%s", background_job.error);
    pthread_mutex_unlock(&background_job.mutex);

    (void)pthread_join(background_job.thread, NULL);
    pthread_mutex_lock(&background_job.mutex);
    background_job.active = false;
    background_job.kind = VP_JOB_NONE;
    pthread_mutex_unlock(&background_job.mutex);

    if (!succeeded) {
        if (kind == VP_JOB_FETCH_COVER) {
            mark_cover_unavailable(item.id);
            return;
        }
        (void)snprintf(app->notice, sizeof(app->notice), "Operacion fallida: %.140s", error);
        return;
    }
    if (kind == VP_JOB_SYNC_PROVIDERS || kind == VP_JOB_PAIR_AND_SYNC) {
        if (kind == VP_JOB_PAIR_AND_SYNC) {
            romm_config = config;
            memset(romm_pair_code, 0, sizeof(romm_pair_code));
            if (!save_romm_config()) {
                (void)snprintf(app->notice, sizeof(app->notice), "Sincronizado, pero no se pudo guardar la configuracion");
                return;
            }
        }
        apply_provider_catalog(app, &background_job.catalog);
        (void)snprintf(app->notice, sizeof(app->notice), "Biblioteca sincronizada: %u juegos",
                       (unsigned)app->catalog.count);
    } else if (kind == VP_JOB_DOWNLOAD_ROM) {
        VpItem *completed = vp_catalog_find(&app->catalog, item.id);
        if (completed != NULL) {
            completed->installed = true;
            (void)snprintf(completed->launch_target, sizeof(completed->launch_target), "%s",
                           destination);
        }
        (void)snprintf(app->notice, sizeof(app->notice), "Descarga finalizada; presiona X para jugar");
    } else if (kind == VP_JOB_INSTALL_HOMEBREW) {
        VpItem *completed = vp_catalog_find(&app->catalog, item.id);
        if (completed != NULL) completed->installed = true;
        (void)snprintf(app->notice, sizeof(app->notice), "Instalacion finalizada; revisa LiveArea");
    }
}

static bool sync_romm_catalog(VpApp *app) {
    (void)app;
    if ((romm_config.base_url[0] == '\0' || romm_config.token[0] == '\0') &&
        homebrew_config.catalog_url[0] == '\0') return false;
    return background_job_start(VP_JOB_SYNC_PROVIDERS, NULL, NULL, NULL);
}

static void settings_text_to_wide(const char *source, SceWChar16 *destination, size_t capacity) {
    size_t index;
    for (index = 0; index + 1 < capacity && source[index] != '\0'; index++)
        destination[index] = (unsigned char)source[index];
    destination[index] = 0;
}

static void settings_wide_to_text(const SceWChar16 *source, char *destination, size_t capacity) {
    size_t index;
    for (index = 0; index + 1 < capacity && source[index] != 0; index++)
        destination[index] = source[index] <= 0x7f ? (char)source[index] : '?';
    destination[index] = '\0';
}

static bool settings_start_ime(void) {
    SceImeDialogParam param;
    const char *value = settings_field == 0 ? romm_config.base_url : romm_pair_code;
    size_t capacity = settings_field == 0 ? sizeof(romm_config.base_url) : sizeof(romm_pair_code);
    settings_text_to_wide(value, settings_ime_buffer, sizeof(settings_ime_buffer) / sizeof(settings_ime_buffer[0]));
    sceImeDialogParamInit(&param);
    param.supportedLanguages = SCE_IME_LANGUAGE_ENGLISH | SCE_IME_LANGUAGE_SPANISH;
    param.type = settings_field == 0 ? SCE_IME_TYPE_URL : SCE_IME_TYPE_BASIC_LATIN;
    param.option = SCE_IME_OPTION_NO_AUTO_CAPITALIZATION;
    param.dialogMode = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
    param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
    param.title = settings_field == 0 ? (const SceWChar16 *)u"URL de RomM" : (const SceWChar16 *)u"Pair code de RomM";
    param.maxTextLength = (SceUInt32)(capacity - 1);
    param.initialText = settings_ime_buffer;
    param.inputTextBuffer = settings_ime_buffer;
    param.enterLabel = SCE_IME_ENTER_LABEL_GO;
    settings_ime_active = sceImeDialogInit(&param) >= 0;
    return settings_ime_active;
}

static void settings_finish_ime(VpApp *app) {
    SceImeDialogResult result;
    if (!settings_ime_active || sceImeDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) return;
    memset(&result, 0, sizeof(result));
    (void)sceImeDialogGetResult(&result);
    if (result.button == SCE_IME_DIALOG_BUTTON_ENTER) {
        char *value = settings_field == 0 ? romm_config.base_url : romm_pair_code;
        size_t capacity = settings_field == 0 ? sizeof(romm_config.base_url) : sizeof(romm_pair_code);
        settings_wide_to_text(settings_ime_buffer, value, capacity);
        (void)snprintf(app->notice, sizeof(app->notice), "%s actualizado; Triangulo guarda y sincroniza",
                       settings_field == 0 ? "URL" : "Pair code");
    }
    (void)sceImeDialogTerm();
    settings_ime_active = false;
}

static void handle_settings_input(VpApp *app, unsigned int pressed) {
    if (!settings_detail_active) {
        if (pressed & SCE_CTRL_UP) settings_section = 0;
        if (pressed & SCE_CTRL_DOWN) settings_section = 1;
        if (pressed & SCE_CTRL_CROSS) settings_detail_active = true;
        return;
    }
    if (pressed & SCE_CTRL_CIRCLE) {
        settings_detail_active = false;
        return;
    }
    if (settings_section != 0) return;
    if (pressed & SCE_CTRL_UP) settings_field = 0;
    if (pressed & SCE_CTRL_DOWN) settings_field = 1;
    if (pressed & SCE_CTRL_CROSS && !settings_start_ime())
        (void)snprintf(app->notice, sizeof(app->notice), "No se pudo abrir el teclado de PS Vita");
    if (pressed & SCE_CTRL_TRIANGLE) {
        if (romm_pair_code[0] != '\0') {
            if (background_job_start(VP_JOB_PAIR_AND_SYNC, NULL, NULL, NULL))
                (void)snprintf(app->notice, sizeof(app->notice), "Canjeando y sincronizando en segundo plano");
            else
                (void)snprintf(app->notice, sizeof(app->notice), "Hay una operacion en curso; espera a que termine");
        } else if (!save_romm_config()) {
            (void)snprintf(app->notice, sizeof(app->notice), "No se pudo guardar la configuracion");
        } else if (sync_romm_catalog(app)) {
            (void)snprintf(app->notice, sizeof(app->notice), "Sincronizacion iniciada en segundo plano");
        } else {
            (void)snprintf(app->notice, sizeof(app->notice), "No se pudo iniciar la sincronizacion");
        }
    }
}

typedef struct {
    char id[VP_TEXT];
    vita2d_texture *texture;
    bool attempted;
} VpCoverSlot;

static VpCoverSlot cover_slots[VP_COVER_SLOTS];
static size_t cover_indexes[VP_COVER_SLOTS];

static void refresh_catalog_view_cache(VpApp *app) {
    if (app == NULL) return;
    if (app->library_level == VP_LEVEL_PLATFORMS) {
        if (catalog_view_cache_dirty || cached_library_level != VP_LEVEL_PLATFORMS) {
            platform_group_count = vp_catalog_platforms(&app->catalog, platform_groups,
                                                        VP_MAX_PLATFORMS);
            cached_library_level = VP_LEVEL_PLATFORMS;
            cached_platform[0] = '\0';
            catalog_view_cache_dirty = false;
        }
        return;
    }
    if (catalog_view_cache_dirty || cached_library_level != VP_LEVEL_ROMS ||
        strcmp(cached_platform, app->active_platform) != 0) {
        visible_count_cache = vp_app_visible(app, visible_indexes, VP_MAX_ITEMS);
        cached_library_level = VP_LEVEL_ROMS;
        (void)snprintf(cached_platform, sizeof(cached_platform), "%s", app->active_platform);
        catalog_view_cache_dirty = false;
    }
}

static void safe_id(const char *id, char *destination, size_t capacity) {
    size_t index;
    if (capacity == 0) return;
    for (index = 0; id[index] != '\0' && index + 1 < capacity; index++) {
        char value = id[index];
        destination[index] = ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                              (value >= '0' && value <= '9') || value == '-' || value == '_') ? value : '_';
    }
    destination[index] = '\0';
}

static void cover_path_for(const char *id, char *path, size_t capacity) {
    char safe[VP_TEXT];
    safe_id(id, safe, sizeof(safe));
    (void)snprintf(path, capacity, "ux0:data/vitaplus/cache/covers/libretro-%s.png", safe);
}

static VpCoverSlot *find_cover_slot(const char *id) {
    size_t index;
    if (id == NULL || id[0] == '\0') return NULL;
    for (index = 0; index < VP_COVER_SLOTS; index++) {
        if (cover_slots[index].id[0] != '\0' && strcmp(cover_slots[index].id, id) == 0)
            return &cover_slots[index];
    }
    return NULL;
}

static vita2d_texture *cover_texture_for(const char *id) {
    VpCoverSlot *slot = find_cover_slot(id);
    return slot != NULL ? slot->texture : NULL;
}

static VpCoverSlot *acquire_slot(const char *id, const size_t *indexes, size_t count) {
    size_t index;
    size_t other;
    VpCoverSlot *slot = NULL;
    for (index = 0; index < VP_COVER_SLOTS; index++) {
        if (cover_slots[index].id[0] == '\0') {
            slot = &cover_slots[index];
            break;
        }
    }
    if (slot == NULL) {
        for (index = 0; index < VP_COVER_SLOTS; index++) {
            bool visible = false;
            for (other = 0; other < count; other++) {
                if (strcmp(cover_slots[index].id, app.catalog.items[indexes[other]].id) == 0) {
                    visible = true;
                    break;
                }
            }
            if (!visible) {
                slot = &cover_slots[index];
                break;
            }
        }
    }
    if (slot == NULL) slot = &cover_slots[0];
    if (slot->texture != NULL) vita2d_free_texture(slot->texture);
    slot->texture = NULL;
    slot->attempted = false;
    (void)snprintf(slot->id, sizeof(slot->id), "%s", id);
    return slot;
}

static vita2d_texture *load_texture_file(const char *path) {
    FILE *file;
    unsigned char header[29];
    size_t read_count;
    long file_size;
    file = fopen(path, "rb");
    if (file == NULL) return NULL;
    if (fseek(file, 0, SEEK_END) != 0 || (file_size = ftell(file)) < 0 || file_size > 512 * 1024 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    read_count = fread(header, 1, sizeof(header), file);
    fclose(file);
    if (vp_safe_cover_png(header, read_count))
        return vita2d_load_PNG_file(path);
    return NULL;
}

static void covers_step(VpApp *app, const size_t *indexes, size_t count) {
    size_t index;
    if (count > VP_COVER_SLOTS) count = VP_COVER_SLOTS;
    for (index = 0; index < count; index++) {
        const VpItem *item = &app->catalog.items[indexes[index]];
        VpCoverSlot *slot;
        VpItem thumbnail_item;
        char path[VP_PATH];
        char thumbnail_url[VP_URL];
        FILE *file;
        if (!vp_libretro_thumbnail_url(item, thumbnail_url, sizeof(thumbnail_url))) continue;
        if (cover_is_unavailable(item->id)) continue;
        if (find_cover_slot(item->id) != NULL) continue;
        thumbnail_item = *item;
        (void)snprintf(thumbnail_item.cover_url, sizeof(thumbnail_item.cover_url), "%s", thumbnail_url);
        cover_path_for(item->id, path, sizeof(path));
        file = fopen(path, "rb");
        if (file != NULL) {
            fclose(file);
            slot = acquire_slot(item->id, indexes, count);
            if (slot == NULL) return;
            slot->texture = load_texture_file(path);
            if (slot->texture == NULL) (void)sceIoRemove(path);
            slot->attempted = true;
            return;
        }
        if (!background_job_start(VP_JOB_FETCH_COVER, &thumbnail_item, path, NULL)) {
            continue;
        }
        return;
    }
}

static void draw_cover_fit(vita2d_texture *texture, int x, int y, int width, int height) {
    int texture_width;
    int texture_height;
    float scale;
    int draw_width;
    int draw_height;
    if (texture == NULL) return;
    texture_width = vita2d_texture_get_width(texture);
    texture_height = vita2d_texture_get_height(texture);
    if (texture_width <= 0 || texture_height <= 0) return;
    scale = (float)width / (float)texture_width;
    if ((float)height / (float)texture_height < scale) scale = (float)height / (float)texture_height;
    draw_width = (int)((float)texture_width * scale);
    draw_height = (int)((float)texture_height * scale);
    vita2d_draw_texture_scale(texture, (float)(x + (width - draw_width) / 2),
                              (float)(y + (height - draw_height) / 2), scale, scale);
}

static void draw_background(vita2d_texture *texture, unsigned int background) {
    vita2d_draw_rectangle(0, 0, 960, 544, background);
    if (texture != NULL) {
        int texture_width = vita2d_texture_get_width(texture);
        int texture_height = vita2d_texture_get_height(texture);
        float scale;
        if (texture_width <= 0 || texture_height <= 0) return;
        scale = 960.0f / (float)texture_width;
        if (544.0f / (float)texture_height > scale) scale = 544.0f / (float)texture_height;
        vita2d_draw_texture_tint_scale(texture, (float)((960 - (int)((float)texture_width * scale)) / 2),
                                       (float)((544 - (int)((float)texture_height * scale)) / 2),
                                       scale, scale, RGBA(80, 85, 100, 255));
    }
}

static size_t platform_first_item(const VpCatalog *catalog, const char *platform) {
    size_t index;
    for (index = 0; index < catalog->count; index++) {
        if (strcmp(catalog->items[index].platform, platform) == 0) return index;
    }
    return catalog->count;
}

static size_t visible_position(const size_t *visible, size_t count, size_t item_index) {
    size_t index;
    for (index = 0; index < count; index++) {
        if (visible[index] == item_index) return index;
    }
    return 0;
}

static void draw_download_badge(int x, int y, bool installed, unsigned int accent);

static void draw_platforms(vita2d_pgf *font, const VpApp *app, size_t count,
                           unsigned int surface, unsigned int text, unsigned int accent,
                           unsigned int muted) {
    size_t start = (app->selected_platform / 8) * 8;
    size_t index;
    for (index = start; index < count && index < start + 8; index++) {
        size_t position = index - start;
        int column = (int)position % 4;
        int row = (int)position / 4;
        int x = 28 + column * 232;
        int y = 145 + row * 132;
        const VpPlatformGroup *group = &platform_groups[index];
        size_t item_index = platform_first_item(&app->catalog, group->platform);
        vita2d_texture *texture = item_index < app->catalog.count
                                      ? cover_texture_for(app->catalog.items[item_index].id)
                                      : NULL;
        unsigned int border = index == app->selected_platform ? accent : surface;
        vita2d_draw_rectangle(x, y, 216, 120, border);
        vita2d_draw_rectangle(x + 3, y + 3, 210, 114, surface);
        if (texture != NULL) draw_cover_fit(texture, x + 8, y + 12, 72, 96);
        else vita2d_draw_rectangle(x + 8, y + 12, 72, 96, muted);
        vita2d_pgf_draw_textf(font, x + 90, y + 42, text, 0.62f, "%.15s", group->display);
        vita2d_pgf_draw_textf(font, x + 90, y + 72, muted, 0.58f, "%u juegos", (unsigned)group->count);
    }
}

static void draw_rom_grid(vita2d_pgf *font, const VpApp *app, const size_t *visible, size_t count,
                          unsigned int surface, unsigned int text, unsigned int accent,
                          unsigned int muted) {
    size_t position = visible_position(visible, count, app->selected_item);
    size_t start = (position / 8) * 8;
    size_t index;
    for (index = start; index < count && index < start + 8; index++) {
        size_t slot = index - start;
        int column = (int)slot % 4;
        int row = (int)slot / 4;
        int x = 28 + column * 232;
        int y = 145 + row * 132;
        const VpItem *item = &app->catalog.items[visible[index]];
        vita2d_texture *texture = cover_texture_for(item->id);
        unsigned int border = visible[index] == app->selected_item ? accent : surface;
        vita2d_draw_rectangle(x, y, 216, 120, border);
        vita2d_draw_rectangle(x + 3, y + 3, 210, 114, surface);
        if (texture != NULL) draw_cover_fit(texture, x + 8, y + 12, 72, 96);
        else vita2d_draw_rectangle(x + 8, y + 12, 72, 96, muted);
        draw_download_badge(x + 54, y + 10, item->installed, accent);
        vita2d_pgf_draw_textf(font, x + 90, y + 38, text, 0.58f, "%.14s%s",
                              item->favorite ? "* " : "", item->title);
        vita2d_pgf_draw_textf(font, x + 90, y + 66, muted, 0.54f, "%.16s", item->platform_display);
    }
}

static void draw_list(vita2d_pgf *font, const VpApp *app, const size_t *visible, size_t count,
                      unsigned int text, unsigned int accent) {
    size_t position = visible_position(visible, count, app->selected_item);
    size_t start = (position / 6) * 6;
    size_t index;
    for (index = start; index < count && index < start + 6; index++) {
        const VpItem *item = &app->catalog.items[visible[index]];
        int y = 150 + (int)(index - start) * 38;
        unsigned int color = visible[index] == app->selected_item ? accent : text;
        draw_download_badge(36, y - 16, item->installed, accent);
        vita2d_pgf_draw_textf(font, 72, y, color, 0.76f, "%c %s%s [%s]",
                              visible[index] == app->selected_item ? '>' : ' ',
                              item->favorite ? "* " : "", item->title, item->platform_display);
    }
}

static void draw_carousel(vita2d_pgf *font, const VpApp *app, const size_t *visible, size_t count,
                          unsigned int surface, unsigned int text, unsigned int accent) {
    size_t position = visible_position(visible, count, app->selected_item);
    size_t previous = (position + count - 1) % count;
    size_t next = (position + 1) % count;
    const VpItem *center = &app->catalog.items[visible[position]];
    vita2d_texture *left = cover_texture_for(app->catalog.items[visible[previous]].id);
    vita2d_texture *middle = cover_texture_for(center->id);
    vita2d_texture *right = cover_texture_for(app->catalog.items[visible[next]].id);
    vita2d_draw_rectangle(45, 180, 200, 180, surface);
    vita2d_draw_rectangle(360, 135, 240, 270, accent);
    vita2d_draw_rectangle(715, 180, 200, 180, surface);
    if (left != NULL) draw_cover_fit(left, 55, 190, 180, 160);
    if (middle != NULL) draw_cover_fit(middle, 370, 145, 220, 250);
    if (right != NULL) draw_cover_fit(right, 725, 190, 180, 160);
    draw_download_badge(562, 149, center->installed, accent);
    vita2d_pgf_draw_textf(font, 62, 280, text, 0.62f, "%.12s", app->catalog.items[visible[previous]].title);
    vita2d_pgf_draw_textf(font, 382, 420, text, 0.72f, "%.18s", center->title);
    vita2d_pgf_draw_textf(font, 382, 445, surface, 0.58f, "%.18s", center->platform_display);
    vita2d_pgf_draw_textf(font, 732, 280, text, 0.62f, "%.12s", app->catalog.items[visible[next]].title);
}

static void draw_download_badge(int x, int y, bool installed, unsigned int accent) {
    unsigned int white = RGBA(255, 255, 255, 255);
    vita2d_draw_rectangle(x, y, 26, 26, RGBA(12, 15, 22, 220));
    if (installed) {
        vita2d_draw_line(x + 6, y + 13, x + 11, y + 18, white);
        vita2d_draw_line(x + 11, y + 18, x + 19, y + 6, white);
        vita2d_draw_line(x + 6, y + 14, x + 11, y + 19, white);
        vita2d_draw_line(x + 11, y + 19, x + 19, y + 7, white);
    } else {
        vita2d_draw_rectangle(x + 12, y + 5, 2, 12, accent);
        vita2d_draw_line(x + 13, y + 16, x + 6, y + 10, accent);
        vita2d_draw_line(x + 13, y + 17, x + 6, y + 11, accent);
        vita2d_draw_line(x + 13, y + 16, x + 20, y + 10, accent);
        vita2d_draw_line(x + 13, y + 17, x + 20, y + 11, accent);
    }
}

static void activate_selected(VpApp *app) {
    VpItem *item;
    if (app->catalog.count == 0 || app->selected_item >= app->catalog.count) return;
    item = &app->catalog.items[app->selected_item];
    if (item->source == VP_SOURCE_HOMEBREW) {
        char destination[VP_TEXT];
        char path[VP_PATH];
        safe_id(item->id, destination, sizeof(destination));
        if (!app->installation_armed) {
            app->installation_armed = true;
            (void)snprintf(app->notice, sizeof(app->notice), "Presiona X otra vez para instalar %.150s", item->title);
            return;
        }
        (void)snprintf(path, sizeof(path), "ux0:data/vitaplus/downloads/%s.vpk", destination);
        if (background_job_start(VP_JOB_INSTALL_HOMEBREW, item, path, NULL))
            (void)snprintf(app->notice, sizeof(app->notice), "Instalando %.110s en segundo plano", item->title);
        else
            (void)snprintf(app->notice, sizeof(app->notice), "Hay una operacion en curso; espera a que termine");
        app->installation_armed = false;
        return;
    }
    if (item->installed && item->launch_target[0] != '\0') {
        const VpLaunchProfile *profile = vp_launch_config_find(&launch_config, item->platform);
        char uri[VP_LAUNCH_URI];
        if (profile == NULL) {
            (void)snprintf(app->notice, sizeof(app->notice),
                           "Sin emulador para %.40s; agrega launch_%.40s=<titleid>[:<core>] en config.ini",
                           item->platform, item->platform);
            return;
        }
        if (!vp_build_launch_uri(profile, item->launch_target, uri, sizeof(uri))) {
            (void)snprintf(app->notice, sizeof(app->notice), "No se pudo preparar el lanzamiento");
            return;
        }
        if (!vp_vita_launch_uri(uri)) {
            (void)snprintf(app->notice, sizeof(app->notice),
                           "No se pudo abrir %.32s; verifica que el emulador este instalado",
                           profile->titleid);
            return;
        }
        sceKernelExitProcess(0);
        return;
    }
    if (item->source == VP_SOURCE_ROMM) {
        char path[VP_PATH];
        char partial[VP_PATH + 8];
        if (item->multi_file) {
            (void)snprintf(app->notice, sizeof(app->notice),
                           "Esta ROM tiene varios archivos; descargala desde RomM y extraela en el emulador");
            return;
        }
        if (item->download_url[0] == '\0') {
            (void)snprintf(app->notice, sizeof(app->notice),
                           "RomM no expuso un archivo descargable para esta ROM");
            return;
        }
        if (!vp_item_download_path(item, "ux0:data/vitaplus/downloads", path, sizeof(path))) {
            (void)snprintf(app->notice, sizeof(app->notice), "No se pudo preparar el nombre de descarga");
            return;
        }
        (void)snprintf(partial, sizeof(partial), "%s.part", path);
        if (background_job_start(VP_JOB_DOWNLOAD_ROM, item, path, partial))
            (void)snprintf(app->notice, sizeof(app->notice), "Descargando %.110s en segundo plano", item->title);
        else
            (void)snprintf(app->notice, sizeof(app->notice), "Hay una operacion en curso; espera a que termine");
        return;
    }
    (void)snprintf(app->notice, sizeof(app->notice), "Este elemento no tiene un archivo ejecutable");
}

static const char *item_state_label(const VpItem *item) {
    if (item->source == VP_SOURCE_ROMM) {
        if (item->multi_file) return "Varios archivos";
        return item->installed ? "Descargado" : "Sin descargar";
    }
    return item->installed ? "Instalado" : "Remoto";
}

static void draw_shell(vita2d_pgf *font, VpApp *app) {
    unsigned int background = theme_color(app->theme.background, RGBA(17, 20, 28, 255));
    unsigned int surface = theme_color(app->theme.surface, RGBA(32, 41, 56, 255));
    unsigned int accent = theme_color(app->theme.accent, RGBA(91, 192, 190, 255));
    unsigned int text = theme_color(app->theme.text, RGBA(238, 242, 246, 255));
    unsigned int muted = theme_color(app->theme.muted, RGBA(156, 169, 184, 255));
    size_t group_count = 0;
    size_t visible_count = 0;
    size_t cover_count = 0;
    const VpItem *selected = NULL;
    const VpPlatformGroup *selected_group = NULL;
    vita2d_texture *background_texture = NULL;
    size_t index;

    if (app->active_screen == VP_SCREEN_LIBRARY) {
        refresh_catalog_view_cache(app);
        if (app->library_level == VP_LEVEL_PLATFORMS) {
            group_count = platform_group_count;
            if (group_count > 0 && app->selected_platform >= group_count)
                app->selected_platform = group_count - 1;
            if (group_count > 0) selected_group = &platform_groups[app->selected_platform];
        } else {
            visible_count = visible_count_cache;
            if (visible_count > 0) selected = &app->catalog.items[app->selected_item];
        }
    }

    if (app->active_screen == VP_SCREEN_LIBRARY) {
        if (app->library_level == VP_LEVEL_PLATFORMS) {
            size_t start;
            start = (app->selected_platform / 8) * 8;
            for (index = start; index < group_count && index < start + 8; index++) {
                size_t item_index = platform_first_item(&app->catalog, platform_groups[index].platform);
                if (item_index < app->catalog.count && cover_count < VP_COVER_SLOTS)
                    cover_indexes[cover_count++] = item_index;
            }
            if (group_count > 0) {
                size_t item_index = platform_first_item(&app->catalog,
                                                         platform_groups[app->selected_platform].platform);
                if (item_index < app->catalog.count)
                    background_texture = cover_texture_for(app->catalog.items[item_index].id);
            }
        } else {
            size_t page_size = app->active_view == VP_VIEW_GRID ? 8 : (app->active_view == VP_VIEW_LIST ? 6 : 3);
            size_t position;
            size_t start;
            position = visible_position(visible_indexes, visible_count, app->selected_item);
            start = (position / page_size) * page_size;
            for (index = start; index < visible_count && index < start + page_size; index++)
                cover_indexes[cover_count++] = visible_indexes[index];
            if (selected != NULL)
                background_texture = cover_texture_for(selected->id);
        }
        covers_step(app, cover_indexes, cover_count);
        if (background_texture == NULL && selected != NULL)
            background_texture = cover_texture_for(selected->id);
        if (background_texture == NULL && selected_group != NULL) {
            size_t item_index = platform_first_item(&app->catalog, selected_group->platform);
            if (item_index < app->catalog.count)
                background_texture = cover_texture_for(app->catalog.items[item_index].id);
        }
    }

    vita2d_clear_screen();
    draw_background(background_texture, background);
    vita2d_pgf_draw_text(font, 28, 42, text, 1.0f, "VitaPlus");
    vita2d_pgf_draw_text(font, 28, 70, muted, 0.58f,
                         "Flechas navega | X abre/descarga/juega | Circulo vuelve | L/R secciones | Triangulo vista | Cuadrado favorito | Start salir");
    vita2d_pgf_draw_text(font, 28, 112, accent, 1.0f, vp_screen_name(app->active_screen));
    vita2d_pgf_draw_text(font, 734, 112, muted, 0.8f, vp_view_name(app->active_view));

    if (app->active_screen == VP_SCREEN_SETTINGS) {
        if (!settings_detail_active) {
            vita2d_pgf_draw_textf(font, 42, 185, settings_section == 0 ? accent : text, 0.82f,
                                  "%sRomM", settings_section == 0 ? "> " : "  ");
            vita2d_pgf_draw_textf(font, 42, 235, settings_section == 1 ? accent : text, 0.82f,
                                  "%sTemas", settings_section == 1 ? "> " : "  ");
            vita2d_pgf_draw_text(font, 42, 290, muted, 0.58f, "Arriba/Abajo selecciona | X abre");
            return;
        }
        if (settings_section == 1) {
            vita2d_pgf_draw_text(font, 42, 175, text, 0.82f, "Temas");
            vita2d_pgf_draw_textf(font, 42, 218, text, 0.68f, "Tema activo: %s", active_theme);
            vita2d_pgf_draw_text(font, 42, 270, muted, 0.58f,
                                 "Instala temas en ux0:data/vitaplus/themes/<tema>/theme.ini");
            vita2d_pgf_draw_text(font, 42, 298, muted, 0.58f, "Circulo vuelve a Ajustes");
            return;
        }
        {
            const char *url_prefix = settings_field == 0 ? "> " : "  ";
            const char *pair_prefix = settings_field == 1 ? "> " : "  ";
            vita2d_pgf_draw_text(font, 42, 175, text, 0.82f, "RomM");
            vita2d_pgf_draw_textf(font, 42, 218, settings_field == 0 ? accent : text, 0.68f,
                                  "%sURL: %s", url_prefix,
                                  romm_config.base_url[0] != '\0' ? romm_config.base_url : "(sin configurar)");
            vita2d_pgf_draw_textf(font, 42, 258, settings_field == 1 ? accent : text, 0.68f,
                                  "%sPair code: %s", pair_prefix,
                                  romm_pair_code[0] != '\0' ? romm_pair_code : "(ingresa el codigo de 8 caracteres)");
            vita2d_pgf_draw_text(font, 42, 314, muted, 0.58f,
                                 "Arriba/Abajo campo | X abre teclado | Triangulo canjea, guarda y sincroniza");
            vita2d_pgf_draw_text(font, 42, 342, muted, 0.58f,
                                 "Circulo vuelve a Ajustes; el codigo se canjea una vez y no se guarda");
            if (app->notice[0] != '\0') vita2d_pgf_draw_text(font, 42, 410, RGBA(245, 174, 79, 255), 0.62f, app->notice);
        }
        return;
    }

    if (app->active_screen == VP_SCREEN_STORES) {
        vita2d_pgf_draw_text(font, 28, 200, text, 0.9f, "Proximamente");
        vita2d_pgf_draw_text(font, 28, 240, muted, 0.66f, "Estamos preparando las tiendas para una proxima version.");
        return;
    }

    if (app->library_level == VP_LEVEL_PLATFORMS) {
        if (group_count == 0) {
            vita2d_pgf_draw_text(font, 28, 160, text, 0.85f,
                                 "Sin catalogo. Configura RomM o una tienda homebrew en ux0:data/vitaplus/config.ini");
            return;
        }
        draw_platforms(font, app, group_count, surface, text, accent, muted);
        vita2d_draw_rectangle(28, 425, 904, 105, surface);
        vita2d_pgf_draw_text(font, 48, 455, text, 0.88f, selected_group->display);
        vita2d_pgf_draw_textf(font, 48, 482, muted, 0.68f, "%u juegos en esta consola",
                              (unsigned)selected_group->count);
        vita2d_pgf_draw_text(font, 48, 510, muted, 0.62f,
                             "X abre la lista de ROMs | Flechas cambian de consola");
        if (app->notice[0] != '\0')
            vita2d_pgf_draw_text(font, 48, 532, RGBA(245, 174, 79, 255), 0.56f, app->notice);
        return;
    }

    if (visible_count == 0) {
        vita2d_pgf_draw_text(font, 28, 160, text, 0.85f, "Esta consola no tiene ROMs en el catalogo");
        return;
    }
    if (app->active_view == VP_VIEW_GRID) draw_rom_grid(font, app, visible_indexes, visible_count, surface, text, accent, muted);
    else if (app->active_view == VP_VIEW_CAROUSEL) draw_carousel(font, app, visible_indexes, visible_count, surface, text, accent);
    else draw_list(font, app, visible_indexes, visible_count, text, accent);

    vita2d_draw_rectangle(28, 425, 904, 105, surface);
    vita2d_pgf_draw_textf(font, 48, 455, text, 0.82f, "%.70s", selected->title);
    vita2d_pgf_draw_textf(font, 48, 482, muted, 0.62f, "%s | %s | %s", selected->platform_display,
                          vp_source_name(selected->source), item_state_label(selected));
    if (app->notice[0] != '\0')
        vita2d_pgf_draw_text(font, 48, 510, RGBA(245, 174, 79, 255), 0.58f, app->notice);
    else if (selected->description[0] != '\0')
        vita2d_pgf_draw_textf(font, 48, 510, muted, 0.56f, "%.120s", selected->description);
    else
        vita2d_pgf_draw_text(font, 48, 510, muted, 0.56f, "Sin descripcion");
}

int main(void) {
    SceCtrlData pad;
    unsigned int previous = 0;
    vita2d_pgf *font;
    vp_app_init(&app);
    vp_launch_config_init(&launch_config);
    if (setenv("OPENSSL_CONF", "app0:openssl.cnf", 1) != 0) return 1;
    (void)sceIoMkdir("ux0:data", 0777);
    (void)sceIoMkdir("ux0:data/vitaplus", 0777);
    (void)sceIoMkdir("ux0:data/vitaplus/cache", 0777);
    (void)sceIoMkdir("ux0:data/vitaplus/cache/covers", 0777);
    (void)sceIoMkdir("ux0:data/vitaplus/downloads", 0777);
    load_providers(&app);
    refresh_downloaded_items(&app);
    vita2d_init();
    font = vita2d_load_default_pgf();
    (void)sceSysmoduleLoadModule(SCE_SYSMODULE_IME);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    if (sync_romm_catalog(&app))
        (void)snprintf(app.notice, sizeof(app.notice), "Sincronizando biblioteca en segundo plano");
    for (;;) {
        unsigned int pressed;
        if (settings_ime_active) {
            (void)vita2d_common_dialog_update();
            settings_finish_ime(&app);
            if (settings_ime_active) {
                vita2d_swap_buffers();
                continue;
            }
        }
        background_job_poll(&app);
        sceCtrlPeekBufferPositive(0, &pad, 1);
        pressed = pad.buttons & ~previous;
        previous = pad.buttons;
        if (pressed & SCE_CTRL_START) break;
        if (app.active_screen == VP_SCREEN_SETTINGS) {
            handle_settings_input(&app, pressed);
            if (!settings_ime_active && !settings_detail_active && (pressed & SCE_CTRL_LTRIGGER))
                vp_app_cycle_screen(&app, -1);
            if (!settings_ime_active && !settings_detail_active && (pressed & SCE_CTRL_RTRIGGER))
                vp_app_cycle_screen(&app, 1);
        } else {
            if (pressed & SCE_CTRL_LTRIGGER) vp_app_cycle_screen(&app, -1);
            if (pressed & SCE_CTRL_RTRIGGER) vp_app_cycle_screen(&app, 1);
            if (app.active_screen == VP_SCREEN_LIBRARY) {
                int columns = app.library_level == VP_LEVEL_PLATFORMS
                                  ? 4
                                  : (app.active_view == VP_VIEW_GRID ? 4 : (app.active_view == VP_VIEW_LIST ? 1 : 3));
                if (pressed & SCE_CTRL_UP) vp_app_move(&app, 0, -1, columns);
                if (pressed & SCE_CTRL_DOWN) vp_app_move(&app, 0, 1, columns);
                if (pressed & SCE_CTRL_LEFT) vp_app_move(&app, -1, 0, columns);
                if (pressed & SCE_CTRL_RIGHT) vp_app_move(&app, 1, 0, columns);
                if (pressed & SCE_CTRL_TRIANGLE) vp_app_cycle_view(&app);
                if ((pressed & SCE_CTRL_SQUARE) && app.library_level == VP_LEVEL_ROMS &&
                    app.selected_item < app.catalog.count)
                    vp_item_toggle_favorite(&app.catalog.items[app.selected_item]);
                if (pressed & SCE_CTRL_CIRCLE) vp_app_back(&app);
                if (pressed & SCE_CTRL_CROSS) {
                    if (app.library_level == VP_LEVEL_PLATFORMS) vp_app_open(&app);
                    else activate_selected(&app);
                }
            }
        }
        vita2d_start_drawing();
        draw_shell(font, &app);
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }
    vita2d_free_pgf(font);
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}
