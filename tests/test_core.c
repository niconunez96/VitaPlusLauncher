#include "vitaplus.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int calls;
    VpTransferAction last_action;
    char url[VP_URL];
    char bearer[VP_TEXT];
    char destination[VP_PATH];
} TransferLog;

typedef struct {
    int calls;
    char url[VP_URL];
    char urls[4][VP_URL];
    char bearer[VP_TEXT];
    const char *payload;
    const char *pages[4];
    size_t page_count;
    int fail_at_call;
} FetchLog;

typedef struct {
    int calls;
    char url[VP_URL];
    char code[VP_TEXT];
    const char *response;
    bool succeed;
} PairLog;

static bool fake_pair(const char *url, const char *code,
                      char *response, size_t response_capacity, void *context) {
    PairLog *log = context;
    log->calls++;
    (void)snprintf(log->url, sizeof(log->url), "%s", url);
    (void)snprintf(log->code, sizeof(log->code), "%s", code);
    if (!log->succeed) return false;
    (void)snprintf(response, response_capacity, "%s", log->response);
    return true;
}

static bool fake_fetch(const char *url, const char *bearer_token,
                       char *response, size_t response_capacity, void *context) {
    FetchLog *log = context;
    const char *payload = log->payload != NULL ? log->payload :
        "{\"total\":1,\"items\":[{\"id\":99,\"name\":\"F-Zero\",\"platform_slug\":\"snes\","
        "\"url_cover\":\"/assets/fzero.png\",\"download_url\":\"/api/file/fzero.sfc\"}]}";
    if ((size_t)log->calls < log->page_count) payload = log->pages[log->calls];
    if (log->fail_at_call > 0 && log->calls == log->fail_at_call) {
        log->calls++;
        return false;
    }
    log->calls++;
    (void)snprintf(log->url, sizeof(log->url), "%s", url);
    if (log->calls <= (int)(sizeof(log->urls) / sizeof(log->urls[0])))
        (void)snprintf(log->urls[log->calls - 1], sizeof(log->urls[0]), "%s", url);
    (void)snprintf(log->bearer, sizeof(log->bearer), "%s", bearer_token);
    (void)snprintf(response, response_capacity, "%s", payload);
    return true;
}

static bool fake_transfer(const char *url, const char *bearer_token, const char *destination,
                          VpTransferAction action, void *context) {
    TransferLog *log = context;
    log->calls++;
    log->last_action = action;
    (void)snprintf(log->url, sizeof(log->url), "%s", url);
    (void)snprintf(log->bearer, sizeof(log->bearer), "%s", bearer_token != NULL ? bearer_token : "");
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
    assert(app.active_screen == VP_SCREEN_LIBRARY);
    assert(strcmp(vp_screen_name(VP_SCREEN_LIBRARY), "Biblioteca") == 0);
    assert(strcmp(vp_screen_name(VP_SCREEN_STORES), "Tiendas") == 0);
    assert(strcmp(vp_screen_name(VP_SCREEN_SETTINGS), "Ajustes") == 0);
    vp_app_cycle_screen(&app, -1);
    assert(app.active_screen == VP_SCREEN_SETTINGS);
    vp_app_cycle_screen(&app, 1);
    assert(app.active_screen == VP_SCREEN_LIBRARY);
    vp_app_cycle_screen(&app, 1);
    assert(app.active_screen == VP_SCREEN_STORES);
    vp_app_cycle_screen(&app, 1);
    assert(app.active_screen == VP_SCREEN_SETTINGS);
    vp_app_cycle_view(&app);
    assert(app.active_view == VP_VIEW_LIST);
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
    vp_app_select(&app, 1);
    assert(app.selected_item == 1);
    vp_app_select(&app, 1);
    assert(app.selected_item == 0);
    assert(strcmp(vp_screen_name(VP_SCREEN_STORES), "Tiendas") == 0);
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

static void test_local_library(void) {
    VpCatalog catalog;
    const char *manifest = "pcse00120|Persona 4 Golden|PS Vita|PCSE00120\n"
                           "snes-smw|Super Mario World|SNES|ux0:data/roms/snes/smw.sfc\n";
    vp_catalog_init(&catalog);
    assert(vp_local_import_manifest(&catalog, manifest));
    assert(catalog.count == 2);
    assert(vp_catalog_find(&catalog, "local:pcse00120") != NULL);
    assert(vp_catalog_find(&catalog, "local:snes-smw")->installed);
}

static void test_romm_pair_code_exchange(void) {
    PairLog pair_log;
    VpRommPairBackend backend;
    VpRommConfig config;

    memset(&pair_log, 0, sizeof(pair_log));
    memset(&config, 0, sizeof(config));
    (void)snprintf(config.base_url, sizeof(config.base_url), "https://romm.local/");
    (void)snprintf(config.token, sizeof(config.token), "rmm_existing_token");
    pair_log.succeed = true;
    pair_log.response = "{\"id\":4,\"raw_token\":\"rmm_paired_token\"}";
    backend.execute = fake_pair;
    backend.context = &pair_log;

    assert(vp_romm_exchange_pair_code(&config, "ABCD-1234", backend));
    assert(pair_log.calls == 1);
    assert(strcmp(pair_log.url, "https://romm.local/api/client-tokens/exchange") == 0);
    assert(strcmp(pair_log.code, "ABCD-1234") == 0);
    assert(strcmp(config.token, "rmm_paired_token") == 0);

    pair_log.succeed = false;
    assert(!vp_romm_exchange_pair_code(&config, "WXYZ-9999", backend));
    assert(strcmp(config.token, "rmm_paired_token") == 0);
    assert(!vp_romm_exchange_pair_code(&config, "invalid code!", backend));
    assert(pair_log.calls == 2);
}

static void test_romm_sync(void) {
    VpCatalog catalog;
    FetchLog fetch_log;
    TransferLog transfer_log;
    VpFetchBackend fetch_backend;
    VpTransferBackend transfer_backend;
    VpRommConfig config;
    VpItem *item;

    memset(&fetch_log, 0, sizeof(fetch_log));
    memset(&transfer_log, 0, sizeof(transfer_log));
    memset(&config, 0, sizeof(config));
    (void)snprintf(config.base_url, sizeof(config.base_url), "http://romm.local/");
    (void)snprintf(config.token, sizeof(config.token), "rmm_test_token");
    fetch_backend.execute = fake_fetch;
    fetch_backend.context = &fetch_log;
    transfer_backend.execute = fake_transfer;
    transfer_backend.context = &transfer_log;
    vp_catalog_init(&catalog);
    assert(vp_romm_sync(&catalog, &config, fetch_backend));
    assert(fetch_log.calls == 1);
    assert(strcmp(fetch_log.url, "http://romm.local/api/roms?limit=1&offset=0&with_char_index=false&with_filter_values=false&with_rom_id_index=false") == 0);
    assert(strcmp(fetch_log.bearer, "rmm_test_token") == 0);
    item = vp_catalog_find(&catalog, "romm:99");
    assert(item != NULL);
    assert(strcmp(item->cover_url, "http://romm.local/assets/fzero.png") == 0);
    assert(vp_cache_cover(item, &config, "ux0:data/vitaplus/cache/fzero.png", transfer_backend));
    assert(transfer_log.calls == 1);
    assert(strcmp(transfer_log.url, "http://romm.local/assets/fzero.png") == 0);
    assert(strcmp(transfer_log.bearer, "rmm_test_token") == 0);
    assert(vp_queue_download(item, &config, "ux0:data/vitaplus/downloads/fzero.sfc", transfer_backend));
    assert(transfer_log.calls == 2);
    assert(strcmp(transfer_log.url, "http://romm.local/api/file/fzero.sfc") == 0);
    assert(strcmp(transfer_log.bearer, "rmm_test_token") == 0);
    (void)snprintf(item->cover_url, sizeof(item->cover_url), "https://cdn.example/fzero.png");
    assert(vp_cache_cover(item, &config, "ux0:data/vitaplus/cache/fzero.png", transfer_backend));
    assert(strcmp(transfer_log.bearer, "") == 0);
    (void)snprintf(item->download_url, sizeof(item->download_url), "https://cdn.example/fzero.sfc");
    assert(vp_queue_download(item, &config, "ux0:data/vitaplus/downloads/fzero.sfc", transfer_backend));
    assert(strcmp(transfer_log.bearer, "") == 0);
}

static void test_romm_pagination(void) {
    VpCatalog catalog;
    FetchLog fetch_log;
    VpFetchBackend backend;
    VpRommConfig config;

    memset(&fetch_log, 0, sizeof(fetch_log));
    memset(&config, 0, sizeof(config));
    fetch_log.page_count = 2;
    fetch_log.pages[0] = "{\"total\":2,\"items\":[{\"id\":1,\"name\":\"First\"}]}";
    fetch_log.pages[1] = "{\"total\":2,\"items\":[{\"id\":2,\"name\":\"Second\"}]}";
    (void)snprintf(config.base_url, sizeof(config.base_url), "https://romm.local");
    (void)snprintf(config.token, sizeof(config.token), "rmm_test_token");
    backend.execute = fake_fetch;
    backend.context = &fetch_log;
    vp_catalog_init(&catalog);

    assert(vp_romm_sync(&catalog, &config, backend));
    assert(catalog.count == 2);
    assert(fetch_log.calls == 2);
    assert(strcmp(fetch_log.urls[0], "https://romm.local/api/roms?limit=1&offset=0&with_char_index=false&with_filter_values=false&with_rom_id_index=false") == 0);
    assert(strcmp(fetch_log.urls[1], "https://romm.local/api/roms?limit=1&offset=1&with_char_index=false&with_filter_values=false&with_rom_id_index=false") == 0);
    assert(strcmp(fetch_log.bearer, "rmm_test_token") == 0);
    assert(vp_catalog_find(&catalog, "romm:1") != NULL);
    assert(vp_catalog_find(&catalog, "romm:2") != NULL);
}

static void test_romm_sync_fails_after_partial_page(void) {
    VpCatalog catalog;
    FetchLog fetch_log;
    VpFetchBackend backend;
    VpRommConfig config;

    memset(&fetch_log, 0, sizeof(fetch_log));
    memset(&config, 0, sizeof(config));
    fetch_log.fail_at_call = 1;
    fetch_log.page_count = 1;
    fetch_log.pages[0] = "{\"total\":2,\"items\":[{\"id\":1,\"name\":\"First\"}]}";
    (void)snprintf(config.base_url, sizeof(config.base_url), "https://romm.local");
    (void)snprintf(config.token, sizeof(config.token), "rmm_test_token");
    backend.execute = fake_fetch;
    backend.context = &fetch_log;
    vp_catalog_init(&catalog);

    assert(!vp_romm_sync(&catalog, &config, backend));
    assert(catalog.count == 1);
    assert(fetch_log.calls == 2);
}

static void test_homebrew_store_sync(void) {
    VpCatalog catalog;
    FetchLog fetch_log;
    VpFetchBackend fetch_backend;
    VpHomebrewConfig config;
    VpItem *item;

    memset(&fetch_log, 0, sizeof(fetch_log));
    memset(&config, 0, sizeof(config));
    fetch_log.payload = "{\"apps\":[{\"id\":\"vitaftp\",\"name\":\"Vita FTP\","
                        "\"description\":\"FTP server\",\"url\":\"https://example.org/vitaftp.vpk\"}]}";
    (void)snprintf(config.catalog_url, sizeof(config.catalog_url),
                   "https://catalog.example.org/vita.json");
    fetch_backend.execute = fake_fetch;
    fetch_backend.context = &fetch_log;
    vp_catalog_init(&catalog);
    assert(vp_homebrew_sync(&catalog, &config, fetch_backend));
    assert(fetch_log.calls == 1);
    assert(fetch_log.bearer[0] == '\0');
    item = vp_catalog_find(&catalog, "homebrew:vitaftp");
    assert(item != NULL);
    assert(item->kind == VP_KIND_APP);
    assert(strcmp(item->description, "FTP server") == 0);
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
    assert(vp_queue_download(rom, NULL, "ux0:data/vitaplus/downloads/snes/game.sfc", backend));
    assert(log.calls == 1 && log.last_action == VP_TRANSFER_DOWNLOAD);
    assert(log.bearer[0] == '\0');
    assert(vp_install_homebrew(homebrew, "ux0:data/vitaplus/downloads/VitaPlus.vpk", backend));
    assert(log.calls == 2 && log.last_action == VP_TRANSFER_INSTALL_VPK);
    assert(!vp_install_homebrew(rom, "ux0:data/vitaplus/x", backend));
}

int main(void) {
    VpItem sample = make_item("local:sample", "Sample");
    assert(strcmp(sample.title, "Sample") == 0);
    test_shell_catalog();
    test_theme();
    test_local_library();
    test_romm_pair_code_exchange();
    test_romm_sync();
    test_romm_pagination();
    test_romm_sync_fails_after_partial_page();
    test_homebrew_store_sync();
    test_providers_and_transfers();
    puts("vitaplus core tests: PASS");
    return 0;
}
