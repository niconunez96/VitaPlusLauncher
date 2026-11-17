#ifndef VITAPLUS_H
#define VITAPLUS_H

#include <stdbool.h>
#include <stddef.h>

#define VP_MAX_ITEMS 256
#define VP_MAX_PLATFORMS 64
#define VP_TEXT 192
#define VP_URL 384
#define VP_PATH 256
#define VP_MAX_QUERY 80
/* A single RomM ROM object already weighs 17-30 KiB with metadata; leave headroom so a
 * metadata-heavy entry does not overflow the fixed core response buffer. */
#define VP_FETCH_RESPONSE_SIZE (256 * 1024)

typedef enum {
    VP_SOURCE_LOCAL,
    VP_SOURCE_ROMM,
    VP_SOURCE_HOMEBREW,
    VP_SOURCE_OFFICIAL_SHORTCUT
} VpSource;

typedef enum {
    VP_KIND_GAME,
    VP_KIND_ROM,
    VP_KIND_APP,
    VP_KIND_STORE
} VpKind;

typedef enum {
    VP_VIEW_GRID,
    VP_VIEW_LIST,
    VP_VIEW_CAROUSEL
} VpView;

typedef enum {
    VP_SCREEN_LIBRARY,
    VP_SCREEN_STORES,
    VP_SCREEN_SETTINGS
} VpScreen;

typedef enum {
    VP_LEVEL_PLATFORMS,
    VP_LEVEL_ROMS
} VpLibraryLevel;

typedef struct {
    char id[VP_TEXT];
    char title[VP_TEXT];
    char platform[VP_TEXT];
    char platform_display[VP_TEXT];
    char description[VP_TEXT];
    char launch_target[VP_PATH];
    char remote_id[32];
    char download_url[VP_URL];
    char cover_url[VP_URL];
    VpSource source;
    VpKind kind;
    bool installed;
    bool favorite;
    unsigned long last_played;
} VpItem;

typedef struct {
    VpItem items[VP_MAX_ITEMS];
    size_t count;
} VpCatalog;

typedef struct {
    char platform[VP_TEXT];
    char display[VP_TEXT];
    size_t count;
} VpPlatformGroup;

typedef struct {
    char id[VP_TEXT];
    char name[VP_TEXT];
    char background[16];
    char surface[16];
    char accent[16];
    char text[16];
    char muted[16];
    VpView view;
    int card_width;
    int card_height;
} VpTheme;

typedef struct {
    VpCatalog catalog;
    VpTheme theme;
    char query[VP_MAX_QUERY];
    VpView active_view;
    VpScreen active_screen;
    VpLibraryLevel library_level;
    char active_platform[VP_TEXT];
    size_t selected_platform;
    size_t selected_item;
    bool installation_armed;
    char notice[VP_TEXT];
} VpApp;

typedef bool (*VpFetchFn)(const char *url, const char *bearer_token,
                          char *response, size_t response_capacity, void *context);

typedef struct {
    VpFetchFn execute;
    void *context;
} VpFetchBackend;

typedef bool (*VpRommPairFn)(const char *url, const char *code,
                             char *response, size_t response_capacity, void *context);

typedef struct {
    VpRommPairFn execute;
    void *context;
} VpRommPairBackend;

typedef struct {
    char base_url[VP_URL];
    char token[VP_TEXT];
} VpRommConfig;

typedef struct {
    char catalog_url[VP_URL];
} VpHomebrewConfig;

typedef enum {
    VP_TRANSFER_DOWNLOAD,
    VP_TRANSFER_INSTALL_VPK
} VpTransferAction;

typedef bool (*VpTransferFn)(const char *url, const char *bearer_token, const char *destination,
                             VpTransferAction action, void *context);


typedef struct {
    VpTransferFn execute;
    void *context;
} VpTransferBackend;

void vp_app_init(VpApp *app);
void vp_app_cycle_screen(VpApp *app, int direction);
void vp_app_cycle_view(VpApp *app);
void vp_app_select(VpApp *app, int direction);
void vp_app_move(VpApp *app, int dx, int dy, int columns);
void vp_app_open(VpApp *app);
void vp_app_back(VpApp *app);
size_t vp_app_visible(VpApp *app, size_t *indexes, size_t capacity);
const char *vp_screen_name(VpScreen screen);
void vp_catalog_init(VpCatalog *catalog);
VpItem *vp_catalog_add(VpCatalog *catalog, const VpItem *item);
VpItem *vp_catalog_find(VpCatalog *catalog, const char *id);
size_t vp_catalog_search(const VpCatalog *catalog, const char *query,
                         size_t *indexes, size_t capacity);
size_t vp_catalog_recent(const VpCatalog *catalog, size_t *indexes, size_t capacity);
size_t vp_catalog_platforms(const VpCatalog *catalog, VpPlatformGroup *groups, size_t capacity);
size_t vp_catalog_platform_items(const VpCatalog *catalog, const char *platform,
                                 size_t *indexes, size_t capacity);
void vp_item_toggle_favorite(VpItem *item);
void vp_item_mark_played(VpItem *item, unsigned long timestamp);

bool vp_theme_load(VpTheme *theme, const char *ini_text);
const char *vp_source_name(VpSource source);
const char *vp_view_name(VpView view);

/* Percent-encodes bytes that RFC 3986 forbids in a URL (spaces, quotes, braces, controls,
 * non-ASCII). Existing %XX sequences are preserved so URLs are never double encoded.
 * Returns false when the escaped result does not fit in destination. */
bool vp_url_escape(const char *url, char *destination, size_t capacity);

bool vp_local_import_manifest(VpCatalog *catalog, const char *manifest);
bool vp_romm_import(VpCatalog *catalog, const char *json);
bool vp_romm_exchange_pair_code(VpRommConfig *config, const char *code,
                                VpRommPairBackend backend);
bool vp_romm_sync(VpCatalog *catalog, const VpRommConfig *config,
                  VpFetchBackend backend);
bool vp_cache_cover(const VpItem *item, const VpRommConfig *config, const char *destination,
                    VpTransferBackend backend);
bool vp_homebrew_import(VpCatalog *catalog, const char *json);
bool vp_homebrew_sync(VpCatalog *catalog, const VpHomebrewConfig *config,
                      VpFetchBackend backend);
bool vp_queue_download(const VpItem *item, const VpRommConfig *config, const char *destination,
                       VpTransferBackend backend);
bool vp_install_homebrew(const VpItem *item, const char *destination,
                         VpTransferBackend backend);

#endif
