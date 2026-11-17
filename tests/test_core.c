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
    assert(app.library_level == VP_LEVEL_PLATFORMS);
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
    /* At the platform level the vertical selector moves between console groups,
     * not between individual catalog items. */
    vp_app_select(&app, 1);
    assert(app.selected_platform == 1);
    vp_app_select(&app, 1);
    assert(app.selected_platform == 0);
    assert(strcmp(vp_screen_name(VP_SCREEN_STORES), "Tiendas") == 0);
}

static void test_platform_grouping(void) {
    VpCatalog catalog;
    VpPlatformGroup groups[8];
    size_t indexes[8];
    VpItem item;

    vp_catalog_init(&catalog);
    item = make_item("romm:1", "Zelda");
    (void)snprintf(item.platform, sizeof(item.platform), "nds");
    (void)snprintf(item.platform_display, sizeof(item.platform_display), "Nintendo DS");
    assert(vp_catalog_add(&catalog, &item) != NULL);
    item = make_item("romm:2", "Chrono Trigger");
    (void)snprintf(item.platform, sizeof(item.platform), "snes");
    (void)snprintf(item.platform_display, sizeof(item.platform_display), "Super Nintendo");
    assert(vp_catalog_add(&catalog, &item) != NULL);
    item = make_item("romm:3", "Patapon");
    (void)snprintf(item.platform, sizeof(item.platform), "psp");
    assert(vp_catalog_add(&catalog, &item) != NULL);
    item = make_item("romm:4", "Advance Wars");
    (void)snprintf(item.platform, sizeof(item.platform), "nds");
    assert(vp_catalog_add(&catalog, &item) != NULL);

    assert(vp_catalog_platforms(&catalog, groups, 8) == 3);
    /* Groups are sorted by display name: Nintendo DS, PSP, Super Nintendo. */
    assert(strcmp(groups[0].platform, "nds") == 0);
    assert(strcmp(groups[0].display, "Nintendo DS") == 0);
    assert(groups[0].count == 2);
    assert(strcmp(groups[1].platform, "psp") == 0);
    assert(groups[1].count == 1);
    assert(strcmp(groups[2].platform, "snes") == 0);
    assert(groups[2].count == 1);

    /* Items inside a console are sorted by title. */
    assert(vp_catalog_platform_items(&catalog, "nds", indexes, 8) == 2);
    assert(strcmp(catalog.items[indexes[0]].title, "Advance Wars") == 0);
    assert(strcmp(catalog.items[indexes[1]].title, "Zelda") == 0);
}

static void test_library_navigation(void) {
    VpApp app;
    VpItem item;
    size_t visible[8];
    size_t index;

    vp_app_init(&app);
    for (index = 0; index < 5; index++) {
        char title[16];
        (void)snprintf(title, sizeof(title), "Game%u", (unsigned)index);
        item = make_item("romm:nav", title);
        (void)snprintf(item.id, sizeof(item.id), "romm:nav%u", (unsigned)index);
        (void)snprintf(item.platform, sizeof(item.platform), "nds");
        assert(vp_catalog_add(&app.catalog, &item) != NULL);
    }
    item = make_item("romm:psp", "Patapon");
    (void)snprintf(item.platform, sizeof(item.platform), "psp");
    assert(vp_catalog_add(&app.catalog, &item) != NULL);

    assert(vp_app_visible(&app, visible, 8) == 0);
    assert(app.selected_platform == 0);
    vp_app_move(&app, 0, 1, 4);
    assert(app.selected_platform == 1);
    vp_app_move(&app, 0, -1, 4);
    assert(app.selected_platform == 0);
    vp_app_move(&app, -1, 0, 4);
    assert(app.selected_platform == 1);

    app.selected_platform = 0;
    vp_app_open(&app);
    assert(app.library_level == VP_LEVEL_ROMS);
    assert(strcmp(app.active_platform, "nds") == 0);
    assert(vp_app_visible(&app, visible, 8) == 5);
    assert(app.selected_item == visible[0]);

    vp_app_move(&app, 0, 1, 3);
    assert(app.selected_item == visible[3]);
    vp_app_move(&app, 0, -1, 3);
    assert(app.selected_item == visible[0]);
    vp_app_move(&app, 1, 0, 3);
    assert(app.selected_item == visible[1]);
    vp_app_move(&app, -1, 0, 3);
    assert(app.selected_item == visible[0]);
    vp_app_move(&app, -1, 0, 3);
    assert(app.selected_item == visible[4]);
    vp_app_move(&app, 1, 0, 3);
    assert(app.selected_item == visible[0]);

    /* A one-column list maps left/right onto vertical movement so every arrow works. */
    vp_app_move(&app, 1, 0, 1);
    assert(app.selected_item == visible[1]);

    vp_app_back(&app);
    assert(app.library_level == VP_LEVEL_PLATFORMS);
}

static void test_romm_metadata(void) {
    VpCatalog catalog;
    VpItem *item;
    const char *json = "{\"items\":[{\"id\":7,\"name\":\"Metroid\","
                       "\"platform_slug\":\"nds\",\"platform_display_name\":\"Nintendo DS\","
                       "\"summary\":\"A sci-fi adventure\","
                       "\"path_cover_small\":\"/assets/romm/resources/roms/7/7/cover/small.png\","
                       "\"path_cover_large\":\"/assets/romm/resources/roms/7/7/cover/big.png\","
                       "\"url_cover\":\"https://provider.example/cover.jpg\","
                       "\"download_url\":\"/api/roms/7/download\"}]}";

    vp_catalog_init(&catalog);
    assert(vp_romm_import(&catalog, json));
    item = vp_catalog_find(&catalog, "romm:7");
    assert(item != NULL);
    assert(strcmp(item->platform_display, "Nintendo DS") == 0);
    assert(strcmp(item->description, "A sci-fi adventure") == 0);
    assert(strcmp(item->cover_url, "/assets/romm/resources/roms/7/7/cover/small.png") == 0);
}

static void test_download_paths(void) {
    VpItem item;
    char extension[16];
    char path[VP_PATH];

    memset(&item, 0, sizeof(item));
    item.source = VP_SOURCE_ROMM;
    (void)snprintf(item.id, sizeof(item.id), "%s", "romm:42");
    (void)snprintf(item.fs_name, sizeof(item.fs_name), "%s", "Super Metroid (USA).sfc");

    assert(vp_item_extension(&item, extension, sizeof(extension)));
    assert(strcmp(extension, "sfc") == 0);
    assert(vp_item_download_path(&item, "ux0:data/vitaplus/downloads", path, sizeof(path)));
    assert(strcmp(path, "ux0:data/vitaplus/downloads/romm_42.sfc") == 0);
    assert(vp_item_download_path(&item, "ux0:data/vitaplus/downloads/", path, sizeof(path)));
    assert(strcmp(path, "ux0:data/vitaplus/downloads/romm_42.sfc") == 0);

    /* RomM does not always expose fs_name; the content URL still carries the extension. */
    item.fs_name[0] = '\0';
    (void)snprintf(item.download_url, sizeof(item.download_url), "%s",
                   "https://romm.local/api/roms/42/content/game.zip?token=1");
    assert(vp_item_extension(&item, extension, sizeof(extension)));
    assert(strcmp(extension, "zip") == 0);
    assert(vp_item_download_path(&item, "ux0:data/vitaplus/downloads", path, sizeof(path)));
    assert(strcmp(path, "ux0:data/vitaplus/downloads/romm_42.zip") == 0);

    (void)snprintf(item.download_url, sizeof(item.download_url), "%s",
                   "https://romm.local/api/roms/42/content/7");
    assert(vp_item_extension(&item, extension, sizeof(extension)));
    assert(strcmp(extension, "rom") == 0);

    assert(!vp_item_download_path(&item, "ux0:data/vitaplus/downloads", path, 8));
}

static void test_launch_profiles(void) {
    VpLaunchConfig config;
    const VpLaunchProfile *profile;
    char uri[VP_LAUNCH_URI];

    vp_launch_config_init(&config);
    profile = vp_launch_config_find(&config, "snes");
    assert(profile != NULL);
    assert(strcmp(profile->titleid, "RETROVITA") == 0);
    assert(strcmp(profile->core, "app0:/snes9x2005_libretro.self") == 0);
    assert(vp_build_launch_uri(profile, "ux0:data/vitaplus/downloads/romm_42.sfc",
                               uri, sizeof(uri)));
    assert(strcmp(uri, "psgm:play?titleid=RETROVITA&param=app0:/snes9x2005_libretro.self"
                       "&param2=ux0:data/vitaplus/downloads/romm_42.sfc") == 0);

    assert(vp_launch_config_set(&config, "nds", "DSVITA000:"));
    profile = vp_launch_config_find(&config, "nds");
    assert(profile != NULL);
    assert(strcmp(profile->titleid, "DSVITA000") == 0);
    assert(profile->core[0] == '\0');
    assert(vp_build_launch_uri(profile, "ux0:data/vitaplus/downloads/romm_7.nds",
                               uri, sizeof(uri)));
    assert(strcmp(uri, "psgm:play?titleid=DSVITA000&param=ux0:data/vitaplus/downloads/romm_7.nds") == 0);

    assert(vp_launch_config_set(&config, "PSP", "VSCU00001:"));
    assert(vp_launch_config_find(&config, "psp") != NULL);
    assert(vp_launch_config_find(&config, "unknown-platform") == NULL);
    assert(!vp_launch_config_set(&config, "snes", ""));
    assert(!vp_build_launch_uri(profile, "ux0:data/vitaplus/downloads/romm_7.nds", uri, 16));
}

static void test_romm_content_download(void) {
    VpCatalog catalog;
    FetchLog fetch_log;
    TransferLog transfer_log;
    VpFetchBackend fetch_backend;
    VpTransferBackend transfer_backend;
    VpRommConfig config;
    VpItem *item;
    char path[VP_PATH];

    memset(&fetch_log, 0, sizeof(fetch_log));
    memset(&transfer_log, 0, sizeof(transfer_log));
    memset(&config, 0, sizeof(config));
    fetch_log.payload = "{\"total\":1,\"items\":[{\"id\":42,\"name\":\"Super Metroid\","
                        "\"platform_slug\":\"snes\",\"fs_name\":\"Super Metroid (USA).sfc\"}]}";
    (void)snprintf(config.base_url, sizeof(config.base_url), "http://romm.local/");
    (void)snprintf(config.token, sizeof(config.token), "rmm_test_token");
    fetch_backend.execute = fake_fetch;
    fetch_backend.context = &fetch_log;
    transfer_backend.execute = fake_transfer;
    transfer_backend.context = &transfer_log;
    vp_catalog_init(&catalog);

    assert(vp_romm_sync(&catalog, &config, fetch_backend));
    item = vp_catalog_find(&catalog, "romm:42");
    assert(item != NULL);
    assert(strcmp(item->fs_name, "Super Metroid (USA).sfc") == 0);
    assert(strcmp(item->download_url,
                  "http://romm.local/api/roms/42/content/Super Metroid (USA).sfc") == 0);
    assert(vp_item_download_path(item, "ux0:data/vitaplus/downloads", path, sizeof(path)));
    assert(vp_queue_download(item, &config, path, transfer_backend));
    assert(strcmp(transfer_log.url,
                  "http://romm.local/api/roms/42/content/Super Metroid (USA).sfc") == 0);
    assert(strcmp(transfer_log.destination,
                  "ux0:data/vitaplus/downloads/romm_42.sfc") == 0);
    assert(strcmp(transfer_log.bearer, "rmm_test_token") == 0);
    assert(!item->multi_file);

    {
        VpCatalog nested_url;
        vp_catalog_init(&nested_url);
        assert(vp_romm_import(&nested_url,
                              "{\"id\":86,\"name\":\"Castlevania\","
                              "\"platform_slug\":\"gba\","
                              "\"fs_name\":\"Castlevania.zip\","
                              "\"launchbox_metadata\":{\"images\":[{\"url\":\""
                              "https://images.launchbox-app.com/cover.jpg\"}]}}"));
        item = vp_catalog_find(&nested_url, "romm:86");
        assert(item != NULL);
        assert(strcmp(item->download_url, "/api/roms/86/content/Castlevania.zip") == 0);
    }

    {
        VpCatalog multi;
        vp_catalog_init(&multi);
        assert(vp_romm_import(&multi,
                              "{\"id\":7,\"name\":\"Final Fantasy VII\","
                              "\"platform_slug\":\"psx\",\"fs_name\":\"Final Fantasy VII\","
                              "\"has_multiple_files\":true}"));
        item = vp_catalog_find(&multi, "romm:7");
        assert(item != NULL);
        assert(item->multi_file);
        assert(strcmp(item->download_url,
                      "/api/roms/7/content/Final Fantasy VII") == 0);
    }
}

static void test_url_escape(void) {
    char escaped[VP_URL * 3];

    /* RomM emits cover timestamps with a literal space, which libcurl rejects outright. */
    assert(vp_url_escape("http://romm.local/assets/1/cover/small.png?ts=2026-10-06 23:56:16",
                         escaped, sizeof(escaped)));
    assert(strcmp(escaped,
                  "http://romm.local/assets/1/cover/small.png?ts=2026-10-06%2023:56:16") == 0);
    /* Already-encoded URLs must not be double encoded. */
    assert(vp_url_escape("http://romm.local/a.png?ts=2026-10-06%2023:56:16",
                         escaped, sizeof(escaped)));
    assert(strcmp(escaped, "http://romm.local/a.png?ts=2026-10-06%2023:56:16") == 0);
    assert(vp_url_escape("http://romm.local/a b/{c}.png", escaped, sizeof(escaped)));
    assert(strcmp(escaped, "http://romm.local/a%20b/%7Bc%7D.png") == 0);
    assert(!vp_url_escape("http://romm.local/a b.png", escaped, 5));
    assert(escaped[0] == '\0');
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
    test_platform_grouping();
    test_library_navigation();
    test_romm_metadata();
    test_download_paths();
    test_launch_profiles();
    test_romm_content_download();
    test_url_escape();
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
