#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(_WIN32) && !defined(__APPLE__)
#include "linux_shortcut.h"
#endif

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
#include <shellapi.h>
#endif

#include "loim/document.h"
#include "loim/edition.h"
#include "loim/export_name.h"
#include "loim/i18n.h"
#include "loim/image_probe.h"
#include "loim/import_queue.h"
#include "loim/layout.h"
#include "loim/machine_code.h"
#include "loim/presentation.h"
#include "loim/seam.h"
#include "loim/settings.h"
#include "loim/status.h"
#include "loim/texture_plan.h"
#include "loim/update.h"

#include "http_client.h"
#include "credential_store.h"

#ifndef LOIM_APP_VERSION
#define LOIM_APP_VERSION "dev"
#endif

#define LOIM_LAYOUT_WIDTH 1000U
#define LOIM_MAX_IMAGE_PIXELS 100000000ULL
#define LOIM_MAX_AUTO_HINTS 4096U
#define LOIM_TOOLBAR_HEIGHT 58.0F
#define LOIM_STATUSBAR_HEIGHT 22.0F
#define LOIM_REGISTER_URL "https://ctdy123.com/auth"
#define LOIM_WEBSITE_URL "https://ctdy123.com"
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

enum {
    APP_COMPLETION_RUNNING = 0,
    APP_COMPLETION_QUEUED,
    APP_COMPLETION_PUSH_FAILED,
    APP_COMPLETION_HANDLED
};

typedef enum app_file_dialog_outcome {
    APP_FILE_DIALOG_PENDING = 0,
    APP_FILE_DIALOG_SELECTED,
    APP_FILE_DIALOG_CANCELED,
    APP_FILE_DIALOG_ERROR
} app_file_dialog_outcome;

typedef struct app_file_dialog_result {
    Uint32 completion_event;
    SDL_AtomicInt completion_state;
    app_file_dialog_outcome outcome;
    path_batch paths;
    char *path;
} app_file_dialog_result;

typedef struct app_import_result {
    Uint32 completion_event;
    uint64_t generation;
    SDL_AtomicInt cancel_requested;
    SDL_AtomicInt completion_state;
    bool force_completion_failure;
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
    SDL_AtomicInt completion_state;
    bool restore_session;
    char email[256];
    char password[256];
    char machine_code[64];
    char platform[16];
    char architecture[16];
    char os_name[128];
    char token[LOIM_CREDENTIAL_TOKEN_CAPACITY];
    loim_login_result result;
    loim_status status;
    int http_status;
} app_auth_task;

typedef struct app_update_task {
    Uint32 completion_event;
    SDL_AtomicInt completion_state;
    char current_version[LOIM_UPDATE_VERSION_CAPACITY];
    char platform[16];
    char architecture[16];
    loim_update_result result;
    loim_status status;
    int http_status;
} app_update_task;

typedef struct app_login_layout {
    SDL_FRect panel;
    SDL_FRect email;
    SDL_FRect password;
    SDL_FRect register_link;
    SDL_FRect cancel;
    SDL_FRect submit;
} app_login_layout;

typedef struct app_about_layout {
    SDL_FRect panel;
    SDL_FRect website;
    SDL_FRect ok;
} app_about_layout;

typedef struct app_text_cache_entry {
    char *text;
    SDL_Texture *texture;
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 alpha;
    float font_size;
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
    bool drop_batch_failed;
    path_batch import_pending;
    size_t import_pending_index;
    size_t import_succeeded;
    size_t import_failed;
    uint64_t import_generation;
    SDL_Thread *import_thread;
    app_import_result *import_result;
    bool dialog_open;
    app_file_dialog_result *file_dialog_result;
    bool quit_after_dialog;
    bool headless;
    bool persist_settings;
    bool running;
    Uint32 dialog_event;
    Uint32 import_event;
    Uint32 auth_event;
    Uint32 export_event;
    Uint32 update_event;
    SDL_Thread *auth_thread;
    app_auth_task *auth_task;
    SDL_Thread *update_thread;
    app_update_task *update_task;
    bool http_ready;
    bool machine_code_ready;
    bool login_open;
    bool about_open;
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
    char subscription_type[64];
    char subscription_expires_at[64];
    loim_locale locale;
    bool ttf_initialized;
    TTF_Font *font;
    app_text_cache_entry text_cache[LOIM_TEXT_CACHE_CAPACITY];
    size_t text_cache_next;
    float left_scroll_y;
    float right_scroll_y;
    float preview_scale;
    float split_ratio;
    int pending_window_width;
    int pending_window_height;
    int pending_window_x;
    int pending_window_y;
    bool has_pending_window_position;
    float margin_ratio;
    float mouse_x;
    float mouse_y;
    float split_drag_start_y;
    float split_drag_current_y;
    size_t split_drag_index;
    bool divider_dragging;
    size_t columns;
    bool import_progress_active;
    bool workspace_ready;
    bool test_force_import_completion_failure;
    size_t export_columns;
    float export_margin_ratio;
    bool export_licensed;
    loim_page_number_mode export_page_number_mode;
    char export_default_path[4096];
    loim_page_number_mode page_number_mode;
    char status[256];
} app_state;

static const SDL_DialogFileFilter app_image_filters_en[] = {
    {"Image files", "png;jpg;jpeg;gif;bmp"}
};

static const SDL_DialogFileFilter app_image_filters_zh[] = {
    {"图像文件", "png;jpg;jpeg;gif;bmp"}
};


static const char *app_translation(const app_state *app, loim_text_key key)
{
    return loim_text(app->locale, key);
}

static bool app_settings_path(char *output, size_t output_capacity);
static void app_save_settings(app_state *app);
static void app_load_settings(app_state *app);
static void app_show_pro_prompt(app_state *app);

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

static void app_file_dialog_result_destroy(app_file_dialog_result *result)
{
    if (result == NULL) {
        return;
    }
    path_batch_destroy(&result->paths);
    SDL_free(result->path);
    SDL_free(result);
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
    size_t length = 0U;

    if (capacity == 0U) {
        return;
    }
    if (source != NULL) {
        while (length + 1U < capacity && source[length] != '\0') {
            destination[length] = source[length];
            length += 1U;
        }
    }
    destination[length] = '\0';
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

#if !defined(_WIN32) && !defined(__APPLE__)
static loim_shortcut_result app_ensure_linux_desktop_shortcut(bool force)
{
    static const char installed_entry[] =
        "/usr/share/applications/com.ctdy123.loimreader.desktop";
    const char *home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    const char *config = SDL_getenv("XDG_CONFIG_HOME");
    char *fallback_config = NULL;
    char *preference_path = NULL;
    char *state_path = NULL;
    char desktop[4096];
    loim_shortcut_result result = LOIM_SHORTCUT_UNAVAILABLE;

    preference_path = SDL_GetPrefPath("ctdy123", "LoimReader");
    if (preference_path == NULL ||
        SDL_asprintf(
            &state_path,
            "%sdesktop-shortcut-v2.state",
            preference_path) < 0) {
        goto cleanup;
    }
    if (!force) {
        SDL_PathInfo state_info;

        if (SDL_GetPathInfo(state_path, &state_info) &&
            state_info.type == SDL_PATHTYPE_FILE) {
            result = LOIM_SHORTCUT_ALREADY_PRESENT;
            goto cleanup;
        }
    }
    if ((config == NULL || config[0] == '\0') && home != NULL &&
        SDL_asprintf(&fallback_config, "%s/.config", home) >= 0) {
        config = fallback_config;
    }
    if (!loim_linux_resolve_desktop_directory(
            home, config, desktop, sizeof(desktop))) {
        goto cleanup;
    }
    result = loim_linux_desktop_shortcut_ensure(
        desktop, installed_entry, "LoimReader.desktop");
    if (result == LOIM_SHORTCUT_CREATED ||
        result == LOIM_SHORTCUT_ALREADY_PRESENT ||
        result == LOIM_SHORTCUT_CONFLICT) {
        const char *state = result == LOIM_SHORTCUT_CONFLICT
            ? "conflict\n"
            : "complete\n";

        (void)SDL_SaveFile(state_path, state, strlen(state));
    }

cleanup:
    SDL_free(state_path);
    SDL_free(preference_path);
    SDL_free(fallback_config);
    return result;
}
#endif

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
    float font_size,
    Uint8 red,
    Uint8 green,
    Uint8 blue,
    Uint8 alpha)
{
    app_text_cache_entry *entry = NULL;
    SDL_Surface *surface;
    SDL_Color color = {red, green, blue, alpha};
    float previous_font_size = TTF_GetFontSize(app->font);
    size_t index;

    for (index = 0U; index < LOIM_TEXT_CACHE_CAPACITY; ++index) {
        app_text_cache_entry *candidate = &app->text_cache[index];

        if (candidate->text != NULL && candidate->red == red &&
            candidate->green == green && candidate->blue == blue &&
            candidate->alpha == alpha && candidate->font_size == font_size &&
            strcmp(candidate->text, text) == 0) {
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
    if (!TTF_SetFontSize(app->font, font_size)) {
        return NULL;
    }
    surface = TTF_RenderText_Blended(app->font, text, 0U, color);
    if (!TTF_SetFontSize(app->font, previous_font_size)) {
        SDL_DestroySurface(surface);
        return NULL;
    }
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
    entry->font_size = font_size;
    entry->width = (float)surface->w;
    entry->height = (float)surface->h;
    SDL_DestroySurface(surface);
    return entry;
}

static bool app_draw_text_sized(
    app_state *app,
    float x,
    float y,
    const char *text,
    float font_size,
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
    entry = app_text_cache_get(
        app, text, font_size, red, green, blue, alpha);
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
    return app_draw_text_sized(
        app, x, y, text, 14.0F, red, green, blue, alpha);
}

static float app_text_width_sized(
    app_state *app,
    const char *text,
    float font_size)
{
    app_text_cache_entry *entry;

    if (app->font == NULL) {
        return (float)strlen(text) * 8.0F;
    }
    entry = app_text_cache_get(
        app, text, font_size, 32U, 32U, 32U, 255U);
    return entry == NULL ? 0.0F : entry->width;
}

static float app_text_width(app_state *app, const char *text)
{
    return app_text_width_sized(app, text, 14.0F);
}

static void app_text_size_sized(
    app_state *app,
    const char *text,
    float font_size,
    float *out_width,
    float *out_height)
{
    app_text_cache_entry *entry;

    if (app->font == NULL) {
        if (out_width != NULL) {
            *out_width = (float)strlen(text) * 8.0F;
        }
        if (out_height != NULL) {
            *out_height = 8.0F;
        }
        return;
    }
    entry = app_text_cache_get(
        app, text, font_size, 32U, 32U, 32U, 255U);
    if (out_width != NULL) {
        *out_width = entry == NULL ? 0.0F : entry->width;
    }
    if (out_height != NULL) {
        *out_height = entry == NULL ? 0.0F : entry->height;
    }
}

/* Draw a button label centered on both axes of the button rectangle. */
static void app_draw_button_label(
    app_state *app,
    const SDL_FRect *button,
    const char *label,
    float font_size,
    Uint8 red,
    Uint8 green,
    Uint8 blue)
{
    float text_width = 0.0F;
    float text_height = 0.0F;

    app_text_size_sized(app, label, font_size, &text_width, &text_height);
    (void)app_draw_text_sized(
        app,
        button->x + (button->w - text_width) / 2.0F,
        button->y + (button->h - text_height) / 2.0F,
        label,
        font_size,
        red,
        green,
        blue,
        255U);
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

static bool app_load_saved_credentials(app_state *app)
{
    loim_credentials credentials;
    bool loaded;

    memset(&credentials, 0, sizeof(credentials));
    loaded = loim_credential_store_load(&credentials) &&
        loim_credentials_can_prefill(&credentials);
    if (loaded) {
        app_copy_text(
            app->login_email, sizeof(app->login_email), credentials.email);
        app_copy_text(
            app->login_password, sizeof(app->login_password), credentials.password);
        app_copy_text(
            app->auth_token, sizeof(app->auth_token), credentials.token);
    }
    loim_credentials_clear(&credentials);
    return loaded;
}

static bool app_save_credentials(const app_auth_task *task)
{
    loim_credentials credentials;
    bool saved;

    if (strlen(task->result.token) >= sizeof(credentials.token)) {
        return false;
    }
    memset(&credentials, 0, sizeof(credentials));
    app_copy_text(credentials.email, sizeof(credentials.email), task->email);
    app_copy_text(credentials.password, sizeof(credentials.password), task->password);
    app_copy_text(credentials.token, sizeof(credentials.token), task->result.token);
    saved = loim_credential_store_save(&credentials);
    loim_credentials_clear(&credentials);
    return saved;
}

static bool app_clear_saved_session(app_state *app)
{
    loim_credentials credentials;
    bool saved;

    memset(&credentials, 0, sizeof(credentials));
    app_copy_text(credentials.email, sizeof(credentials.email), app->login_email);
    app_copy_text(
        credentials.password, sizeof(credentials.password), app->login_password);
    saved = loim_credential_store_save(&credentials);
    loim_credentials_clear(&credentials);
    return saved;
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
    if (task->restore_session) {
        task->status = loim_auth_restore_session(
            &request,
            task->token,
            loim_http_post_json_authorized,
            NULL,
            &task->result,
            &task->http_status);
    } else {
        task->status = loim_auth_login(
            &request,
            loim_http_post_json,
            NULL,
            &task->result,
            &task->http_status);
    }
    SDL_zero(event);
    event.type = task->completion_event;
    event.user.data1 = task;
    if (SDL_PushEvent(&event)) {
        (void)SDL_SetAtomicInt(
            &task->completion_state, APP_COMPLETION_QUEUED);
        return 0;
    }
    (void)SDL_SetAtomicInt(
        &task->completion_state, APP_COMPLETION_PUSH_FAILED);
    return 1;
}

static void app_open_login(app_state *app)
{
    if (!loim_edition_is_pro()) {
        app_show_pro_prompt(app);
        return;
    }
    app->about_open = false;
    app->login_open = true;
    app->login_focus = APP_LOGIN_EMAIL;
    if (!app->logged_in && app->login_password[0] == '\0') {
        (void)app_load_saved_credentials(app);
    }
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

static void app_open_about(app_state *app)
{
    if (app->login_open || app->dialog_open) {
        return;
    }
    app->about_open = true;
}

static void app_close_about(app_state *app)
{
    app->about_open = false;
}

static void app_submit_login(app_state *app)
{
    app_auth_task *task;

    if (app->login_submitting) {
        return;
    }
    if (!app->http_ready || !app->machine_code_ready) {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(
                app,
                app->http_ready
                    ? LOIM_TEXT_LOGIN_UNAVAILABLE
                    : LOIM_TEXT_HTTPS_UNAVAILABLE));
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

static void app_submit_session_restore(app_state *app)
{
    app_auth_task *task;

    if (app->login_submitting || app->auth_token[0] == '\0' ||
        !app->http_ready || !app->machine_code_ready) {
        return;
    }
    task = SDL_calloc(1U, sizeof(*task));
    if (task == NULL) {
        app_open_login(app);
        return;
    }
    task->completion_event = app->auth_event;
    task->restore_session = true;
    app_copy_text(task->email, sizeof(task->email), app->login_email);
    app_copy_text(task->password, sizeof(task->password), app->login_password);
    app_copy_text(task->token, sizeof(task->token), app->auth_token);
    app_copy_text(task->machine_code, sizeof(task->machine_code), app->machine_code);
    app_copy_text(task->platform, sizeof(task->platform), app_platform_name());
    app_copy_text(task->architecture, sizeof(task->architecture), app_architecture_name());
    app_copy_text(task->os_name, sizeof(task->os_name), SDL_GetPlatform());
    app->auth_task = task;
    app->login_submitting = true;
    app_copy_text(
        app->login_message,
        sizeof(app->login_message),
        app_translation(app, LOIM_TEXT_RESTORING_LOGIN));
    app_set_status(app, app_translation(app, LOIM_TEXT_RESTORING_LOGIN));
    app->auth_thread = SDL_CreateThread(app_auth_thread_main, "loim-session", task);
    if (app->auth_thread == NULL) {
        app->auth_task = NULL;
        app->login_submitting = false;
        app_auth_task_destroy(task);
        app_open_login(app);
    }
}

static bool app_session_is_invalid(const app_auth_task *task)
{
    return task->restore_session && task->status == LOIM_OK &&
        (task->http_status == 401 || task->http_status == 403 ||
         task->http_status == 404);
}

static bool app_session_restore_pending(const app_state *app)
{
    return app->login_submitting && app->auth_task != NULL &&
        app->auth_task->restore_session;
}

static void app_handle_auth_event(app_state *app, SDL_Event *event)
{
    app_auth_task *task = event->user.data1;

    if (task == NULL || task != app->auth_task) {
        return;
    }
    SDL_WaitThread(app->auth_thread, NULL);
    app->auth_thread = NULL;
    (void)SDL_SetAtomicInt(
        &task->completion_state, APP_COMPLETION_HANDLED);
    app->login_submitting = false;
    if (task->status == LOIM_OK && task->result.success) {
        bool credentials_saved = task->restore_session || app_save_credentials(task);

        if (!task->restore_session) {
            app_copy_text(app->auth_token, sizeof(app->auth_token), task->result.token);
        }
        app_copy_text(app->account_email, sizeof(app->account_email), task->result.email);
        app_copy_text(
            app->subscription_type,
            sizeof(app->subscription_type),
            task->result.subscription_type);
        app_copy_text(
            app->subscription_expires_at,
            sizeof(app->subscription_expires_at),
            task->result.subscription_expires_at);
        app->logged_in = true;
        app->licensed = loim_auth_result_is_licensed(&task->result);
        app->login_open = false;
        (void)SDL_StopTextInput(app->window);
        if (credentials_saved) {
            (void)snprintf(
                app->status,
                sizeof(app->status),
                app_translation(app, LOIM_TEXT_SIGNED_IN_FORMAT),
                app->account_email);
        } else {
            app_set_status(
                app, app_translation(app, LOIM_TEXT_CREDENTIAL_SAVE_FAILED));
        }
    } else if (task->status != LOIM_OK) {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(
                app,
                task->restore_session && task->http_status >= 200 &&
                        task->http_status < 300
                    ? LOIM_TEXT_SESSION_DETAILS_UNAVAILABLE
                    : LOIM_TEXT_LOGIN_NETWORK_ERROR));
    } else if (app_session_is_invalid(task)) {
        app_copy_text(
            app->login_message,
            sizeof(app->login_message),
            app_translation(app, LOIM_TEXT_SESSION_EXPIRED));
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
    if (task->status == LOIM_OK && task->result.success) {
        app_secure_zero(app->login_password, sizeof(app->login_password));
    } else if (task->restore_session) {
        if (app_session_is_invalid(task)) {
            app_secure_zero(app->auth_token, sizeof(app->auth_token));
            (void)app_clear_saved_session(app);
        }
        app->login_open = true;
        app->login_focus = APP_LOGIN_PASSWORD;
        (void)SDL_StartTextInput(app->window);
    }
    app->auth_task = NULL;
    app_auth_task_destroy(task);
}

static void app_poll_auth_completion(app_state *app)
{
    SDL_Event event;

    if (app->auth_task == NULL ||
        SDL_GetAtomicInt(&app->auth_task->completion_state) !=
            APP_COMPLETION_PUSH_FAILED) {
        return;
    }
    SDL_zero(event);
    event.user.data1 = app->auth_task;
    app_handle_auth_event(app, &event);
}

static void app_update_task_destroy(app_update_task *task)
{
    if (task != NULL) {
        SDL_free(task);
    }
}

static int SDLCALL app_update_thread_main(void *userdata)
{
    app_update_task *task = userdata;
    loim_update_request request;
    SDL_Event event;

    request.current_version = task->current_version;
    request.platform = task->platform;
    request.architecture = task->architecture;
    task->status = loim_update_check(
        &request,
        loim_http_post_json,
        NULL,
        &task->result,
        &task->http_status);
    SDL_zero(event);
    event.type = task->completion_event;
    event.user.data1 = task;
    if (SDL_PushEvent(&event)) {
        (void)SDL_SetAtomicInt(
            &task->completion_state, APP_COMPLETION_QUEUED);
        return 0;
    }
    (void)SDL_SetAtomicInt(
        &task->completion_state, APP_COMPLETION_PUSH_FAILED);
    return 1;
}

static void app_start_update_check(app_state *app)
{
    app_update_task *task;

    if (app == NULL || app->headless || !app->http_ready ||
        app->update_task != NULL || strcmp(LOIM_APP_VERSION, "dev") == 0) {
        return;
    }
    task = SDL_calloc(1U, sizeof(*task));
    if (task == NULL) {
        return;
    }
    task->completion_event = app->update_event;
    app_copy_text(
        task->current_version, sizeof(task->current_version), LOIM_APP_VERSION);
    app_copy_text(task->platform, sizeof(task->platform), app_platform_name());
    app_copy_text(
        task->architecture, sizeof(task->architecture), app_architecture_name());
    app->update_task = task;
    app->update_thread = SDL_CreateThread(
        app_update_thread_main, "loim-update-check", task);
    if (app->update_thread == NULL) {
        app->update_task = NULL;
        app_update_task_destroy(task);
    }
}

static void app_show_update_prompt(
    app_state *app,
    const loim_update_result *update)
{
    enum { APP_UPDATE_LATER = 0, APP_UPDATE_DOWNLOAD = 1 };
    char message[1200];
    SDL_MessageBoxButtonData buttons[2];
    SDL_MessageBoxData message_box;
    int button_id = APP_UPDATE_LATER;

    if (update->release_notes[0] != '\0') {
        (void)snprintf(
            message,
            sizeof(message),
            app_translation(app, LOIM_TEXT_UPDATE_AVAILABLE_NOTES_FORMAT),
            update->latest_version,
            update->release_notes);
    } else {
        (void)snprintf(
            message,
            sizeof(message),
            app_translation(app, LOIM_TEXT_UPDATE_AVAILABLE_FORMAT),
            update->latest_version);
    }
    SDL_zeroa(buttons);
    buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    buttons[0].buttonID = APP_UPDATE_LATER;
    buttons[0].text = app_translation(app, LOIM_TEXT_UPDATE_LATER);
    buttons[1].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    buttons[1].buttonID = APP_UPDATE_DOWNLOAD;
    buttons[1].text = app_translation(app, LOIM_TEXT_UPDATE_DOWNLOAD);
    SDL_zero(message_box);
    message_box.flags = SDL_MESSAGEBOX_INFORMATION;
    message_box.window = app->window;
    message_box.title = app_translation(app, LOIM_TEXT_UPDATE_AVAILABLE_TITLE);
    message_box.message = message;
    message_box.numbuttons = (int)SDL_arraysize(buttons);
    message_box.buttons = buttons;
    if (SDL_ShowMessageBox(&message_box, &button_id) &&
        button_id == APP_UPDATE_DOWNLOAD && !SDL_OpenURL(update->download_url)) {
        app_set_status(
            app, app_translation(app, LOIM_TEXT_UPDATE_OPEN_FAILED));
    }
}

static void app_show_pro_prompt(app_state *app)
{
    enum { APP_PRO_LATER = 0, APP_PRO_VISIT = 1 };
    SDL_MessageBoxButtonData buttons[2];
    SDL_MessageBoxData message_box;
    int button_id = APP_PRO_LATER;

    SDL_zeroa(buttons);
    buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    buttons[0].buttonID = APP_PRO_LATER;
    buttons[0].text = app_translation(app, LOIM_TEXT_PRO_PROMPT_LATER);
    buttons[1].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    buttons[1].buttonID = APP_PRO_VISIT;
    buttons[1].text = app_translation(app, LOIM_TEXT_PRO_PROMPT_VISIT);
    SDL_zero(message_box);
    message_box.flags = SDL_MESSAGEBOX_INFORMATION;
    message_box.window = app->window;
    message_box.title = app_translation(app, LOIM_TEXT_PRO_PROMPT_TITLE);
    message_box.message = app_translation(app, LOIM_TEXT_PRO_PROMPT_MESSAGE);
    message_box.numbuttons = (int)SDL_arraysize(buttons);
    message_box.buttons = buttons;
    if (SDL_ShowMessageBox(&message_box, &button_id) &&
        button_id == APP_PRO_VISIT && !SDL_OpenURL(LOIM_WEBSITE_URL)) {
        app_set_status(
            app, app_translation(app, LOIM_TEXT_ABOUT_OPEN_FAILED));
    }
}

static void app_handle_update_event(app_state *app, SDL_Event *event)
{
    app_update_task *task = event->user.data1;

    if (task == NULL || task != app->update_task) {
        return;
    }
    if (app->update_thread != NULL) {
        SDL_WaitThread(app->update_thread, NULL);
        app->update_thread = NULL;
    }
    (void)SDL_SetAtomicInt(
        &task->completion_state, APP_COMPLETION_HANDLED);
    if (task->status == LOIM_OK && task->result.has_update) {
        app_show_update_prompt(app, &task->result);
    }
    app->update_task = NULL;
    app_update_task_destroy(task);
}

static void app_poll_update_completion(app_state *app)
{
    SDL_Event event;

    if (app->update_task == NULL ||
        SDL_GetAtomicInt(&app->update_task->completion_state) !=
            APP_COMPLETION_PUSH_FAILED) {
        return;
    }
    SDL_zero(event);
    event.user.data1 = app->update_task;
    app_handle_update_event(app, &event);
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
        app->workspace_ready = false;
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
    if (!result->force_completion_failure && SDL_PushEvent(&event)) {
        (void)SDL_SetAtomicInt(
            &result->completion_state, APP_COMPLETION_QUEUED);
        return 0;
    }
    (void)SDL_SetAtomicInt(
        &result->completion_state, APP_COMPLETION_PUSH_FAILED);
    return 1;
}

static void app_finish_import_batch(app_state *app)
{
    char message[256];
    loim_status layout_status = app_rebuild_layout(app);

    if (layout_status != LOIM_OK) {
        app->workspace_ready = false;
        (void)snprintf(
            message,
            sizeof(message),
            app_translation(app, LOIM_TEXT_LAYOUT_FAILED_FORMAT),
            loim_status_text(app->locale, layout_status));
    } else {
        app->workspace_ready = app->image_count > 0U &&
            app->layout.page_count > 0U;
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
    if (app->import_thread != NULL) {
        char message[256];

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
        return;
    }
    while (app->import_pending_index < app->import_pending.count) {
        app_import_result *result = SDL_calloc(1U, sizeof(*result));
        const char *path = app->import_pending.paths[app->import_pending_index];
        char message[256];

        if (result == NULL) {
            app->import_failed += 1U;
            app->import_pending_index += 1U;
            continue;
        }
        result->generation = app->import_generation;
        result->force_completion_failure =
            app->test_force_import_completion_failure;
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
    bool stale;

    if (result == NULL || result != app->import_result) {
        return;
    }
    SDL_WaitThread(app->import_thread, NULL);
    app->import_thread = NULL;
    (void)SDL_SetAtomicInt(
        &result->completion_state, APP_COMPLETION_HANDLED);
    stale = !loim_import_result_should_commit(
        app->import_generation,
        result->generation,
        SDL_GetAtomicInt(&result->cancel_requested) != 0);
    if (stale) {
        app->import_result = NULL;
        app_import_result_destroy(result);
        if (app->import_progress_active && app->import_pending.count > 0U) {
            app_start_next_import(app);
        }
        return;
    }
    status = result->status;
    if (status == LOIM_OK) {
        status = app_commit_decoded_image(
            app, result->path, &result->probe, result->surface);
    }
    if (status == LOIM_OK) {
        app->import_succeeded += 1U;
    } else {
        app->import_failed += 1U;
    }
    app->import_result = NULL;
    app_import_result_destroy(result);
    app->import_pending_index += 1U;
    app_start_next_import(app);
}

static void app_poll_import_completion(app_state *app)
{
    SDL_Event event;

    if (app->import_result == NULL ||
        SDL_GetAtomicInt(&app->import_result->completion_state) !=
            APP_COMPLETION_PUSH_FAILED) {
        return;
    }
    SDL_zero(event);
    event.user.data1 = app->import_result;
    app_handle_import_event(app, &event);
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
        app->workspace_ready = false;
        (void)snprintf(
            message,
            sizeof(message),
            app_translation(app, LOIM_TEXT_LAYOUT_FAILED_FORMAT),
            loim_status_text(app->locale, layout_status));
    } else {
        app->workspace_ready = app->image_count > 0U &&
            app->layout.page_count > 0U;
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
    if ((app->import_thread != NULL && app->import_result != NULL &&
         app->import_result->generation == app->import_generation) ||
        app->import_pending.count != 0U) {
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

#define LOIM_GRID_GAP_RATIO 0.025F


static const loim_page_slice *app_slice_at(const app_state *app, size_t slice_index)
{
    return app != NULL && slice_index < app->layout.slice_count
        ? &app->layout.slices[slice_index]
        : NULL;
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
    size_t item_slot;

    if (current == NULL || destination == NULL) {
        return false;
    }
    for (item_slot = 0U; item_slot < loim_sheet_slot_count(columns); ++item_slot) {
        size_t item_row;
        size_t item_index = loim_sheet_slice_index(sheet_index, columns, item_slot);
        const loim_page_slice *item = app_slice_at(app, item_index);
        float item_height;

        if (loim_sheet_slot_column(columns, item_slot) != column || item == NULL) {
            continue;
        }
        item_row = loim_sheet_slot_row(columns, item_slot);
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


static void SDLCALL app_dialog_callback(
    void *userdata,
    const char *const *filelist,
    int filter)
{
    app_file_dialog_result *result = userdata;
    SDL_Event event;
    size_t index;

    (void)filter;
    if (filelist == NULL) {
        result->outcome = APP_FILE_DIALOG_ERROR;
    } else {
        for (index = 0U; filelist[index] != NULL; ++index) {
            if (!path_batch_append(&result->paths, filelist[index])) {
                result->outcome = APP_FILE_DIALOG_ERROR;
                break;
            }
        }
        if (result->outcome == APP_FILE_DIALOG_PENDING) {
            result->outcome = result->paths.count > 0U
                ? APP_FILE_DIALOG_SELECTED
                : APP_FILE_DIALOG_CANCELED;
        }
    }
    SDL_zero(event);
    event.type = result->completion_event;
    event.user.data1 = result;
    if (SDL_PushEvent(&event)) {
        (void)SDL_SetAtomicInt(
            &result->completion_state, APP_COMPLETION_QUEUED);
    } else {
        (void)SDL_SetAtomicInt(
            &result->completion_state, APP_COMPLETION_PUSH_FAILED);
    }
}


static void app_open_dialog(app_state *app)
{
    app_file_dialog_result *result;

    if (app->dialog_open) {
        return;
    }
    result = SDL_calloc(1U, sizeof(*result));
    if (result == NULL) {
        app_set_status(app, app_translation(app, LOIM_TEXT_FILE_DIALOG_ERROR));
        return;
    }
    result->completion_event = app->dialog_event;
    app->file_dialog_result = result;
    app->dialog_open = true;
    app_set_status(app, app_translation(app, LOIM_TEXT_CHOOSE_IMAGES));
    {
        const SDL_DialogFileFilter *filters = app->locale == LOIM_LOCALE_ZH_CN
            ? app_image_filters_zh
            : app_image_filters_en;

        SDL_ShowOpenFileDialog(
            app_dialog_callback,
            result,
            app->window,
            filters,
            1,
            NULL,
            true);
    }
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
    loim_status status;

    if (!loim_workspace_clear_enabled(
            app->image_count,
            app->import_progress_active,
            app->dropped.collecting)) {
        return true;
    }
    status = loim_document_create(&replacement);

    if (status != LOIM_OK) {
        app_set_status(app, app_translation(app, LOIM_TEXT_CLEAR_FAILED));
        return false;
    }
    app->import_generation = loim_import_generation_next(
        app->import_generation);
    if (app->import_result != NULL) {
        (void)SDL_SetAtomicInt(&app->import_result->cancel_requested, 1);
    }
    path_batch_destroy(&app->import_pending);
    app->import_pending_index = 0U;
    app->import_succeeded = 0U;
    app->import_failed = 0U;
    app->import_progress_active = false;
    loim_import_queue_destroy(&app->dropped);
    loim_import_queue_init(&app->dropped);
    app->drop_batch_failed = false;
    loim_layout_destroy(&app->layout);
    app_destroy_images(app);
    loim_document_destroy(app->document);
    app->document = replacement;
    app->workspace_ready = false;
    app->left_scroll_y = 0.0F;
    app->right_scroll_y = 0.0F;
    app->divider_dragging = false;
    app->split_drag_index = SIZE_MAX;
    app_set_status(app, app_translation(app, LOIM_TEXT_CLEARED));
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
    TOOLBAR_CLEAR,
    TOOLBAR_EXPORT,
    TOOLBAR_PRINT,
    TOOLBAR_TWO_COLUMNS,
    TOOLBAR_PAGE_NUMBERS,
    TOOLBAR_AUTO_SPLIT,
    TOOLBAR_LESS_MARGIN,
    TOOLBAR_MORE_MARGIN,
    TOOLBAR_PREVIEW_WIDER,
    TOOLBAR_PREVIEW_NARROWER,
    TOOLBAR_ABOUT
} toolbar_action;

typedef struct toolbar_item {
    toolbar_action action;
    float x;
} toolbar_item;

static const toolbar_item toolbar_items[] = {
    {TOOLBAR_LOGIN, 18.0F},
    {TOOLBAR_OPEN, 68.0F},
    {TOOLBAR_CLEAR, 108.0F},
    {TOOLBAR_EXPORT, 158.0F},
    {TOOLBAR_PRINT, 198.0F},
    {TOOLBAR_TWO_COLUMNS, 248.0F},
    {TOOLBAR_PAGE_NUMBERS, 288.0F},
    {TOOLBAR_AUTO_SPLIT, 338.0F},
    {TOOLBAR_LESS_MARGIN, 388.0F},
    {TOOLBAR_MORE_MARGIN, 428.0F},
    {TOOLBAR_PREVIEW_WIDER, 478.0F},
    {TOOLBAR_PREVIEW_NARROWER, 518.0F},
    {TOOLBAR_ABOUT, 568.0F}
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
    loim_workspace_mode mode = loim_workspace_mode_resolve(
        app->image_count, app->import_progress_active, app->workspace_ready);

    if (action == TOOLBAR_LOGIN || action == TOOLBAR_ABOUT) {
        return true;
    }
    if (action == TOOLBAR_CLEAR) {
        return !app->dialog_open && loim_workspace_clear_enabled(
            app->image_count,
            app->import_progress_active,
            app->dropped.collecting);
    }
    if (action == TOOLBAR_OPEN) {
        return !app->import_progress_active && !app->dialog_open;
    }
    if (app->import_progress_active || mode != LOIM_WORKSPACE_READY) {
        return false;
    }
    if ((action == TOOLBAR_EXPORT || action == TOOLBAR_PRINT) &&
        app_session_restore_pending(app)) {
        return false;
    }
    if (action == TOOLBAR_PREVIEW_WIDER) {
        return loim_preview_scale_adjust(app->preview_scale, 1) >
            app->preview_scale;
    }
    if (action == TOOLBAR_PREVIEW_NARROWER) {
        return loim_preview_scale_adjust(app->preview_scale, -1) <
            app->preview_scale;
    }
    return true;
}

static bool toolbar_action_active(const app_state *app, toolbar_action action)
{
    return (action == TOOLBAR_LOGIN && app->logged_in) ||
           (action == TOOLBAR_TWO_COLUMNS && app->columns > 1U) ||
           (action == TOOLBAR_PAGE_NUMBERS &&
            app->page_number_mode != LOIM_PAGE_NUMBER_NONE) ||
           (action == TOOLBAR_ABOUT && app->about_open);
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
    case TOOLBAR_CLEAR:
        (void)SDL_RenderRect(app->renderer, &box);
        draw_line(
            app->renderer,
            center_x - 5.0F,
            center_y - 5.0F,
            center_x + 5.0F,
            center_y + 5.0F);
        draw_line(
            app->renderer,
            center_x + 5.0F,
            center_y - 5.0F,
            center_x - 5.0F,
            center_y + 5.0F);
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
        if (app->page_number_mode == LOIM_PAGE_NUMBER_BOTTOM_RIGHT) {
            (void)SDL_RenderDebugText(
                app->renderer, right - 8.0F, bottom - 8.0F, "1");
        } else if (app->page_number_mode == LOIM_PAGE_NUMBER_BOTTOM_CENTER) {
            (void)SDL_RenderDebugText(
                app->renderer, center_x - 4.0F, bottom - 8.0F, "1");
        }
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
    case TOOLBAR_PREVIEW_WIDER:
    case TOOLBAR_PREVIEW_NARROWER:
        (void)SDL_RenderRect(app->renderer, &box);
        draw_line(app->renderer, center_x - 6.0F, center_y, center_x + 6.0F, center_y);
        if (action == TOOLBAR_PREVIEW_WIDER) {
            draw_line(app->renderer, center_x, center_y - 6.0F, center_x, center_y + 6.0F);
        }
        break;
    case TOOLBAR_ABOUT:
        draw_circle(app->renderer, (int)center_x, (int)center_y, 10);
        /* 手绘 "i" 标记，确保相对圆心精确居中 */
        {
            SDL_FRect dot = {center_x - 1.0F, center_y - 5.0F, 2.0F, 2.0F};
            SDL_FRect stem = {center_x - 1.0F, center_y - 1.0F, 2.0F, 6.0F};

            (void)SDL_RenderFillRect(app->renderer, &dot);
            (void)SDL_RenderFillRect(app->renderer, &stem);
        }
        break;
    }
}

/* Resolve the hover tooltip text for a toolbar button. */
static const char *app_toolbar_tooltip(
    app_state *app,
    toolbar_action action,
    char *buffer,
    size_t capacity)
{
    switch (action) {
    case TOOLBAR_LOGIN:
        return app_translation(
            app,
            app->logged_in
                ? LOIM_TEXT_TOOLTIP_ACCOUNT
                : LOIM_TEXT_TOOLTIP_LOGIN);
    case TOOLBAR_OPEN:
        return app_translation(app, LOIM_TEXT_TOOLTIP_OPEN);
    case TOOLBAR_CLEAR:
        (void)snprintf(
            buffer,
            capacity,
            "%s — %s",
            app_translation(app, LOIM_TEXT_CLEAR),
            app_translation(app, LOIM_TEXT_CLEAR_TOOLTIP));
        return buffer;
    case TOOLBAR_EXPORT:
        return app_translation(app, LOIM_TEXT_TOOLTIP_EXPORT);
    case TOOLBAR_PRINT:
        return app_translation(app, LOIM_TEXT_TOOLTIP_PRINT);
    case TOOLBAR_TWO_COLUMNS:
        return app_translation(app, LOIM_TEXT_TOOLTIP_COLUMNS);
    case TOOLBAR_PAGE_NUMBERS:
        return app_translation(app, LOIM_TEXT_TOOLTIP_PAGE_NUMBERS);
    case TOOLBAR_AUTO_SPLIT:
        return app_translation(app, LOIM_TEXT_TOOLTIP_AUTO_SPLIT);
    case TOOLBAR_LESS_MARGIN:
        return app_translation(app, LOIM_TEXT_TOOLTIP_LESS_MARGIN);
    case TOOLBAR_MORE_MARGIN:
        return app_translation(app, LOIM_TEXT_TOOLTIP_MORE_MARGIN);
    case TOOLBAR_PREVIEW_WIDER:
        return app_translation(app, LOIM_TEXT_TOOLTIP_PREVIEW_LARGER);
    case TOOLBAR_PREVIEW_NARROWER:
        return app_translation(app, LOIM_TEXT_TOOLTIP_PREVIEW_SMALLER);
    case TOOLBAR_ABOUT:
        return app_translation(app, LOIM_TEXT_TOOLTIP_ABOUT);
    }
    return "";
}

static void app_render_toolbar(app_state *app, float width)
{
    SDL_FRect bar = {0.0F, 0.0F, width, LOIM_TOOLBAR_HEIGHT};
    size_t index;
    int hovered_action = -1;
    float hovered_center_x = 0.0F;
    static const float separators[] = {
        58.0F, 148.0F, 238.0F, 328.0F, 378.0F, 468.0F, 558.0F
    };

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
        if (hovered) {
            hovered_action = (int)toolbar_items[index].action;
            hovered_center_x = button.x + button.w / 2.0F;
        }
        draw_toolbar_icon(app, toolbar_items[index].action, &button, enabled);
    }
    (void)SDL_SetRenderDrawColor(app->renderer, 188U, 188U, 188U, 255U);
    draw_line(app->renderer, 0.0F, LOIM_TOOLBAR_HEIGHT - 1.0F, width, LOIM_TOOLBAR_HEIGHT - 1.0F);
    if (hovered_action >= 0) {
        char tooltip[256];
        const char *text = app_toolbar_tooltip(
            app,
            (toolbar_action)hovered_action,
            tooltip,
            sizeof(tooltip));
        float tooltip_width = app_text_width_sized(app, text, 13.0F) + 18.0F;
        SDL_FRect tooltip_rect = {
            hovered_center_x - tooltip_width / 2.0F,
            LOIM_TOOLBAR_HEIGHT - 5.0F,
            tooltip_width,
            28.0F
        };

        if (tooltip_rect.x + tooltip_rect.w > width - 8.0F) {
            tooltip_rect.x = width - tooltip_rect.w - 8.0F;
        }
        if (tooltip_rect.x < 8.0F) {
            tooltip_rect.x = 8.0F;
        }
        draw_filled_rect(app->renderer, &tooltip_rect, 255U, 248U, 240U, 255U);
        (void)SDL_SetRenderDrawColor(app->renderer, 230U, 74U, 59U, 255U);
        (void)SDL_RenderRect(app->renderer, &tooltip_rect);
        (void)app_draw_text_sized(
            app,
            tooltip_rect.x + 9.0F,
            tooltip_rect.y + 6.0F,
            text,
            13.0F,
            82U,
            45U,
            40U,
            255U);
    }
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

static void app_reanchor_preview_scroll(
    app_state *app,
    float old_viewport_width,
    float new_viewport_width,
    float viewport_height,
    float old_scale,
    float new_scale)
{
    size_t columns = loim_columns_normalize(app->columns);
    size_t sheet_count = loim_sheet_count(app->layout.slice_count, columns);
    float old_paper_width = loim_preview_paper_width(
        old_viewport_width, old_scale);
    float new_paper_width = loim_preview_paper_width(
        new_viewport_width, new_scale);

    app->left_scroll_y = loim_preview_scroll_reanchor(
        app->left_scroll_y,
        viewport_height,
        old_paper_width,
        new_paper_width,
        sheet_count);
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
    float paper_width = loim_preview_paper_width(width, app->preview_scale);
    float paper_height;
    float viewport_height = bottom - top;
    float total_height;
    float y;
    size_t sheet_index;
    SDL_Rect clip = {(int)left, (int)top, (int)width, (int)viewport_height};

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
            if (app->page_number_mode != LOIM_PAGE_NUMBER_NONE) {
                char number[24];
                float number_x;
                float number_y;
                float number_width;
                float number_height = app->font == NULL
                    ? 8.0F
                    : (float)TTF_GetFontHeight(app->font);

                (void)snprintf(number, sizeof(number), "%zu", sheet_index + 1U);
                number_width = app_text_width(app, number);
                if (loim_page_number_origin(
                        app->page_number_mode,
                        paper.w,
                        paper.h,
                        margin_x,
                        margin_y,
                        number_width,
                        number_height,
                        &number_x,
                        &number_y)) {
                    (void)app_draw_text(
                        app,
                        paper.x + number_x,
                        paper.y + number_y,
                        number,
                        110U,
                        110U,
                        110U,
                        255U);
                }
            }
        }
        y += paper_height + 24.0F;
    }
    (void)SDL_SetRenderClipRect(app->renderer, NULL);
}

static float editor_scale(float width)
{
    float scale = width * 0.78F / (float)LOIM_LAYOUT_WIDTH;

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
    float scale = editor_scale(width);
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

static void app_paint_split_handle(
    app_state *app,
    const SDL_FRect *painted,
    bool active,
    bool hovered)
{
    float center_y = painted->y + painted->h / 2.0F;
    int dot;

    if (active) {
        draw_filled_rect(app->renderer, painted, 0U, 102U, 204U, 225U);
    } else if (hovered) {
        draw_filled_rect(app->renderer, painted, 74U, 144U, 226U, 120U);
    } else {
        draw_filled_rect(app->renderer, painted, 132U, 132U, 132U, 255U);
    }
    (void)SDL_SetRenderDrawColor(app->renderer, 220U, 220U, 220U, 220U);
    draw_line(
        app->renderer, painted->x, center_y, painted->x + painted->w, center_y);
    for (dot = -2; dot <= 2; ++dot) {
        SDL_FRect marker = {
            painted->x + painted->w / 2.0F + (float)dot * 5.0F - 1.0F,
            center_y - 1.0F,
            2.0F,
            2.0F
        };

        (void)SDL_RenderFillRect(app->renderer, &marker);
    }
}

static void app_render_editor(
    app_state *app,
    float left,
    float top,
    float width,
    float bottom)
{
    float scale = editor_scale(width);
    float canvas_width = (float)LOIM_LAYOUT_WIDTH * scale;
    float viewport_height = bottom - top;
    float canvas_x = left + (width - canvas_width) / 2.0F;
    float y;
    size_t page_index;
    bool drag_painting = false;
    SDL_FRect dragged_handle = {0.0F, 0.0F, 0.0F, 0.0F};
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

            if (app->split_drag_index == page_index) {
                /* Deferred: painted after the page loop so it floats above
                   every page in both drag directions. */
                dragged_handle = handle;
                drag_painting = true;
            } else {
                app_paint_split_handle(
                    app,
                    &handle,
                    false,
                    point_in_rect(app->mouse_x, app->mouse_y, &handle));
            }
            y += LOIM_SPLIT_HANDLE_HEIGHT;
        }
    }
    if (drag_painting) {
        SDL_FRect painted = {
            dragged_handle.x,
            dragged_handle.y + app->split_drag_current_y - app->split_drag_start_y,
            dragged_handle.w,
            dragged_handle.h
        };

        app_paint_split_handle(app, &painted, true, false);
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

static void draw_dashed_rect(
    SDL_Renderer *renderer,
    const SDL_FRect *rect,
    float dash,
    float gap)
{
    float cursor;
    float end;

    for (cursor = rect->x; cursor < rect->x + rect->w; cursor += dash + gap) {
        end = cursor + dash;
        if (end > rect->x + rect->w) {
            end = rect->x + rect->w;
        }
        draw_line(renderer, cursor, rect->y, end, rect->y);
        draw_line(renderer, cursor, rect->y + rect->h, end, rect->y + rect->h);
    }
    for (cursor = rect->y; cursor < rect->y + rect->h; cursor += dash + gap) {
        end = cursor + dash;
        if (end > rect->y + rect->h) {
            end = rect->y + rect->h;
        }
        draw_line(renderer, rect->x, cursor, rect->x, end);
        draw_line(renderer, rect->x + rect->w, cursor, rect->x + rect->w, end);
    }
}

static void app_render_empty(
    app_state *app,
    float left,
    float top,
    float width,
    float bottom,
    loim_workspace_mode mode)
{
    char progress_text[256];
    bool dragging = app->dropped.collecting;
    bool processing = mode == LOIM_WORKSPACE_IMPORTING_EMPTY;
    const char *title = app_translation(
        app,
        dragging
            ? LOIM_TEXT_DROP_RELEASE
            : processing
                ? LOIM_TEXT_EMPTY_PROCESSING
                : LOIM_TEXT_EMPTY_TITLE);
    const char *subtitle = app_translation(app, LOIM_TEXT_EMPTY_SUBTITLE);
    float content_height = bottom - top;
    float horizontal_inset = width * 0.08F;
    float vertical_inset = content_height * 0.08F;
    float title_size = content_height < 330.0F ? 20.0F : 26.0F;
    float subtitle_size = content_height < 330.0F ? 13.0F : 15.0F;
    SDL_FRect background = {left, top, width, content_height};
    SDL_FRect card;
    SDL_FRect back_page;
    SDL_FRect front_page;
    float center_x;
    float center_y;
    float icon_top;
    float title_width;
    float subtitle_width;

    if (processing) {
        if (app->import_pending.count > 0U) {
            (void)snprintf(
                progress_text,
                sizeof(progress_text),
                app_translation(app, LOIM_TEXT_IMPORT_PROGRESS_FORMAT),
                app->import_pending_index + 1U,
                app->import_pending.count,
                loim_progress_percent(
                    app->import_pending_index,
                    app->import_pending.count));
            subtitle = progress_text;
        } else {
            subtitle = app_translation(app, LOIM_TEXT_EMPTY_PROCESSING);
        }
    }

    if (horizontal_inset < 28.0F) {
        horizontal_inset = 28.0F;
    } else if (horizontal_inset > 110.0F) {
        horizontal_inset = 110.0F;
    }
    if (vertical_inset < 24.0F) {
        vertical_inset = 24.0F;
    } else if (vertical_inset > 64.0F) {
        vertical_inset = 64.0F;
    }
    card.x = left + horizontal_inset;
    card.y = top + vertical_inset;
    card.w = width - horizontal_inset * 2.0F;
    card.h = content_height - vertical_inset * 2.0F;
    center_x = card.x + card.w / 2.0F;
    center_y = card.y + card.h / 2.0F;
    icon_top = center_y - (content_height < 330.0F ? 88.0F : 122.0F);

    draw_filled_rect(app->renderer, &background, 246U, 240U, 234U, 255U);
    draw_filled_rect(
        app->renderer,
        &card,
        255U,
        dragging ? 239U : 248U,
        dragging ? 232U : 240U,
        255U);
    (void)SDL_SetRenderDrawColor(app->renderer, 230U, 74U, 59U, 255U);
    draw_dashed_rect(app->renderer, &card, dragging ? 16.0F : 11.0F, 7.0F);
    if (dragging) {
        SDL_FRect inner = {
            card.x + 4.0F,
            card.y + 4.0F,
            card.w - 8.0F,
            card.h - 8.0F
        };

        draw_dashed_rect(app->renderer, &inner, 16.0F, 7.0F);
    }

    back_page.x = center_x - 22.0F;
    back_page.y = icon_top + 9.0F;
    back_page.w = 62.0F;
    back_page.h = 76.0F;
    front_page.x = center_x - 40.0F;
    front_page.y = icon_top;
    front_page.w = 62.0F;
    front_page.h = 76.0F;
    draw_filled_rect(app->renderer, &back_page, 255U, 248U, 240U, 255U);
    (void)SDL_SetRenderDrawColor(app->renderer, 230U, 74U, 59U, 255U);
    (void)SDL_RenderRect(app->renderer, &back_page);
    draw_filled_rect(app->renderer, &front_page, 255U, 248U, 240U, 255U);
    (void)SDL_SetRenderDrawColor(app->renderer, 230U, 74U, 59U, 255U);
    (void)SDL_RenderRect(app->renderer, &front_page);
    draw_circle(
        app->renderer,
        (int)(front_page.x + front_page.w - 15.0F),
        (int)(front_page.y + 16.0F),
        4);
    draw_line(
        app->renderer,
        front_page.x + 8.0F,
        front_page.y + front_page.h - 12.0F,
        front_page.x + 27.0F,
        front_page.y + 38.0F);
    draw_line(
        app->renderer,
        front_page.x + 27.0F,
        front_page.y + 38.0F,
        front_page.x + front_page.w - 7.0F,
        front_page.y + front_page.h - 12.0F);

    title_width = app_text_width_sized(app, title, title_size);
    subtitle_width = app_text_width_sized(app, subtitle, subtitle_size);
    (void)app_draw_text_sized(
        app,
        center_x - title_width / 2.0F,
        icon_top + 104.0F,
        title,
        title_size,
        dragging ? 230U : 122U,
        dragging ? 74U : 36U,
        dragging ? 59U : 28U,
        255U);
    (void)app_draw_text_sized(
        app,
        center_x - subtitle_width / 2.0F,
        icon_top + 145.0F,
        subtitle,
        subtitle_size,
        105U,
        78U,
        70U,
        255U);
    if (processing && app->import_pending.count > 0U) {
        unsigned percent = loim_progress_percent(
            app->import_pending_index, app->import_pending.count);
        SDL_FRect track = {
            center_x - 150.0F,
            icon_top + 184.0F,
            300.0F,
            8.0F
        };
        SDL_FRect progress = track;

        progress.w *= (float)percent / 100.0F;
        draw_filled_rect(app->renderer, &track, 225U, 211U, 202U, 255U);
        draw_filled_rect(app->renderer, &progress, 230U, 74U, 59U, 255U);
    }
}

static void app_render_drop_overlay(
    app_state *app,
    float width,
    float top,
    float bottom)
{
    const char *label = app_translation(app, LOIM_TEXT_DROP_RELEASE);
    const float label_size = 22.0F;
    float label_width = 0.0F;
    float label_height = 0.0F;
    SDL_FRect overlay = {0.0F, top, width, bottom - top};
    SDL_FRect border = {10.0F, top + 10.0F, width - 20.0F, bottom - top - 20.0F};
    SDL_FRect border_inner = {
        border.x + 1.0F,
        border.y + 1.0F,
        border.w - 2.0F,
        border.h - 2.0F
    };
    SDL_FRect card;
    SDL_FRect shadow;
    float card_cx;
    float icon_x;
    float icon_y;

    app_text_size_sized(app, label, label_size, &label_width, &label_height);

    card.w = label_width + 96.0F;
    if (card.w < 260.0F) {
        card.w = 260.0F;
    }
    card.h = 148.0F;
    card.x = (width - card.w) / 2.0F;
    card.y = top + (bottom - top - card.h) / 2.0F;
    card_cx = card.x + card.w / 2.0F;

    /* 遮罩整个工作区 */
    draw_filled_rect(app->renderer, &overlay, 255U, 248U, 240U, 165U);

    /* 双次偏移绘制虚线框，呈现 2px 粗的虚线边框 */
    (void)SDL_SetRenderDrawColor(app->renderer, 230U, 74U, 59U, 255U);
    draw_dashed_rect(app->renderer, &border, 14.0F, 8.0F);
    draw_dashed_rect(app->renderer, &border_inner, 14.0F, 8.0F);

    /* 居中卡片：柔和投影 + 底色 + 描边 */
    shadow.x = card.x + 3.0F;
    shadow.y = card.y + 5.0F;
    shadow.w = card.w;
    shadow.h = card.h;
    draw_filled_rect(app->renderer, &shadow, 122U, 36U, 28U, 40U);
    draw_filled_rect(app->renderer, &card, 255U, 248U, 240U, 252U);
    (void)SDL_SetRenderDrawColor(app->renderer, 230U, 74U, 59U, 255U);
    (void)SDL_RenderRect(app->renderer, &card);

    /* 拖放图标：向下箭头落入托盘，整体在卡片内居中 */
    icon_x = card_cx;
    icon_y = card.y + 26.0F;
    draw_line(app->renderer, icon_x, icon_y, icon_x, icon_y + 26.0F);
    draw_line(app->renderer, icon_x + 1.0F, icon_y, icon_x + 1.0F, icon_y + 26.0F);
    draw_line(
        app->renderer, icon_x - 7.0F, icon_y + 18.0F, icon_x, icon_y + 26.0F);
    draw_line(
        app->renderer, icon_x + 8.0F, icon_y + 18.0F, icon_x + 1.0F, icon_y + 26.0F);
    draw_line(
        app->renderer, icon_x - 18.0F, icon_y + 28.0F, icon_x - 18.0F, icon_y + 40.0F);
    draw_line(
        app->renderer, icon_x + 19.0F, icon_y + 28.0F, icon_x + 19.0F, icon_y + 40.0F);
    draw_line(
        app->renderer, icon_x - 18.0F, icon_y + 39.0F, icon_x + 19.0F, icon_y + 39.0F);
    draw_line(
        app->renderer, icon_x - 18.0F, icon_y + 40.0F, icon_x + 19.0F, icon_y + 40.0F);

    (void)app_draw_text_sized(
        app,
        card_cx - label_width / 2.0F,
        card.y + 92.0F,
        label,
        label_size,
        122U,
        36U,
        28U,
        255U);
}

static app_login_layout app_login_layout_for(float width, float height)
{
    app_login_layout layout;

    layout.panel.w = 520.0F;
    layout.panel.h = 354.0F;
    layout.panel.x = (width - layout.panel.w) / 2.0F;
    layout.panel.y = (height - layout.panel.h) / 2.0F;
    layout.email.x = layout.panel.x + 30.0F;
    layout.email.y = layout.panel.y + 82.0F;
    layout.email.w = layout.panel.w - 60.0F;
    layout.email.h = 38.0F;
    layout.password = layout.email;
    layout.password.y += 72.0F;
    layout.register_link.x = layout.panel.x + 30.0F;
    layout.register_link.y = layout.panel.y + 252.0F;
    layout.register_link.w = 260.0F;
    layout.register_link.h = 24.0F;
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
        layout.panel.y + 218.0F,
        app->login_message,
        100U,
        100U,
        100U,
        255U);
    (void)app_draw_text(
        app,
        layout.register_link.x,
        layout.register_link.y + 3.0F,
        app_translation(app, LOIM_TEXT_REGISTER_NOW),
        0U,
        102U,
        204U,
        255U);
    draw_filled_rect(app->renderer, &layout.cancel, 225U, 225U, 225U, 255U);
    (void)SDL_SetRenderDrawColor(app->renderer, 115U, 115U, 115U, 255U);
    (void)SDL_RenderRect(app->renderer, &layout.cancel);
    app_draw_button_label(
        app,
        &layout.cancel,
        app_translation(app, LOIM_TEXT_CANCEL),
        14.0F,
        80U,
        80U,
        80U);
    draw_filled_rect(
        app->renderer,
        &layout.submit,
        app->login_submitting ? 150U : 0U,
        app->login_submitting ? 150U : 122U,
        app->login_submitting ? 150U : 255U,
        255U);
    app_draw_button_label(
        app,
        &layout.submit,
        submit_label,
        14.0F,
        255U,
        255U,
        255U);
}

static app_about_layout app_about_layout_for(float width, float height)
{
    app_about_layout layout;

    layout.panel.w = 480.0F;
    layout.panel.h = 430.0F;
    layout.panel.x = (width - layout.panel.w) / 2.0F;
    layout.panel.y = (height - layout.panel.h) / 2.0F;
    layout.website.x = layout.panel.x + 42.0F;
    layout.website.y = layout.panel.y + 290.0F;
    layout.website.w = 180.0F;
    layout.website.h = 28.0F;
    layout.ok.w = 96.0F;
    layout.ok.h = 34.0F;
    layout.ok.x = layout.panel.x + (layout.panel.w - layout.ok.w) / 2.0F;
    layout.ok.y = layout.panel.y + layout.panel.h - 58.0F;
    return layout;
}

static const char *app_subscription_name(const app_state *app)
{
    if (SDL_strcasecmp(app->subscription_type, "monthly") == 0) {
        return app_translation(app, LOIM_TEXT_SUBSCRIPTION_MONTHLY);
    }
    if (SDL_strcasecmp(app->subscription_type, "yearly") == 0) {
        return app_translation(app, LOIM_TEXT_SUBSCRIPTION_YEARLY);
    }
    if (SDL_strcasecmp(app->subscription_type, "team") == 0) {
        return app_translation(app, LOIM_TEXT_SUBSCRIPTION_TEAM);
    }
    if (SDL_strcasecmp(app->subscription_type, "enterprise") == 0) {
        return app_translation(app, LOIM_TEXT_SUBSCRIPTION_ENTERPRISE);
    }
    if (app->licensed) {
        return app_translation(app, LOIM_TEXT_SUBSCRIPTION_PAID);
    }
    return app_translation(app, LOIM_TEXT_SUBSCRIPTION_FREE);
}

static void app_render_about(app_state *app, float width, float height)
{
    app_about_layout layout = app_about_layout_for(width, height);
    SDL_FRect overlay = {0.0F, 0.0F, width, height};
    char version[96];
    char account[256];
    char subscription[160];
    char expires[128];
    const char *ok = app_translation(app, LOIM_TEXT_ABOUT_OK);

    (void)snprintf(
        version,
        sizeof(version),
        app_translation(app, LOIM_TEXT_ABOUT_VERSION_FORMAT),
        LOIM_APP_VERSION,
        app_translation(
            app,
            loim_edition_is_pro()
                ? LOIM_TEXT_EDITION_PRO
                : LOIM_TEXT_EDITION_COMMUNITY));
    draw_filled_rect(app->renderer, &overlay, 24U, 24U, 24U, 150U);
    draw_filled_rect(app->renderer, &layout.panel, 255U, 248U, 240U, 255U);
    (void)SDL_SetRenderDrawColor(app->renderer, 105U, 105U, 105U, 255U);
    (void)SDL_RenderRect(app->renderer, &layout.panel);
    (void)app_draw_text(
        app, layout.panel.x + 42.0F, layout.panel.y + 30.0F,
        app_translation(app, LOIM_TEXT_ABOUT_TITLE),
        32U, 32U, 32U, 255U);
    (void)app_draw_text(
        app, layout.panel.x + 42.0F, layout.panel.y + 76.0F,
        app_translation(app, LOIM_TEXT_APP_TITLE),
        50U, 50U, 50U, 255U);
    (void)app_draw_text(
        app, layout.panel.x + 42.0F, layout.panel.y + 108.0F,
        version, 90U, 90U, 90U, 255U);
    (void)app_draw_text(
        app, layout.panel.x + 42.0F, layout.panel.y + 154.0F,
        app_translation(app, LOIM_TEXT_ABOUT_ACCOUNT_SECTION),
        48U, 48U, 48U, 255U);
    if (app->logged_in) {
        (void)snprintf(
            account,
            sizeof(account),
            app_translation(app, LOIM_TEXT_ABOUT_ACCOUNT_EMAIL_FORMAT),
            app->account_email);
        if (app->licensed) {
            (void)snprintf(
                subscription,
                sizeof(subscription),
                app_translation(
                    app, LOIM_TEXT_ABOUT_SUBSCRIPTION_ACTIVE_FORMAT),
                app_subscription_name(app));
        } else {
            app_copy_text(
                subscription,
                sizeof(subscription),
                app_translation(app, LOIM_TEXT_ABOUT_SUBSCRIPTION_INACTIVE));
        }
        (void)app_draw_text(
            app, layout.panel.x + 42.0F, layout.panel.y + 186.0F,
            account, 82U, 82U, 82U, 255U);
        (void)app_draw_text(
            app, layout.panel.x + 42.0F, layout.panel.y + 216.0F,
            subscription, 82U, 82U, 82U, 255U);
        if (app->subscription_expires_at[0] != '\0') {
            (void)snprintf(
                expires,
                sizeof(expires),
                app_translation(
                    app, LOIM_TEXT_ABOUT_SUBSCRIPTION_EXPIRES_FORMAT),
                app->subscription_expires_at);
            (void)app_draw_text(
                app, layout.panel.x + 42.0F, layout.panel.y + 246.0F,
                expires, 82U, 82U, 82U, 255U);
        }
    } else {
        (void)app_draw_text(
            app, layout.panel.x + 42.0F, layout.panel.y + 186.0F,
            app_translation(
                app,
                app->login_submitting
                    ? LOIM_TEXT_RESTORING_LOGIN
                    : LOIM_TEXT_ABOUT_NOT_SIGNED_IN),
            82U, 82U, 82U, 255U);
    }
    (void)app_draw_text(
        app, layout.website.x, layout.website.y + 4.0F,
        app_translation(app, LOIM_TEXT_ABOUT_WEBSITE),
        0U, 102U, 204U, 255U);
    (void)app_draw_text(
        app, layout.panel.x + 42.0F, layout.panel.y + 330.0F,
        app_translation(app, LOIM_TEXT_ABOUT_COPYRIGHT),
        90U, 90U, 90U, 255U);
    draw_filled_rect(app->renderer, &layout.ok, 230U, 80U, 64U, 255U);
    app_draw_button_label(
        app,
        &layout.ok,
        ok,
        14.0F,
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
    loim_workspace_mode mode;
    SDL_FRect content;

    if (!SDL_GetRenderOutputSize(app->renderer, &output_width, &output_height)) {
        return;
    }
    width = (float)output_width;
    height = (float)output_height;
    content_bottom = height - LOIM_STATUSBAR_HEIGHT;
    mode = loim_workspace_mode_resolve(
        app->image_count, app->import_progress_active, app->workspace_ready);
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
    if (mode == LOIM_WORKSPACE_READY) {
        draw_filled_rect(app->renderer, &content, 150U, 150U, 150U, 255U);
        app_render_preview(
            app, 0.0F, LOIM_TOOLBAR_HEIGHT, left_width, content_bottom);
        app_render_editor(
            app, right_left, LOIM_TOOLBAR_HEIGHT, right_width, content_bottom);
    } else {
        app_render_empty(
            app,
            0.0F,
            LOIM_TOOLBAR_HEIGHT,
            width,
            content_bottom,
            mode);
    }
    if (loim_workspace_shows_divider(mode)) {
        app_render_divider(
            app, divider_x, LOIM_TOOLBAR_HEIGHT, content_bottom);
    }
    if (app->dropped.collecting && mode == LOIM_WORKSPACE_READY) {
        app_render_drop_overlay(
            app, width, LOIM_TOOLBAR_HEIGHT, content_bottom);
    }
    /* Keep the toolbar above the scrolling canvas and drop overlay. */
    app_render_toolbar(app, width);
    app_render_statusbar(app, width, height);
    if (app->login_open) {
        app_render_login(app, width, height);
    } else if (app->about_open) {
        app_render_about(app, width, height);
    }
    (void)SDL_RenderPresent(app->renderer);
}

static bool app_adjust_preview_scale(app_state *app, int direction)
{
    float old_scale = app->preview_scale;
    float new_scale = loim_preview_scale_adjust(old_scale, direction);
    int output_width = 0;
    int output_height = 0;

    if (new_scale == old_scale) {
        return false;
    }
    if (SDL_GetRenderOutputSize(app->renderer, &output_width, &output_height) &&
        output_width > 0 &&
        output_height > (int)(LOIM_TOOLBAR_HEIGHT + LOIM_STATUSBAR_HEIGHT)) {
        float preview_width = (float)output_width * app->split_ratio;
        float viewport_height = (float)output_height - LOIM_TOOLBAR_HEIGHT -
            LOIM_STATUSBAR_HEIGHT;

        app_reanchor_preview_scroll(
            app,
            preview_width,
            preview_width,
            viewport_height,
            old_scale,
            new_scale);
    }
    app->preview_scale = new_scale;
    app_save_settings(app);
    return true;
}

static void app_set_split_ratio(
    app_state *app,
    float split_ratio,
    int output_width,
    int output_height)
{
    float old_ratio = app->split_ratio;

    if (split_ratio < LOIM_SPLIT_RATIO_MIN) {
        split_ratio = LOIM_SPLIT_RATIO_MIN;
    } else if (split_ratio > LOIM_SPLIT_RATIO_MAX) {
        split_ratio = LOIM_SPLIT_RATIO_MAX;
    }
    if (split_ratio == old_ratio) {
        return;
    }
    if (output_width > 0 &&
        output_height > (int)(LOIM_TOOLBAR_HEIGHT + LOIM_STATUSBAR_HEIGHT)) {
        float viewport_height = (float)output_height - LOIM_TOOLBAR_HEIGHT -
            LOIM_STATUSBAR_HEIGHT;

        app_reanchor_preview_scroll(
            app,
            (float)output_width * old_ratio,
            (float)output_width * split_ratio,
            viewport_height,
            app->preview_scale,
            app->preview_scale);
    }
    app->split_ratio = split_ratio;
}

static void app_handle_click(app_state *app, float x, float y)
{
    size_t index;

    if (y > LOIM_TOOLBAR_HEIGHT) {
        int output_width = 0;
        int output_height = 0;
        loim_workspace_mode mode = loim_workspace_mode_resolve(
            app->image_count,
            app->import_progress_active,
            app->workspace_ready);

        if (mode == LOIM_WORKSPACE_EMPTY && !app->dropped.collecting &&
            toolbar_action_enabled(app, TOOLBAR_OPEN) &&
            SDL_GetRenderOutputSize(
                app->renderer, &output_width, &output_height) &&
            y < (float)output_height - LOIM_STATUSBAR_HEIGHT) {
            app_open_dialog(app);
        }
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
        case TOOLBAR_CLEAR:
            (void)app_clear_document(app);
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
            app_save_settings(app);
            break;
        case TOOLBAR_PAGE_NUMBERS:
            app->page_number_mode = loim_page_number_next(app->page_number_mode);
            app_set_status(
                app,
                app_translation(
                    app,
                    app->page_number_mode == LOIM_PAGE_NUMBER_BOTTOM_RIGHT
                        ? LOIM_TEXT_PAGE_NUMBERS_BOTTOM_RIGHT
                        : app->page_number_mode == LOIM_PAGE_NUMBER_BOTTOM_CENTER
                            ? LOIM_TEXT_PAGE_NUMBERS_BOTTOM_CENTER
                            : LOIM_TEXT_PAGE_NUMBERS_NONE));
            app_save_settings(app);
            break;
        case TOOLBAR_AUTO_SPLIT:
            (void)app_rebuild_layout(app);
            app_set_status(app, app_translation(app, LOIM_TEXT_AUTO_SPLIT_APPLIED));
            break;
        case TOOLBAR_LESS_MARGIN:
            app->margin_ratio -= 0.01F;
            if (app->margin_ratio < LOIM_MARGIN_RATIO_MIN) {
                app->margin_ratio = LOIM_MARGIN_RATIO_MIN;
            }
            app_set_status(app, app_translation(app, LOIM_TEXT_MARGIN_REDUCED));
            app_save_settings(app);
            break;
        case TOOLBAR_MORE_MARGIN:
            app->margin_ratio += 0.01F;
            if (app->margin_ratio > LOIM_MARGIN_RATIO_MAX) {
                app->margin_ratio = LOIM_MARGIN_RATIO_MAX;
            }
            app_set_status(app, app_translation(app, LOIM_TEXT_MARGIN_INCREASED));
            app_save_settings(app);
            break;
        case TOOLBAR_PREVIEW_WIDER:
            if (app_adjust_preview_scale(app, 1)) {
                app_set_status(
                    app, app_translation(app, LOIM_TEXT_PREVIEW_ENLARGED));
            }
            break;
        case TOOLBAR_PREVIEW_NARROWER:
            if (app_adjust_preview_scale(app, -1)) {
                app_set_status(
                    app, app_translation(app, LOIM_TEXT_PREVIEW_REDUCED));
            }
            break;
        case TOOLBAR_LOGIN:
            app_open_login(app);
            break;
        case TOOLBAR_EXPORT:
            app_show_pro_prompt(app);
            break;
        case TOOLBAR_PRINT:
            app_show_pro_prompt(app);
            break;
        case TOOLBAR_ABOUT:
            app_open_about(app);
            break;
        }
        return;
    }
}

static void app_handle_dialog_event(app_state *app, SDL_Event *event)
{
    app_file_dialog_result *result = event->user.data1;

    if (result == NULL || result != app->file_dialog_result) {
        return;
    }
    (void)SDL_SetAtomicInt(
        &result->completion_state, APP_COMPLETION_HANDLED);
    app->file_dialog_result = NULL;
    app->dialog_open = false;
    if (app->quit_after_dialog) {
        app_file_dialog_result_destroy(result);
        app->running = false;
        return;
    }
    if (result->outcome == APP_FILE_DIALOG_SELECTED) {
        app_import_batch(app, &result->paths);
    } else if (result->outcome == APP_FILE_DIALOG_ERROR) {
        app_set_status(app, app_translation(app, LOIM_TEXT_FILE_DIALOG_ERROR));
    } else {
        app_set_status(app, app_translation(app, LOIM_TEXT_IMPORT_CANCELED));
    }
    app_file_dialog_result_destroy(result);
}

static void app_poll_file_dialog_completion(app_state *app)
{
    app_file_dialog_result *result = app->file_dialog_result;
    SDL_Event event;

    if (result == NULL ||
        SDL_GetAtomicInt(&result->completion_state) !=
            APP_COMPLETION_PUSH_FAILED) {
        return;
    }
    SDL_zero(event);
    event.type = result->completion_event;
    event.user.data1 = result;
    app_handle_dialog_event(app, &event);
}

/* A dragged seam gets exactly one manual override.  Before recording the new
   override we delete the manual hint that produced the seam's current
   position: leaving it in place would pin the seam, because the layout scores
   every manual hint with the same bonus and prefers the one nearest the ideal
   page height. */
static void app_remove_manual_hints_at(app_state *app, uint64_t seam_virtual_row)
{
    uint64_t cursor = 0U;
    size_t source_index;

    for (source_index = 0U; source_index < app->image_count; ++source_index) {
        const app_image *image = &app->images[source_index];
        uint64_t normalized_height =
            ((uint64_t)image->height * (uint64_t)LOIM_LAYOUT_WIDTH +
             (uint64_t)image->width - 1U) /
            (uint64_t)image->width;
        size_t hint_index = loim_document_split_hint_count(
            app->document, source_index);

        while (hint_index > 0U) {
            loim_split_hint_info info;

            hint_index -= 1U;
            if (loim_document_split_hint_at(
                    app->document, source_index, hint_index, &info) == LOIM_OK &&
                info.kind == LOIM_SPLIT_HINT_MANUAL) {
                uint64_t candidate = cursor +
                    ((uint64_t)info.row * normalized_height +
                     (uint64_t)image->height / 2U) /
                    (uint64_t)image->height;

                if (candidate == seam_virtual_row) {
                    (void)loim_document_remove_split_hint(
                        app->document, source_index, hint_index);
                }
            }
        }
        cursor += normalized_height;
        if (source_index + 1U < app->image_count) {
            cursor += (uint64_t)app->layout_options.inter_image_gap_px;
        }
    }
}

static void app_commit_split_drag(app_state *app, float editor_width)
{
    const loim_page *page;
    float scale;
    double candidate;
    double legal_minimum;
    double legal_maximum;
    uint64_t seam_position;
    uint64_t virtual_row;
    uint64_t cursor = 0U;
    size_t source_index;
    loim_status status = LOIM_ERROR_NOT_FOUND;

    if (app->split_drag_index == SIZE_MAX ||
        app->split_drag_index + 1U >= app->layout.page_count) {
        return;
    }
    page = &app->layout.pages[app->split_drag_index];
    scale = editor_scale(editor_width);
    seam_position = (uint64_t)page->virtual_y_px + (uint64_t)page->height_px;
    candidate = (double)seam_position +
        (double)(app->split_drag_current_y - app->split_drag_start_y) /
            (double)scale;
    /* Keep the drop inside the page's legal height range so the manual hint
       is always honored instead of snapping back to the automatic break. */
    legal_minimum = (double)page->virtual_y_px +
        (double)app->layout_options.minimum_page_height_px;
    legal_maximum = (double)page->virtual_y_px +
        (double)app->layout_options.maximum_page_height_px;
    if (legal_maximum > (double)app->layout.virtual_height_px - 1.0) {
        legal_maximum = (double)app->layout.virtual_height_px - 1.0;
    }
    if (candidate < legal_minimum) {
        candidate = legal_minimum;
    } else if (candidate > legal_maximum) {
        candidate = legal_maximum;
    }
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
            app_remove_manual_hints_at(app, seam_position);
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

static void app_complete_drop_batch(app_state *app)
{
    if (loim_import_queue_complete(&app->dropped) != LOIM_OK ||
        app->drop_batch_failed) {
        app_set_status(app, app_translation(app, LOIM_TEXT_DROP_FAILED));
    } else {
        app_import_drop_queue(app);
    }
    loim_import_queue_destroy(&app->dropped);
    loim_import_queue_init(&app->dropped);
    app->drop_batch_failed = false;
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
                               app->mouse_x,
                               app->mouse_y,
                               &layout.register_link)) {
                    if (!SDL_OpenURL(LOIM_REGISTER_URL)) {
                        app_copy_text(
                            app->login_message,
                            sizeof(app->login_message),
                            app_translation(
                                app, LOIM_TEXT_REGISTER_OPEN_FAILED));
                    }
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

static bool app_handle_about_event(app_state *app, SDL_Event *event)
{
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
                app_about_layout layout = app_about_layout_for(
                    (float)output_width, (float)output_height);

                if (point_in_rect(app->mouse_x, app->mouse_y, &layout.ok)) {
                    app_close_about(app);
                } else if (point_in_rect(
                               app->mouse_x, app->mouse_y, &layout.website) &&
                           !SDL_OpenURL(LOIM_WEBSITE_URL)) {
                    app_set_status(
                        app,
                        app_translation(app, LOIM_TEXT_ABOUT_OPEN_FAILED));
                }
            }
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE || event->key.key == SDLK_RETURN ||
            event->key.key == SDLK_KP_ENTER) {
            app_close_about(app);
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
    if (event->type == app->update_event) {
        app_handle_update_event(app, event);
        return;
    }
    (void)SDL_ConvertEventToRenderCoordinates(app->renderer, event);
    if (app->login_open && app_handle_login_event(app, event)) {
        return;
    }
    if (app->about_open && app_handle_about_event(app, event)) {
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
                app_set_split_ratio(
                    app,
                    app->mouse_x / (float)output_width,
                    output_width,
                    output_height);
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
                loim_workspace_mode mode = loim_workspace_mode_resolve(
                    app->image_count,
                    app->import_progress_active,
                    app->workspace_ready);
                SDL_FRect divider = {
                    divider_x - LOIM_DIVIDER_WIDTH,
                    LOIM_TOOLBAR_HEIGHT,
                    LOIM_DIVIDER_WIDTH * 2.0F,
                    (float)output_height - LOIM_TOOLBAR_HEIGHT - LOIM_STATUSBAR_HEIGHT
                };

                if (mode == LOIM_WORKSPACE_READY &&
                    point_in_rect(app->mouse_x, app->mouse_y, &divider)) {
                    app->divider_dragging = true;
                } else if (mode == LOIM_WORKSPACE_READY &&
                           app->mouse_x > divider_x) {
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
        loim_workspace_mode mode = loim_workspace_mode_resolve(
            app->image_count,
            app->import_progress_active,
            app->workspace_ready);

        app->mouse_x = event->wheel.mouse_x;
        app->mouse_y = event->wheel.mouse_y;
        if (mode != LOIM_WORKSPACE_READY) {
            break;
        }
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
            if (!app->dialog_open) {
                app->running = false;
            }
        } else if ((event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0U &&
                   event->key.key == SDLK_O &&
                   toolbar_action_enabled(app, TOOLBAR_OPEN)) {
            app_open_dialog(app);
        } else if ((event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0U &&
                   event->key.key == SDLK_N && !app->dialog_open) {
            (void)app_clear_document(app);
        } else if (event->key.key == SDLK_PLUS || event->key.key == SDLK_EQUALS) {
            if (toolbar_action_enabled(app, TOOLBAR_PREVIEW_WIDER) &&
                app_adjust_preview_scale(app, 1)) {
                app_set_status(
                    app, app_translation(app, LOIM_TEXT_PREVIEW_ENLARGED));
            }
        } else if (event->key.key == SDLK_MINUS) {
            if (toolbar_action_enabled(app, TOOLBAR_PREVIEW_NARROWER) &&
                app_adjust_preview_scale(app, -1)) {
                app_set_status(
                    app, app_translation(app, LOIM_TEXT_PREVIEW_REDUCED));
            }
        }
        break;
    case SDL_EVENT_DROP_BEGIN:
        (void)loim_import_queue_begin(&app->dropped);
        app->drop_batch_failed = false;
        app_set_status(app, app_translation(app, LOIM_TEXT_DROP_RELEASE));
        break;
    case SDL_EVENT_DROP_POSITION:
        if (!app->dropped.collecting) {
            (void)loim_import_queue_begin(&app->dropped);
            app->drop_batch_failed = false;
        }
        app_set_status(app, app_translation(app, LOIM_TEXT_DROP_RELEASE));
        break;
    case SDL_EVENT_DROP_FILE:
        if (event->drop.data != NULL) {
            loim_status status = loim_import_queue_add(
                &app->dropped, event->drop.data);

            if (status != LOIM_OK) {
                app->drop_batch_failed = true;
                app_set_status(app, app_translation(app, LOIM_TEXT_DROP_FAILED));
            } else if (event->drop.windowID == 0U) {
                app_complete_drop_batch(app);
            }
        }
        break;
    case SDL_EVENT_DROP_COMPLETE:
        if (app->dropped.collecting) {
            app_complete_drop_batch(app);
        }
        break;
    default:
        break;
    }
}

static bool app_settings_path(char *output, size_t output_capacity)
{
    char *preference_path = SDL_GetPrefPath("ctdy123", "LoimReader");
    int written;

    if (preference_path == NULL) {
        return false;
    }
    written = snprintf(
        output, output_capacity, "%ssettings.ini", preference_path);
    SDL_free(preference_path);
    return written > 0 && (size_t)written < output_capacity;
}

static void app_save_settings(app_state *app)
{
    char path[4096];
    char text[LOIM_SETTINGS_TEXT_CAPACITY];
    loim_settings settings;

    if (app == NULL || !app->persist_settings) {
        return;
    }
    if (!app_settings_path(path, sizeof(path))) {
        return;
    }
    settings.columns = app->columns;
    settings.page_number_mode = app->page_number_mode;
    settings.margin_ratio = app->margin_ratio;
    settings.preview_scale = app->preview_scale;
    settings.split_ratio = app->split_ratio;
    settings.window_width = LOIM_WINDOW_DEFAULT_WIDTH;
    settings.window_height = LOIM_WINDOW_DEFAULT_HEIGHT;
    settings.window_x = 0;
    settings.window_y = 0;
    settings.has_window_position = false;
    if (app->window != NULL) {
        int window_x = 0;
        int window_y = 0;

        (void)SDL_GetWindowSize(
            app->window, &settings.window_width, &settings.window_height);
        if (SDL_GetWindowPosition(app->window, &window_x, &window_y)) {
            settings.window_x = window_x;
            settings.window_y = window_y;
            settings.has_window_position = true;
        }
    }
    if (loim_settings_encode(&settings, text, sizeof(text)) != LOIM_OK) {
        return;
    }
    (void)SDL_SaveFile(path, text, strlen(text));
}

static void app_load_settings(app_state *app)
{
    char path[4096];
    char buffer[512];
    char *text;
    size_t size = 0U;
    loim_settings settings;

    if (app == NULL || app->headless) {
        return;
    }
    if (!app_settings_path(path, sizeof(path))) {
        return;
    }
    text = SDL_LoadFile(path, &size);
    if (text == NULL) {
        return;
    }
    if (size < sizeof(buffer)) {
        /* Seed with defaults overlaid by the in-app values so missing keys
         * keep them. */
        loim_settings_defaults(&settings);
        settings.columns = app->columns;
        settings.page_number_mode = app->page_number_mode;
        settings.margin_ratio = app->margin_ratio;
        settings.preview_scale = app->preview_scale;
        settings.split_ratio = app->split_ratio;
        memcpy(buffer, text, size);
        buffer[size] = '\0';
        if (loim_settings_decode(buffer, &settings) == LOIM_OK) {
            app->columns = loim_columns_normalize(settings.columns);
            app->page_number_mode =
                loim_page_number_normalize(settings.page_number_mode);
            app->margin_ratio = settings.margin_ratio;
            app->preview_scale = settings.preview_scale;
            app->split_ratio = settings.split_ratio;
            app->pending_window_width = settings.window_width;
            app->pending_window_height = settings.window_height;
            app->pending_window_x = settings.window_x;
            app->pending_window_y = settings.window_y;
            app->has_pending_window_position = settings.has_window_position;
            app->export_columns = loim_columns_normalize(app->columns);
            app->export_margin_ratio = app->margin_ratio;
            app->export_page_number_mode = app->page_number_mode;
        }
    }
    SDL_free(text);
}

static void app_apply_window_geometry(app_state *app)
{
    int width = app->pending_window_width;
    int height = app->pending_window_height;

    if (width < LOIM_WINDOW_MIN_WIDTH) {
        width = LOIM_WINDOW_MIN_WIDTH;
    }
    if (height < LOIM_WINDOW_MIN_HEIGHT) {
        height = LOIM_WINDOW_MIN_HEIGHT;
    }
    (void)SDL_SetWindowSize(app->window, width, height);
    if (!app->has_pending_window_position) {
        return;
    }
    {
        SDL_Rect target = {
            app->pending_window_x,
            app->pending_window_y,
            width,
            height
        };
        SDL_DisplayID *displays;
        bool visible = false;
        int count = 0;
        int index;

        /* The saved position is only honored when a meaningful part of the
         * window (including its title bar) is still on a connected display;
         * otherwise fall back to a centered window. */
        displays = SDL_GetDisplays(&count);
        if (displays != NULL) {
            for (index = 0; index < count; ++index) {
                SDL_Rect bounds;
                SDL_Rect overlap;

                if (SDL_GetDisplayBounds(displays[index], &bounds) &&
                    SDL_GetRectIntersection(&target, &bounds, &overlap) &&
                    overlap.w >= 100 && overlap.h >= 60) {
                    visible = true;
                    break;
                }
            }
            SDL_free(displays);
        }
        if (visible) {
            (void)SDL_SetWindowPosition(
                app->window,
                app->pending_window_x,
                app->pending_window_y);
        } else {
            (void)SDL_SetWindowPosition(
                app->window,
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED);
        }
    }
}

static void app_destroy(app_state *app)
{
    app_save_settings(app);
    if (app->update_thread != NULL) {
        SDL_WaitThread(app->update_thread, NULL);
        app->update_thread = NULL;
    }
    app_update_task_destroy(app->update_task);
    app->update_task = NULL;
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
    app_file_dialog_result_destroy(app->file_dialog_result);
    app->file_dialog_result = NULL;
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
    bool saved_credentials = false;
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    memset(app, 0, sizeof(*app));
    app->preview_scale = LOIM_PREVIEW_SCALE_DEFAULT;
    app->columns = 1U;
    app->export_columns = 1U;
    app->page_number_mode = LOIM_PAGE_NUMBER_NONE;
    app->export_page_number_mode = LOIM_PAGE_NUMBER_NONE;
    app->split_ratio = 0.5F;
    app->margin_ratio = 0.08F;
    app->split_drag_index = SIZE_MAX;
    app->import_generation = 1U;
    app->headless = headless;
    app->running = true;
    if (!headless) {
        app_load_settings(app);
        if (loim_edition_is_pro()) {
            saved_credentials = app_load_saved_credentials(app);
        }
    }
    loim_import_queue_init(&app->dropped);
    loim_layout_options_a4(&app->layout_options, LOIM_LAYOUT_WIDTH);
    status = loim_document_create(&app->document);
    if (status != LOIM_OK) {
        (void)fprintf(stderr, "Cannot create document: %s\n", loim_status_string(status));
        return false;
    }
    if (!SDL_SetHintWithPriority(
            SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1", SDL_HINT_OVERRIDE)) {
        (void)fprintf(stderr, "Cannot enable focus click-through: %s\n", SDL_GetError());
        loim_document_destroy(app->document);
        app->document = NULL;
        return false;
    }
    if (!SDL_SetAppMetadata(
            "LoimReader", LOIM_APP_VERSION, "com.ctdy123.loimreader")) {
        (void)fprintf(stderr, "Cannot set application metadata: %s\n", SDL_GetError());
        loim_document_destroy(app->document);
        app->document = NULL;
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
    SDL_SetEventEnabled(SDL_EVENT_DROP_TEXT, false);
    if (!SDL_CreateWindowAndRenderer(
            app_translation(app, LOIM_TEXT_APP_TITLE),
            LOIM_WINDOW_DEFAULT_WIDTH,
            LOIM_WINDOW_DEFAULT_HEIGHT,
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
    (void)SDL_SetWindowMinimumSize(
        app->window, LOIM_WINDOW_MIN_WIDTH, LOIM_WINDOW_MIN_HEIGHT);
    if (!headless) {
        app_apply_window_geometry(app);
    }
    app->dialog_event = SDL_RegisterEvents(5);
    if (app->dialog_event == 0U) {
        (void)fprintf(stderr, "Cannot allocate file dialog event: %s\n", SDL_GetError());
        app_destroy(app);
        return false;
    }
    app->import_event = app->dialog_event + 1U;
    app->auth_event = app->dialog_event + 2U;
    app->export_event = app->dialog_event + 3U;
    app->update_event = app->dialog_event + 4U;
    if (!headless) {
        app->http_ready = loim_http_initialize();
        if (app->http_ready) {
            app->machine_code_ready = app_initialize_machine_code(app);
        }
    }
    (void)SDL_SetRenderVSync(app->renderer, 1);
    (void)SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
    app_set_status(
        app,
        app->http_ready || headless
            ? app_translation(app, LOIM_TEXT_READY)
            : app_translation(app, LOIM_TEXT_LOGIN_UNAVAILABLE));
    if (saved_credentials && app->http_ready && app->machine_code_ready) {
        if (app->auth_token[0] != '\0') {
            app_submit_session_restore(app);
        } else {
            app_open_login(app);
        }
    }
#if !defined(_WIN32) && !defined(__APPLE__)
    if (!headless) {
        (void)app_ensure_linux_desktop_shortcut(false);
    }
#endif
    /* Only persist once initialization fully succeeded, so a failed startup
     * never overwrites the user's saved preferences with defaults. */
    app->persist_settings = !headless;
    return true;
}

static bool app_wait_for_import_idle(app_state *app, Uint64 timeout_ms)
{
    Uint64 started = SDL_GetTicks();
    SDL_Event event;

    while (app->import_thread != NULL || app->import_progress_active) {
        while (SDL_PollEvent(&event)) {
            app_handle_event(app, &event);
        }
        app_poll_import_completion(app);
        if (SDL_GetTicks() - started > timeout_ms) {
            return false;
        }
        SDL_Delay(1U);
    }
    return true;
}

static bool app_run_async_clear_smoke(
    app_state *app,
    const char *discarded_path,
    const char *kept_path)
{
    path_batch first = {0};
    path_batch second = {0};
    bool success;

    app->columns = 3U;
    app->page_number_mode = LOIM_PAGE_NUMBER_BOTTOM_CENTER;
    app->margin_ratio = 0.12F;
    app->preview_scale = 1.2F;
    app->split_ratio = 0.62F;
    app->logged_in = true;
    app->licensed = true;
    app->test_force_import_completion_failure = true;
    app->headless = false;
    if (!path_batch_append(&first, discarded_path)) {
        return false;
    }
    app_import_batch(app, &first);
    path_batch_destroy(&first);
    success = app->import_thread != NULL && app->import_progress_active &&
        app_clear_document(app) && app->image_count == 0U &&
        app->layout.page_count == 0U && !app->workspace_ready &&
        !app->import_progress_active;
    if (success && path_batch_append(&second, kept_path)) {
        app_import_batch(app, &second);
    } else {
        success = false;
    }
    path_batch_destroy(&second);
    success = success && app_wait_for_import_idle(app, 10000U) &&
        app->import_thread == NULL && app->import_result == NULL &&
        app->import_pending.count == 0U && app->image_count == 1U &&
        app->layout.page_count > 0U && app->workspace_ready &&
        strcmp(app->images[0].path, kept_path) == 0 &&
        app->columns == 3U &&
        app->page_number_mode == LOIM_PAGE_NUMBER_BOTTOM_CENTER &&
        app->margin_ratio == 0.12F && app->preview_scale == 1.2F &&
        app->split_ratio == 0.62F && app->logged_in && app->licensed;
    if (success) {
        success = app_clear_document(app) && app->image_count == 0U &&
            app->layout.page_count == 0U && !app->workspace_ready &&
            app->columns == 3U &&
            app->page_number_mode == LOIM_PAGE_NUMBER_BOTTOM_CENTER &&
            app->margin_ratio == 0.12F && app->preview_scale == 1.2F &&
            app->split_ratio == 0.62F && app->logged_in && app->licensed;
    }
    (void)printf(
        "async_clear=%s event_fallback=1 images=%zu pages=%zu\n",
        success ? "pass" : "fail",
        app->image_count,
        app->layout.page_count);
    return success;
}

int main(int argc, char **argv)
{
    app_state app;
    SDL_Event event;
    bool smoke_test = argc > 1 && strcmp(argv[1], "--smoke-test") == 0;
    bool export_smoke_test = argc > 1 &&
        strcmp(argv[1], "--export-smoke-test") == 0;
    bool async_clear_smoke_test = argc > 1 &&
        strcmp(argv[1], "--async-clear-smoke-test") == 0;
    bool persist_smoke_test = argc > 1 &&
        strcmp(argv[1], "--persist-smoke-test") == 0;
    bool ensure_desktop_shortcut = argc == 2 &&
        strcmp(argv[1], "--ensure-desktop-shortcut") == 0;
    bool headless = smoke_test || export_smoke_test || async_clear_smoke_test ||
        persist_smoke_test;
    int first_path = smoke_test ? 2 : export_smoke_test ? 6 : 1;

    if (ensure_desktop_shortcut) {
#if !defined(_WIN32) && !defined(__APPLE__)
        loim_shortcut_result result;

        if (!SDL_Init(0)) {
            return EXIT_FAILURE;
        }
        result = app_ensure_linux_desktop_shortcut(false);
        SDL_Quit();
        return result == LOIM_SHORTCUT_CREATED ||
               result == LOIM_SHORTCUT_ALREADY_PRESENT ||
               result == LOIM_SHORTCUT_CONFLICT
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
#else
        return EXIT_FAILURE;
#endif
    }

    if (async_clear_smoke_test && argc != 4) {
        (void)fprintf(
            stderr,
            "usage: %s --async-clear-smoke-test discarded-image kept-image\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    if (export_smoke_test) {
        (void)fprintf(
            stderr,
            "%s: --export-smoke-test is a Pro edition feature\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    if (!app_initialize(&app, headless)) {
        return EXIT_FAILURE;
    }
    app_start_update_check(&app);
    if (async_clear_smoke_test) {
        bool success = app_run_async_clear_smoke(&app, argv[2], argv[3]);

        app_destroy(&app);
        return success ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (persist_smoke_test) {
        char path[4096];
        char *backup = NULL;
        size_t backup_size = 0U;
        bool success;

        /* Back up any real user settings so the test leaves them untouched. */
        if (app_settings_path(path, sizeof(path))) {
            backup = SDL_LoadFile(path, &backup_size);
        }
        app.persist_settings = true;
        app.headless = false;
        app.columns = 3U;
        app.page_number_mode = LOIM_PAGE_NUMBER_BOTTOM_CENTER;
        app.margin_ratio = 0.12F;
        app.preview_scale = 1.2F;
        app.split_ratio = 0.62F;
        app_save_settings(&app);

        /* Reset to defaults, then verify the saved file restores them. */
        app.columns = 1U;
        app.page_number_mode = LOIM_PAGE_NUMBER_NONE;
        app.margin_ratio = 0.08F;
        app.preview_scale = LOIM_PREVIEW_SCALE_DEFAULT;
        app.split_ratio = 0.5F;
        app.export_columns = 1U;
        app.export_page_number_mode = LOIM_PAGE_NUMBER_NONE;
        app_load_settings(&app);
        success = app.columns == 3U &&
            app.page_number_mode == LOIM_PAGE_NUMBER_BOTTOM_CENTER &&
            app.margin_ratio == 0.12F && app.preview_scale == 1.2F &&
            app.split_ratio == 0.62F &&
            app.export_columns == 3U &&
            app.export_page_number_mode == LOIM_PAGE_NUMBER_BOTTOM_CENTER;

        {
            size_t loaded_columns = app.columns;
            double loaded_margin = (double)app.margin_ratio;
            double loaded_zoom = (double)app.preview_scale;
            double loaded_split = (double)app.split_ratio;

            /* Disable persistence before teardown so app_destroy cannot save
             * the test values over the user's restored file. */
            app.persist_settings = false;
            app_destroy(&app);
            if (app_settings_path(path, sizeof(path))) {
                if (backup != NULL) {
                    (void)SDL_SaveFile(path, backup, backup_size);
                } else {
                    (void)SDL_RemovePath(path);
                }
            }
            SDL_free(backup);
            (void)printf(
                "persist_smoke=%s columns=%zu margin=%.2f zoom=%.1f split=%.2f\n",
                success ? "pass" : "fail",
                loaded_columns,
                loaded_margin,
                loaded_zoom,
                loaded_split);
        }
        return success ? EXIT_SUCCESS : EXIT_FAILURE;
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
            app.layout.page_count > 0U &&
            SDL_GetHintBoolean(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, false);

        app_render(&app);
        app_destroy(&app);
        return success ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    while (app.running) {
        while (SDL_PollEvent(&event)) {
            app_handle_event(&app, &event);
        }
        app_poll_file_dialog_completion(&app);
        app_poll_import_completion(&app);
        app_poll_auth_completion(&app);
        app_poll_update_completion(&app);
        app_render(&app);
        SDL_Delay(8U);
    }
    app_destroy(&app);
    return EXIT_SUCCESS;
}
