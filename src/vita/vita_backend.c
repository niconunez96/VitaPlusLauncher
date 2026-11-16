#include "vita_backend.h"

#include <curl/curl.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/promoterutil.h>
#include <psp2/sysmodule.h>
#include <zip.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define VP_STAGE_DIR "ux0:data/vitaplus/staging/latest"

typedef struct {
    char *buffer;
    size_t capacity;
    size_t used;
} ResponseBuffer;

typedef struct {
    SceUID fd;
} DownloadFile;

static size_t response_write(char *data, size_t size, size_t count, void *context) {
    ResponseBuffer *buffer = context;
    size_t bytes = size * count;
    if (bytes == 0 || buffer->used + bytes + 1 > buffer->capacity) return 0;
    memcpy(buffer->buffer + buffer->used, data, bytes);
    buffer->used += bytes;
    buffer->buffer[buffer->used] = '\0';
    return bytes;
}

static size_t file_write(char *data, size_t size, size_t count, void *context) {
    DownloadFile *file = context;
    size_t bytes = size * count;
    size_t written = 0;
    while (written < bytes) {
        int result = sceIoWrite(file->fd, data + written, bytes - written);
        if (result <= 0) return written;
        written += (size_t)result;
    }
    return bytes;
}

static bool has_unsafe_path(const char *path) {
    const char *cursor;
    if (path == NULL || path[0] == '/' || strchr(path, '\\') != NULL) return true;
    for (cursor = path; *cursor != '\0'; cursor++) {
        if (cursor[0] == '.' && cursor[1] == '.' &&
            (cursor == path || cursor[-1] == '/') &&
            (cursor[2] == '\0' || cursor[2] == '/')) return true;
    }
    return false;
}

static bool make_parent_dirs(const char *path) {
    char buffer[512];
    char *cursor;
    if (strlen(path) >= sizeof(buffer)) return false;
    (void)snprintf(buffer, sizeof(buffer), "%s", path);
    for (cursor = buffer + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\0';
            (void)sceIoMkdir(buffer, 0777);
            *cursor = '/';
        }
    }
    return true;
}

static bool extract_vpk(const char *vpk_path, const char *destination) {
    int error = 0;
    zip_t *archive = zip_open(vpk_path, ZIP_RDONLY, &error);
    zip_int64_t index;
    if (archive == NULL) return false;
    for (index = 0; index < zip_get_num_entries(archive, 0); index++) {
        const char *entry = zip_get_name(archive, (zip_uint64_t)index, 0);
        char output[512];
        zip_file_t *input;
        SceUID output_fd;
        if (entry == NULL || has_unsafe_path(entry) ||
            snprintf(output, sizeof(output), "%s/%s", destination, entry) >= (int)sizeof(output)) {
            zip_close(archive);
            return false;
        }
        if (entry[strlen(entry) - 1] == '/') {
            (void)sceIoMkdir(output, 0777);
            continue;
        }
        if (!make_parent_dirs(output)) {
            zip_close(archive);
            return false;
        }
        input = zip_fopen_index(archive, (zip_uint64_t)index, 0);
        output_fd = sceIoOpen(output, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
        if (input == NULL || output_fd < 0) {
            if (input != NULL) zip_fclose(input);
            zip_close(archive);
            return false;
        }
        for (;;) {
            char block[8192];
            zip_int64_t read_count = zip_fread(input, block, sizeof(block));
            size_t offset = 0;
            if (read_count < 0) {
                zip_fclose(input);
                sceIoClose(output_fd);
                zip_close(archive);
                return false;
            }
            if (read_count == 0) break;
            while (offset < (size_t)read_count) {
                int write_count = sceIoWrite(output_fd, block + offset, (size_t)read_count - offset);
                if (write_count <= 0) {
                    zip_fclose(input);
                    sceIoClose(output_fd);
                    zip_close(archive);
                    return false;
                }
                offset += (size_t)write_count;
            }
        }
        zip_fclose(input);
        sceIoClose(output_fd);
    }
    zip_close(archive);
    return true;
}

static bool promote_vpk(const char *vpk_path) {
    int result;
    (void)sceIoMkdir("ux0:data/vitaplus", 0777);
    (void)sceIoMkdir("ux0:data/vitaplus/staging", 0777);
    (void)sceIoMkdir(VP_STAGE_DIR, 0777);
    if (!extract_vpk(vpk_path, VP_STAGE_DIR)) return false;
    result = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if (result < 0) return false;
    result = scePromoterUtilityInit();
    if (result == 0) {
        result = scePromoterUtilityPromotePkg(VP_STAGE_DIR, 1);
        (void)scePromoterUtilityExit();
    }
    (void)sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    return result == 0;
}

bool vp_vita_fetch(const char *url, const char *bearer_token,
                   char *response, size_t response_capacity, void *context) {
    CURL *curl;
    struct curl_slist *headers = NULL;
    ResponseBuffer buffer;
    char authorization[VP_TEXT + 32];
    CURLcode result;
    (void)context;
    if (url == NULL || response == NULL || response_capacity == 0) return false;
    response[0] = '\0';
    buffer.buffer = response;
    buffer.capacity = response_capacity;
    buffer.used = 0;
    curl = curl_easy_init();
    if (curl == NULL) return false;
    if (bearer_token != NULL && bearer_token[0] != '\0') {
        (void)snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s", bearer_token);
        headers = curl_slist_append(headers, authorization);
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    result = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && buffer.used > 0;
}

bool vp_vita_transfer(const char *url, const char *destination,
                      VpTransferAction action, void *context) {
    CURL *curl;
    DownloadFile file;
    CURLcode result;
    bool installed = true;
    (void)context;
    if (url == NULL || destination == NULL || !make_parent_dirs(destination)) return false;
    file.fd = sceIoOpen(destination, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (file.fd < 0) return false;
    curl = curl_easy_init();
    if (curl == NULL) {
        sceIoClose(file.fd);
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    sceIoClose(file.fd);
    if (result != CURLE_OK) return false;
    if (action == VP_TRANSFER_INSTALL_VPK) installed = promote_vpk(destination);
    return installed;
}
