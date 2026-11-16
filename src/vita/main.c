#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include "vitaplus.h"

#define RGBA(r, g, b, a) ((unsigned int)((r) | ((g) << 8) | ((b) << 16) | ((a) << 24)))

static void draw_shell(vita2d_pgf *font, const VpApp *app) {
    const VpItem *selected = NULL;
    int index;
    vita2d_pgf_draw_text(font, 28, 42, RGBA(238, 242, 246, 255), 1.0f, "VitaPlus");
    vita2d_pgf_draw_text(font, 28, 70, RGBA(156, 169, 184, 255), 0.72f,
                         "L/R secciones  |  Arriba/Abajo selecciona  |  Triangulo vista  |  Start salir");
    vita2d_pgf_draw_text(font, 28, 112, RGBA(91, 192, 190, 255), 1.0f,
                         vp_screen_name(app->active_screen));
    vita2d_pgf_draw_text(font, 734, 112, RGBA(156, 169, 184, 255), 0.8f,
                         vp_view_name(app->active_view));

    if (app->catalog.count == 0) {
        vita2d_pgf_draw_text(font, 28, 160, RGBA(238, 242, 246, 255), 0.85f,
                             "No hay elementos locales aun.");
        return;
    }
    selected = &app->catalog.items[app->selected_item];
    for (index = 0; index < (int)app->catalog.count && index < 6; index++) {
        const VpItem *item = &app->catalog.items[index];
        int y = 150 + index * 34;
        unsigned int color = ((size_t)index == app->selected_item)
            ? RGBA(91, 192, 190, 255) : RGBA(238, 242, 246, 255);
        vita2d_pgf_draw_textf(font, 42, y, color, 0.82f, "%c %s [%s]",
                              ((size_t)index == app->selected_item) ? '>' : ' ',
                              item->title, item->platform);
    }
    vita2d_draw_rectangle(28, 370, 904, 126, RGBA(32, 41, 56, 255));
    vita2d_pgf_draw_text(font, 48, 408, RGBA(238, 242, 246, 255), 1.0f, selected->title);
    vita2d_pgf_draw_textf(font, 48, 438, RGBA(156, 169, 184, 255), 0.75f,
                          "%s · %s · %s", selected->platform,
                          vp_source_name(selected->source), selected->installed ? "Instalado" : "Remoto");
    vita2d_pgf_draw_text(font, 48, 470, RGBA(238, 242, 246, 255), 0.68f,
                         selected->description[0] ? selected->description : "Sin descripcion");
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
