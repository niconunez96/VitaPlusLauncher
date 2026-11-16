#include "vitaplus.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void vp_copy(char *destination, size_t capacity, const char *source) {
    if (capacity == 0) return;
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }
    (void)snprintf(destination, capacity, "%s", source);
}

static bool vp_contains_ci(const char *text, const char *needle) {
    size_t i;
    size_t needle_len;
    if (needle == NULL || needle[0] == '\0') return true;
    if (text == NULL) return false;
    needle_len = strlen(needle);
    for (i = 0; text[i] != '\0'; i++) {
        size_t j = 0;
        while (j < needle_len && text[i + j] != '\0' &&
               tolower((unsigned char)text[i + j]) == tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == needle_len) return true;
    }
    return false;
}

static const char *vp_skip_space(const char *value) {
    while (value != NULL && *value != '\0' && isspace((unsigned char)*value)) value++;
    return value;
}

static bool vp_json_value(const char *object, const char *object_end,
                          const char *key, char *destination, size_t capacity) {
    char pattern[VP_TEXT + 8];
    const char *match;
    const char *value;
    size_t used = 0;

    if (object == NULL || object_end == NULL || key == NULL || capacity == 0) return false;
    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    match = strstr(object, pattern);
    if (match == NULL || match >= object_end) return false;
    value = strchr(match + strlen(pattern), ':');
    if (value == NULL || value >= object_end) return false;
    value = vp_skip_space(value + 1);
    if (value == NULL || value >= object_end) return false;

    if (*value == '"') {
        value++;
        while (value < object_end && *value != '"' && used + 1 < capacity) {
            if (*value == '\\' && value + 1 < object_end) value++;
            destination[used++] = *value++;
        }
    } else {
        while (value < object_end && *value != ',' && *value != '}' &&
               !isspace((unsigned char)*value) && used + 1 < capacity) {
            destination[used++] = *value++;
        }
    }
    destination[used] = '\0';
    return used > 0;
}

static const char *vp_object_end(const char *begin) {
    int depth = 0;
    bool quoted = false;
    const char *cursor;
    for (cursor = begin; *cursor != '\0'; cursor++) {
        if (*cursor == '"' && (cursor == begin || cursor[-1] != '\\')) quoted = !quoted;
        if (quoted) continue;
        if (*cursor == '{') depth++;
        if (*cursor == '}') {
            depth--;
            if (depth == 0) return cursor + 1;
        }
    }
    return NULL;
}

static void vp_default_item(VpItem *item) {
    memset(item, 0, sizeof(*item));
    item->source = VP_SOURCE_LOCAL;
    item->kind = VP_KIND_GAME;
}

void vp_catalog_init(VpCatalog *catalog) {
    if (catalog != NULL) memset(catalog, 0, sizeof(*catalog));
}

VpItem *vp_catalog_add(VpCatalog *catalog, const VpItem *item) {
    VpItem *existing;
    if (catalog == NULL || item == NULL || item->id[0] == '\0') return NULL;
    existing = vp_catalog_find(catalog, item->id);
    if (existing != NULL) {
        *existing = *item;
        return existing;
    }
    if (catalog->count >= VP_MAX_ITEMS) return NULL;
    catalog->items[catalog->count] = *item;
    catalog->count++;
    return &catalog->items[catalog->count - 1];
}

VpItem *vp_catalog_find(VpCatalog *catalog, const char *id) {
    size_t index;
    if (catalog == NULL || id == NULL) return NULL;
    for (index = 0; index < catalog->count; index++) {
        if (strcmp(catalog->items[index].id, id) == 0) return &catalog->items[index];
    }
    return NULL;
}

size_t vp_catalog_search(const VpCatalog *catalog, const char *query,
                         size_t *indexes, size_t capacity) {
    size_t index;
    size_t found = 0;
    if (catalog == NULL || indexes == NULL) return 0;
    for (index = 0; index < catalog->count && found < capacity; index++) {
        const VpItem *item = &catalog->items[index];
        if (vp_contains_ci(item->title, query) || vp_contains_ci(item->platform, query)) {
            indexes[found++] = index;
        }
    }
    return found;
}

size_t vp_catalog_recent(const VpCatalog *catalog, size_t *indexes, size_t capacity) {
    size_t index;
    size_t found = 0;
    if (catalog == NULL || indexes == NULL) return 0;
    for (index = 0; index < catalog->count && found < capacity; index++) {
        size_t insertion = found;
        size_t cursor;
        if (catalog->items[index].last_played == 0) continue;
        for (cursor = 0; cursor < found; cursor++) {
            if (catalog->items[index].last_played > catalog->items[indexes[cursor]].last_played) {
                insertion = cursor;
                break;
            }
        }
        if (found < capacity) found++;
        for (cursor = found - 1; cursor > insertion; cursor--) indexes[cursor] = indexes[cursor - 1];
        indexes[insertion] = index;
    }
    return found;
}

void vp_item_toggle_favorite(VpItem *item) {
    if (item != NULL) item->favorite = !item->favorite;
}

void vp_item_mark_played(VpItem *item, unsigned long timestamp) {
    if (item != NULL) item->last_played = timestamp;
}

const char *vp_source_name(VpSource source) {
    switch (source) {
        case VP_SOURCE_LOCAL: return "local";
        case VP_SOURCE_ROMM: return "romm";
        case VP_SOURCE_HOMEBREW: return "homebrew";
        case VP_SOURCE_OFFICIAL_SHORTCUT: return "official-shortcut";
        default: return "unknown";
    }
}

const char *vp_view_name(VpView view) {
    switch (view) {
        case VP_VIEW_GRID: return "grid";
        case VP_VIEW_LIST: return "list";
        case VP_VIEW_CAROUSEL: return "carousel";
        default: return "grid";
    }
}

void vp_app_init(VpApp *app) {
    if (app == NULL) return;
    memset(app, 0, sizeof(*app));
    vp_catalog_init(&app->catalog);
    vp_copy(app->theme.id, sizeof(app->theme.id), "default");
    vp_copy(app->theme.name, sizeof(app->theme.name), "VitaPlus Default");
    vp_copy(app->theme.background, sizeof(app->theme.background), "#11141c");
    vp_copy(app->theme.surface, sizeof(app->theme.surface), "#202938");
    vp_copy(app->theme.accent, sizeof(app->theme.accent), "#5bc0be");
    vp_copy(app->theme.text, sizeof(app->theme.text), "#eef2f6");
    vp_copy(app->theme.muted, sizeof(app->theme.muted), "#9ca9b8");
    app->theme.view = VP_VIEW_GRID;
    app->theme.card_width = 154;
    app->theme.card_height = 216;
    app->active_view = VP_VIEW_GRID;
    app->active_screen = VP_SCREEN_HOME;
    app->selected_item = 0;
}

const char *vp_screen_name(VpScreen screen) {
    switch (screen) {
        case VP_SCREEN_HOME: return "Inicio";
        case VP_SCREEN_LIBRARY: return "Biblioteca";
        case VP_SCREEN_STORES: return "Tiendas";
        case VP_SCREEN_THEMES: return "Temas";
        case VP_SCREEN_SETTINGS: return "Ajustes";
        default: return "Inicio";
    }
}

void vp_app_cycle_screen(VpApp *app, int direction) {
    int value;
    if (app == NULL || direction == 0) return;
    value = (int)app->active_screen + (direction > 0 ? 1 : -1);
    if (value < (int)VP_SCREEN_HOME) value = (int)VP_SCREEN_SETTINGS;
    if (value > (int)VP_SCREEN_SETTINGS) value = (int)VP_SCREEN_HOME;
    app->active_screen = (VpScreen)value;
}

void vp_app_cycle_view(VpApp *app) {
    if (app == NULL) return;
    app->active_view = (VpView)(((int)app->active_view + 1) % 3);
}

void vp_app_select(VpApp *app, int direction) {
    size_t count;
    if (app == NULL || direction == 0) return;
    count = app->catalog.count;
    if (count == 0) {
        app->selected_item = 0;
        return;
    }
    if (direction > 0) app->selected_item = (app->selected_item + 1) % count;
    else app->selected_item = (app->selected_item + count - 1) % count;
}

static bool vp_parse_view(const char *value, VpView *view) {
    if (strcmp(value, "grid") == 0) *view = VP_VIEW_GRID;
    else if (strcmp(value, "list") == 0) *view = VP_VIEW_LIST;
    else if (strcmp(value, "carousel") == 0) *view = VP_VIEW_CAROUSEL;
    else return false;
    return true;
}

bool vp_theme_load(VpTheme *theme, const char *ini_text) {
    char buffer[2048];
    char *line;
    char *cursor;
    if (theme == NULL || ini_text == NULL || strlen(ini_text) >= sizeof(buffer)) return false;
    vp_copy(buffer, sizeof(buffer), ini_text);
    cursor = buffer;
    while (cursor != NULL && *cursor != '\0') {
        char *equals;
        char *next = strchr(cursor, '\n');
        if (next != NULL) *next++ = '\0';
        line = cursor;
        cursor = next;
        if (*line == '#' || *line == '\0') continue;
        equals = strchr(line, '=');
        if (equals == NULL) continue;
        *equals++ = '\0';
        if (strcmp(line, "id") == 0) vp_copy(theme->id, sizeof(theme->id), equals);
        else if (strcmp(line, "name") == 0) vp_copy(theme->name, sizeof(theme->name), equals);
        else if (strcmp(line, "background") == 0) vp_copy(theme->background, sizeof(theme->background), equals);
        else if (strcmp(line, "surface") == 0) vp_copy(theme->surface, sizeof(theme->surface), equals);
        else if (strcmp(line, "accent") == 0) vp_copy(theme->accent, sizeof(theme->accent), equals);
        else if (strcmp(line, "text") == 0) vp_copy(theme->text, sizeof(theme->text), equals);
        else if (strcmp(line, "muted") == 0) vp_copy(theme->muted, sizeof(theme->muted), equals);
        else if (strcmp(line, "view") == 0 && !vp_parse_view(equals, &theme->view)) return false;
        else if (strcmp(line, "card_width") == 0) theme->card_width = atoi(equals);
        else if (strcmp(line, "card_height") == 0) theme->card_height = atoi(equals);
    }
    return theme->id[0] != '\0' && theme->name[0] != '\0' &&
           theme->card_width > 0 && theme->card_height > 0;
}

static bool vp_import_items(VpCatalog *catalog, const char *json, VpSource source,
                            VpKind kind, const char *default_platform) {
    const char *cursor = json;
    size_t imported = 0;
    if (catalog == NULL || json == NULL) return false;
    while ((cursor = strchr(cursor, '{')) != NULL) {
        const char *end = vp_object_end(cursor);
        VpItem item;
        char remote_id[VP_TEXT - 24];
        if (end == NULL) return false;
        vp_default_item(&item);
        item.source = source;
        item.kind = kind;
        if (!vp_json_value(cursor, end, "id", remote_id, sizeof(remote_id))) {
            cursor = end;
            continue;
        }
        (void)snprintf(item.id, sizeof(item.id), "%s:%s", vp_source_name(source), remote_id);
        if (!vp_json_value(cursor, end, "name", item.title, sizeof(item.title))) {
            (void)vp_json_value(cursor, end, "title", item.title, sizeof(item.title));
        }
        if (item.title[0] == '\0') {
            cursor = end;
            continue;
        }
        if (!vp_json_value(cursor, end, "platform", item.platform, sizeof(item.platform))) {
            if (!vp_json_value(cursor, end, "platform_slug", item.platform, sizeof(item.platform))) {
                vp_copy(item.platform, sizeof(item.platform), default_platform);
            }
        }
        (void)vp_json_value(cursor, end, "description", item.description, sizeof(item.description));
        if (!vp_json_value(cursor, end, "download_url", item.download_url, sizeof(item.download_url))) {
            (void)vp_json_value(cursor, end, "url", item.download_url, sizeof(item.download_url));
        }
        if (!vp_json_value(cursor, end, "cover_url", item.cover_url, sizeof(item.cover_url))) {
            (void)vp_json_value(cursor, end, "url_cover", item.cover_url, sizeof(item.cover_url));
        }
        if (vp_catalog_add(catalog, &item) != NULL) imported++;
        cursor = end;
    }
    return imported > 0;
}

bool vp_local_import_manifest(VpCatalog *catalog, const char *manifest) {
    char buffer[4096];
    char *cursor;
    size_t imported = 0;
    if (catalog == NULL || manifest == NULL || strlen(manifest) >= sizeof(buffer)) return false;
    vp_copy(buffer, sizeof(buffer), manifest);
    cursor = buffer;
    while (cursor != NULL && *cursor != '\0') {
        char *next = strchr(cursor, '\n');
        char *id;
        char *title;
        char *platform;
        char *target;
        VpItem item;
        if (next != NULL) *next++ = '\0';
        if (*cursor == '\0' || *cursor == '#') {
            cursor = next;
            continue;
        }
        id = cursor;
        title = strchr(id, '|');
        if (title == NULL) {
            cursor = next;
            continue;
        }
        *title++ = '\0';
        platform = strchr(title, '|');
        if (platform == NULL) {
            cursor = next;
            continue;
        }
        *platform++ = '\0';
        target = strchr(platform, '|');
        if (target == NULL) {
            cursor = next;
            continue;
        }
        *target++ = '\0';
        vp_default_item(&item);
        (void)snprintf(item.id, sizeof(item.id), "local:%.180s", id);
        vp_copy(item.title, sizeof(item.title), title);
        vp_copy(item.platform, sizeof(item.platform), platform);
        vp_copy(item.launch_target, sizeof(item.launch_target), target);
        item.installed = true;
        item.source = VP_SOURCE_LOCAL;
        item.kind = VP_KIND_GAME;
        if (item.title[0] != '\0' && vp_catalog_add(catalog, &item) != NULL) imported++;
        cursor = next;
    }
    return imported > 0;
}

bool vp_romm_import(VpCatalog *catalog, const char *json) {
    return vp_import_items(catalog, json, VP_SOURCE_ROMM, VP_KIND_ROM, "romm");
}

bool vp_homebrew_import(VpCatalog *catalog, const char *json) {
    return vp_import_items(catalog, json, VP_SOURCE_HOMEBREW, VP_KIND_APP, "PS Vita");
}

bool vp_queue_download(const VpItem *item, const char *destination,
                       VpTransferBackend backend) {
    if (item == NULL || destination == NULL || item->download_url[0] == '\0' ||
        backend.execute == NULL) return false;
    return backend.execute(item->download_url, destination, VP_TRANSFER_DOWNLOAD, backend.context);
}

bool vp_install_homebrew(const VpItem *item, const char *destination,
                         VpTransferBackend backend) {
    if (item == NULL || item->source != VP_SOURCE_HOMEBREW || destination == NULL ||
        item->download_url[0] == '\0' || backend.execute == NULL) return false;
    return backend.execute(item->download_url, destination, VP_TRANSFER_INSTALL_VPK, backend.context);
}
