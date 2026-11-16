#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include <stdio.h>
#include <string.h>

#include "vita_backend.h"
#include "vitaplus.h"

#define RGBA(r, g, b, a) ((unsigned int)((r) | ((g) << 8) | ((b) << 16) | ((a) << 24)))
#define VP_CONFIG_PATH "ux0:data/vitaplus/config.ini"

static VpRommConfig romm_config;
static VpHomebrewConfig homebrew_config;
static VpApp app;
static VpCatalog romm_catalog;
static char active_theme[VP_TEXT] = "default";
static int settings_field;
static size_t settings_character;
static bool settings_editing;

static const char settings_characters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/._-?&=@";

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
    VpFetchBackend fetch_backend = {vp_vita_fetch, NULL};
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
    }
    fclose(file);
    if (strcmp(active_theme, "default") != 0) {
        char path[VP_PATH];
        (void)snprintf(path, sizeof(path), "ux0:data/vitaplus/themes/%s/theme.ini", active_theme);
        load_theme_file(app, path);
    }
    if (romm_config.base_url[0] != '\0' && romm_config.token[0] != '\0')
        (void)vp_romm_sync(&app->catalog, &romm_config, fetch_backend);
    if (homebrew_config.catalog_url[0] != '\0')
        (void)vp_homebrew_sync(&app->catalog, &homebrew_config, fetch_backend);
}

static bool save_romm_config(void) {
    FILE *file;
    (void)sceIoMkdir("ux0:data", 0777);
    (void)sceIoMkdir("ux0:data/vitaplus", 0777);
    file = fopen(VP_CONFIG_PATH, "w");
    if (file == NULL) return false;
    (void)fprintf(file, "romm_url=%s\nromm_token=%s\nhomebrew_catalog=%s\nactive_theme=%s\n",
                  romm_config.base_url, romm_config.token, homebrew_config.catalog_url, active_theme);
    return fclose(file) == 0;
}

static bool sync_romm_catalog(VpApp *app) {
    VpFetchBackend fetch_backend = {vp_vita_fetch, NULL};
    size_t index = 0;
    if (app == NULL || romm_config.base_url[0] == '\0' || romm_config.token[0] == '\0') return false;
    vp_catalog_init(&romm_catalog);
    if (!vp_romm_sync(&romm_catalog, &romm_config, fetch_backend)) return false;
    while (index < app->catalog.count) {
        if (app->catalog.items[index].source == VP_SOURCE_ROMM) {
            app->catalog.items[index] = app->catalog.items[app->catalog.count - 1];
            app->catalog.count--;
        } else {
            index++;
        }
    }
    for (index = 0; index < romm_catalog.count; index++) {
        if (vp_catalog_add(&app->catalog, &romm_catalog.items[index]) == NULL) return false;
    }
    if (app->selected_item >= app->catalog.count) app->selected_item = 0;
    return true;
}

static void settings_append_selected_character(void) {
    char *value = settings_field == 0 ? romm_config.base_url : romm_config.token;
    size_t capacity = settings_field == 0 ? sizeof(romm_config.base_url) : sizeof(romm_config.token);
    size_t length = strlen(value);
    if (length + 1 < capacity) {
        value[length] = settings_characters[settings_character];
        value[length + 1] = '\0';
    }
}

static void settings_backspace(void) {
    char *value = settings_field == 0 ? romm_config.base_url : romm_config.token;
    size_t length = strlen(value);
    if (length > 0) value[length - 1] = '\0';
}

static void handle_settings_input(VpApp *app, unsigned int pressed) {
    if (!settings_editing) {
        if (pressed & SCE_CTRL_UP) settings_field = 0;
        if (pressed & SCE_CTRL_DOWN) settings_field = 1;
        if (pressed & SCE_CTRL_CROSS) settings_editing = true;
        return;
    }
    if (pressed & SCE_CTRL_LEFT) {
        settings_character = (settings_character + strlen(settings_characters) - 1) % strlen(settings_characters);
    }
    if (pressed & SCE_CTRL_RIGHT) settings_character = (settings_character + 1) % strlen(settings_characters);
    if (pressed & SCE_CTRL_CROSS) settings_append_selected_character();
    if (pressed & SCE_CTRL_SQUARE) settings_backspace();
    if (pressed & SCE_CTRL_CIRCLE) settings_editing = false;
    if (pressed & SCE_CTRL_TRIANGLE) {
        settings_editing = false;
        if (!save_romm_config()) {
            (void)snprintf(app->notice, sizeof(app->notice), "No se pudo guardar la configuracion");
        } else if (sync_romm_catalog(app)) {
            (void)snprintf(app->notice, sizeof(app->notice), "RomM sincronizado: %u juegos", (unsigned)app->catalog.count);
        } else {
            (void)snprintf(app->notice, sizeof(app->notice), "Guardado; no se pudo sincronizar RomM");
        }
    }
}

static void download_selected(VpApp *app) {
    VpItem *item;
    VpTransferBackend transfer_backend = {vp_vita_transfer, NULL};
    char destination[VP_TEXT];
    size_t index;
    if (app->catalog.count == 0) return;
    item = &app->catalog.items[app->selected_item];
    for (index = 0; item->id[index] != '\0' && index < sizeof(destination) - 1; index++) {
        char value = item->id[index];
        destination[index] = ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                              (value >= '0' && value <= '9') || value == '-' || value == '_') ? value : '_';
    }
    destination[index] = '\0';
    if (item->source == VP_SOURCE_HOMEBREW) {
        char path[VP_PATH];
        if (!app->installation_armed) {
            app->installation_armed = true;
            (void)snprintf(app->notice, sizeof(app->notice), "Presiona X otra vez para instalar %.150s", item->title);
            return;
        }
        (void)snprintf(path, sizeof(path), "ux0:data/vitaplus/downloads/%s.vpk", destination);
        if (vp_install_homebrew(item, path, transfer_backend))
            (void)snprintf(app->notice, sizeof(app->notice), "Instalacion finalizada; revisa LiveArea");
        else
            (void)snprintf(app->notice, sizeof(app->notice), "Falló la instalacion; revisa la red y el archivo");
        app->installation_armed = false;
    } else if (item->source == VP_SOURCE_ROMM) {
        char path[VP_PATH];
        (void)snprintf(path, sizeof(path), "ux0:data/vitaplus/downloads/%s.rom", destination);
        if (vp_queue_download(item, &romm_config, path, transfer_backend))
            (void)snprintf(app->notice, sizeof(app->notice), "Descarga finalizada en %.160s", path);
        else
            (void)snprintf(app->notice, sizeof(app->notice), "Falló la descarga; revisa la red y el archivo");
    }
}

static void draw_list(vita2d_pgf *font, const VpApp *app, unsigned int text, unsigned int accent) {
    int index;
    for (index = 0; index < (int)app->catalog.count && index < 6; index++) {
        const VpItem *item = &app->catalog.items[index];
        int y = 150 + index * 34;
        unsigned int color = ((size_t)index == app->selected_item) ? accent : text;
        vita2d_pgf_draw_textf(font, 42, y, color, 0.82f, "%c %s%s [%s]",
                              ((size_t)index == app->selected_item) ? '>' : ' ',
                              item->favorite ? "* " : "", item->title, item->platform);
    }
}

static void draw_grid(vita2d_pgf *font, const VpApp *app, unsigned int surface, unsigned int text, unsigned int accent) {
    int index;
    for (index = 0; index < (int)app->catalog.count && index < 6; index++) {
        const VpItem *item = &app->catalog.items[index];
        int column = index % 3;
        int row = index / 3;
        int x = 34 + column * 302;
        int y = 145 + row * 142;
        unsigned int border = ((size_t)index == app->selected_item) ? accent : surface;
        vita2d_draw_rectangle(x, y, 278, 120, border);
        vita2d_draw_rectangle(x + 3, y + 3, 272, 114, surface);
        vita2d_pgf_draw_text(font, x + 12, y + 38, text, 0.78f, item->title);
        vita2d_pgf_draw_textf(font, x + 12, y + 72, accent, 0.66f, "%s · %s", item->platform, vp_source_name(item->source));
    }
}

static void draw_carousel(vita2d_pgf *font, const VpApp *app, unsigned int surface, unsigned int text, unsigned int accent) {
    size_t count = app->catalog.count;
    size_t previous = (app->selected_item + count - 1) % count;
    size_t next = (app->selected_item + 1) % count;
    vita2d_draw_rectangle(45, 180, 200, 180, surface);
    vita2d_draw_rectangle(360, 135, 240, 270, accent);
    vita2d_draw_rectangle(715, 180, 200, 180, surface);
    vita2d_pgf_draw_text(font, 62, 280, text, 0.72f, app->catalog.items[previous].title);
    vita2d_pgf_draw_text(font, 382, 265, text, 0.95f, app->catalog.items[app->selected_item].title);
    vita2d_pgf_draw_textf(font, 382, 305, surface, 0.72f, "%s", app->catalog.items[app->selected_item].platform);
    vita2d_pgf_draw_text(font, 732, 280, text, 0.72f, app->catalog.items[next].title);
}

static void draw_shell(vita2d_pgf *font, const VpApp *app) {
    const VpItem *selected;
    unsigned int background = theme_color(app->theme.background, RGBA(17, 20, 28, 255));
    unsigned int surface = theme_color(app->theme.surface, RGBA(32, 41, 56, 255));
    unsigned int accent = theme_color(app->theme.accent, RGBA(91, 192, 190, 255));
    unsigned int text = theme_color(app->theme.text, RGBA(238, 242, 246, 255));
    unsigned int muted = theme_color(app->theme.muted, RGBA(156, 169, 184, 255));
    vita2d_clear_screen();
    vita2d_draw_rectangle(0, 0, 960, 544, background);
    vita2d_pgf_draw_text(font, 28, 42, text, 1.0f, "VitaPlus");
    vita2d_pgf_draw_text(font, 28, 70, muted, 0.62f,
                         "X descarga/instala | L/R secciones | Arriba/Abajo selecciona | Triangulo vista | Cuadrado favorito | Start salir");
    vita2d_pgf_draw_text(font, 28, 112, accent, 1.0f, vp_screen_name(app->active_screen));
    vita2d_pgf_draw_text(font, 734, 112, muted, 0.8f, vp_view_name(app->active_view));
    if (app->active_screen == VP_SCREEN_SETTINGS) {
        const char *url_prefix = settings_field == 0 ? "> " : "  ";
        const char *token_prefix = settings_field == 1 ? "> " : "  ";
        vita2d_pgf_draw_text(font, 42, 175, text, 0.82f, "RomM server");
        vita2d_pgf_draw_textf(font, 42, 218, settings_field == 0 ? accent : text, 0.68f,
                              "%sURL: %s", url_prefix,
                              romm_config.base_url[0] != '\0' ? romm_config.base_url : "(sin configurar)");
        vita2d_pgf_draw_textf(font, 42, 258, settings_field == 1 ? accent : text, 0.68f,
                              "%sToken: %s", token_prefix,
                              romm_config.token[0] != '\0' ? "********" : "(sin configurar)");
        if (settings_editing) {
            vita2d_pgf_draw_textf(font, 42, 314, accent, 0.72f, "Caracter: %c", settings_characters[settings_character]);
            vita2d_pgf_draw_text(font, 42, 348, muted, 0.58f,
                                 "Izq/Der caracter | X agrega | Cuadrado borra | Triangulo guarda y sincroniza | Circulo termina");
        } else {
            vita2d_pgf_draw_text(font, 42, 314, muted, 0.58f,
                                 "Arriba/Abajo campo | X editar | Usa token de solo lectura de RomM");
        }
        if (app->notice[0] != '\0') vita2d_pgf_draw_text(font, 42, 410, RGBA(245, 174, 79, 255), 0.62f, app->notice);
        return;
    }
    if (app->catalog.count == 0) {
        vita2d_pgf_draw_text(font, 28, 160, text, 0.85f,
                             "Sin catalogo. Configura RomM o una tienda homebrew en ux0:data/vitaplus/config.ini");
        return;
    }
    if (app->active_view == VP_VIEW_GRID) draw_grid(font, app, surface, text, accent);
    else if (app->active_view == VP_VIEW_CAROUSEL) draw_carousel(font, app, surface, text, accent);
    else draw_list(font, app, text, accent);
    selected = &app->catalog.items[app->selected_item];
    vita2d_draw_rectangle(28, 425, 904, 105, surface);
    vita2d_pgf_draw_text(font, 48, 455, text, 0.88f, selected->title);
    vita2d_pgf_draw_textf(font, 48, 482, muted, 0.68f, "%s · %s · %s", selected->platform,
                          vp_source_name(selected->source), selected->installed ? "Instalado" : "Remoto");
    if (app->notice[0] != '\0') vita2d_pgf_draw_text(font, 48, 512, RGBA(245, 174, 79, 255), 0.62f, app->notice);
}

int main(void) {
    SceCtrlData pad;
    unsigned int previous = 0;
    vita2d_pgf *font;
    const char *local_manifest =
        "pcse00120|Persona 4 Golden|PS Vita|PCSE00120\n"
        "snes-smw|Super Mario World|SNES|ux0:data/roms/snes/smw.sfc\n"
        "psp-patapon|Patapon|PSP|ux0:pspemu/ISO/Patapon.iso\n";
    vp_app_init(&app);
    (void)vp_local_import_manifest(&app.catalog, local_manifest);
    load_providers(&app);
    vita2d_init();
    font = vita2d_load_default_pgf();
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    for (;;) {
        unsigned int pressed;
        sceCtrlPeekBufferPositive(0, &pad, 1);
        pressed = pad.buttons & ~previous;
        previous = pad.buttons;
        if (pressed & SCE_CTRL_START) break;
        if (app.active_screen == VP_SCREEN_SETTINGS) {
            handle_settings_input(&app, pressed);
            if (!settings_editing && (pressed & SCE_CTRL_LTRIGGER)) vp_app_cycle_screen(&app, -1);
            if (!settings_editing && (pressed & SCE_CTRL_RTRIGGER)) vp_app_cycle_screen(&app, 1);
        } else {
            if (pressed & SCE_CTRL_LTRIGGER) vp_app_cycle_screen(&app, -1);
            if (pressed & SCE_CTRL_RTRIGGER) vp_app_cycle_screen(&app, 1);
            if (pressed & SCE_CTRL_UP) vp_app_select(&app, -1);
            if (pressed & SCE_CTRL_DOWN) vp_app_select(&app, 1);
            if (pressed & SCE_CTRL_TRIANGLE) vp_app_cycle_view(&app);
            if ((pressed & SCE_CTRL_SQUARE) && app.catalog.count > 0) vp_item_toggle_favorite(&app.catalog.items[app.selected_item]);
            if (pressed & SCE_CTRL_CROSS) download_selected(&app);
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
