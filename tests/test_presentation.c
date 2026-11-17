#include "ui_presentation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    VpUiAppearance appearance = {180, 16, 130};
    const char *aliases[][2] = {
        {"SNES", "snes"}, {"megadrive", "genesis"}, {"mega-drive", "genesis"},
        {"playstation", "psx"}, {"PS Vita", "vita"}, {"gbc", "gameboy"},
        {"neogeo", "arcade"}, {"turbografx-16", "pce"}, {"sms", "mastersystem"}
    };
    size_t i;
    const char *requested[][2] = {
        {"arcade", "platform_arcade"}, {"dreamcast", "platform_dreamcast"}, {"dc", "platform_dreamcast"},
        {"gba", "platform_gba"}, {"3ds", "platform_n3ds"}, {"n3ds", "platform_n3ds"},
        {"nintendo-3ds", "platform_n3ds"}, {"nds", "platform_nds"}, {"ds", "platform_nds"},
        {"gamecube", "platform_gamecube"}, {"ngc", "platform_gamecube"}, {"nintendo-gamecube", "platform_gamecube"},
        {"ps1", "platform_psx"}, {"psx", "platform_psx"}, {"ps2", "platform_ps2"},
        {"playstation2", "platform_ps2"}, {"playstation-2", "platform_ps2"}, {"psp", "platform_psp"},
        {"genesis", "platform_genesis"}, {"snes", "platform_snes"}, {"wii", "platform_wii"},
        {"nintendo-wii", "platform_wii"}, {"Dreamcast", "platform_dreamcast"}
    };
    for (i = 0; i < sizeof(requested) / sizeof(requested[0]); ++i)
        assert(strcmp(vp_ui_platform(requested[i][0])->asset, requested[i][1]) == 0);
    for (i = 0; i < sizeof(aliases) / sizeof(aliases[0]); ++i)
        assert(strcmp(vp_ui_platform(aliases[i][0])->asset, vp_ui_platform(aliases[i][1])->asset) == 0);
    assert(strcmp(vp_ui_platform("unknown")->asset, "platform_generic") == 0);
    assert(strcmp(vp_ui_platform(NULL)->asset, "platform_generic") == 0);
    for (i = 0; i < 8; ++i) {
        VpUiRect r = vp_ui_tile(i);
        assert(r.x >= 24 && r.y >= 140 && r.x + r.w <= 936 && r.y + r.h <= 414);
    }
    assert(vp_ui_ease(-1) == 0 && vp_ui_ease(1) == 1 && vp_ui_ease(2) == 1);
    assert(vp_ui_ease(.5f) > .5f);
    vp_ui_appearance_load(&appearance, "id=legacy\nbackground=#101624\n");
    assert(appearance.motion_ms == 180 && appearance.radius == 16 && appearance.wallpaper_alpha == 130);
    vp_ui_appearance_load(&appearance, "ui_motion_ms=0\nui_radius=12\nui_wallpaper_alpha=85\n");
    assert(appearance.motion_ms == 0 && appearance.radius == 12 && appearance.wallpaper_alpha == 85);
    vp_ui_appearance_load(&appearance, "ui_motion_ms=999999999999\nui_radius=1\nui_wallpaper_alpha=-1\n");
    assert(appearance.motion_ms == 0 && appearance.radius == 12 && appearance.wallpaper_alpha == 85);
    puts("presentation tests passed");
    return 0;
}
