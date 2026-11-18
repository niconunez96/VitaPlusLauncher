/* Presentation-only data: never normalize catalog slugs or persisted records. */
#include "ui_presentation.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int same(const char *a, const char *b) {
    if (a == NULL) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return 0;
    }
    return *a == *b;
}

const VpUiPlatform *vp_ui_platform(const char *slug) {
    static const struct { const char *slug; VpUiPlatform style; } styles[] = {
        {"nes", {"platform_nes", 0xec8c82}}, {"famicom", {"platform_famicom", 0xe6aa79}},
        {"snes", {"platform_snes", 0xab9def}}, {"gb", {"platform_gameboy", 0xa8c98d}},
        {"gameboy", {"platform_gameboy", 0xa8c98d}}, {"gbc", {"platform_gameboy", 0xa8c98d}},
        {"gba", {"platform_gba", 0xb499ee}}, {"nds", {"platform_nds", 0x96c9e2}},
        {"ds", {"platform_nds", 0x96c9e2}},
        {"dreamcast", {"platform_dreamcast", 0xe6aa79}}, {"dc", {"platform_dreamcast", 0xe6aa79}},
        {"3ds", {"platform_n3ds", 0x96c9e2}}, {"n3ds", {"platform_n3ds", 0x96c9e2}},
        {"nintendo-3ds", {"platform_n3ds", 0x96c9e2}},
        {"gamecube", {"platform_gamecube", 0xb499ee}}, {"ngc", {"platform_gamecube", 0xb499ee}},
        {"nintendo-gamecube", {"platform_gamecube", 0xb499ee}},
        {"ps2", {"platform_ps2", 0x93b6e7}}, {"playstation2", {"platform_ps2", 0x93b6e7}},
        {"playstation-2", {"platform_ps2", 0x93b6e7}},
        {"wii", {"platform_wii", 0x96c9e2}}, {"nintendo-wii", {"platform_wii", 0x96c9e2}},
        {"genesis", {"platform_genesis", 0x849de8}}, {"mega-drive", {"platform_genesis", 0x849de8}},
        {"megadrive", {"platform_genesis", 0x849de8}},
        {"mastersystem", {"platform_mastersystem", 0xe78d95}}, {"sms", {"platform_mastersystem", 0xe78d95}},
        {"gamegear", {"platform_gamegear", 0x8cc7e2}}, {"psx", {"platform_psx", 0x93b6e7}},
        {"playstation", {"platform_psx", 0x93b6e7}}, {"ps1", {"platform_psx", 0x93b6e7}},
        {"psp", {"platform_psp", 0x9eaee8}},
        {"ps vita", {"platform_vita", 0x86daca}}, {"vita", {"platform_vita", 0x86daca}},
        {"psvita", {"platform_vita", 0x86daca}}, {"arcade", {"platform_arcade", 0xe7b883}},
        {"neogeo", {"platform_arcade", 0xe7b883}}, {"pce", {"platform_pce", 0xe9b499}},
        {"pc-engine", {"platform_pce", 0xe9b499}}, {"turbografx-16", {"platform_pce", 0xe9b499}},
        {"atari2600", {"platform_atari2600", 0xdea571}}, {"wonderswan", {"platform_wonderswan", 0x9dcbd8}},
        {"homebrew", {"platform_homebrew", 0x9cdab3}}
    };
    static const VpUiPlatform fallback = {"platform_generic", 0x96d8cb};
    size_t i;
    for (i = 0; i < sizeof(styles) / sizeof(styles[0]); ++i)
        if (same(slug, styles[i].slug)) return &styles[i].style;
    return &fallback;
}

VpUiRect vp_ui_tile(size_t slot) {
    VpUiRect r = { 28 + (int)(slot % 4) * 230, 146 + (int)((slot % 8) / 4) * 134, 214, 122 };
    return r;
}

int vp_ui_touch_tile_slot(int x, int y) {
    size_t slot;
    for (slot = 0; slot < 8; slot++) {
        VpUiRect r = vp_ui_tile(slot);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return (int)slot;
    }
    return -1;
}

int vp_ui_touch_list_slot(int x, int y) {
    int slot;
    if (x < 28 || x >= 932) return -1;
    for (slot = 0; slot < 6; slot++) {
        int row_y = 146 + slot * 43;
        if (y >= row_y && y < row_y + 39) return slot;
    }
    return -1;
}

int vp_ui_touch_dock_screen(int x, int y) {
    if (y < 480 || y >= 528 || x < 278 || x >= 746) return -1;
    return (x - 278) / 156;
}

bool vp_ui_touch_view_toggle(int x, int y) {
    return x >= 802 && x < 932 && y >= 76 && y < 112;
}

bool vp_ui_touch_back(int x, int y) {
    return x >= 685 && x < 932 && y >= 421 && y < 472;
}

float vp_ui_ease(float p) {
    if (p < 0) return 0;
    if (p > 1) return 1;
    p = 1 - p;
    return 1 - p * p * p;
}

void vp_ui_appearance_load(VpUiAppearance *appearance, const char *ini) {
    const char *line=ini;
    while (*line) {
        const char *end=strchr(line,'\n');
        const char *equals;
        char *tail;
        long value;
        int *target=NULL, min=0, max=0;
        size_t length=end?(size_t)(end-line):strlen(line);
        equals=memchr(line,'=',length);
        if (equals) {
            size_t key=(size_t)(equals-line);
            if (key==12 && strncmp(line,"ui_motion_ms",key)==0) { target=&appearance->motion_ms; max=600; }
            else if (key==9 && strncmp(line,"ui_radius",key)==0) { target=&appearance->radius; min=6; max=20; }
            else if (key==18 && strncmp(line,"ui_wallpaper_alpha",key)==0) { target=&appearance->wallpaper_alpha; max=255; }
            errno=0; value=strtol(equals+1,&tail,10);
            if (target && errno==0 && tail!=equals+1 && (*tail=='\0' || *tail=='\r' || *tail=='\n') && value>=min && value<=max)
                *target=(int)value;
        }
        if (!end) break;
        line=end+1;
    }
}
