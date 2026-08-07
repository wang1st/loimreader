#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <spawn.h>
#include <sys/wait.h>
#elif !defined(_WIN32)
#include <spawn.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "loim/document.h"
#include "loim/i18n.h"
#include "loim/image_probe.h"
#include "loim/import_queue.h"
#include "loim/layout.h"
#include "loim/machine_code.h"
#include "loim/pdf.h"
#include "loim/presentation.h"
#include "loim/seam.h"
#include "loim/status.h"
#include "loim/texture_plan.h"

#include "http_client.h"

#ifndef LOIM_APP_VERSION
#define LOIM_APP_VERSION "dev"
#endif

#define LOIM_LAYOUT_WIDTH 1000U
#define LOIM_MAX_IMAGE_PIXELS 100000000ULL
#define LOIM_MAX_AUTO_HINTS 4096U
#define LOIM_TOOLBAR_HEIGHT 58.0F
#define LOIM_STATUSBAR_HEIGHT 22.0F
#define LOIM_DIVIDER_WIDTH 6.0F
#define LOIM_SPLIT_HANDLE_HEIGHT 18.0F
#define LOIM_TEXT_CACHE_CAPACITY 96U

typedef struct app_texture_tile {
    SDL_Texture *texture;
    uint32_t source_y_px;
    uint32_t height_px;
} app_texture_tile;

typedef struct app_image {
    char *path;
    app_texture_tile *tiles;
    size_t tile_count;
    uint32_t width;
    uint32_t height;
} app_image;

typedef struct path_batch {
    char **paths;
    size_t count;
    size_t capacity;
    char *error;
} path_batch;

typedef struct app_import_result {
    Uint32 completion_event;
    char *path;
    loim_image_info probe;
    SDL_Surface *surface;
    loim_status status;
} app_import_result;

typedef enum app_login_focus {
    APP_LOGIN_EMAIL = 0,
    APP_LOGIN_PASSWORD
} app_login_focus;

typedef struct app_auth_task {
    Uint32 completion_event;
    char email[256];
    char password[256];
    char machine_code[64];
    char platform[16];
    char architecture[16];
    char os_name[128];
    loim_login_result result;
    loim_status status;
    int http_status;
} app_auth_task;

typedef struct app_login_layout {
    SDL_FRect panel;
    SDL_FRect email;
    SDL_FRect password;
    SDL_FRect cancel;
    SDL_FRect submit;
} app_login_layout;

typedef struct app_text_cache_entry {
    char *text;
    SDL_Texture *texture;
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 alpha;
    float width;
    float height;
} app_text_cache_entry;

typedef struct app_state {
    SDL_Window *window;
    SDL_Renderer *renderer;
    loim_document *document;
    loim_layout layout;
    loim_layout_options layout_options;
    app_image *images;
    size_t image_count;
    size_t image_capacity;
    loim_import_queue dropped;
    path_batch import_pending;
    size_t import_pending_index;
    size_t import_succeeded;
    size_t import_failed;
    SDL_Thread *import_thread;
    app_import_result *import_result;
    bool dialog_open;
    bool quit_after_dialog;
    bool headless;
    bool running;
    Uint32 dialog_event;
    Uint32 import_event;
    Uint32 auth_event;
    Uint32 export_event;
    SDL_Thread *auth_thread;
    app_auth_task *auth_task;
    bool http_ready;
    bool login_open;
    bool login_submitting;
    bool logged_in;
    bool licensed;
    app_login_focus login_focus;
    char login_email[256];
    char login_password[256];
    char login_message[256];
    char machine_code[64];
    char auth_token[4096];
    char account_email[256];
    loim_locale locale;
    bool ttf_initialized;
    TTF_Font *font;
    app_text_cache_entry text_cache[LOIM_TEXT_CACHE_CAPACITY];
    size_t text_cache_next;
    float left_scroll_y;
    float right_scroll_y;
    float zoom;
    float split_ratio;
    float margin_ratio;
    float mouse_x;
    float mouse_y;
    float split_drag_start_y;
    float split_drag_current_y;
    size_t split_drag_index;
    bool divider_dragging;
    size_t columns;
    bool import_progress_active;
    size_t export_columns;
    float export_margin_ratio;
    bool export_licensed;
    bool export_show_page_numbers;
    char export_default_path[4096];
    bool show_page_numbers;
    char status[256];
} app_state;

static const SDL_DialogFileFilter app_image_filters_en[] = {
    {"Image files", "png;jpg;jpeg;gif;bmp"}
};

static const SDL_DialogFileFilter app_image_filters_zh[] = {
    {"图像文件", "png;jpg;jpeg;gif;bmp"}
};

static const SDL_DialogFileFilter app_pdf_filters[] = {
    {"PDF", "pdf"}
};

static const char *app_translation(const app_state *app, loim_text_key key)
{
    return loim_text(app->locale, key);
}

static void path_batch_destroy(path_batch *batch)
{
    size_t index;

    if (batch == NULL) {
        return;
    }
    for (index = 0U; index < batch->count; ++index) {
        SDL_free(batch->paths[index]);
    }
    SDL_free(batch->paths);
    SDL_free(batch->error);
    memset(batch, 0, sizeof(*batch));
}

static bool path_batch_append(path_batch *batch, const char *path)
{
    char **grown;
    char *copy;
    size_t capacity;

    if (batch == NULL || path == NULL || path[0] == '\0') {
        return false;
    }
    if (batch->count == batch->capacity) {
        capacity = batch->capacity == 0U ? 8U : batch->capacity * 2U;
        if (capacity < batch->capacity || capacity > SIZE_MAX / sizeof(*batch->paths)) {
            return false;
        }
        grown = SDL_realloc(batch->paths, capacity * sizeof(*batch->paths));
        if (grown == NULL) {
            return false;
        }
        batch->paths = grown;
        batch->capacity = capacity;
    }
    copy = SDL_strdup(path);
    if (copy == NULL) {
        return false;
    }
    batch->paths[batch->count] = copy;
    batch->count += 1U;
    return true;
}

static void app_set_status(app_state *app, const char *message)
{
    if (message == NULL) {
        message = "";
    }
    (void)snprintf(app->status, sizeof(app->status), "%s", message);
    (void)SDL_SetWindowTitle(
        app->window, app_translation(app, LOIM_TEXT_APP_TITLE));
}

static void app_secure_zero(void *data, size_t size)
{
    volatile unsigned char *bytes = data;

    while (size > 0U) {
        *bytes = 0U;
        bytes += 1;
        size -= 1U;
    }
}

static void app_copy_text(char *destination, size_t capacity, const char *source)
{
    if (capacity == 0U) {
        return;
    }
    (void)snprintf(destination, capacity, "%s", source == NULL ? "" : source);
}

static loim_locale app_system_locale(void)
{
    const char *override = SDL_getenv("LOIMREADER_LANG");
    SDL_Locale **locales = SDL_GetPreferredLocales(NULL);
    loim_locale locale = LOIM_LOCALE_EN;

    if (override != NULL && override[0] != '\0') {
        locale = loim_locale_from_name(override);
    } else if (locales != NULL && locales[0] != NULL) {
        locale = loim_locale_from_name(locales[0]->language);
    }
    SDL_free(locales);
    return locale;
}

static TTF_Font *app_open_ui_font(void)
{
#if defined(__APPLE__)
    static const char *const candidates[] = {
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Helvetica.ttc"
    };
#elif defined(_WIN32)
    static const char *const candidates[] = {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyh.ttf",
        "C:/Windows/Fonts/segoeui.ttf"
    };
#else
    static const char *const candidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };
#endif
    size_t index;

    for (index = 0U; index < sizeof(candidates) / sizeof(candidates[0]); ++index) {
        TTF_Font *font = TTF_OpenFont(candidates[index], 14.0F);

        if (font != NULL) {
            return font;
        }
    }
    return NULL;
}

static void app_text_cache_clear(app_state *app)
{
    size_t index;

    for (index = 0U; index < LOIM_TEXT_CACHE_CAPACITY; ++index) {
        SDL_free(app->text_cache[index].text);
        SDL_DestroyTexture(app->text_cache[index].texture);
        memset(&app->text_cache[index], 0, sizeof(app->text_cache[index]));
    }
    app->text_cache_next = 0U;
}

static app_text_cache_entry *app_text_cache_get(
    app_state *app,
    const char *text,
    Uint8 red,
    Uint8 green,
    Uint8 blue,
    Uint8 alpha)
{
    app_text_cache_entry *entry = NULL;
    SDL_Surface *surface;
    SDL_Color color = {red, green, blue, alpha};
    size_t index;

    for (index = 0U; index < LOIM_TEXT_CACHE_CAPACITY; ++index) {
        app_text_cache_entry *candidate = &app->text_cache[index];

        if (candidate->text != NULL && candidate->red == red &&
            candidate->green == green && candidate->blue == blue &&
            candidate->alpha == alpha && strcmp(candidate->text, text) == 0) {
            return candidate;
        }
        if (entry == NULL && candidate->text == NULL) {
            entry = candidate;
        }
    }
    if (entry == NULL) {
        entry = &app->text_cache[
            app->text_cache_next % LOIM_TEXT_CACHE_CAPACITY];
        app->text_cache_next += 1U;
        SDL_free(entry->text);
        SDL_DestroyTexture(entry->texture);
        memset(entry, 0, sizeof(*entry));
    }
    surface = TTF_RenderText_Blended(app->font, text, 0U, color);
    if (surface == NULL) {
        return NULL;
    }
    entry->texture = SDL_CreateTextureFromSurface(app->renderer, surface);
    if (entry->texture != NULL) {
        entry->text = SDL_strdup(text);
    }
    if (entry->texture == NULL || entry->text == NULL) {
        SDL_DestroyTexture(entry->texture);
        entry->texture = NULL;
        SDL_free(entry->text);
        entry->text = NULL;
        SDL_DestroySurface(surface);
        return NULL;
    }
    entry->red = red;
    entry->green = green;
    entry->blue = blue;
    entry->alpha = alpha;
    entry->width = (float)surface->w;
    entry->height = (float)surface->h;
    SDL_DestroySurface(surface);
    return entry;
}

static bool app_draw_text(
    app_state *app,
    float x,
    float y,
    const char *text,
    Uint8 red,
    Uint8 green,
    Uint8 blue,
    Uint8 alpha)
{
    app_text_cache_entry *entry;
    SDL_FRect destination;

    if (text == NULL || text[0] == '\0') {
        return true;
    }
    if (app->font == NULL) {
        (void)SDL_SetRenderDrawColor(app->renderer, red, green, blue, alpha);
        return SDL_RenderDebugText(app->renderer, x, y, text);
    }
    entry = app_text_cache_get(app, text, red, green, blue, alpha);
    if (entry == NULL) {
        return false;
    }
    destination.x = x;
    destination.y = y;
    destination.w = entry->width;
    destination.h = entry->height;
    return SDL_RenderTexture(
        app->renderer, entry->texture, NULL, &destination);
}

static float app_text_width(app_state *app, const char *text)
{
    app_text_cache_entry *entry;

    if (app->font == NULL) {
        return (float)strlen(text) * 8.0F;
    }
    entry = app_text_cache_get(app, text, 32U, 32U, 32U, 255U);
    return entry == NULL ? 0.0F : entry->width;
}

static const char *app_platform_name(void)
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

static const char *app_architecture_name(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#else
    return "amd64";
#endif
}

static bool app_device_code_valid(const char *code, size_t length)
{
    size_t index;

    if (length < 8U || length >= 64U) {
        return false;
    }
    for (index = 0U; index < length; ++index) {
        unsigned char value = (unsigned char)code[index];

        if (!((value >= '0' && value <= '9') ||
              (value >= 'A' && value <= 'Z') ||
              (value >= 'a' && value <= 'z') || value == '-')) {
            return false;
        }
    }
    return true;
}

static bool app_read_native_machine_id(
    uint8_t *output,
    size_t capacity,
    size_t *out_size)
{
#if defined(__APPLE__)
    io_service_t service;
    CFTypeRef value;
    char uuid[128];
    int length;

    service = IOServiceGetMatchingService(
        kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (service == IO_OBJECT_NULL) {
        return false;
    }
    value = IORegistryEntryCreateCFProperty(
        service,
        CFSTR("IOPlatformUUID"),
        kCFAllocatorDefault,
        0U);
    IOObjectRelease(service);
    if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID() ||
        !CFStringGetCString(
            (CFStringRef)value,
            uuid,
            (CFIndex)sizeof(uuid),
            kCFStringEncodingUTF8)) {
        if (value != NULL) {
            CFRelease(value);
        }
        return false;
    }
    CFRelease(value);
    length = snprintf((char *)output, capacity, "\"%s\"\n", uuid);
    if (length <= 0 || (size_t)length >= capacity) {
        return false;
    }
    *out_size = (size_t)length;
    return true;
#elif defined(_WIN32)
    DWORD byte_count;
    LSTATUS status;

    if (capacity > UINT32_MAX) {
        return false;
    }
    byte_count = (DWORD)capacity;
    status = RegGetValueA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Cryptography",
        "MachineGuid",
        RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
        NULL,
        output,
        &byte_count);
    if (status != ERROR_SUCCESS) {
        byte_count = (DWORD)capacity;
        status = RegGetValueA(
            HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Cryptography",
            "MachineGuid",
            RRF_RT_REG_SZ,
            NULL,
            output,
            &byte_count);
    }
    if (status != ERROR_SUCCESS || byte_count <= 1U ||
        output[byte_count - 1U] != 0U) {
        return false;
    }
    *out_size = (size_t)byte_count - 1U;
    return true;
#else
    static const char *paths[] = {
        "/var/lib/dbus/machine-id",
        "/etc/machine-id"
    };
    size_t index;

    for (index = 0U; index < sizeof(paths) / sizeof(paths[0]); ++index) {
        FILE *file = fopen(paths[index], "rb");

        if (file != NULL) {
            size_t size = fread(output, 1U, capacity, file);
            bool valid = !ferror(file) && size > 0U && size < capacity;

            (void)fclose(file);
            if (valid) {
                *out_size = size;
                return true;
            }
        }
    }
    return false;
#endif
}

static bool app_initialize_machine_code(app_state *app)
{
    char *preference_path = SDL_GetPrefPath("ctdy123", "LoimReader");
    char *device_path = NULL;
    void *loaded = NULL;
    size_t loaded_size = 0U;
    bool ready = false;
    uint8_t native_id[512];
    size_t native_id_size = 0U;

    if (preference_path == NULL ||
        SDL_asprintf(&device_path, "%sdevice-id.txt", preference_path) < 0) {
        SDL_free(preference_path);
        SDL_free(device_path);
        return false;
    }
    if (app_read_native_machine_id(
            native_id, sizeof(native_id), &native_id_size) &&
        loim_machine_code_from_bytes(
            native_id,
            native_id_size,
            app->machine_code,
            sizeof(app->machine_code)) == LOIM_OK) {
        ready = true;
        (void)SDL_SaveFile(
            device_path, app->machine_code, strlen(app->machine_code));
    }
    if (!ready) {
        loaded = SDL_LoadFile(device_path, &loaded_size);
        if (loaded != NULL && app_device_code_valid(loaded, loaded_size)) {
            memcpy(app->machine_code, loaded, loaded_size);
            app->machine_code[loaded_size] = '\0';
            ready = true;
        }
    }
    SDL_free(loaded);
    if (!ready) {
        int length = snprintf(
            app->machine_code,
            sizeof(app->machine_code),
            "LOIM-%08x%08x%08x%08x",
            SDL_rand_bits(),
            SDL_rand_bits(),
            SDL_rand_bits(),
            SDL_rand_bits());

        if (length > 0 && (size_t)length < sizeof(app->machine_code)) {
            ready = true;
            (void)SDL_SaveFile(device_path, app->machine_code, (size_t)length);
        }
    }
    SDL_free(device_path);
    SDL_free(preference_path);
    return ready;
}

static void app_auth_task_destroy(app_auth_task *task)
{
    if (task != NULL) {
        app_secure_zero(task, sizeof(*task));
        SDL_free(task);
    }
}

static int SDLCALL app_auth_thread_main(void *userdata)
{
    app_auth_task *task = userdata;
    loim_login_request request;
    SDL_Event event;

    request.email = task->email;
    request.password = task->password;
    request.machine_code = task->machine_code;
    request.platform = task->platform;
    request.architecture = task->architecture;
    request.os_name = task->os_name;
    request.app_version = LOIM_APP_VERSION;
    task->status = loim_auth_login(
        &request,
        loim_http_post_json,
        NULL,
        &task->result,
        &task->http_status);
    app_secure_zero(task->password, sizeof(task->password));
    SDL_zero(event);
    event.type = task->completion_event;
    event.user.data1 = task;
    return SDL_PushEvent(&event) ? 0 : 1;
}

static void app_open_login(app_state *app)
{
    app->login_open = true;
    app->login_focus = APP_LOGIN_EMAIL;
    if (app->logged_in) {
        (void)snprintf(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_SIGNED_IN_FORMAT),
            app->account_email);
    } else {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_LOGIN_PROMPT));
    }
    (void)SDL_StartTextInput(app->window);
}

static void app_close_login(app_state *app)
{
    if (app->login_submitting) {
        return;
    }
    app->login_open = false;
    app_secure_zero(app->login_password, sizeof(app->login_password));
    (void)SDL_StopTextInput(app->window);
}

static void app_submit_login(app_state *app)
{
    app_auth_task *task;

    if (app->login_submitting) {
        return;
    }
    if (!app->http_ready) {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_HTTPS_UNAVAILABLE));
        return;
    }
    if (strchr(app->login_email, '@') == NULL || app->login_password[0] == '\0') {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_LOGIN_FIELDS_REQUIRED));
        return;
    }
    task = SDL_calloc(1U, sizeof(*task));
    if (task == NULL) {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_LOGIN_START_FAILED));
        return;
    }
    task->completion_event = app->auth_event;
    app_copy_text(task->email, sizeof(task->email), app->login_email);
    app_copy_text(task->password, sizeof(task->password), app->login_password);
    app_copy_text(task->machine_code, sizeof(task->machine_code), app->machine_code);
    app_copy_text(task->platform, sizeof(task->platform), app_platform_name());
    app_copy_text(task->architecture, sizeof(task->architecture), app_architecture_name());
    app_copy_text(task->os_name, sizeof(task->os_name), SDL_GetPlatform());
    app->auth_task = task;
    app->login_submitting = true;
    app_copy_text(
        app->login_message,
        sizeof(app->login_message),
        app_translation(app, LOIM_TEXT_SIGNING_IN));
    app->auth_thread = SDL_CreateThread(app_auth_thread_main, "loim-login", task);
    if (app->auth_thread == NULL) {
        app->auth_task = NULL;
        app->login_submitting = false;
        app_auth_task_destroy(task);
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_LOGIN_START_FAILED));
    }
}

static void app_handle_auth_event(app_state *app, SDL_Event *event)
{
    app_auth_task *task = event->user.data1;

    if (task == NULL || task != app->auth_task) {
        return;
    }
    SDL_WaitThread(app->auth_thread, NULL);
    app->auth_thread = NULL;
    app->login_submitting = false;
    if (task->status == LOIM_OK && task->result.success) {
        app_copy_text(app->auth_token, sizeof(app->auth_token), task->result.token);
        app_copy_text(app->account_email, sizeof(app->account_email), task->result.email);
        app->logged_in = true;
        app->licensed = task->result.subscription_type[0] == '\0' ||
            SDL_strcasecmp(task->result.subscription_type, "free") != 0;
        app->login_open = false;
        (void)SDL_StopTextInput(app->window);
        (void)snprintf(
            app->status,
            sizeof(app->status),
            app_translation(app, LOIM_TEXT_SIGNED_IN_FORMAT),
            app->account_email);
    } else if (task->status != LOIM_OK) {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_LOGIN_NETWORK_ERROR));
    } else if (strcmp(task->result.error_code, "INVALID_CREDENTIALS") == 0) {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_LOGIN_INVALID_CREDENTIALS));
    } else if (strcmp(task->result.error_code, "DEVICE_LIMIT_EXCEEDED") == 0) {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_LOGIN_DEVICE_LIMIT));
    } else if (strcmp(task->result.error_code, "ACCOUNT_DISABLED") == 0) {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_LOGIN_ACCOUNT_DISABLED));
    } else {
        (void)snprintf(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_LOGIN_FAILED_FORMAT),
            task->http_status);
    }
    app_secure_zero(app->login_password, sizeof(app->login_password));
    app->auth_task = NULL;
    app_auth_task_destroy(task);
}

static bool app_grow_images(app_state *app)
{
    app_image *grown;
    size_t capacity;

    if (app->image_count < app->image_capacity) {
        return true;
    }
    capacity = app->image_capacity == 0U ? 8U : app->image_capacity * 2U;
    if (capacity < app->image_capacity || capacity > SIZE_MAX / sizeof(*app->images)) {
        return false;
    }
    grown = realloc(app->images, capacity * sizeof(*app->images));
    if (grown == NULL) {
        return false;
    }
    app->images = grown;
    app->image_capacity = capacity;
    return true;
}

static void app_add_seam_hints(
    app_state *app,
    size_t source_index,
    const SDL_Surface *surface,
    uint32_t width,
    uint32_t height)
{
    uint64_t normalized_height;
    uint64_t position;
    size_t hint_count = 0U;

    if (surface->pixels == NULL || surface->pitch <= 0 || width == 0U || height < 2U) {
        return;
    }
    normalized_height =
        ((uint64_t)height * (uint64_t)LOIM_LAYOUT_WIDTH + (uint64_t)width - 1U) /
        (uint64_t)width;
    position = (uint64_t)app->layout_options.target_page_height_px;
    while (position < normalized_height && hint_count < LOIM_MAX_AUTO_HINTS) {
        uint64_t source_target =
            (position * (uint64_t)height + normalized_height / 2U) /
            normalized_height;
        uint64_t source_radius =
            ((uint64_t)app->layout_options.search_radius_px * (uint64_t)height +
             normalized_height - 1U) /
            normalized_height;
        loim_seam_result result;
        loim_status status;

        if (source_target == 0U) {
            source_target = 1U;
        } else if (source_target >= (uint64_t)height) {
            source_target = (uint64_t)height - 1U;
        }
        if (source_radius > UINT32_MAX) {
            source_radius = UINT32_MAX;
        }
        status = loim_seam_find_rgba8(
            (const uint8_t *)surface->pixels,
            width,
            height,
            (size_t)surface->pitch,
            (uint32_t)source_target,
            (uint32_t)source_radius,
            &result);
        if (status == LOIM_OK && result.quality >= 0.52F) {
            (void)loim_document_add_split_hint(
                app->document,
                source_index,
                result.row,
                result.quality,
                LOIM_SPLIT_HINT_WHITESPACE);
        }
        if (position > UINT64_MAX - (uint64_t)app->layout_options.target_page_height_px) {
            break;
        }
        position += (uint64_t)app->layout_options.target_page_height_px;
        hint_count += 1U;
    }
}

static void app_destroy_texture_tiles(app_texture_tile *tiles, size_t count)
{
    size_t index;

    for (index = 0U; index < count; ++index) {
        SDL_DestroyTexture(tiles[index].texture);
    }
    free(tiles);
}

static void app_import_result_destroy(app_import_result *result)
{
    if (result == NULL) {
        return;
    }
    SDL_DestroySurface(result->surface);
    SDL_free(result->path);
    SDL_free(result);
}

static loim_status app_decode_image(
    const char *path,
    loim_image_info *out_probe,
    SDL_Surface **out_surface)
{
    loim_image_info probe;
    SDL_Surface *loaded = NULL;
    SDL_Surface *rgba = NULL;
    loim_status status;
    uint64_t pixel_count;

    if (path == NULL || out_probe == NULL || out_surface == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    *out_surface = NULL;
    status = loim_image_probe_file(path, &probe);
    if (status != LOIM_OK) {
        return status;
    }
    pixel_count = (uint64_t)probe.width_px * (uint64_t)probe.height_px;
    if (pixel_count > LOIM_MAX_IMAGE_PIXELS) {
        return LOIM_ERROR_OVERFLOW;
    }
    loaded = IMG_Load(path);
    if (loaded == NULL) {
        return LOIM_ERROR_CORRUPT_IMAGE;
    }
    rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (rgba == NULL) {
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    if (rgba->w <= 0 || rgba->h <= 0 ||
        (uint32_t)rgba->w != probe.width_px || (uint32_t)rgba->h != probe.height_px) {
        SDL_DestroySurface(rgba);
        return LOIM_ERROR_CORRUPT_IMAGE;
    }
    *out_probe = probe;
    *out_surface = rgba;
    return LOIM_OK;
}

static loim_status app_create_texture_tiles(
    app_state *app,
    SDL_Surface *surface,
    app_texture_tile **out_tiles,
    size_t *out_count)
{
    Sint64 maximum_size_value;
    uint32_t maximum_size;
    loim_texture_tile *plan = NULL;
    app_texture_tile *tiles = NULL;
    size_t tile_count = 0U;
    size_t index;
    loim_status status;

    if (app == NULL || surface == NULL || out_tiles == NULL || out_count == NULL ||
        surface->w <= 0 || surface->h <= 0 || surface->pitch <= 0 ||
        surface->pixels == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    *out_tiles = NULL;
    *out_count = 0U;
    maximum_size_value = SDL_GetNumberProperty(
        SDL_GetRendererProperties(app->renderer),
        SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER,
        4096);
    if (maximum_size_value <= 0) {
        maximum_size_value = 4096;
    } else if ((uint64_t)maximum_size_value > UINT32_MAX) {
        maximum_size_value = UINT32_MAX;
    }
    maximum_size = (uint32_t)maximum_size_value;
    status = loim_texture_plan(
        (uint32_t)surface->w,
        (uint32_t)surface->h,
        maximum_size,
        NULL,
        0U,
        &tile_count);
    if (status != LOIM_OK) {
        return status;
    }
    if (tile_count > SIZE_MAX / sizeof(*plan) ||
        tile_count > SIZE_MAX / sizeof(*tiles)) {
        return LOIM_ERROR_OVERFLOW;
    }
    plan = calloc(tile_count, sizeof(*plan));
    tiles = calloc(tile_count, sizeof(*tiles));
    if (plan == NULL || tiles == NULL) {
        free(plan);
        free(tiles);
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    status = loim_texture_plan(
        (uint32_t)surface->w,
        (uint32_t)surface->h,
        maximum_size,
        plan,
        tile_count,
        &tile_count);
    if (status != LOIM_OK) {
        free(plan);
        free(tiles);
        return status;
    }
    for (index = 0U; index < tile_count; ++index) {
        uint8_t *pixels = (uint8_t *)surface->pixels +
            (size_t)plan[index].source_y_px * (size_t)surface->pitch;
        SDL_Surface *tile_surface = SDL_CreateSurfaceFrom(
            surface->w,
            (int)plan[index].height_px,
            surface->format,
            pixels,
            surface->pitch);

        if (tile_surface == NULL) {
            app_destroy_texture_tiles(tiles, index);
            free(plan);
            return LOIM_ERROR_OUT_OF_MEMORY;
        }
        tiles[index].texture = SDL_CreateTextureFromSurface(app->renderer, tile_surface);
        SDL_DestroySurface(tile_surface);
        if (tiles[index].texture == NULL) {
            app_destroy_texture_tiles(tiles, index);
            free(plan);
            return LOIM_ERROR_OUT_OF_MEMORY;
        }
        (void)SDL_SetTextureScaleMode(tiles[index].texture, SDL_SCALEMODE_LINEAR);
        tiles[index].source_y_px = plan[index].source_y_px;
        tiles[index].height_px = plan[index].height_px;
    }
    free(plan);
    *out_tiles = tiles;
    *out_count = tile_count;
    return LOIM_OK;
}

static loim_status app_commit_decoded_image(
    app_state *app,
    const char *path,
    const loim_image_info *probe,
    SDL_Surface *rgba)
{
    app_texture_tile *tiles = NULL;
    size_t tile_count = 0U;
    char *path_copy = NULL;
    loim_source_info source;
    size_t source_index = 0U;
    loim_status status;

    status = app_create_texture_tiles(app, rgba, &tiles, &tile_count);
    if (status != LOIM_OK) {
        return status;
    }
    path_copy = SDL_strdup(path);
    if (path_copy == NULL || !app_grow_images(app)) {
        SDL_free(path_copy);
        app_destroy_texture_tiles(tiles, tile_count);
        return LOIM_ERROR_OUT_OF_MEMORY;
    }

    source.path = path;
    source.width_px = probe->width_px;
    source.height_px = probe->height_px;
    status = loim_document_add_source(app->document, &source, &source_index);
    if (status != LOIM_OK || source_index != app->image_count) {
        SDL_free(path_copy);
        app_destroy_texture_tiles(tiles, tile_count);
        return status == LOIM_OK ? LOIM_ERROR_INVALID_ARGUMENT : status;
    }
    app->images[app->image_count].path = path_copy;
    app->images[app->image_count].tiles = tiles;
    app->images[app->image_count].tile_count = tile_count;
    app->images[app->image_count].width = probe->width_px;
    app->images[app->image_count].height = probe->height_px;
    app->image_count += 1U;
    app_add_seam_hints(app, source_index, rgba, probe->width_px, probe->height_px);
    return LOIM_OK;
}

static loim_status app_import_one(app_state *app, const char *path)
{
    loim_image_info probe;
    SDL_Surface *rgba = NULL;
    loim_status status = app_decode_image(path, &probe, &rgba);

    if (status == LOIM_OK) {
        status = app_commit_decoded_image(app, path, &probe, rgba);
    }
    SDL_DestroySurface(rgba);
    return status;
}

static loim_status app_rebuild_layout(app_state *app)
{
    loim_layout next = {0};
    loim_status status;

    if (app->image_count == 0U) {
        loim_layout_destroy(&app->layout);
        app->left_scroll_y = 0.0F;
        app->right_scroll_y = 0.0F;
        return LOIM_OK;
    }
    status = loim_layout_build(app->document, &app->layout_options, &next);
    if (status != LOIM_OK) {
        return status;
    }
    loim_layout_destroy(&app->layout);
    app->layout = next;
    app->left_scroll_y = 0.0F;
    app->right_scroll_y = 0.0F;
    return LOIM_OK;
}

static int SDLCALL app_import_thread_main(void *userdata)
{
    app_import_result *result = userdata;
    SDL_Event event;

    result->status = app_decode_image(result->path, &result->probe, &result->surface);
    SDL_zero(event);
    event.type = result->completion_event;
    event.user.data1 = result;
    return SDL_PushEvent(&event) ? 0 : 1;
}

static void app_finish_import_batch(app_state *app)
{
    char message[256];
    loim_status layout_status = app_rebuild_layout(app);

    if (layout_status != LOIM_OK) {
        (void)snprintf(
            message,
            sizeof(message),
            app_translation(app, LOIM_TEXT_LAYOUT_FAILED_FORMAT),
            loim_status_text(app->locale, layout_status));
    } else {
        (void)snprintf(
            message,
            sizeof(message),
            app_translation(app, LOIM_TEXT_IMPORTED_FORMAT),
            app->import_succeeded,
            app->import_failed,
            app->image_count,
            app->layout.page_count);
    }
    app_set_status(app, message);
    app->import_progress_active = false;
    path_batch_destroy(&app->import_pending);
    app->import_pending_index = 0U;
}

static void app_start_next_import(app_state *app)
{
    while (app->import_pending_index < app->import_pending.count) {
        app_import_result *result = SDL_calloc(1U, sizeof(*result));
        const char *path = app->import_pending.paths[app->import_pending_index];
        char message[256];

        if (result == NULL) {
            app->import_failed += 1U;
            app->import_pending_index += 1U;
            continue;
        }
        result->path = SDL_strdup(path);
        result->completion_event = app->import_event;
        if (result->path == NULL) {
            app_import_result_destroy(result);
            app->import_failed += 1U;
            app->import_pending_index += 1U;
            continue;
        }
        (void)snprintf(
            message,
            sizeof(message),
            app_translation(app, LOIM_TEXT_IMPORT_PROGRESS_FORMAT),
            app->import_pending_index + 1U,
            app->import_pending.count,
            loim_progress_percent(
                app->import_pending_index,
                app->import_pending.count));
        app_set_status(app, message);
        app->import_result = result;
        app->import_thread = SDL_CreateThread(
            app_import_thread_main, "loim-image-decode", result);
        if (app->import_thread != NULL) {
            return;
        }
        app->import_result = NULL;
        app_import_result_destroy(result);
        app->import_failed += 1U;
        app->import_pending_index += 1U;
    }
    app_finish_import_batch(app);
}

static void app_handle_import_event(app_state *app, SDL_Event *event)
{
    app_import_result *result = event->user.data1;
    loim_status status;

    if (result == NULL || result != app->import_result) {
        return;
    }
    SDL_WaitThread(app->import_thread, NULL);
    app->import_thread = NULL;
    status = result->status;
    if (status == LOIM_OK) {
        status = app_commit_decoded_image(
            app, result->path, &result->probe, result->surface);
    }
    if (status == LOIM_OK) {
        app->import_succeeded += 1U;
        (void)app_rebuild_layout(app);
    } else {
        app->import_failed += 1U;
    }
    app->import_result = NULL;
    app_import_result_destroy(result);
    app->import_pending_index += 1U;
    app_start_next_import(app);
}

static void app_import_batch_synchronously(app_state *app, const path_batch *batch)
{
    size_t index;
    size_t imported = 0U;
    size_t failed = 0U;
    char message[256];
    loim_status layout_status;

    for (index = 0U; index < batch->count; ++index) {
        loim_status status = app_import_one(app, batch->paths[index]);

        if (status == LOIM_OK) {
            imported += 1U;
        } else {
            failed += 1U;
        }
    }
    layout_status = app_rebuild_layout(app);
    if (layout_status != LOIM_OK) {
        (void)snprintf(
            message,
            sizeof(message),
            app_translation(app, LOIM_TEXT_LAYOUT_FAILED_FORMAT),
            loim_status_text(app->locale, layout_status));
    } else {
        (void)snprintf(
            message,
            sizeof(message),
            app_translation(app, LOIM_TEXT_IMPORTED_FORMAT),
            imported,
            failed,
            app->image_count,
            app->layout.page_count);
    }
    app_set_status(app, message);
}

static void app_import_batch(app_state *app, const path_batch *batch)
{
    size_t index;

    if (batch->error != NULL) {
        app_set_status(app, app_translation(app, LOIM_TEXT_FILE_DIALOG_ERROR));
        return;
    }
    if (batch->count == 0U) {
        app_set_status(app, app_translation(app, LOIM_TEXT_IMPORT_CANCELED));
        return;
    }
    if (app->headless) {
        app_import_batch_synchronously(app, batch);
        return;
    }
    if (app->import_thread != NULL || app->import_pending.count != 0U) {
        app_set_status(app, app_translation(app, LOIM_TEXT_IMPORT_BUSY));
        return;
    }
    app->import_progress_active = true;
    for (index = 0U; index < batch->count; ++index) {
        if (!path_batch_append(&app->import_pending, batch->paths[index])) {
            path_batch_destroy(&app->import_pending);
            app->import_progress_active = false;
            app_set_status(app, app_translation(app, LOIM_TEXT_IMPORT_QUEUE_FAILED));
            return;
        }
    }
    app->import_pending_index = 0U;
    app->import_succeeded = 0U;
    app->import_failed = 0U;
    app_start_next_import(app);
}

#define LOIM_PDF_WIDTH 2480U
#define LOIM_PDF_HEIGHT 3508U
#define LOIM_GRID_GAP_RATIO 0.025F

static void app_pdf_fill_white(uint8_t *rgb, size_t bytes)
{
    size_t index;

    for (index = 0U; index < bytes; ++index) {
        rgb[index] = 255U;
    }
}

static void app_pdf_blend_pixel(uint8_t *destination, const uint8_t *source)
{
    unsigned alpha = source[3];
    unsigned inverse = 255U - alpha;

    destination[0] = (uint8_t)((source[0] * alpha + destination[0] * inverse) / 255U);
    destination[1] = (uint8_t)((source[1] * alpha + destination[1] * inverse) / 255U);
    destination[2] = (uint8_t)((source[2] * alpha + destination[2] * inverse) / 255U);
}

static SDL_Surface *app_pdf_text_surface(
    app_state *app,
    const char *text,
    float font_size,
    SDL_Color color)
{
    SDL_Surface *rendered;
    SDL_Surface *converted;

    if (!TTF_SetFontSize(app->font, font_size)) {
        return NULL;
    }
    rendered = TTF_RenderText_Blended(app->font, text, 0U, color);
    (void)TTF_SetFontSize(app->font, 14.0F);
    if (rendered == NULL) {
        return NULL;
    }
    converted = SDL_ConvertSurface(rendered, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(rendered);
    return converted;
}

static void app_pdf_stamp_surface(
    uint8_t *rgb,
    const SDL_Surface *surface,
    int destination_x,
    int destination_y,
    unsigned opacity)
{
    int y;

    if (surface == NULL || surface->pixels == NULL) {
        return;
    }
    for (y = 0; y < surface->h; ++y) {
        const uint8_t *source = (const uint8_t *)surface->pixels +
            (size_t)y * (size_t)surface->pitch;
        int x;

        for (x = 0; x < surface->w; ++x) {
            int output_x = destination_x + x;
            int output_y = destination_y + y;
            uint8_t pixel[4];

            if (output_x < 0 || output_y < 0 ||
                output_x >= (int)LOIM_PDF_WIDTH || output_y >= (int)LOIM_PDF_HEIGHT) {
                continue;
            }
            memcpy(pixel, source + (size_t)x * 4U, sizeof(pixel));
            pixel[3] = (uint8_t)((unsigned)pixel[3] * opacity / 255U);
            app_pdf_blend_pixel(
                rgb + ((size_t)output_y * LOIM_PDF_WIDTH + (size_t)output_x) * 3U,
                pixel);
        }
    }
}

static void app_pdf_apply_watermark(app_state *app, uint8_t *rgb)
{
    SDL_Color gray = {150U, 150U, 150U, 255U};
    SDL_Surface *text = app_pdf_text_surface(
        app, "影谷长图阅读器 ctdy123.com", 34.0F, gray);
    int y;

    if (text == NULL) {
        return;
    }
    for (y = 280; y < (int)LOIM_PDF_HEIGHT; y += 480) {
        int x;
        int offset = ((y / 480) % 2) * 260;

        for (x = -260 + offset; x < (int)LOIM_PDF_WIDTH; x += 760) {
            app_pdf_stamp_surface(rgb, text, x, y, 92U);
        }
    }
    SDL_DestroySurface(text);
}

static void app_pdf_apply_page_number(
    app_state *app,
    uint8_t *rgb,
    size_t page_number,
    float margin_ratio)
{
    char number[32];
    SDL_Color gray = {110U, 110U, 110U, 255U};
    SDL_Surface *text;
    int margin_x = (int)((float)LOIM_PDF_WIDTH * margin_ratio);
    int margin_y = (int)((float)LOIM_PDF_HEIGHT * margin_ratio);

    (void)snprintf(number, sizeof(number), "%zu", page_number);
    text = app_pdf_text_surface(app, number, 30.0F, gray);
    if (text == NULL) {
        return;
    }
    app_pdf_stamp_surface(
        rgb,
        text,
        (int)LOIM_PDF_WIDTH - margin_x - text->w,
        (int)LOIM_PDF_HEIGHT - margin_y - text->h,
        255U);
    SDL_DestroySurface(text);
}

static const loim_page_slice *app_slice_at(const app_state *app, size_t slice_index)
{
    size_t page_index;

    for (page_index = 0U; page_index < app->layout.page_count; ++page_index) {
        const loim_page *page = &app->layout.pages[page_index];

        if (slice_index < page->slice_count) {
            return &app->layout.slices[page->first_slice + slice_index];
        }
        slice_index -= page->slice_count;
    }
    return NULL;
}

static bool app_grid_destination(
    const app_state *app,
    size_t sheet_index,
    size_t columns,
    size_t slot,
    float column_x,
    float content_y,
    float column_width,
    float content_height,
    SDL_FRect *destination)
{
    size_t column = loim_sheet_slot_column(columns, slot);
    size_t row = loim_sheet_slot_row(columns, slot);
    size_t current_index = loim_sheet_slice_index(sheet_index, columns, slot);
    const loim_page_slice *current = app_slice_at(app, current_index);
    float unscaled_stack_height = 0.0F;
    float preceding_height = 0.0F;
    float current_height = 0.0F;
    float stack_scale = 1.0F;
    size_t item_row;

    if (current == NULL || destination == NULL) {
        return false;
    }
    for (item_row = 0U; item_row < columns; ++item_row) {
        size_t item_slot = item_row * columns + column;
        size_t item_index = loim_sheet_slice_index(sheet_index, columns, item_slot);
        const loim_page_slice *item = app_slice_at(app, item_index);
        float item_height;

        if (item == NULL) {
            continue;
        }
        item_height = (float)item->source_height_px * column_width /
            (float)app->images[item->source_index].width;
        if (item_row < row) {
            preceding_height += item_height;
        } else if (item_row == row) {
            current_height = item_height;
        }
        unscaled_stack_height += item_height;
    }
    if (unscaled_stack_height > content_height) {
        stack_scale = content_height / unscaled_stack_height;
    }
    destination->w = column_width * stack_scale;
    destination->h = current_height * stack_scale;
    destination->x = column_x + (column_width - destination->w) / 2.0F;
    destination->y = content_y +
        (content_height - unscaled_stack_height * stack_scale) / 2.0F +
        preceding_height * stack_scale;
    return true;
}

static void app_pdf_copy_slice(
    uint8_t *rgb,
    SDL_Surface *source,
    uint32_t source_y,
    uint32_t source_height,
    unsigned destination_x,
    unsigned destination_y,
    unsigned destination_width,
    unsigned destination_height,
    unsigned clip_x,
    unsigned clip_y,
    unsigned clip_width,
    unsigned clip_height)
{
    unsigned y;
    unsigned x;

    if (source == NULL || source->pixels == NULL || source_height == 0U ||
        destination_width == 0U || destination_height == 0U) {
        return;
    }
    for (y = 0U; y < destination_height; ++y) {
        uint32_t source_row = source_y + (uint32_t)(((uint64_t)y * source_height) /
            (uint64_t)destination_height);
        const uint8_t *source_pixels = (const uint8_t *)source->pixels +
            (size_t)source_row * (size_t)source->pitch;
        unsigned out_y = destination_y + y;
        if (out_y < clip_y || out_y >= clip_y + clip_height) continue;
        uint8_t *destination = rgb +
            ((size_t)out_y * LOIM_PDF_WIDTH + destination_x) * 3U;

        for (x = 0U; x < destination_width; ++x) {
            unsigned out_x = destination_x + x;
            if (out_x < clip_x || out_x >= clip_x + clip_width) continue;
            uint32_t source_column = (uint32_t)(((uint64_t)x * (uint64_t)source->w) /
                (uint64_t)destination_width);

            app_pdf_blend_pixel(destination + (size_t)x * 3U,
                                source_pixels + (size_t)source_column * 4U);
        }
    }
}

static loim_status app_render_pdf_sheet(
    app_state *app,
    size_t sheet_index,
    size_t columns,
    float margin_ratio,
    uint8_t *rgb)
{
    const float margin_x = (float)LOIM_PDF_WIDTH * margin_ratio;
    const float margin_y = (float)LOIM_PDF_HEIGHT * margin_ratio;
    const float gap = (float)LOIM_PDF_WIDTH * LOIM_GRID_GAP_RATIO;
    const float content_width = (float)LOIM_PDF_WIDTH - margin_x * 2.0F;
    const float content_height = (float)LOIM_PDF_HEIGHT - margin_y * 2.0F;
    const float cell_width = (content_width - gap * (float)(columns - 1U)) /
        (float)columns;
    size_t slot;
    size_t cached_source = SIZE_MAX;
    SDL_Surface *cached_surface = NULL;

    app_pdf_fill_white(rgb, (size_t)LOIM_PDF_WIDTH * LOIM_PDF_HEIGHT * 3U);
    for (slot = 0U; slot < columns * columns; ++slot) {
        size_t slice_index = loim_sheet_slice_index(sheet_index, columns, slot);
        const loim_page_slice *slice = app_slice_at(app, slice_index);
        size_t column = loim_sheet_slot_column(columns, slot);
        SDL_FRect destination;
        const app_image *image;

        if (slice == NULL || !app_grid_destination(
                app,
                sheet_index,
                columns,
                slot,
                margin_x + (cell_width + gap) * (float)column,
                margin_y,
                cell_width,
                content_height,
                &destination)) {
            continue;
        }
        image = &app->images[slice->source_index];
        if (cached_source != slice->source_index) {
            SDL_DestroySurface(cached_surface);
            cached_surface = NULL;
            cached_source = slice->source_index;
            {
                loim_image_info ignored_probe;

                if (app_decode_image(
                        image->path, &ignored_probe, &cached_surface) != LOIM_OK) {
                    SDL_DestroySurface(cached_surface);
                    cached_surface = NULL;
                    return LOIM_ERROR_CORRUPT_IMAGE;
                }
            }
        }
        app_pdf_copy_slice(
            rgb,
            cached_surface,
            slice->source_y_px,
            slice->source_height_px,
            (unsigned)destination.x,
            (unsigned)destination.y,
            (unsigned)destination.w,
            (unsigned)destination.h,
            (unsigned)(margin_x + (cell_width + gap) * (float)column),
            (unsigned)margin_y,
            (unsigned)cell_width,
            (unsigned)content_height);
    }
    SDL_DestroySurface(cached_surface);
    return LOIM_OK;
}

static loim_status app_export_pdf(
    app_state *app,
    const char *path,
    size_t columns,
    float margin_ratio,
    bool licensed,
    bool show_page_numbers)
{
    loim_pdf_rgb_page *pages = NULL;
    columns = loim_columns_normalize(columns);
    size_t sheet_count = loim_sheet_count(app->layout.slice_count, columns);
    size_t page_bytes = (size_t)LOIM_PDF_WIDTH * LOIM_PDF_HEIGHT * 3U;
    loim_status status = LOIM_OK;
    size_t index;

    if (app->layout.page_count == 0U || sheet_count == 0U) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    if (sheet_count > SIZE_MAX / sizeof(*pages) || page_bytes > SIZE_MAX / sheet_count) {
        return LOIM_ERROR_OVERFLOW;
    }
    pages = SDL_calloc(sheet_count, sizeof(*pages));
    if (pages == NULL) {
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < sheet_count; ++index) {
        pages[index].width_px = LOIM_PDF_WIDTH;
        pages[index].height_px = LOIM_PDF_HEIGHT;
        pages[index].media_width_pt = 595U;
        pages[index].media_height_pt = 842U;
        pages[index].rgb = SDL_malloc(page_bytes);
        if (pages[index].rgb == NULL) {
            status = LOIM_ERROR_OUT_OF_MEMORY;
            break;
        }
        status = app_render_pdf_sheet(
            app, index, columns, margin_ratio, (uint8_t *)pages[index].rgb);
        if (status != LOIM_OK) {
            break;
        }
        if (loim_export_page_requires_watermark(licensed, columns, index)) {
            app_pdf_apply_watermark(app, (uint8_t *)pages[index].rgb);
        }
        if (show_page_numbers) {
            app_pdf_apply_page_number(
                app, (uint8_t *)pages[index].rgb, index + 1U, margin_ratio);
        }
    }
    if (status == LOIM_OK) {
        status = loim_pdf_write_rgb(path, pages, sheet_count);
    }
    for (index = 0U; index < sheet_count; ++index) {
        SDL_free((void *)pages[index].rgb);
    }
    SDL_free(pages);
    return status;
}

static void SDLCALL app_dialog_callback(
    void *userdata,
    const char *const *filelist,
    int filter)
{
    app_state *app = (app_state *)userdata;
    path_batch *batch;
    SDL_Event event;
    size_t index;

    (void)filter;
    batch = SDL_calloc(1U, sizeof(*batch));
    if (batch == NULL) {
        return;
    }
    if (filelist == NULL) {
        const char *error = SDL_GetError();

        batch->error = SDL_strdup(error == NULL ? "unknown error" : error);
    } else {
        for (index = 0U; filelist[index] != NULL; ++index) {
            if (!path_batch_append(batch, filelist[index])) {
                batch->error = SDL_strdup("out of memory while reading selected paths");
                break;
            }
        }
    }
    SDL_zero(event);
    event.type = app->dialog_event;
    event.user.data1 = batch;
    if (!SDL_PushEvent(&event)) {
        path_batch_destroy(batch);
        SDL_free(batch);
    }
}

static void SDLCALL app_export_dialog_callback(
    void *userdata,
    const char *const *filelist,
    int filter)
{
    app_state *app = (app_state *)userdata;
    SDL_Event event;
    char *path = NULL;

    (void)filter;
    if (filelist != NULL && filelist[0] != NULL) {
        path = SDL_strdup(filelist[0]);
    }
    SDL_zero(event);
    event.type = app->export_event;
    event.user.data1 = path;
    if (!SDL_PushEvent(&event)) {
        SDL_free(path);
    }
}

static void app_open_dialog(app_state *app)
{
    if (app->dialog_open) {
        return;
    }
    app->dialog_open = true;
    app_set_status(app, app_translation(app, LOIM_TEXT_CHOOSE_IMAGES));
    {
        const SDL_DialogFileFilter *filters = app->locale == LOIM_LOCALE_ZH_CN
            ? app_image_filters_zh
            : app_image_filters_en;

        SDL_ShowOpenFileDialog(
            app_dialog_callback,
            app,
            app->window,
            filters,
            1,
            NULL,
            true);
    }
}

static void app_open_export_dialog(app_state *app)
{
    const char *documents_path;
    const char *image_path;

    if (app->dialog_open) {
        return;
    }
    if (app->layout.page_count == 0U) {
        app_set_status(app, app_translation(app, LOIM_TEXT_EXPORT_FAILED));
        return;
    }
    app->dialog_open = true;
    app->export_columns = loim_columns_normalize(app->columns);
    app->export_margin_ratio = app->margin_ratio;
    app->export_licensed = app->licensed;
    app->export_show_page_numbers = app->show_page_numbers;
    documents_path = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    image_path = app->image_count > 0U ? app->images[0].path : NULL;
    if (!loim_export_default_path(
            documents_path,
            image_path,
            app->export_default_path,
            sizeof(app->export_default_path))) {
        app_copy_text(
            app->export_default_path,
            sizeof(app->export_default_path),
            "LoimReader.pdf");
    }
    SDL_ShowSaveFileDialog(
        app_export_dialog_callback,
        app,
        app->window,
        app_pdf_filters,
        1,
        app->export_default_path);
}

static bool app_print_file(const char *path)
{
#if defined(_WIN32)
    return (INT_PTR)ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL) > 32;
#elif defined(__APPLE__)
    extern char **environ;
    pid_t process;
    char *const arguments[] = {
        (char *)"osascript",
        (char *)"-e", (char *)"on run argv",
        (char *)"-e", (char *)"set pdfFile to POSIX file (item 1 of argv) as alias",
        (char *)"-e", (char *)"tell application \"Preview\"",
        (char *)"-e", (char *)"open pdfFile",
        (char *)"-e", (char *)"activate",
        (char *)"-e", (char *)"repeat 50 times",
        (char *)"-e", (char *)"if (count documents) > 0 then exit repeat",
        (char *)"-e", (char *)"delay 0.2",
        (char *)"-e", (char *)"end repeat",
        (char *)"-e", (char *)"if (count documents) > 0 then print document 1 with print dialog",
        (char *)"-e", (char *)"end tell",
        (char *)"-e", (char *)"end run",
        (char *)path,
        NULL};

    return posix_spawnp(&process, "osascript", NULL, NULL, arguments, environ) == 0;
#else
    extern char **environ;
    pid_t process;
    char *const arguments[] = {(char *)"xdg-open", (char *)path, NULL};

    return posix_spawnp(&process, "xdg-open", NULL, NULL, arguments, environ) == 0;
#endif
}

static void app_start_print(app_state *app)
{
    char *preference_path;
    char *pdf_path = NULL;
    size_t columns;
    loim_status status;

    if (app->layout.page_count == 0U) {
        app_set_status(app, app_translation(app, LOIM_TEXT_PRINT_FAILED));
        return;
    }
    preference_path = SDL_GetPrefPath("ctdy123", "LoimReader");
    if (preference_path == NULL ||
        SDL_asprintf(&pdf_path, "%sprint-preview.pdf", preference_path) < 0) {
        SDL_free(preference_path);
        app_set_status(app, app_translation(app, LOIM_TEXT_PRINT_FAILED));
        return;
    }
    SDL_free(preference_path);
    columns = loim_columns_normalize(app->columns);
    status = app_export_pdf(
        app,
        pdf_path,
        columns,
        app->margin_ratio,
        app->licensed,
        app->show_page_numbers);
    if (status != LOIM_OK) {
        app_set_status(app, app_translation(app, LOIM_TEXT_PRINT_FAILED));
    } else {
        app_set_status(app, app_print_file(pdf_path)
            ? app_translation(app, LOIM_TEXT_PRINT_STARTED)
            : app_translation(app, LOIM_TEXT_PRINT_FAILED));
    }
    SDL_free(pdf_path);
}

static bool app_pdf_path_has_extension(const char *path)
{
    size_t length = strlen(path);

    if (length >= 4U && SDL_strcasecmp(path + length - 4U, ".pdf") == 0) {
        return true;
    }
    /* Treat the full-width dot commonly produced by a Chinese IME as a dot. */
    return length >= 6U &&
        memcmp(path + length - 6U, "\xE3\x80\x82pdf", 6U) == 0;
}

static void app_handle_export_event(app_state *app, SDL_Event *event)
{
    char *path = (char *)event->user.data1;
    char *pdf_path = path;
    loim_status status;

    app->dialog_open = false;
    if (path == NULL) {
        return;
    }
    if (!app_pdf_path_has_extension(path)) {
        if (SDL_asprintf(&pdf_path, "%s.pdf", path) < 0) {
            pdf_path = NULL;
        }
    }
    if (pdf_path == NULL) {
        app_set_status(app, app_translation(app, LOIM_TEXT_EXPORT_FAILED));
        SDL_free(path);
        return;
    }
    status = app_export_pdf(
        app,
        pdf_path,
        app->export_columns,
        app->export_margin_ratio,
        app->export_licensed,
        app->export_show_page_numbers);
    if (status != LOIM_OK) {
        app_set_status(app, app_translation(app, LOIM_TEXT_EXPORT_FAILED));
    } else {
        char message[256];
        const char *layout_name = app_translation(
            app,
            app->export_columns == 1U
                ? LOIM_TEXT_ONE_COLUMN
                : app->export_columns == 2U
                    ? LOIM_TEXT_TWO_COLUMNS
                    : LOIM_TEXT_THREE_COLUMNS);
        int used;

        used = snprintf(
            message,
            sizeof(message),
            app_translation(app, LOIM_TEXT_EXPORT_SUCCESS_FORMAT),
            pdf_path);
        if (used >= 0 && (size_t)used < sizeof(message)) {
            (void)snprintf(
                message + (size_t)used,
                sizeof(message) - (size_t)used,
                " · %s · %zu",
                layout_name,
                loim_sheet_slot_count(app->export_columns));
        }
        app_set_status(app, message);
    }
    if (pdf_path != path) {
        SDL_free(pdf_path);
    }
    SDL_free(path);
}

static bool point_in_rect(float x, float y, const SDL_FRect *rect)
{
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static void app_destroy_images(app_state *app)
{
    size_t index;

    for (index = 0U; index < app->image_count; ++index) {
        app_destroy_texture_tiles(
            app->images[index].tiles,
            app->images[index].tile_count);
        SDL_free(app->images[index].path);
    }
    free(app->images);
    app->images = NULL;
    app->image_count = 0U;
    app->image_capacity = 0U;
}

static bool app_clear_document(app_state *app)
{
    loim_document *replacement = NULL;
    loim_status status = loim_document_create(&replacement);

    if (status != LOIM_OK) {
        app_set_status(app, app_translation(app, LOIM_TEXT_CLEAR_FAILED));
        return false;
    }
    loim_layout_destroy(&app->layout);
    app_destroy_images(app);
    loim_document_destroy(app->document);
    app->document = replacement;
    app->left_scroll_y = 0.0F;
    app->right_scroll_y = 0.0F;
    app_set_status(app, app_translation(app, LOIM_TEXT_READY));
    return true;
}

static void draw_filled_rect(
    SDL_Renderer *renderer,
    const SDL_FRect *rect,
    Uint8 red,
    Uint8 green,
    Uint8 blue,
    Uint8 alpha)
{
    (void)SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
    (void)SDL_RenderFillRect(renderer, rect);
}

typedef enum toolbar_action {
    TOOLBAR_LOGIN = 0,
    TOOLBAR_OPEN,
    TOOLBAR_EXPORT,
    TOOLBAR_PRINT,
    TOOLBAR_TWO_COLUMNS,
    TOOLBAR_PAGE_NUMBERS,
    TOOLBAR_AUTO_SPLIT,
    TOOLBAR_LESS_MARGIN,
    TOOLBAR_MORE_MARGIN,
    TOOLBAR_ZOOM_IN,
    TOOLBAR_ZOOM_OUT
} toolbar_action;

typedef struct toolbar_item {
    toolbar_action action;
    float x;
} toolbar_item;

static const toolbar_item toolbar_items[] = {
    {TOOLBAR_LOGIN, 18.0F},
    {TOOLBAR_OPEN, 68.0F},
    {TOOLBAR_EXPORT, 108.0F},
    {TOOLBAR_PRINT, 148.0F},
    {TOOLBAR_TWO_COLUMNS, 198.0F},
    {TOOLBAR_PAGE_NUMBERS, 238.0F},
    {TOOLBAR_AUTO_SPLIT, 288.0F},
    {TOOLBAR_LESS_MARGIN, 338.0F},
    {TOOLBAR_MORE_MARGIN, 378.0F},
    {TOOLBAR_ZOOM_IN, 428.0F},
    {TOOLBAR_ZOOM_OUT, 468.0F}
};

static void draw_line(
    SDL_Renderer *renderer,
    float x1,
    float y1,
    float x2,
    float y2)
{
    (void)SDL_RenderLine(renderer, x1, y1, x2, y2);
}

static void draw_circle(SDL_Renderer *renderer, int center_x, int center_y, int radius)
{
    int x = radius;
    int y = 0;
    int error = 1 - radius;

    while (x >= y) {
        (void)SDL_RenderPoint(renderer, (float)(center_x + x), (float)(center_y + y));
        (void)SDL_RenderPoint(renderer, (float)(center_x + y), (float)(center_y + x));
        (void)SDL_RenderPoint(renderer, (float)(center_x - y), (float)(center_y + x));
        (void)SDL_RenderPoint(renderer, (float)(center_x - x), (float)(center_y + y));
        (void)SDL_RenderPoint(renderer, (float)(center_x - x), (float)(center_y - y));
        (void)SDL_RenderPoint(renderer, (float)(center_x - y), (float)(center_y - x));
        (void)SDL_RenderPoint(renderer, (float)(center_x + y), (float)(center_y - x));
        (void)SDL_RenderPoint(renderer, (float)(center_x + x), (float)(center_y - y));
        y += 1;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            x -= 1;
            error += 2 * (y - x) + 1;
        }
    }
}

static bool toolbar_action_enabled(const app_state *app, toolbar_action action)
{
    return action == TOOLBAR_LOGIN || action == TOOLBAR_OPEN || app->image_count > 0U;
}

static bool toolbar_action_active(const app_state *app, toolbar_action action)
{
    return (action == TOOLBAR_LOGIN && app->logged_in) ||
           (action == TOOLBAR_TWO_COLUMNS && app->columns > 1U) ||
           (action == TOOLBAR_PAGE_NUMBERS && app->show_page_numbers);
}

static void draw_toolbar_icon(
    app_state *app,
    toolbar_action action,
    const SDL_FRect *button,
    bool enabled)
{
    float left = button->x + 7.0F;
    float top = button->y + 7.0F;
    float right = button->x + button->w - 7.0F;
    float bottom = button->y + button->h - 7.0F;
    float center_x = (left + right) / 2.0F;
    float center_y = (top + bottom) / 2.0F;
    SDL_FRect box = {left, top, right - left, bottom - top};
    SDL_FRect inner;
    Uint8 shade = enabled ? 105U : 167U;

    if (toolbar_action_active(app, action)) {
        (void)SDL_SetRenderDrawColor(app->renderer, 0U, 122U, 255U, 255U);
    } else {
        (void)SDL_SetRenderDrawColor(app->renderer, shade, shade, shade, 255U);
    }
    switch (action) {
    case TOOLBAR_LOGIN:
        draw_circle(app->renderer, (int)center_x, (int)(top + 7.0F), 5);
        draw_line(app->renderer, left + 3.0F, bottom, left + 3.0F, center_y + 4.0F);
        draw_line(app->renderer, left + 3.0F, center_y + 4.0F, center_x, center_y + 1.0F);
        draw_line(app->renderer, center_x, center_y + 1.0F, right - 3.0F, center_y + 4.0F);
        draw_line(app->renderer, right - 3.0F, center_y + 4.0F, right - 3.0F, bottom);
        break;
    case TOOLBAR_OPEN:
        (void)SDL_RenderRect(app->renderer, &box);
        draw_circle(app->renderer, (int)(right - 6.0F), (int)(top + 6.0F), 2);
        draw_line(app->renderer, left + 3.0F, bottom - 3.0F, center_x - 2.0F, center_y);
        draw_line(app->renderer, center_x - 2.0F, center_y, right - 3.0F, bottom - 3.0F);
        break;
    case TOOLBAR_EXPORT:
        draw_line(app->renderer, left, top, center_x, top);
        draw_line(app->renderer, left, top, left, bottom);
        draw_line(app->renderer, left, bottom, right, bottom);
        draw_line(app->renderer, right, center_y, right, bottom);
        draw_line(app->renderer, center_x - 2.0F, center_y + 2.0F, right, top);
        draw_line(app->renderer, right, top, right - 7.0F, top);
        draw_line(app->renderer, right, top, right, top + 7.0F);
        break;
    case TOOLBAR_PRINT:
        inner.x = left + 4.0F;
        inner.y = top;
        inner.w = box.w - 8.0F;
        inner.h = box.h;
        (void)SDL_RenderRect(app->renderer, &inner);
        box.y = top + 7.0F;
        box.h = 10.0F;
        (void)SDL_RenderRect(app->renderer, &box);
        break;
    case TOOLBAR_TWO_COLUMNS:
        (void)SDL_RenderRect(app->renderer, &box);
        if (app->columns >= 3U) {
            draw_line(app->renderer, center_x - 5.0F, top + 3.0F,
                      center_x - 5.0F, bottom - 3.0F);
            draw_line(app->renderer, center_x + 5.0F, top + 3.0F,
                      center_x + 5.0F, bottom - 3.0F);
        } else {
            draw_line(app->renderer, center_x, top + 3.0F, center_x, bottom - 3.0F);
        }
        break;
    case TOOLBAR_PAGE_NUMBERS:
        (void)SDL_RenderRect(app->renderer, &box);
        (void)SDL_RenderDebugText(app->renderer, center_x - 4.0F, center_y - 4.0F, "1");
        break;
    case TOOLBAR_AUTO_SPLIT:
        (void)SDL_RenderRect(app->renderer, &box);
        inner.x = left + 5.0F;
        inner.y = top + 5.0F;
        inner.w = box.w - 10.0F;
        inner.h = box.h - 10.0F;
        (void)SDL_RenderRect(app->renderer, &inner);
        draw_line(app->renderer, left - 2.0F, center_y, left + 5.0F, center_y);
        draw_line(app->renderer, right - 5.0F, center_y, right + 2.0F, center_y);
        break;
    case TOOLBAR_LESS_MARGIN:
    case TOOLBAR_MORE_MARGIN:
        (void)SDL_RenderRect(app->renderer, &box);
        inner.w = action == TOOLBAR_LESS_MARGIN ? box.w * 0.68F : box.w * 0.40F;
        inner.h = action == TOOLBAR_LESS_MARGIN ? box.h * 0.68F : box.h * 0.40F;
        inner.x = center_x - inner.w / 2.0F;
        inner.y = center_y - inner.h / 2.0F;
        (void)SDL_RenderRect(app->renderer, &inner);
        break;
    case TOOLBAR_ZOOM_IN:
    case TOOLBAR_ZOOM_OUT:
        (void)SDL_RenderRect(app->renderer, &box);
        draw_line(app->renderer, center_x - 6.0F, center_y, center_x + 6.0F, center_y);
        if (action == TOOLBAR_ZOOM_IN) {
            draw_line(app->renderer, center_x, center_y - 6.0F, center_x, center_y + 6.0F);
        }
        break;
    }
}

static void app_render_toolbar(app_state *app, float width)
{
    SDL_FRect bar = {0.0F, 0.0F, width, LOIM_TOOLBAR_HEIGHT};
    size_t index;
    static const float separators[] = {58.0F, 188.0F, 278.0F, 328.0F, 418.0F};

    draw_filled_rect(app->renderer, &bar, 218U, 218U, 218U, 255U);
    for (index = 0U; index < SDL_arraysize(separators); ++index) {
        (void)SDL_SetRenderDrawColor(app->renderer, 174U, 174U, 174U, 255U);
        draw_line(app->renderer, separators[index], 10.0F, separators[index], 48.0F);
    }
    for (index = 0U; index < SDL_arraysize(toolbar_items); ++index) {
        SDL_FRect button = {toolbar_items[index].x, 9.0F, 34.0F, 40.0F};
        bool hovered = point_in_rect(app->mouse_x, app->mouse_y, &button);
        bool enabled = toolbar_action_enabled(app, toolbar_items[index].action);

        if (hovered && enabled) {
            draw_filled_rect(app->renderer, &button, 198U, 198U, 198U, 255U);
        }
        draw_toolbar_icon(app, toolbar_items[index].action, &button, enabled);
    }
    (void)SDL_SetRenderDrawColor(app->renderer, 188U, 188U, 188U, 255U);
    draw_line(app->renderer, 0.0F, LOIM_TOOLBAR_HEIGHT - 1.0F, width, LOIM_TOOLBAR_HEIGHT - 1.0F);
}

static void app_render_image_slice(
    app_state *app,
    const app_image *image,
    uint32_t source_y_px,
    uint32_t source_height_px,
    const SDL_FRect *destination)
{
    uint64_t slice_start = source_y_px;
    uint64_t slice_end = slice_start + source_height_px;
    size_t index;

    if (source_height_px == 0U) {
        return;
    }
    for (index = 0U; index < image->tile_count; ++index) {
        const app_texture_tile *tile = &image->tiles[index];
        uint64_t tile_start = tile->source_y_px;
        uint64_t tile_end = tile_start + tile->height_px;
        uint64_t overlap_start = slice_start > tile_start ? slice_start : tile_start;
        uint64_t overlap_end = slice_end < tile_end ? slice_end : tile_end;
        float relative_start;
        float relative_height;
        SDL_FRect source;
        SDL_FRect tile_destination;

        if (overlap_start >= overlap_end) {
            continue;
        }
        relative_start = (float)(overlap_start - slice_start) /
            (float)source_height_px;
        relative_height = (float)(overlap_end - overlap_start) /
            (float)source_height_px;
        source.x = 0.0F;
        source.y = (float)(overlap_start - tile_start);
        source.w = (float)image->width;
        source.h = (float)(overlap_end - overlap_start);
        tile_destination.x = destination->x;
        tile_destination.y = destination->y + destination->h * relative_start;
        tile_destination.w = destination->w;
        tile_destination.h = destination->h * relative_height;
        (void)SDL_RenderTexture(
            app->renderer,
            tile->texture,
            &source,
            &tile_destination);
    }
}

static void app_render_page_grid_tile(
    app_state *app,
    size_t slice_index,
    const SDL_FRect *cell,
    size_t columns,
    size_t column,
    size_t row)
{
    size_t slots = loim_sheet_slot_count(columns);
    size_t sheet_index = slice_index / slots;
    size_t slot = slice_index % slots;
    const loim_page_slice *slice = app_slice_at(app, slice_index);
    SDL_FRect destination;

    if (slice == NULL || !app_grid_destination(
            app,
            sheet_index,
            columns,
            slot,
            cell->x,
            cell->y - (float)row * cell->h,
            cell->w,
            cell->h * (float)columns,
            &destination)) {
        return;
    }
    {
        const app_image *image = &app->images[slice->source_index];
        SDL_Rect clip = {
            (int)cell->x,
            (int)(cell->y - (float)row * cell->h),
            (int)cell->w,
            (int)(cell->h * (float)columns)};

        (void)SDL_SetRenderClipRect(app->renderer, &clip);
        app_render_image_slice(
            app,
            image,
            slice->source_y_px,
            slice->source_height_px,
            &destination);
    }
    (void)column;
}

static void clamp_scroll(float *scroll, float content_height, float viewport_height)
{
    float maximum = content_height > viewport_height
        ? content_height - viewport_height + 18.0F
        : 0.0F;

    if (*scroll < 0.0F) {
        *scroll = 0.0F;
    } else if (*scroll > maximum) {
        *scroll = maximum;
    }
}

static void app_render_preview(
    app_state *app,
    float left,
    float top,
    float width,
    float bottom)
{
    size_t columns = loim_columns_normalize(app->columns);
    size_t pages_per_sheet = loim_sheet_slot_count(columns);
    size_t slice_count = app->layout.slice_count;
    size_t sheet_count = loim_sheet_count(slice_count, columns);
    float paper_width = width * 0.74F;
    float paper_height;
    float viewport_height = bottom - top;
    float total_height;
    float y;
    size_t sheet_index;
    SDL_Rect clip = {(int)left, (int)top, (int)width, (int)viewport_height};

    if (paper_width > 760.0F) {
        paper_width = 760.0F;
    }
    if (paper_width < 90.0F) {
        paper_width = 90.0F;
    }
    paper_height = paper_width * 297.0F / 210.0F;
    total_height = 24.0F + (float)sheet_count * (paper_height + 24.0F);
    clamp_scroll(&app->left_scroll_y, total_height, viewport_height);
    y = top + 16.0F - app->left_scroll_y;
    (void)SDL_SetRenderClipRect(app->renderer, &clip);

    for (sheet_index = 0U; sheet_index < sheet_count; ++sheet_index) {
        float x = left + (width - paper_width) / 2.0F;
        SDL_FRect shadow = {x + 3.0F, y + 4.0F, paper_width, paper_height};
        SDL_FRect paper = {x, y, paper_width, paper_height};
        float margin_x = paper_width * app->margin_ratio;
        float margin_y = paper_height * app->margin_ratio;
        SDL_FRect content = {
            x + margin_x,
            y + margin_y,
            paper_width - margin_x * 2.0F,
            paper_height - margin_y * 2.0F
        };
        size_t slot;

        if (y + paper_height >= top && y <= bottom) {
            draw_filled_rect(app->renderer, &shadow, 100U, 100U, 100U, 90U);
            draw_filled_rect(app->renderer, &paper, 255U, 255U, 255U, 255U);
            for (slot = 0U; slot < pages_per_sheet; ++slot) {
                size_t page_index = loim_sheet_slice_index(sheet_index, columns, slot);
                SDL_FRect cell = content;

                if (page_index >= slice_count) {
                    break;
                }
                {
                    float gap = paper_width * LOIM_GRID_GAP_RATIO;
                    size_t column = loim_sheet_slot_column(columns, slot);
                    size_t row = loim_sheet_slot_row(columns, slot);

                    cell.w = (content.w - gap * (float)(columns - 1U)) /
                        (float)columns;
                    cell.h = content.h / (float)columns;
                    cell.x = x + margin_x + (float)column * (cell.w + gap);
                    cell.y = y + margin_y + (float)row * cell.h;
                    app_render_page_grid_tile(
                        app, page_index, &cell, columns, column, row);
                    (void)SDL_SetRenderClipRect(app->renderer, &clip);
                }
            }
            if (app->show_page_numbers) {
                char number[24];

                (void)snprintf(number, sizeof(number), "%zu", sheet_index + 1U);
                (void)SDL_SetRenderDrawColor(app->renderer, 120U, 120U, 120U, 255U);
                (void)SDL_RenderDebugText(
                    app->renderer,
                    paper.x + paper.w - 22.0F,
                    paper.y + paper.h - 16.0F,
                    number);
            }
        }
        y += paper_height + 24.0F;
    }
    (void)SDL_SetRenderClipRect(app->renderer, NULL);
}

static float editor_scale(const app_state *app, float width)
{
    float scale = width * 0.78F / (float)LOIM_LAYOUT_WIDTH * app->zoom;

    if (scale < 0.06F) {
        scale = 0.06F;
    } else if (scale > 2.4F) {
        scale = 2.4F;
    }
    return scale;
}

static float editor_total_height(const app_state *app, float scale)
{
    float height = 32.0F;
    size_t index;

    for (index = 0U; index < app->layout.page_count; ++index) {
        height += (float)app->layout.pages[index].height_px * scale;
        if (index + 1U < app->layout.page_count) {
            height += LOIM_SPLIT_HANDLE_HEIGHT;
        }
    }
    return height;
}

static SDL_FRect editor_split_rect(
    const app_state *app,
    size_t split_index,
    float left,
    float top,
    float width)
{
    float scale = editor_scale(app, width);
    float canvas_width = (float)LOIM_LAYOUT_WIDTH * scale;
    float y = top + 16.0F - app->right_scroll_y;
    size_t index;
    SDL_FRect rect;

    for (index = 0U; index <= split_index; ++index) {
        y += (float)app->layout.pages[index].height_px * scale;
        if (index < split_index) {
            y += LOIM_SPLIT_HANDLE_HEIGHT;
        }
    }
    rect.x = left + (width - canvas_width) / 2.0F;
    rect.y = y;
    rect.w = canvas_width;
    rect.h = LOIM_SPLIT_HANDLE_HEIGHT;
    return rect;
}

static size_t editor_split_at(
    const app_state *app,
    float x,
    float y,
    float left,
    float top,
    float width)
{
    size_t index;

    for (index = 0U; index + 1U < app->layout.page_count; ++index) {
        SDL_FRect rect = editor_split_rect(app, index, left, top, width);

        if (point_in_rect(x, y, &rect)) {
            return index;
        }
    }
    return SIZE_MAX;
}

static void app_render_editor(
    app_state *app,
    float left,
    float top,
    float width,
    float bottom)
{
    float scale = editor_scale(app, width);
    float canvas_width = (float)LOIM_LAYOUT_WIDTH * scale;
    float viewport_height = bottom - top;
    float canvas_x = left + (width - canvas_width) / 2.0F;
    float y;
    size_t page_index;
    SDL_Rect clip = {(int)left, (int)top, (int)width, (int)viewport_height};

    clamp_scroll(
        &app->right_scroll_y,
        editor_total_height(app, scale),
        viewport_height);
    y = top + 16.0F - app->right_scroll_y;
    (void)SDL_SetRenderClipRect(app->renderer, &clip);
    for (page_index = 0U; page_index < app->layout.page_count; ++page_index) {
        const loim_page *page = &app->layout.pages[page_index];
        float page_height = (float)page->height_px * scale;
        SDL_FRect page_rect = {canvas_x, y, canvas_width, page_height};
        size_t offset;

        if (y + page_height >= top && y <= bottom) {
            draw_filled_rect(app->renderer, &page_rect, 255U, 255U, 255U, 255U);
            for (offset = 0U; offset < page->slice_count; ++offset) {
                const loim_page_slice *slice =
                    &app->layout.slices[page->first_slice + offset];
                const app_image *image = &app->images[slice->source_index];
                SDL_FRect destination = {
                    canvas_x,
                    y + (float)slice->destination_y_px * scale,
                    canvas_width,
                    (float)slice->destination_height_px * scale
                };

                app_render_image_slice(
                    app,
                    image,
                    slice->source_y_px,
                    slice->source_height_px,
                    &destination);
            }
        }
        y += page_height;
        if (page_index + 1U < app->layout.page_count) {
            SDL_FRect handle = {canvas_x, y, canvas_width, LOIM_SPLIT_HANDLE_HEIGHT};
            bool active = app->split_drag_index == page_index;
            bool hovered = point_in_rect(app->mouse_x, app->mouse_y, &handle);
            float handle_y = active
                ? y + app->split_drag_current_y - app->split_drag_start_y
                : y;
            SDL_FRect painted = {handle.x, handle_y, handle.w, handle.h};
            float center_y = painted.y + painted.h / 2.0F;
            int dot;

            if (active) {
                draw_filled_rect(app->renderer, &painted, 0U, 102U, 204U, 225U);
            } else if (hovered) {
                draw_filled_rect(app->renderer, &painted, 74U, 144U, 226U, 120U);
            } else {
                draw_filled_rect(app->renderer, &painted, 132U, 132U, 132U, 255U);
            }
            (void)SDL_SetRenderDrawColor(app->renderer, 220U, 220U, 220U, 220U);
            draw_line(app->renderer, painted.x, center_y, painted.x + painted.w, center_y);
            for (dot = -2; dot <= 2; ++dot) {
                SDL_FRect marker = {
                    painted.x + painted.w / 2.0F + (float)dot * 5.0F - 1.0F,
                    center_y - 1.0F,
                    2.0F,
                    2.0F
                };

                (void)SDL_RenderFillRect(app->renderer, &marker);
            }
            y += LOIM_SPLIT_HANDLE_HEIGHT;
        }
    }
    (void)SDL_SetRenderClipRect(app->renderer, NULL);
}

static void app_render_divider(app_state *app, float x, float top, float bottom)
{
    SDL_FRect divider = {x - LOIM_DIVIDER_WIDTH / 2.0F, top,
                         LOIM_DIVIDER_WIDTH, bottom - top};
    bool hovered = point_in_rect(app->mouse_x, app->mouse_y, &divider);
    int row;

    if (app->divider_dragging) {
        draw_filled_rect(app->renderer, &divider, 0U, 102U, 204U, 220U);
    } else if (hovered) {
        draw_filled_rect(app->renderer, &divider, 74U, 144U, 226U, 120U);
    } else {
        draw_filled_rect(app->renderer, &divider, 128U, 128U, 128U, 255U);
    }
    (void)SDL_SetRenderDrawColor(app->renderer, 220U, 220U, 220U, 180U);
    for (row = -1; row <= 1; ++row) {
        (void)SDL_RenderPoint(app->renderer, x - 1.0F, (top + bottom) / 2.0F + (float)row * 5.0F);
        (void)SDL_RenderPoint(app->renderer, x + 1.0F, (top + bottom) / 2.0F + (float)row * 5.0F);
    }
}

static void app_render_statusbar(app_state *app, float width, float height)
{
    SDL_FRect bar = {0.0F, height - LOIM_STATUSBAR_HEIGHT, width, LOIM_STATUSBAR_HEIGHT};
    char version[48];

    draw_filled_rect(app->renderer, &bar, 247U, 247U, 247U, 255U);
    (void)SDL_SetRenderDrawColor(app->renderer, 195U, 195U, 195U, 255U);
    draw_line(app->renderer, 0.0F, bar.y, width, bar.y);
    (void)app_draw_text(
        app, 8.0F, bar.y + 2.0F, app->status, 102U, 102U, 102U, 255U);
    (void)snprintf(version, sizeof(version), "v%s", LOIM_APP_VERSION);
    (void)app_draw_text(
        app,
        width - app_text_width(app, version) - 8.0F,
        bar.y + 2.0F,
        version,
        102U,
        102U,
        102U,
        255U);
    if (app->import_progress_active && app->import_pending.count > 0U) {
        unsigned percent = loim_progress_percent(
            app->import_pending_index, app->import_pending.count);
        SDL_FRect progress = {
            0.0F,
            bar.y + bar.h - 4.0F,
            width * (float)percent / 100.0F,
            4.0F
        };

        draw_filled_rect(app->renderer, &progress, 0U, 122U, 255U, 255U);
    }
}

static void app_render_empty(app_state *app, float left, float top, float width, float bottom)
{
    const char *label = app_translation(app, LOIM_TEXT_DROP_IMAGES);
    float label_width = app_text_width(app, label);

    (void)app_draw_text(
        app,
        left + (width - label_width) / 2.0F,
        top + (bottom - top) / 2.0F - 8.0F,
        label,
        226U,
        226U,
        226U,
        255U);
}

static app_login_layout app_login_layout_for(float width, float height)
{
    app_login_layout layout;

    layout.panel.w = 520.0F;
    layout.panel.h = 310.0F;
    layout.panel.x = (width - layout.panel.w) / 2.0F;
    layout.panel.y = (height - layout.panel.h) / 2.0F;
    layout.email.x = layout.panel.x + 30.0F;
    layout.email.y = layout.panel.y + 82.0F;
    layout.email.w = layout.panel.w - 60.0F;
    layout.email.h = 38.0F;
    layout.password = layout.email;
    layout.password.y += 72.0F;
    layout.cancel.x = layout.panel.x + layout.panel.w - 220.0F;
    layout.cancel.y = layout.panel.y + layout.panel.h - 54.0F;
    layout.cancel.w = 88.0F;
    layout.cancel.h = 32.0F;
    layout.submit = layout.cancel;
    layout.submit.x += 100.0F;
    layout.submit.w = 102.0F;
    return layout;
}

static void app_render_login_field(
    app_state *app,
    const SDL_FRect *field,
    const char *text,
    bool focused,
    bool password)
{
    char masked[56];
    const char *display = text;
    size_t text_length = strlen(text);
    size_t visible_characters = (size_t)((field->w - 20.0F) / 8.0F);

    draw_filled_rect(app->renderer, field, 255U, 255U, 255U, 255U);
    if (password) {
        size_t count = text_length;

        if (count >= sizeof(masked)) {
            count = sizeof(masked) - 1U;
        }
        memset(masked, '*', count);
        masked[count] = '\0';
        display = masked;
    } else if (text_length > visible_characters) {
        display = text + text_length - visible_characters;
    }
    if (focused) {
        (void)SDL_SetRenderDrawColor(app->renderer, 0U, 122U, 255U, 255U);
    } else {
        (void)SDL_SetRenderDrawColor(app->renderer, 168U, 168U, 168U, 255U);
    }
    (void)SDL_RenderRect(app->renderer, field);
    if (focused) {
        SDL_FRect inner = {
            field->x + 1.0F,
            field->y + 1.0F,
            field->w - 2.0F,
            field->h - 2.0F
        };

        (void)SDL_RenderRect(app->renderer, &inner);
    }
    (void)app_draw_text(
        app,
        field->x + 10.0F,
        field->y + 10.0F,
        display,
        45U,
        45U,
        45U,
        255U);
}

static void app_render_login(app_state *app, float width, float height)
{
    app_login_layout layout = app_login_layout_for(width, height);
    SDL_FRect overlay = {0.0F, 0.0F, width, height};
    const char *submit_label = app->login_submitting
        ? app_translation(app, LOIM_TEXT_PLEASE_WAIT)
        : app_translation(app, LOIM_TEXT_SIGN_IN);

    draw_filled_rect(app->renderer, &overlay, 24U, 24U, 24U, 150U);
    draw_filled_rect(app->renderer, &layout.panel, 248U, 248U, 248U, 255U);
    (void)SDL_SetRenderDrawColor(app->renderer, 105U, 105U, 105U, 255U);
    (void)SDL_RenderRect(app->renderer, &layout.panel);
    (void)app_draw_text(
        app,
        layout.panel.x + 30.0F,
        layout.panel.y + 24.0F,
        app_translation(app, LOIM_TEXT_SIGN_IN_TITLE),
        32U,
        32U,
        32U,
        255U);
    (void)app_draw_text(
        app,
        layout.email.x,
        layout.email.y - 16.0F,
        app_translation(app, LOIM_TEXT_EMAIL),
        90U,
        90U,
        90U,
        255U);
    (void)app_draw_text(
        app,
        layout.password.x,
        layout.password.y - 16.0F,
        app_translation(app, LOIM_TEXT_PASSWORD),
        90U,
        90U,
        90U,
        255U);
    app_render_login_field(
        app,
        &layout.email,
        app->login_email,
        app->login_focus == APP_LOGIN_EMAIL,
        false);
    app_render_login_field(
        app,
        &layout.password,
        app->login_password,
        app->login_focus == APP_LOGIN_PASSWORD,
        true);
    (void)app_draw_text(
        app,
        layout.panel.x + 30.0F,
        layout.panel.y + 211.0F,
        app->login_message,
        100U,
        100U,
        100U,
        255U);
    draw_filled_rect(app->renderer, &layout.cancel, 225U, 225U, 225U, 255U);
    (void)SDL_SetRenderDrawColor(app->renderer, 115U, 115U, 115U, 255U);
    (void)SDL_RenderRect(app->renderer, &layout.cancel);
    (void)app_draw_text(
        app,
        layout.cancel.x + 20.0F,
        layout.cancel.y + 8.0F,
        app_translation(app, LOIM_TEXT_CANCEL),
        80U,
        80U,
        80U,
        255U);
    draw_filled_rect(
        app->renderer,
        &layout.submit,
        app->login_submitting ? 150U : 0U,
        app->login_submitting ? 150U : 122U,
        app->login_submitting ? 150U : 255U,
        255U);
    (void)app_draw_text(
        app,
        layout.submit.x + (layout.submit.w - app_text_width(app, submit_label)) / 2.0F,
        layout.submit.y + 8.0F,
        submit_label,
        255U,
        255U,
        255U,
        255U);
}

static void app_render(app_state *app)
{
    int output_width = 0;
    int output_height = 0;
    float width;
    float height;
    float content_bottom;
    float divider_x;
    float left_width;
    float right_left;
    float right_width;
    SDL_FRect content;

    if (!SDL_GetRenderOutputSize(app->renderer, &output_width, &output_height)) {
        return;
    }
    width = (float)output_width;
    height = (float)output_height;
    content_bottom = height - LOIM_STATUSBAR_HEIGHT;
    divider_x = width * app->split_ratio;
    left_width = divider_x - LOIM_DIVIDER_WIDTH / 2.0F;
    right_left = divider_x + LOIM_DIVIDER_WIDTH / 2.0F;
    right_width = width - right_left;
    (void)SDL_SetRenderDrawColor(app->renderer, 150U, 150U, 150U, 255U);
    (void)SDL_RenderClear(app->renderer);
    content.x = 0.0F;
    content.y = LOIM_TOOLBAR_HEIGHT;
    content.w = width;
    content.h = content_bottom - LOIM_TOOLBAR_HEIGHT;
    draw_filled_rect(app->renderer, &content, 150U, 150U, 150U, 255U);
    if (app->image_count == 0U) {
        app_render_empty(app, 0.0F, LOIM_TOOLBAR_HEIGHT, left_width, content_bottom);
        app_render_empty(app, right_left, LOIM_TOOLBAR_HEIGHT, right_width, content_bottom);
    } else {
        app_render_preview(
            app, 0.0F, LOIM_TOOLBAR_HEIGHT, left_width, content_bottom);
        app_render_editor(
            app, right_left, LOIM_TOOLBAR_HEIGHT, right_width, content_bottom);
    }
    /* Keep the toolbar above the scrolling canvas so pages can never cover it. */
    app_render_toolbar(app, width);
    if (app->dropped.collecting) {
        SDL_FRect drop_border = {
            5.0F,
            LOIM_TOOLBAR_HEIGHT + 5.0F,
            width - 10.0F,
            content_bottom - LOIM_TOOLBAR_HEIGHT - 10.0F
        };

        (void)SDL_SetRenderDrawColor(app->renderer, 0U, 122U, 255U, 255U);
        (void)SDL_RenderRect(app->renderer, &drop_border);
        drop_border.x += 2.0F;
        drop_border.y += 2.0F;
        drop_border.w -= 4.0F;
        drop_border.h -= 4.0F;
        (void)SDL_RenderRect(app->renderer, &drop_border);
    }
    app_render_divider(app, divider_x, LOIM_TOOLBAR_HEIGHT, content_bottom);
    app_render_statusbar(app, width, height);
    if (app->login_open) {
        app_render_login(app, width, height);
    }
    (void)SDL_RenderPresent(app->renderer);
}

static void app_adjust_zoom(app_state *app, float delta)
{
    app->zoom += delta;
    if (app->zoom < 0.35F) {
        app->zoom = 0.35F;
    } else if (app->zoom > 1.6F) {
        app->zoom = 1.6F;
    }
}

static void app_handle_click(app_state *app, float x, float y)
{
    size_t index;

    if (y > LOIM_TOOLBAR_HEIGHT) {
        return;
    }
    for (index = 0U; index < SDL_arraysize(toolbar_items); ++index) {
        SDL_FRect button = {toolbar_items[index].x, 9.0F, 34.0F, 40.0F};
        toolbar_action action = toolbar_items[index].action;

        if (!point_in_rect(x, y, &button) || !toolbar_action_enabled(app, action)) {
            continue;
        }
        switch (action) {
        case TOOLBAR_OPEN:
            app_open_dialog(app);
            break;
        case TOOLBAR_TWO_COLUMNS:
            app->columns = loim_columns_next(app->columns);
            app->left_scroll_y = 0.0F;
            app_set_status(app, app_translation(
                app,
                app->columns == 1U
                    ? LOIM_TEXT_ONE_COLUMN
                    : app->columns == 2U
                        ? LOIM_TEXT_TWO_COLUMNS
                        : LOIM_TEXT_THREE_COLUMNS));
            break;
        case TOOLBAR_PAGE_NUMBERS:
            app->show_page_numbers = !app->show_page_numbers;
            app_set_status(
                app,
                app_translation(
                    app,
                    app->show_page_numbers
                        ? LOIM_TEXT_PAGE_NUMBERS_ON
                        : LOIM_TEXT_PAGE_NUMBERS_OFF));
            break;
        case TOOLBAR_AUTO_SPLIT:
            (void)app_rebuild_layout(app);
            app_set_status(app, app_translation(app, LOIM_TEXT_AUTO_SPLIT_APPLIED));
            break;
        case TOOLBAR_LESS_MARGIN:
            app->margin_ratio -= 0.01F;
            if (app->margin_ratio < 0.02F) {
                app->margin_ratio = 0.02F;
            }
            app_set_status(app, app_translation(app, LOIM_TEXT_MARGIN_REDUCED));
            break;
        case TOOLBAR_MORE_MARGIN:
            app->margin_ratio += 0.01F;
            if (app->margin_ratio > 0.22F) {
                app->margin_ratio = 0.22F;
            }
            app_set_status(app, app_translation(app, LOIM_TEXT_MARGIN_INCREASED));
            break;
        case TOOLBAR_ZOOM_IN:
            app_adjust_zoom(app, 0.1F);
            app_set_status(app, app_translation(app, LOIM_TEXT_ZOOMED_IN));
            break;
        case TOOLBAR_ZOOM_OUT:
            app_adjust_zoom(app, -0.1F);
            app_set_status(app, app_translation(app, LOIM_TEXT_ZOOMED_OUT));
            break;
        case TOOLBAR_LOGIN:
            app_open_login(app);
            break;
        case TOOLBAR_EXPORT:
            app_open_export_dialog(app);
            break;
        case TOOLBAR_PRINT:
            app_start_print(app);
            break;
        }
        return;
    }
}

static void app_handle_dialog_event(app_state *app, SDL_Event *event)
{
    path_batch *batch = (path_batch *)event->user.data1;

    app->dialog_open = false;
    if (batch != NULL) {
        app_import_batch(app, batch);
        path_batch_destroy(batch);
        SDL_free(batch);
    }
    if (app->quit_after_dialog) {
        app->running = false;
    }
}

static void app_commit_split_drag(app_state *app, float editor_width)
{
    const loim_page *page;
    float scale;
    double candidate;
    uint64_t virtual_row;
    uint64_t cursor = 0U;
    size_t source_index;
    loim_status status = LOIM_ERROR_NOT_FOUND;

    if (app->split_drag_index == SIZE_MAX ||
        app->split_drag_index + 1U >= app->layout.page_count) {
        return;
    }
    page = &app->layout.pages[app->split_drag_index];
    scale = editor_scale(app, editor_width);
    candidate = (double)page->virtual_y_px + (double)page->height_px +
        (double)(app->split_drag_current_y - app->split_drag_start_y) / (double)scale;
    if (candidate < 1.0 || candidate >= (double)app->layout.virtual_height_px) {
        app_set_status(app, app_translation(app, LOIM_TEXT_SPLIT_OUTSIDE));
        return;
    }
    virtual_row = (uint64_t)(candidate + 0.5);
    for (source_index = 0U; source_index < app->image_count; ++source_index) {
        const app_image *image = &app->images[source_index];
        uint64_t normalized_height =
            ((uint64_t)image->height * (uint64_t)LOIM_LAYOUT_WIDTH +
             (uint64_t)image->width - 1U) /
            (uint64_t)image->width;
        uint64_t source_end = cursor + normalized_height;

        if (virtual_row > cursor && virtual_row < source_end) {
            uint64_t source_row =
                ((virtual_row - cursor) * (uint64_t)image->height +
                 normalized_height / 2U) /
                normalized_height;

            if (source_row == 0U) {
                source_row = 1U;
            } else if (source_row >= (uint64_t)image->height) {
                source_row = (uint64_t)image->height - 1U;
            }
            status = loim_document_add_split_hint(
                app->document,
                source_index,
                (uint32_t)source_row,
                1.0F,
                LOIM_SPLIT_HINT_MANUAL);
            break;
        }
        cursor = source_end;
        if (source_index + 1U < app->image_count) {
            cursor += (uint64_t)app->layout_options.inter_image_gap_px;
        }
    }
    if (status == LOIM_OK) {
        float preview_scroll = app->left_scroll_y;
        float editor_scroll = app->right_scroll_y;

        status = app_rebuild_layout(app);
        app->left_scroll_y = preview_scroll;
        app->right_scroll_y = editor_scroll;
    }
    app_set_status(
        app,
        app_translation(
            app,
            status == LOIM_OK ? LOIM_TEXT_SPLIT_UPDATED : LOIM_TEXT_SPLIT_FAILED));
}

static void app_import_drop_queue(app_state *app)
{
    path_batch batch;

    memset(&batch, 0, sizeof(batch));
    batch.paths = app->dropped.paths;
    batch.count = app->dropped.count;
    batch.capacity = app->dropped.capacity;
    app_import_batch(app, &batch);
}

static void app_login_backspace(char *text)
{
    size_t length = strlen(text);

    if (length == 0U) {
        return;
    }
    length -= 1U;
    while (length > 0U && ((unsigned char)text[length] & 0xC0U) == 0x80U) {
        length -= 1U;
    }
    text[length] = '\0';
}

static void app_login_append_text(
    char *destination,
    size_t capacity,
    const char *source,
    bool email)
{
    const unsigned char *cursor = (const unsigned char *)source;
    size_t length = strlen(destination);

    while (*cursor != 0U) {
        size_t sequence_length = 1U;
        size_t offset;
        bool valid = true;

        if (*cursor < 0x20U || *cursor == 0x7FU || (email && *cursor == ' ')) {
            cursor += 1;
            continue;
        }
        if (*cursor >= 0xC2U && *cursor <= 0xDFU) {
            sequence_length = 2U;
        } else if (*cursor >= 0xE0U && *cursor <= 0xEFU) {
            sequence_length = 3U;
        } else if (*cursor >= 0xF0U && *cursor <= 0xF4U) {
            sequence_length = 4U;
        } else if (*cursor >= 0x80U) {
            valid = false;
        }
        for (offset = 1U; valid && offset < sequence_length; ++offset) {
            if (cursor[offset] == 0U ||
                (cursor[offset] & 0xC0U) != 0x80U) {
                valid = false;
            }
        }
        if (!valid) {
            cursor += 1;
            continue;
        }
        if (length >= capacity || sequence_length >= capacity ||
            length > capacity - 1U - sequence_length) {
            break;
        }
        memcpy(destination + length, cursor, sequence_length);
        length += sequence_length;
        destination[length] = '\0';
        cursor += sequence_length;
    }
}

static bool app_handle_login_event(app_state *app, SDL_Event *event)
{
    char *field = app->login_focus == APP_LOGIN_EMAIL
        ? app->login_email
        : app->login_password;
    size_t capacity = app->login_focus == APP_LOGIN_EMAIL
        ? sizeof(app->login_email)
        : sizeof(app->login_password);

    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        app->mouse_x = event->motion.x;
        app->mouse_y = event->motion.y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            int output_width = 0;
            int output_height = 0;

            app->mouse_x = event->button.x;
            app->mouse_y = event->button.y;
            if (SDL_GetRenderOutputSize(
                    app->renderer, &output_width, &output_height)) {
                app_login_layout layout = app_login_layout_for(
                    (float)output_width, (float)output_height);

                if (point_in_rect(app->mouse_x, app->mouse_y, &layout.email)) {
                    app->login_focus = APP_LOGIN_EMAIL;
                } else if (point_in_rect(
                               app->mouse_x, app->mouse_y, &layout.password)) {
                    app->login_focus = APP_LOGIN_PASSWORD;
                } else if (point_in_rect(
                               app->mouse_x, app->mouse_y, &layout.cancel)) {
                    app_close_login(app);
                } else if (point_in_rect(
                               app->mouse_x, app->mouse_y, &layout.submit)) {
                    app_submit_login(app);
                }
            }
        }
        break;
    case SDL_EVENT_TEXT_INPUT:
        if (!app->login_submitting) {
            app_login_append_text(
                field,
                capacity,
                event->text.text,
                app->login_focus == APP_LOGIN_EMAIL);
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            app_close_login(app);
        } else if (!app->login_submitting && event->key.key == SDLK_TAB) {
            app->login_focus = app->login_focus == APP_LOGIN_EMAIL
                ? APP_LOGIN_PASSWORD
                : APP_LOGIN_EMAIL;
        } else if (!app->login_submitting &&
                   (event->key.key == SDLK_RETURN ||
                    event->key.key == SDLK_KP_ENTER)) {
            app_submit_login(app);
        } else if (!app->login_submitting &&
                   event->key.key == SDLK_BACKSPACE) {
            app_login_backspace(field);
        } else if (!app->login_submitting &&
                   (event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0U &&
                   event->key.key == SDLK_V) {
            char *clipboard = SDL_GetClipboardText();

            if (clipboard != NULL) {
                app_login_append_text(
                    field,
                    capacity,
                    clipboard,
                    app->login_focus == APP_LOGIN_EMAIL);
                SDL_free(clipboard);
            }
        }
        break;
    default:
        break;
    }
    return event->type != SDL_EVENT_QUIT;
}

static void app_handle_event(app_state *app, SDL_Event *event)
{
    if (event->type == app->dialog_event) {
        app_handle_dialog_event(app, event);
        return;
    }
    if (event->type == app->import_event) {
        app_handle_import_event(app, event);
        return;
    }
    if (event->type == app->auth_event) {
        app_handle_auth_event(app, event);
        return;
    }
    if (event->type == app->export_event) {
        app_handle_export_event(app, event);
        return;
    }
    (void)SDL_ConvertEventToRenderCoordinates(app->renderer, event);
    if (app->login_open && app_handle_login_event(app, event)) {
        return;
    }
    switch (event->type) {
    case SDL_EVENT_QUIT:
        if (app->dialog_open) {
            app->quit_after_dialog = true;
            (void)SDL_HideWindow(app->window);
        } else {
            app->running = false;
        }
        break;
    case SDL_EVENT_LOCALE_CHANGED:
    {
        loim_locale locale = app_system_locale();

        if (locale != app->locale) {
            app->locale = locale;
            app_text_cache_clear(app);
            if (app->login_open) {
                if (app->login_submitting) {
                    app_copy_text(
                        app->login_message,
                        sizeof(app->login_message),
                        app_translation(app, LOIM_TEXT_SIGNING_IN));
                } else {
                    app_open_login(app);
                }
            }
            app_set_status(app, app_translation(app, LOIM_TEXT_READY));
        }
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
        app->mouse_x = event->motion.x;
        app->mouse_y = event->motion.y;
        if (app->divider_dragging) {
            int output_width = 0;
            int output_height = 0;

            if (SDL_GetRenderOutputSize(app->renderer, &output_width, &output_height) &&
                output_width > 0) {
                app->split_ratio = app->mouse_x / (float)output_width;
                if (app->split_ratio < 0.18F) {
                    app->split_ratio = 0.18F;
                } else if (app->split_ratio > 0.82F) {
                    app->split_ratio = 0.82F;
                }
            }
        }
        if (app->split_drag_index != SIZE_MAX) {
            app->split_drag_current_y = app->mouse_y;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            int output_width = 0;
            int output_height = 0;

            app->mouse_x = event->button.x;
            app->mouse_y = event->button.y;
            if (SDL_GetRenderOutputSize(app->renderer, &output_width, &output_height)) {
                float width = (float)output_width;
                float divider_x = width * app->split_ratio;
                SDL_FRect divider = {
                    divider_x - LOIM_DIVIDER_WIDTH,
                    LOIM_TOOLBAR_HEIGHT,
                    LOIM_DIVIDER_WIDTH * 2.0F,
                    (float)output_height - LOIM_TOOLBAR_HEIGHT - LOIM_STATUSBAR_HEIGHT
                };

                if (point_in_rect(app->mouse_x, app->mouse_y, &divider)) {
                    app->divider_dragging = true;
                } else if (app->image_count > 0U && app->mouse_x > divider_x) {
                    float right_left = divider_x + LOIM_DIVIDER_WIDTH / 2.0F;
                    float right_width = width - right_left;

                    app->split_drag_index = editor_split_at(
                        app,
                        app->mouse_x,
                        app->mouse_y,
                        right_left,
                        LOIM_TOOLBAR_HEIGHT,
                        right_width);
                    if (app->split_drag_index != SIZE_MAX) {
                        app->split_drag_start_y = app->mouse_y;
                        app->split_drag_current_y = app->mouse_y;
                    }
                }
            }
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            int output_width = 0;
            int output_height = 0;
            bool handled_drag = app->divider_dragging || app->split_drag_index != SIZE_MAX;

            app->mouse_x = event->button.x;
            app->mouse_y = event->button.y;
            if (app->split_drag_index != SIZE_MAX &&
                SDL_GetRenderOutputSize(app->renderer, &output_width, &output_height)) {
                float divider_x = (float)output_width * app->split_ratio;
                float editor_left = divider_x + LOIM_DIVIDER_WIDTH / 2.0F;

                app->split_drag_current_y = app->mouse_y;
                app_commit_split_drag(app, (float)output_width - editor_left);
            }
            app->divider_dragging = false;
            app->split_drag_index = SIZE_MAX;
            if (!handled_drag) {
                app_handle_click(app, app->mouse_x, app->mouse_y);
            }
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
    {
        int output_width = 0;
        int output_height = 0;

        app->mouse_x = event->wheel.mouse_x;
        app->mouse_y = event->wheel.mouse_y;
        if (SDL_GetRenderOutputSize(app->renderer, &output_width, &output_height) &&
            app->mouse_x < (float)output_width * app->split_ratio) {
            app->left_scroll_y -= event->wheel.y * 72.0F;
            if (app->left_scroll_y < 0.0F) {
                app->left_scroll_y = 0.0F;
            }
        } else {
            app->right_scroll_y -= event->wheel.y * 72.0F;
            if (app->right_scroll_y < 0.0F) {
                app->right_scroll_y = 0.0F;
            }
        }
        break;
    }
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            if (app->dialog_open) {
                app->quit_after_dialog = true;
                (void)SDL_HideWindow(app->window);
            } else {
                app->running = false;
            }
        } else if ((event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0U &&
                   event->key.key == SDLK_O) {
            app_open_dialog(app);
        } else if (((event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0U &&
                    event->key.key == SDLK_N) || event->key.key == SDLK_DELETE) {
            (void)app_clear_document(app);
        } else if (event->key.key == SDLK_PLUS || event->key.key == SDLK_EQUALS) {
            app_adjust_zoom(app, 0.1F);
        } else if (event->key.key == SDLK_MINUS) {
            app_adjust_zoom(app, -0.1F);
        }
        break;
    case SDL_EVENT_DROP_BEGIN:
        (void)loim_import_queue_begin(&app->dropped);
        app_set_status(app, app_translation(app, LOIM_TEXT_DROP_RELEASE));
        break;
    case SDL_EVENT_DROP_POSITION:
        if (!app->dropped.collecting) {
            (void)loim_import_queue_begin(&app->dropped);
        }
        app_set_status(app, app_translation(app, LOIM_TEXT_DROP_RELEASE));
        break;
    case SDL_EVENT_DROP_FILE:
        if (event->drop.data != NULL) {
            loim_status status = loim_import_queue_add(
                &app->dropped, event->drop.data);

            if (status != LOIM_OK) {
                app_set_status(app, app_translation(app, LOIM_TEXT_DROP_FAILED));
            } else if (event->drop.windowID == 0U) {
                (void)loim_import_queue_complete(&app->dropped);
                app_import_drop_queue(app);
            }
        }
        break;
    case SDL_EVENT_DROP_COMPLETE:
        if (app->dropped.collecting) {
            (void)loim_import_queue_complete(&app->dropped);
            app_import_drop_queue(app);
        }
        break;
    default:
        break;
    }
}

static void app_destroy(app_state *app)
{
    if (app->auth_thread != NULL) {
        SDL_WaitThread(app->auth_thread, NULL);
        app->auth_thread = NULL;
    }
    app_auth_task_destroy(app->auth_task);
    app->auth_task = NULL;
    if (app->import_thread != NULL) {
        SDL_WaitThread(app->import_thread, NULL);
        app->import_thread = NULL;
    }
    app_import_result_destroy(app->import_result);
    app->import_result = NULL;
    path_batch_destroy(&app->import_pending);
    loim_import_queue_destroy(&app->dropped);
    loim_layout_destroy(&app->layout);
    app_destroy_images(app);
    loim_document_destroy(app->document);
    app_text_cache_clear(app);
    if (app->font != NULL) {
        TTF_CloseFont(app->font);
        app->font = NULL;
    }
    if (app->ttf_initialized) {
        TTF_Quit();
        app->ttf_initialized = false;
    }
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    if (app->http_ready) {
        loim_http_cleanup();
        app->http_ready = false;
    }
    app_secure_zero(app->login_password, sizeof(app->login_password));
    app_secure_zero(app->auth_token, sizeof(app->auth_token));
    app_secure_zero(app->machine_code, sizeof(app->machine_code));
    SDL_Quit();
}

static bool app_initialize(app_state *app, bool headless)
{
    loim_status status;
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    memset(app, 0, sizeof(*app));
    app->zoom = 1.0F;
    app->columns = 1U;
    app->export_columns = 1U;
    app->split_ratio = 0.5F;
    app->margin_ratio = 0.08F;
    app->split_drag_index = SIZE_MAX;
    app->headless = headless;
    app->running = true;
    loim_import_queue_init(&app->dropped);
    loim_layout_options_a4(&app->layout_options, LOIM_LAYOUT_WIDTH);
    status = loim_document_create(&app->document);
    if (status != LOIM_OK) {
        (void)fprintf(stderr, "Cannot create document: %s\n", loim_status_string(status));
        return false;
    }
    if (headless) {
        (void)SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        window_flags |= SDL_WINDOW_HIDDEN;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        (void)fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        loim_document_destroy(app->document);
        app->document = NULL;
        return false;
    }
    app->locale = app_system_locale();
    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
    SDL_SetEventEnabled(SDL_EVENT_DROP_TEXT, true);
    if (!SDL_CreateWindowAndRenderer(
            app_translation(app, LOIM_TEXT_APP_TITLE),
            1440,
            900,
            window_flags,
            &app->window,
            &app->renderer)) {
        (void)fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        app_destroy(app);
        return false;
    }
    if (!TTF_Init()) {
        (void)fprintf(stderr, "Font engine initialization failed: %s\n", SDL_GetError());
        app_destroy(app);
        return false;
    }
    app->ttf_initialized = true;
    app->font = app_open_ui_font();
    if (app->font == NULL) {
        (void)fprintf(stderr, "Unable to open a system UI font: %s\n", SDL_GetError());
        app_destroy(app);
        return false;
    }
    (void)SDL_SetWindowMinimumSize(app->window, 760, 520);
    app->dialog_event = SDL_RegisterEvents(4);
    if (app->dialog_event == 0U) {
        (void)fprintf(stderr, "Cannot allocate file dialog event: %s\n", SDL_GetError());
        app_destroy(app);
        return false;
    }
    app->import_event = app->dialog_event + 1U;
    app->auth_event = app->dialog_event + 2U;
    app->export_event = app->dialog_event + 3U;
    if (!headless) {
        app->http_ready = loim_http_initialize();
        if (app->http_ready && !app_initialize_machine_code(app)) {
            loim_http_cleanup();
            app->http_ready = false;
        }
    }
    (void)SDL_SetRenderVSync(app->renderer, 1);
    (void)SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
    app_set_status(
        app,
        app->http_ready || headless
            ? app_translation(app, LOIM_TEXT_READY)
            : app_translation(app, LOIM_TEXT_LOGIN_UNAVAILABLE));
    return true;
}

int main(int argc, char **argv)
{
    app_state app;
    SDL_Event event;
    bool smoke_test = argc > 1 && strcmp(argv[1], "--smoke-test") == 0;
    int first_path = smoke_test ? 2 : 1;

    if (!app_initialize(&app, smoke_test)) {
        return EXIT_FAILURE;
    }
    if (argc > first_path) {
        path_batch startup = {0};
        int index;

        for (index = first_path; index < argc; ++index) {
            (void)path_batch_append(&startup, argv[index]);
        }
        app_import_batch(&app, &startup);
        path_batch_destroy(&startup);
    }
    if (smoke_test) {
        bool success = app.image_count == (size_t)(argc - first_path) &&
            app.layout.page_count > 0U;

        app_render(&app);
        app_destroy(&app);
        return success ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    while (app.running) {
        while (SDL_PollEvent(&event)) {
            app_handle_event(&app, &event);
        }
        app_render(&app);
        SDL_Delay(8U);
    }
    app_destroy(&app);
    return EXIT_SUCCESS;
}
