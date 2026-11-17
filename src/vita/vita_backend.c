#include "vita_backend.h"

#include <curl/curl.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/promoterutil.h>
#include <psp2/sysmodule.h>
#include <zip.h>

#include <ctype.h>
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
    bool truncated;
} ResponseBuffer;

typedef struct {
    SceUID fd;
} DownloadFile;

/* Transfers run on the single UI thread, so this avoids an oversized Vita stack frame. */
static char extract_block[8192];
static char last_error[VP_TEXT];

const char *vp_vita_last_error(void) {
    return last_error[0] != '\0' ? last_error : "sin detalle";
}

static void set_last_error(CURLcode result, long status, const char *response, bool truncated) {
    const char *message;
    if (truncated) {
        (void)snprintf(last_error, sizeof(last_error),
                       "Respuesta demasiado grande (%u KiB); revisa network.log",
                       (unsigned)(VP_FETCH_RESPONSE_SIZE / 1024));
    } else if (result != CURLE_OK) {
        (void)snprintf(last_error, sizeof(last_error), "Red: %s", curl_easy_strerror(result));
    } else if (status == 422 && response != NULL &&
               (message = strstr(response, "\"msg\":\"")) != NULL) {
        message += strlen("\"msg\":\"");
        (void)snprintf(last_error, sizeof(last_error), "HTTP 422: %.96s", message);
    } else if (status < 200 || status >= 300) {
        (void)snprintf(last_error, sizeof(last_error), "HTTP %ld", status);
    } else {
        last_error[0] = '\0';
    }
}

static void record_http_failure(const char *url, CURLcode result, long status, const char *response,
                                bool truncated) {
    FILE *log;
    if (result == CURLE_OK && status >= 200 && status < 300) return;
    (void)sceIoMkdir("ux0:data", 0777);
    (void)sceIoMkdir("ux0:data/vitaplus", 0777);
    log = fopen("ux0:data/vitaplus/network.log", "a");
    if (log == NULL) return;
    (void)fprintf(log, "url=%s\ncurl=%d (%s)\nhttp=%ld\ntruncated=%d\nbody=%.1024s\n---\n",
                  url != NULL ? url : "(null)", (int)result, curl_easy_strerror(result), status,
                  truncated ? 1 : 0, response != NULL ? response : "");
    fclose(log);
}

static bool valid_bearer_token(const char *token) {
    return token == NULL || (strchr(token, '\r') == NULL && strchr(token, '\n') == NULL);
}

static bool valid_pair_code(const char *code) {
    const char *cursor;
    if (code == NULL || code[0] == '\0') return false;
    for (cursor = code; *cursor != '\0'; cursor++) {
        if (!isalnum((unsigned char)*cursor) && *cursor != '-') return false;
    }
    return true;
}

static size_t response_write(char *data, size_t size, size_t count, void *context) {
    ResponseBuffer *buffer = context;
    size_t bytes = size * count;
    if (bytes == 0) return 0;
    if (buffer->used + bytes + 1 > buffer->capacity) {
        /* Returning a short count makes libcurl report CURLE_WRITE_ERROR, whose text talks
         * about writing to disk. Flag the real cause so the UI does not blame the network. */
        buffer->truncated = true;
        return 0;
    }
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
            zip_int64_t read_count = zip_fread(input, extract_block, sizeof(extract_block));
            size_t offset = 0;
            if (read_count < 0) {
                zip_fclose(input);
                sceIoClose(output_fd);
                zip_close(archive);
                return false;
            }
            if (read_count == 0) break;
            while (offset < (size_t)read_count) {
                int write_count = sceIoWrite(output_fd, extract_block + offset, (size_t)read_count - offset);
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
    long status = 0;
    (void)context;
    if (url == NULL || response == NULL || response_capacity == 0 || !valid_bearer_token(bearer_token)) return false;
    response[0] = '\0';
    buffer.buffer = response;
    buffer.capacity = response_capacity;
    buffer.used = 0;
    buffer.truncated = false;
    curl = curl_easy_init();
    if (curl == NULL) return false;
    if (bearer_token != NULL && bearer_token[0] != '\0') {
        (void)snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s", bearer_token);
        headers = curl_slist_append(headers, authorization);
        if (headers == NULL) {
            curl_easy_cleanup(curl);
            return false;
        }
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,
                     bearer_token != NULL && bearer_token[0] != '\0' ? 0L : 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    result = curl_easy_perform(curl);
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    set_last_error(result, status, response, buffer.truncated);
    record_http_failure(url, result, status, response, buffer.truncated);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && status >= 200 && status < 300 && buffer.used > 0;
}

bool vp_vita_exchange_pair_code(const char *url, const char *code,
                                char *response, size_t response_capacity, void *context) {
    CURL *curl;
    struct curl_slist *headers = NULL;
    ResponseBuffer buffer;
    char payload[VP_TEXT + 16];
    CURLcode result;
    long status = 0;
    (void)context;
    if (url == NULL || response == NULL || response_capacity == 0 || !valid_pair_code(code)) return false;
    if (snprintf(payload, sizeof(payload), "{\"code\":\"%s\"}", code) >= (int)sizeof(payload)) return false;
    response[0] = '\0';
    buffer.buffer = response;
    buffer.capacity = response_capacity;
    buffer.used = 0;
    buffer.truncated = false;
    curl = curl_easy_init();
    if (curl == NULL) return false;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (headers == NULL) {
        curl_easy_cleanup(curl);
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    result = curl_easy_perform(curl);
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && status >= 200 && status < 300 && buffer.used > 0;
}

bool vp_vita_transfer(const char *url, const char *bearer_token, const char *destination,
                      VpTransferAction action, void *context) {
    CURL *curl;
    struct curl_slist *headers = NULL;
    DownloadFile file;
    CURLcode result;
    long status = 0;
    bool installed = true;
    bool downloaded;
    char authorization[VP_TEXT + 32];
    (void)context;
    if (url == NULL || destination == NULL || !valid_bearer_token(bearer_token) ||
        !make_parent_dirs(destination)) return false;
    file.fd = sceIoOpen(destination, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (file.fd < 0) return false;
    curl = curl_easy_init();
    if (curl == NULL) {
        sceIoClose(file.fd);
        (void)sceIoRemove(destination);
        return false;
    }
    if (bearer_token != NULL && bearer_token[0] != '\0') {
        (void)snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s", bearer_token);
        headers = curl_slist_append(headers, authorization);
        if (headers == NULL) {
            curl_easy_cleanup(curl);
            sceIoClose(file.fd);
            (void)sceIoRemove(destination);
            return false;
        }
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,
                     bearer_token != NULL && bearer_token[0] != '\0' ? 0L : 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    result = curl_easy_perform(curl);
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    sceIoClose(file.fd);
    downloaded = result == CURLE_OK && status >= 200 && status < 300;
    if (!downloaded) {
        (void)sceIoRemove(destination);
        return false;
    }
    if (action == VP_TRANSFER_INSTALL_VPK) installed = promote_vpk(destination);
    return installed;
}
