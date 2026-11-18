#ifndef VP_UI_RENDERER_H
#define VP_UI_RENDERER_H
#include <vita2d.h>
#include "vitaplus.h"

/* Read-only presentation snapshot. All catalog actions remain in main/core. */
typedef struct {
    const VpApp *app;
    vita2d_pgf *font;
    const VpPlatformGroup *groups;
    size_t group_count;
    const size_t *visible;
    size_t visible_count;
    vita2d_texture *covers[8];
    vita2d_texture *background;
    const VpDownloadQueue *downloads;
    size_t download_selection;
    int settings_section, settings_field;
    bool settings_detail;
    const char *romm_url, *pair_code, *active_theme;
    VpRommConnectionState romm_state;
    bool busy;
    unsigned int notice_id;
} VpUiFrame;

void vp_ui_render(const VpUiFrame *frame);
void vp_ui_load_theme(const char *ini);
void vp_ui_shutdown(void);
#endif
