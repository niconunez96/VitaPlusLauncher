#ifndef VP_UI_TEXT_H
#define VP_UI_TEXT_H
#include <stdbool.h>
#include <vita2d.h>

typedef enum {
    VP_TYPE_BRAND, VP_TYPE_NAV, VP_TYPE_STATUS, VP_TYPE_PAGE_TITLE,
    VP_TYPE_PAGE_SUBTITLE, VP_TYPE_CONSOLE_TITLE, VP_TYPE_HERO_TITLE,
    VP_TYPE_LIST_TITLE, VP_TYPE_GRID_TITLE, VP_TYPE_DESCRIPTION,
    VP_TYPE_FOOTER, VP_TYPE_SETTINGS_ROW, VP_TYPE_COUNT
} VpTypeRole;
typedef struct { unsigned int size, weight; float leading; } VpTypeToken;
typedef struct { vita2d_font *weights[4]; } VpUiFonts;
const VpTypeToken *vp_ui_type(VpTypeRole role);
bool vp_ui_fonts_load(VpUiFonts *fonts);
void vp_ui_fonts_free(VpUiFonts *fonts);
int vp_ui_text_width(VpUiFonts *fonts, VpTypeRole role, bool selected, const char *text);
void vp_ui_text(VpUiFonts *fonts, VpTypeRole role, bool selected,
                int x, int y, int width, unsigned int color, const char *text);
void vp_ui_paragraph(VpUiFonts *fonts, VpTypeRole role, int x, int y,
                     int width, int lines, unsigned int color, const char *text);
#endif
