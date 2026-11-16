#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include <stdio.h>
#include <string.h>

#include "vita_backend.h"
#include "vitaplus.h"

#define RGBA(r, g, b, a) ((unsigned int)((r) | ((g) << 8) | ((b) << 16) | ((a) << 24)))
#define VP_CONFIG_PATH "ux0:data/vitaplus/config.ini"

static VpRommConfig romm_config;

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
    VpHomebrewConfig homebrew = {0};
    VpFetchBackend fetch_backend = {vp_vita_fetch, NULL};
    char line[512];
    char theme_id[VP_TEXT] = "default";
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
            (void)snprintf(homebrew.catalog_url, sizeof(homebrew.catalog_url), "%s", equals);
        else if (strcmp(line, "active_theme") == 0)
            (void)snprintf(theme_id, sizeof(theme_id), "%s", equals);
    }
    fclose(file);
    if (strcmp(theme_id, "default") != 0) {
        char path[VP_PATH];
        (void)snprintf(path, sizeof(path), "ux0:data/vitaplus/themes/%s/theme.ini", theme_id);
        load_theme_file(app, path);
    }
    if (romm_config.base_url[0] != '\0' && romm_config.token[0] != '\0')
        (void)vp_romm_sync(&app->catalog, &romm_config, fetch_backend);
    if (homebrew.catalog_url[0] != '\0')
        (void)vp_homebrew_sync(&app->catalog, &homebrew, fetch_backend);
}

static void download_selected(VpApp *app) {
    VpItem *item;
    VpTransferBackend transfer_backend = {vp_vita_transfer, NULL};
    char destination[256];
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
            (void)snprintf(app->notice, sizeof(app->notice), "Presiona X otra vez para instalar %s", item->title);
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
            (void)snprintf(app->notice, sizeof(app->notice), "Descarga finalizada en %s", path);
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
    VpApp app;
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
        if (pressed & SCE_CTRL_LTRIGGER) vp_app_cycle_screen(&app, -1);
        if (pressed & SCE_CTRL_RTRIGGER) vp_app_cycle_screen(&app, 1);
        if (pressed & SCE_CTRL_UP) vp_app_select(&app, -1);
        if (pressed & SCE_CTRL_DOWN) vp_app_select(&app, 1);
        if (pressed & SCE_CTRL_TRIANGLE) vp_app_cycle_view(&app);
        if ((pressed & SCE_CTRL_SQUARE) && app.catalog.count > 0) vp_item_toggle_favorite(&app.catalog.items[app.selected_item]);
        if (pressed & SCE_CTRL_CROSS) download_selected(&app);
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
