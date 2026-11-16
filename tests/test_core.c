#include "vitaplus.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int calls;
    VpTransferAction last_action;
    char url[VP_URL];
    char destination[VP_PATH];
} TransferLog;

static bool fake_transfer(const char *url, const char *destination,
                          VpTransferAction action, void *context) {
    TransferLog *log = context;
    log->calls++;
    log->last_action = action;
    (void)snprintf(log->url, sizeof(log->url), "%s", url);
    (void)snprintf(log->destination, sizeof(log->destination), "%s", destination);
    return true;
}

static VpItem make_item(const char *id, const char *title) {
    VpItem item;
    memset(&item, 0, sizeof(item));
    (void)snprintf(item.id, sizeof(item.id), "%s", id);
    (void)snprintf(item.title, sizeof(item.title), "%s", title);
    (void)snprintf(item.platform, sizeof(item.platform), "SNES");
    item.source = VP_SOURCE_LOCAL;
    item.kind = VP_KIND_ROM;
    return item;
}

static void test_shell_catalog(void) {
    VpApp app;
    VpItem *item;
    size_t indexes[4];

    vp_app_init(&app);
    assert(app.active_view == VP_VIEW_GRID);
    assert(vp_catalog_add(&app.catalog, &(VpItem){0}) == NULL);

    item = vp_catalog_add(&app.catalog, &(VpItem){
        .id = "local:chrono", .title = "Chrono Trigger", .platform = "SNES",
        .source = VP_SOURCE_LOCAL, .kind = VP_KIND_ROM, .installed = true
    });
    assert(item != NULL);
    assert(vp_catalog_add(&app.catalog, &(VpItem){
        .id = "local:metroid", .title = "Metroid Fusion", .platform = "GBA",
        .source = VP_SOURCE_LOCAL, .kind = VP_KIND_ROM, .installed = true
    }) != NULL);
    assert(vp_catalog_search(&app.catalog, "chrono", indexes, 4) == 1);
    assert(indexes[0] == 0);
    vp_item_toggle_favorite(item);
    assert(item->favorite);
    vp_item_mark_played(item, 100);
    vp_item_mark_played(&app.catalog.items[1], 200);
    assert(vp_catalog_recent(&app.catalog, indexes, 4) == 2);
    assert(indexes[0] == 1);
}

static void test_theme(void) {
    VpTheme theme;
    const char *ini = "id=amber\nname=Amber Pulse\nbackground=#000000\nsurface=#111111\n"
                      "accent=#ffaa00\ntext=#ffffff\nmuted=#999999\nview=carousel\n"
                      "card_width=170\ncard_height=220\n";
    memset(&theme, 0, sizeof(theme));
    assert(vp_theme_load(&theme, ini));
    assert(strcmp(theme.id, "amber") == 0);
    assert(theme.view == VP_VIEW_CAROUSEL);
    assert(theme.card_width == 170);
    assert(!vp_theme_load(&theme, "id=x\nname=x\nview=broken\n"));
}

static void test_providers_and_transfers(void) {
    VpCatalog catalog;
    TransferLog log;
    VpTransferBackend backend;
    VpItem *rom;
    VpItem *homebrew;
    const char *romm = "{\"items\":[{\"id\":42,\"name\":\"Super Metroid\","
                       "\"platform_slug\":\"snes\",\"url_cover\":\"http://romm/cover.png\","
                       "\"download_url\":\"http://romm/game.sfc\"}]}";
    const char *store = "[{\"id\":\"vitaplus\",\"name\":\"VitaPlus\","
                        "\"description\":\"Custom shell\",\"url\":\"https://example.org/vitaplus.vpk\"}]";

    memset(&log, 0, sizeof(log));
    backend.execute = fake_transfer;
    backend.context = &log;
    vp_catalog_init(&catalog);
    assert(vp_romm_import(&catalog, romm));
    assert(vp_homebrew_import(&catalog, store));
    assert(catalog.count == 2);
    rom = vp_catalog_find(&catalog, "romm:42");
    homebrew = vp_catalog_find(&catalog, "homebrew:vitaplus");
    assert(rom != NULL && homebrew != NULL);
    assert(strcmp(rom->platform, "snes") == 0);
    assert(vp_queue_download(rom, "ux0:data/vitaplus/downloads/snes/game.sfc", backend));
    assert(log.calls == 1 && log.last_action == VP_TRANSFER_DOWNLOAD);
    assert(vp_install_homebrew(homebrew, "ux0:data/vitaplus/downloads/VitaPlus.vpk", backend));
    assert(log.calls == 2 && log.last_action == VP_TRANSFER_INSTALL_VPK);
    assert(!vp_install_homebrew(rom, "ux0:data/vitaplus/x", backend));
}

int main(void) {
    VpItem sample = make_item("local:sample", "Sample");
    assert(strcmp(sample.title, "Sample") == 0);
    test_shell_catalog();
    test_theme();
    test_providers_and_transfers();
    puts("vitaplus core tests: PASS");
    return 0;
}
