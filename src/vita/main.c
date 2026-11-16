#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include <stdio.h>
#include <string.h>

#include "vita_backend.h"
#include "vitaplus.h"

#define RGBA(r, g, b, a) ((unsigned int)((r) | ((g) << 8) | ((b) << 16) | ((a) << 24)))
#define VP_CONFIG_PATH "ux0:data/vitaplus/config.ini"

static void trim(char *value) {
    size_t length;
    char *start = value;
    while (*start == ' ' || *start == '\t') start++;
    if (start != value) memmove(value, start, strlen(start) + 1);
    length = strlen(value);
    while (length > 0 && (value[length - 1] == '\r' || value[length - 1] == '\n' ||
                          value[length - 1] == ' ' || value[length - 1] == '\t')) {
        value[--length] = '\0';
    }
}

static void load_providers(VpApp *app) {
    FILE *file = fopen(VP_CONFIG_PATH, "r");
    VpRommConfig romm = {0};
    VpHomebrewConfig homebrew = {0};
    VpFetchBackend fetch_backend = {vp_vita_fetch, NULL};
    char line[512];
    if (file == NULL) return;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *equals = strchr(line, '=');
        if (equals == NULL || line[0] == '#') continue;
        *equals++ = '\0';
        trim(line);
        trim(equals);
        if (strcmp(line, "romm_url") == 0) (void)snprintf(romm.base_url, sizeof(romm.base_url), "%s", equals);
        else if (strcmp(line, "romm_token") == 0) (void)snprintf(romm.token, sizeof(romm.token), "%s", equals);
        else if (strcmp(line, "homebrew_catalog") == 0)
            (void)snprintf(homebrew.catalog_url, sizeof(homebrew.catalog_url), "%s", equals);
    }
    fclose(file);
    if (romm.base_url[0] != '\0' && romm.token[0] != '\0')
        (void)vp_romm_sync(&app->catalog, &romm, fetch_backend);
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
        (void)vp_install_homebrew(item, path, transfer_backend);
        app->installation_armed = false;
        (void)snprintf(app->notice, sizeof(app->notice), "Instalacion finalizada o fallida; revisa LiveArea");
    } else if (item->source == VP_SOURCE_ROMM) {
        char path[VP_PATH];
        (void)snprintf(path, sizeof(path), "ux0:data/vitaplus/downloads/%s.rom", destination);
        (void)vp_queue_download(item, path, transfer_backend);
        (void)snprintf(app->notice, sizeof(app->notice), "Descarga enviada a %s", path);
    }
}

static void draw_shell(vita2d_pgf *font, const VpApp *app) {
    const VpItem *selected = NULL;
    int index;
    vita2d_pgf_draw_text(font, 28, 42, RGBA(238, 242, 246, 255), 1.0f, "VitaPlus");
    vita2d_pgf_draw_text(font, 28, 70, RGBA(156, 169, 184, 255), 0.72f,
                         "X descarga/instala | L/R secciones | Arriba/Abajo selecciona | Triangulo vista | Cuadrado favorito | Start salir");
    vita2d_pgf_draw_text(font, 28, 112, RGBA(91, 192, 190, 255), 1.0f,
                         vp_screen_name(app->active_screen));
    vita2d_pgf_draw_text(font, 734, 112, RGBA(156, 169, 184, 255), 0.8f,
                         vp_view_name(app->active_view));

    if (app->catalog.count == 0) {
        vita2d_pgf_draw_text(font, 28, 160, RGBA(238, 242, 246, 255), 0.85f,
                             "Sin catalogo. Configura RomM o una tienda homebrew en ux0:data/vitaplus/config.ini");
        return;
    }
    selected = &app->catalog.items[app->selected_item];
    for (index = 0; index < (int)app->catalog.count && index < 6; index++) {
        const VpItem *item = &app->catalog.items[index];
        int y = 150 + index * 34;
        unsigned int color = ((size_t)index == app->selected_item)
            ? RGBA(91, 192, 190, 255) : RGBA(238, 242, 246, 255);
        vita2d_pgf_draw_textf(font, 42, y, color, 0.82f, "%c %s%s [%s]",
                              ((size_t)index == app->selected_item) ? '>' : ' ',
                              item->favorite ? "* " : "", item->title, item->platform);
    }
    vita2d_draw_rectangle(28, 370, 904, 126, RGBA(32, 41, 56, 255));
    vita2d_pgf_draw_text(font, 48, 408, RGBA(238, 242, 246, 255), 1.0f, selected->title);
    vita2d_pgf_draw_textf(font, 48, 438, RGBA(156, 169, 184, 255), 0.75f,
                          "%s · %s · %s", selected->platform,
                          vp_source_name(selected->source), selected->installed ? "Instalado" : "Remoto");
    vita2d_pgf_draw_text(font, 48, 470, RGBA(238, 242, 246, 255), 0.68f,
                         selected->description[0] ? selected->description : "Sin descripcion");
    if (app->notice[0] != '\0')
        vita2d_pgf_draw_text(font, 48, 516, RGBA(245, 174, 79, 255), 0.68f, app->notice);
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
        if ((pressed & SCE_CTRL_SQUARE) && app.catalog.count > 0)
            vp_item_toggle_favorite(&app.catalog.items[app.selected_item]);
        if (pressed & SCE_CTRL_CROSS) download_selected(&app);

        vita2d_start_drawing();
        vita2d_clear_screen();
        draw_shell(font, &app);
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    vita2d_free_pgf(font);
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}
