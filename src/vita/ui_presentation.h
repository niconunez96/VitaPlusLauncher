#ifndef VP_UI_PRESENTATION_H
#define VP_UI_PRESENTATION_H
#include <stddef.h>
#include <stdint.h>

typedef struct { int x, y, w, h; } VpUiRect;
typedef struct { const char *asset; uint32_t rgb; } VpUiPlatform;
typedef struct { int motion_ms, radius, wallpaper_alpha; } VpUiAppearance;
const VpUiPlatform *vp_ui_platform(const char *slug);
VpUiRect vp_ui_tile(size_t slot);
float vp_ui_ease(float progress);
void vp_ui_appearance_load(VpUiAppearance *appearance, const char *ini);
#endif
