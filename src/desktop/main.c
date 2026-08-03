#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loim/document.h"
#include "loim/image_probe.h"
#include "loim/layout.h"
#include "loim/seam.h"
#include "loim/status.h"

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

typedef struct app_image {
    char *path;
    SDL_Texture *texture;
    uint32_t width;
    uint32_t height;
} app_image;

typedef struct path_batch {
    char **paths;
    size_t count;
    size_t capacity;
    char *error;
} path_batch;

typedef struct app_state {
    SDL_Window *window;
    SDL_Renderer *renderer;
    loim_document *document;
    loim_layout layout;
    loim_layout_options layout_options;
    app_image *images;
    size_t image_count;
    size_t image_capacity;
    path_batch dropped;
    bool drop_active;
    bool dialog_open;
    bool quit_after_dialog;
    bool running;
    Uint32 dialog_event;
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
    bool two_columns;
    bool show_page_numbers;
    char status[256];
} app_state;

static const SDL_DialogFileFilter app_image_filters[] = {
    {"Image files", "png;jpg;jpeg;gif;bmp"}
};

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
    (void)SDL_SetWindowTitle(app->window, "影谷长图阅读器");
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

static loim_status app_import_one(app_state *app, const char *path)
{
    loim_image_info probe;
    SDL_Surface *loaded = NULL;
    SDL_Surface *rgba = NULL;
    SDL_Texture *texture = NULL;
    char *path_copy = NULL;
    loim_source_info source;
    size_t source_index = 0U;
    loim_status status;
    uint64_t pixel_count;

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
    texture = SDL_CreateTextureFromSurface(app->renderer, rgba);
    if (texture == NULL) {
        SDL_DestroySurface(rgba);
        return LOIM_ERROR_OUT_OF_MEMORY;
    }
    (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    path_copy = SDL_strdup(path);
    if (path_copy == NULL || !app_grow_images(app)) {
        SDL_free(path_copy);
        SDL_DestroyTexture(texture);
        SDL_DestroySurface(rgba);
        return LOIM_ERROR_OUT_OF_MEMORY;
    }

    source.path = path;
    source.width_px = probe.width_px;
    source.height_px = probe.height_px;
    status = loim_document_add_source(app->document, &source, &source_index);
    if (status != LOIM_OK || source_index != app->image_count) {
        SDL_free(path_copy);
        SDL_DestroyTexture(texture);
        SDL_DestroySurface(rgba);
        return status == LOIM_OK ? LOIM_ERROR_INVALID_ARGUMENT : status;
    }
    app->images[app->image_count].path = path_copy;
    app->images[app->image_count].texture = texture;
    app->images[app->image_count].width = probe.width_px;
    app->images[app->image_count].height = probe.height_px;
    app->image_count += 1U;
    app_add_seam_hints(app, source_index, rgba, probe.width_px, probe.height_px);
    SDL_DestroySurface(rgba);
    return LOIM_OK;
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

static void app_import_batch(app_state *app, const path_batch *batch)
{
    size_t index;
    size_t imported = 0U;
    size_t failed = 0U;
    char message[256];
    loim_status layout_status;

    if (batch->error != NULL) {
        (void)snprintf(message, sizeof(message), "File dialog error: %.180s", batch->error);
        app_set_status(app, message);
        return;
    }
    if (batch->count == 0U) {
        app_set_status(app, "Import canceled");
        return;
    }
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
            "Layout failed: %s",
            loim_status_string(layout_status));
    } else {
        (void)snprintf(
            message,
            sizeof(message),
            "Imported %zu, skipped %zu - %zu images, %zu pages",
            imported,
            failed,
            app->image_count,
            app->layout.page_count);
    }
    app_set_status(app, message);
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

static void app_open_dialog(app_state *app)
{
    if (app->dialog_open) {
        return;
    }
    app->dialog_open = true;
    app_set_status(app, "Choose one or more images");
    SDL_ShowOpenFileDialog(
        app_dialog_callback,
        app,
        app->window,
        app_image_filters,
        (int)SDL_arraysize(app_image_filters),
        NULL,
        true);
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
        SDL_DestroyTexture(app->images[index].texture);
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
        app_set_status(app, "Unable to clear document: out of memory");
        return false;
    }
    loim_layout_destroy(&app->layout);
    app_destroy_images(app);
    loim_document_destroy(app->document);
    app->document = replacement;
    app->left_scroll_y = 0.0F;
    app->right_scroll_y = 0.0F;
    app_set_status(app, "Ready - drop images here or choose Import");
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
    return (action == TOOLBAR_TWO_COLUMNS && app->two_columns) ||
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
        draw_line(app->renderer, center_x, top + 3.0F, center_x, bottom - 3.0F);
        draw_line(app->renderer, left + 3.0F, center_y, right - 3.0F, center_y);
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

static void app_render_page_content(
    app_state *app,
    size_t page_index,
    const SDL_FRect *container)
{
    const loim_page *page = &app->layout.pages[page_index];
    float scale_x = container->w / (float)LOIM_LAYOUT_WIDTH;
    float scale_y = container->h / (float)page->height_px;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    float content_width = (float)LOIM_LAYOUT_WIDTH * scale;
    float content_height = (float)page->height_px * scale;
    float origin_x = container->x + (container->w - content_width) / 2.0F;
    float origin_y = container->y + (container->h - content_height) / 2.0F;
    size_t offset;

    for (offset = 0U; offset < page->slice_count; ++offset) {
        const loim_page_slice *slice =
            &app->layout.slices[page->first_slice + offset];
        const app_image *image = &app->images[slice->source_index];
        SDL_FRect source = {
            0.0F,
            (float)slice->source_y_px,
            (float)image->width,
            (float)slice->source_height_px
        };
        SDL_FRect destination = {
            origin_x,
            origin_y + (float)slice->destination_y_px * scale,
            content_width,
            (float)slice->destination_height_px * scale
        };

        (void)SDL_RenderTexture(app->renderer, image->texture, &source, &destination);
    }
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
    size_t pages_per_sheet = app->two_columns ? 4U : 1U;
    size_t sheet_count = (app->layout.page_count + pages_per_sheet - 1U) / pages_per_sheet;
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
                size_t page_index = sheet_index * pages_per_sheet + slot;
                SDL_FRect cell = content;

                if (page_index >= app->layout.page_count) {
                    break;
                }
                if (pages_per_sheet == 4U) {
                    size_t column = slot / 2U;
                    size_t row = slot % 2U;
                    float gap = paper_width * 0.025F;

                    cell.w = (content.w - gap) / 2.0F;
                    cell.h = (content.h - gap) / 2.0F;
                    cell.x += (float)column * (cell.w + gap);
                    cell.y += (float)row * (cell.h + gap);
                }
                app_render_page_content(app, page_index, &cell);
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
                SDL_FRect source = {
                    0.0F,
                    (float)slice->source_y_px,
                    (float)image->width,
                    (float)slice->source_height_px
                };
                SDL_FRect destination = {
                    canvas_x,
                    y + (float)slice->destination_y_px * scale,
                    canvas_width,
                    (float)slice->destination_height_px * scale
                };

                (void)SDL_RenderTexture(app->renderer, image->texture, &source, &destination);
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
    (void)SDL_SetRenderDrawColor(app->renderer, 102U, 102U, 102U, 255U);
    (void)SDL_RenderDebugText(app->renderer, 8.0F, bar.y + 7.0F, app->status);
    (void)snprintf(version, sizeof(version), "v%s", LOIM_APP_VERSION);
    (void)SDL_RenderDebugText(app->renderer, width - 70.0F, bar.y + 7.0F, version);
}

static void app_render_empty(app_state *app, float left, float top, float width, float bottom)
{
    const char *label = "Drop images here or press Ctrl+O";

    (void)SDL_SetRenderDrawColor(app->renderer, 226U, 226U, 226U, 255U);
    (void)SDL_RenderDebugText(
        app->renderer,
        left + (width - 240.0F) / 2.0F,
        top + (bottom - top) / 2.0F,
        label);
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
    app_render_toolbar(app, width);
    if (app->image_count == 0U) {
        app_render_empty(app, 0.0F, LOIM_TOOLBAR_HEIGHT, left_width, content_bottom);
        app_render_empty(app, right_left, LOIM_TOOLBAR_HEIGHT, right_width, content_bottom);
    } else {
        app_render_preview(
            app, 0.0F, LOIM_TOOLBAR_HEIGHT, left_width, content_bottom);
        app_render_editor(
            app, right_left, LOIM_TOOLBAR_HEIGHT, right_width, content_bottom);
    }
    app_render_divider(app, divider_x, LOIM_TOOLBAR_HEIGHT, content_bottom);
    app_render_statusbar(app, width, height);
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
            app->two_columns = !app->two_columns;
            app->left_scroll_y = 0.0F;
            app_set_status(app, app->two_columns ? "Two-column layout" : "Single-page layout");
            break;
        case TOOLBAR_PAGE_NUMBERS:
            app->show_page_numbers = !app->show_page_numbers;
            app_set_status(app, app->show_page_numbers ? "Page numbers on" : "Page numbers off");
            break;
        case TOOLBAR_AUTO_SPLIT:
            (void)app_rebuild_layout(app);
            app_set_status(app, "Automatic split applied");
            break;
        case TOOLBAR_LESS_MARGIN:
            app->margin_ratio -= 0.01F;
            if (app->margin_ratio < 0.02F) {
                app->margin_ratio = 0.02F;
            }
            app_set_status(app, "Print margin reduced");
            break;
        case TOOLBAR_MORE_MARGIN:
            app->margin_ratio += 0.01F;
            if (app->margin_ratio > 0.22F) {
                app->margin_ratio = 0.22F;
            }
            app_set_status(app, "Print margin increased");
            break;
        case TOOLBAR_ZOOM_IN:
            app_adjust_zoom(app, 0.1F);
            app_set_status(app, "Editor zoomed in");
            break;
        case TOOLBAR_ZOOM_OUT:
            app_adjust_zoom(app, -0.1F);
            app_set_status(app, "Editor zoomed out");
            break;
        case TOOLBAR_LOGIN:
            app_set_status(app, "Account migration is not enabled in this preview");
            break;
        case TOOLBAR_EXPORT:
            app_set_status(app, "PDF export migration is not enabled in this preview");
            break;
        case TOOLBAR_PRINT:
            app_set_status(app, "Print migration is not enabled in this preview");
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
        app_set_status(app, "Split position is outside the document");
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
        status == LOIM_OK ? "Manual split updated" : "Unable to place split here");
}

static void app_handle_event(app_state *app, SDL_Event *event)
{
    if (event->type == app->dialog_event) {
        app_handle_dialog_event(app, event);
        return;
    }
    (void)SDL_ConvertEventToRenderCoordinates(app->renderer, event);
    switch (event->type) {
    case SDL_EVENT_QUIT:
        if (app->dialog_open) {
            app->quit_after_dialog = true;
            (void)SDL_HideWindow(app->window);
        } else {
            app->running = false;
        }
        break;
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
        path_batch_destroy(&app->dropped);
        app->drop_active = true;
        break;
    case SDL_EVENT_DROP_FILE:
        if (event->drop.data != NULL) {
            if (app->drop_active) {
                (void)path_batch_append(&app->dropped, event->drop.data);
            } else {
                path_batch single = {0};

                if (path_batch_append(&single, event->drop.data)) {
                    app_import_batch(app, &single);
                }
                path_batch_destroy(&single);
            }
        }
        break;
    case SDL_EVENT_DROP_COMPLETE:
        if (app->drop_active) {
            app_import_batch(app, &app->dropped);
            path_batch_destroy(&app->dropped);
            app->drop_active = false;
        }
        break;
    default:
        break;
    }
}

static void app_destroy(app_state *app)
{
    path_batch_destroy(&app->dropped);
    loim_layout_destroy(&app->layout);
    app_destroy_images(app);
    loim_document_destroy(app->document);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_Quit();
}

static bool app_initialize(app_state *app, bool headless)
{
    loim_status status;
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    memset(app, 0, sizeof(*app));
    app->zoom = 1.0F;
    app->split_ratio = 0.5F;
    app->margin_ratio = 0.08F;
    app->split_drag_index = SIZE_MAX;
    app->running = true;
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
    if (!SDL_CreateWindowAndRenderer(
            "影谷长图阅读器",
            1440,
            900,
            window_flags,
            &app->window,
            &app->renderer)) {
        (void)fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        app_destroy(app);
        return false;
    }
    app->dialog_event = SDL_RegisterEvents(1);
    if (app->dialog_event == 0U) {
        (void)fprintf(stderr, "Cannot allocate file dialog event: %s\n", SDL_GetError());
        app_destroy(app);
        return false;
    }
    (void)SDL_SetRenderVSync(app->renderer, 1);
    (void)SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
    app_set_status(app, "Ready - drop images here or choose Import");
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
