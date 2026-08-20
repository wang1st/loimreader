#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "loim/export_name.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "loim/presentation.h"

typedef struct loim_text_view {
    const char *data;
    size_t length;
} loim_text_view;

static int export_path_separator(char value)
{
    return value == '/' || value == '\\';
}

static int export_trim_character(unsigned char value)
{
    return value <= (unsigned char)' ' || value == (unsigned char)'-' ||
        value == (unsigned char)'_' || value == (unsigned char)'.' ||
        value == (unsigned char)',' || value == (unsigned char)'(' ||
        value == (unsigned char)')' || value == (unsigned char)'[' ||
        value == (unsigned char)']';
}

static loim_text_view export_view_trim(loim_text_view view)
{
    while (view.length > 0U &&
           export_trim_character((unsigned char)view.data[0])) {
        view.data += 1;
        view.length -= 1U;
    }
    while (view.length > 0U &&
           export_trim_character((unsigned char)view.data[view.length - 1U])) {
        view.length -= 1U;
    }
    return view;
}

static loim_text_view export_path_leaf(loim_text_view path)
{
    size_t start = 0U;
    size_t index;

    for (index = 0U; index < path.length; ++index) {
        if (export_path_separator(path.data[index])) {
            start = index + 1U;
        }
    }
    path.data += start;
    path.length -= start;
    return path;
}

static loim_text_view export_path_parent(const char *path)
{
    loim_text_view parent = {NULL, 0U};
    size_t index;

    if (path == NULL) {
        return parent;
    }
    parent.data = path;
    parent.length = strlen(path);
    for (index = parent.length; index > 0U; --index) {
        if (export_path_separator(parent.data[index - 1U])) {
            parent.length = index - 1U;
            if (parent.length == 0U) {
                parent.length = 1U;
            }
            return parent;
        }
    }
    parent.length = 0U;
    return parent;
}

static loim_text_view export_image_stem(const char *path)
{
    loim_text_view stem = {NULL, 0U};
    size_t index;

    if (path == NULL) {
        return stem;
    }
    stem.data = path;
    stem.length = strlen(path);
    stem = export_path_leaf(stem);
    for (index = stem.length; index > 1U; --index) {
        if (stem.data[index - 1U] == '.') {
            stem.length = index - 1U;
            break;
        }
    }
    return export_view_trim(stem);
}

static unsigned char export_ascii_lower(unsigned char value)
{
    return value >= (unsigned char)'A' && value <= (unsigned char)'Z'
        ? (unsigned char)(value + ((unsigned char)'a' - (unsigned char)'A'))
        : value;
}

static int export_view_ends_ascii_case_insensitive(
    loim_text_view view,
    const char *suffix)
{
    size_t suffix_length = strlen(suffix);
    size_t index;

    if (suffix_length > view.length) {
        return 0;
    }
    for (index = 0U; index < suffix_length; ++index) {
        unsigned char actual = (unsigned char)view.data[
            view.length - suffix_length + index];
        unsigned char expected = (unsigned char)suffix[index];

        if (export_ascii_lower(actual) != export_ascii_lower(expected)) {
            return 0;
        }
    }
    return 1;
}

static int export_view_ends_bytes(loim_text_view view, const char *suffix)
{
    size_t suffix_length = strlen(suffix);

    return suffix_length <= view.length &&
        memcmp(view.data + view.length - suffix_length, suffix, suffix_length) == 0;
}

static loim_text_view export_strip_sequence_suffix(loim_text_view view)
{
    size_t before_digits;
    int removed = 0;

    view = export_view_trim(view);
    if (export_view_ends_bytes(view, "副本")) {
        view.length -= strlen("副本");
        return export_view_trim(view);
    }
    if (export_view_ends_ascii_case_insensitive(view, "copy")) {
        view.length -= strlen("copy");
        return export_view_trim(view);
    }
    if (export_view_ends_bytes(view, "页")) {
        view.length -= strlen("页");
        view = export_view_trim(view);
        before_digits = view.length;
        while (view.length > 0U && view.data[view.length - 1U] >= '0' &&
               view.data[view.length - 1U] <= '9') {
            view.length -= 1U;
        }
        if (view.length < before_digits) {
            removed = 1;
            if (export_view_ends_bytes(view, "第")) {
                view.length -= strlen("第");
            }
        }
    } else if (view.length > 0U && view.data[view.length - 1U] == ')') {
        view.length -= 1U;
        before_digits = view.length;
        while (view.length > 0U && view.data[view.length - 1U] >= '0' &&
               view.data[view.length - 1U] <= '9') {
            view.length -= 1U;
        }
        if (view.length < before_digits && view.length > 0U &&
            view.data[view.length - 1U] == '(') {
            view.length -= 1U;
            removed = 1;
        }
    } else {
        before_digits = view.length;
        while (view.length > 0U && view.data[view.length - 1U] >= '0' &&
               view.data[view.length - 1U] <= '9') {
            view.length -= 1U;
        }
        removed = view.length < before_digits;
    }
    return removed ? export_view_trim(view) : view;
}

static size_t export_utf8_unit_length(const char *text, size_t remaining)
{
    unsigned char lead;
    size_t expected;
    size_t index;

    if (remaining == 0U) {
        return 0U;
    }
    lead = (unsigned char)text[0];
    if (lead < 0x80U) {
        return 1U;
    }
    if (lead >= 0xC2U && lead <= 0xDFU) {
        expected = 2U;
    } else if (lead >= 0xE0U && lead <= 0xEFU) {
        expected = 3U;
    } else if (lead >= 0xF0U && lead <= 0xF4U) {
        expected = 4U;
    } else {
        return 1U;
    }
    if (expected > remaining) {
        return 1U;
    }
    for (index = 1U; index < expected; ++index) {
        if (((unsigned char)text[index] & 0xC0U) != 0x80U) {
            return 1U;
        }
    }
    return expected;
}

static size_t export_utf8_character_count(const char *text, size_t length)
{
    size_t count = 0U;
    size_t offset = 0U;

    while (offset < length) {
        size_t unit = export_utf8_unit_length(text + offset, length - offset);

        offset += unit;
        count += 1U;
    }
    return count;
}

static int export_view_is_generic(loim_text_view view, int folder)
{
    static const char *const generic_names[] = {
        "img", "image", "photo", "picture", "screenshot", "screenshotimage",
        "scan", "sample", "example", "untitled", "wechatimage", "wxcamera",
        "dsc", "page"
    };
    static const char *const generic_folders[] = {
        "document", "documents", "mydocuments", "desktop", "download",
        "downloads", "picture", "pictures", "image", "images", "photo",
        "photos", "tmp", "temp", "home", "users"
    };
    static const char *const generic_chinese_names[] = {
        "图片", "照片", "截图", "屏幕截图", "扫描", "样例", "示例",
        "未命名", "微信图片"
    };
    static const char *const generic_chinese_folders[] = {
        "文档", "桌面", "下载", "图片", "照片"
    };
    char normalized[64];
    size_t normalized_length = 0U;
    size_t index;

    view = export_strip_sequence_suffix(export_view_trim(view));
    for (index = 0U;
         index < sizeof(generic_chinese_names) / sizeof(generic_chinese_names[0]);
         ++index) {
        size_t length = strlen(generic_chinese_names[index]);

        if (view.length == length &&
            memcmp(view.data, generic_chinese_names[index], length) == 0) {
            return 1;
        }
    }
    if (folder) {
        for (index = 0U;
             index < sizeof(generic_chinese_folders) /
                sizeof(generic_chinese_folders[0]);
             ++index) {
            size_t length = strlen(generic_chinese_folders[index]);

            if (view.length == length &&
                memcmp(view.data, generic_chinese_folders[index], length) == 0) {
                return 1;
            }
        }
    }
    for (index = 0U; index < view.length; ++index) {
        unsigned char value = (unsigned char)view.data[index];

        if (value >= 0x80U) {
            return 0;
        }
        if ((value >= (unsigned char)'0' && value <= (unsigned char)'9') ||
            value == (unsigned char)' ' || value == (unsigned char)'-' ||
            value == (unsigned char)'_' || value == (unsigned char)'.' ||
            value == (unsigned char)'(' || value == (unsigned char)')') {
            continue;
        }
        if (normalized_length + 1U < sizeof(normalized)) {
            normalized[normalized_length++] = (char)export_ascii_lower(value);
        }
    }
    normalized[normalized_length] = '\0';
    for (index = 0U;
         index < sizeof(generic_names) / sizeof(generic_names[0]);
         ++index) {
        if (strcmp(normalized, generic_names[index]) == 0) {
            return 1;
        }
    }
    if (folder) {
        if (view.length == 2U &&
            ((view.data[0] >= 'A' && view.data[0] <= 'Z') ||
             (view.data[0] >= 'a' && view.data[0] <= 'z')) &&
            view.data[1] == ':') {
            return 1;
        }
        for (index = 0U;
             index < sizeof(generic_folders) / sizeof(generic_folders[0]);
             ++index) {
            if (strcmp(normalized, generic_folders[index]) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int export_view_meaningful(loim_text_view view, int folder)
{
    size_t character_count;
    size_t index;
    int has_non_ascii = 0;

    view = export_view_trim(view);
    if (view.length == 0U || export_view_is_generic(view, folder)) {
        return 0;
    }
    character_count = export_utf8_character_count(view.data, view.length);
    for (index = 0U; index < view.length; ++index) {
        if ((unsigned char)view.data[index] >= 0x80U) {
            has_non_ascii = 1;
            break;
        }
    }
    return has_non_ascii ? character_count >= 2U : character_count >= 4U;
}

static loim_text_view export_common_stem(
    const char *const *image_paths,
    size_t image_count)
{
    loim_text_view common = export_strip_sequence_suffix(
        export_image_stem(image_paths[0]));
    size_t image_index;

    for (image_index = 1U; image_index < image_count && common.length > 0U;
         ++image_index) {
        loim_text_view candidate = export_strip_sequence_suffix(
            export_image_stem(image_paths[image_index]));
        size_t prefix = 0U;

        while (prefix < common.length && prefix < candidate.length &&
               common.data[prefix] == candidate.data[prefix]) {
            prefix += 1U;
        }
        while (prefix > 0U && prefix < common.length &&
               ((unsigned char)common.data[prefix] & 0xC0U) == 0x80U) {
            prefix -= 1U;
        }
        common.length = prefix;
        common = export_view_trim(common);
    }
    return common;
}

static loim_text_view export_shared_folder(
    const char *const *image_paths,
    size_t image_count)
{
    loim_text_view parent = export_path_parent(image_paths[0]);
    size_t image_index;

    if (parent.length == 0U) {
        return parent;
    }
    for (image_index = 1U; image_index < image_count; ++image_index) {
        loim_text_view candidate = export_path_parent(image_paths[image_index]);

        if (candidate.length != parent.length ||
            memcmp(candidate.data, parent.data, parent.length) != 0) {
            parent.length = 0U;
            return parent;
        }
    }
    return export_view_trim(export_path_leaf(parent));
}

static int export_forbidden_filename_character(unsigned char value)
{
    return value < 0x20U || value == (unsigned char)'<' ||
        value == (unsigned char)'>' || value == (unsigned char)':' ||
        value == (unsigned char)'"' || value == (unsigned char)'/' ||
        value == (unsigned char)'\\' || value == (unsigned char)'|' ||
        value == (unsigned char)'?' || value == (unsigned char)'*';
}

static int export_sanitize_view(
    loim_text_view view,
    char *output,
    size_t output_capacity)
{
    size_t input_offset = 0U;
    size_t output_length = 0U;
    size_t first = 0U;

    if (output == NULL || output_capacity == 0U) {
        return 0;
    }
    while (input_offset < view.length) {
        unsigned char value = (unsigned char)view.data[input_offset];
        size_t unit = export_utf8_unit_length(
            view.data + input_offset, view.length - input_offset);

        if (value < 0x80U && export_forbidden_filename_character(value)) {
            if (output_length > 0U && output[output_length - 1U] != '-' &&
                output_length + 1U < output_capacity) {
                output[output_length++] = '-';
            }
            input_offset += 1U;
            continue;
        }
        if (output_length + unit >= output_capacity) {
            break;
        }
        memcpy(output + output_length, view.data + input_offset, unit);
        output_length += unit;
        input_offset += unit;
    }
    output[output_length] = '\0';
    while (first < output_length &&
           export_trim_character((unsigned char)output[first])) {
        first += 1U;
    }
    while (output_length > first &&
           export_trim_character((unsigned char)output[output_length - 1U])) {
        output_length -= 1U;
    }
    if (first > 0U && output_length > first) {
        memmove(output, output + first, output_length - first);
    }
    output_length -= first;
    output[output_length] = '\0';
    return output_length > 0U;
}

static int export_choose_content_name(
    const char *const *image_paths,
    size_t image_count,
    loim_locale locale,
    const char *timestamp,
    char *output,
    size_t output_capacity)
{
    loim_text_view selected = {NULL, 0U};
    char fallback[128];
    int written;

    if (image_paths != NULL && image_count > 0U && image_paths[0] != NULL) {
        if (image_count == 1U) {
            selected = export_image_stem(image_paths[0]);
        } else {
            selected = export_common_stem(image_paths, image_count);
        }
        if (!export_view_meaningful(selected, 0)) {
            selected = export_shared_folder(image_paths, image_count);
            if (!export_view_meaningful(selected, 1)) {
                selected.data = NULL;
                selected.length = 0U;
            }
        }
    }
    if (selected.length > 0U) {
        return export_sanitize_view(selected, output, output_capacity);
    }
    written = timestamp != NULL && timestamp[0] != '\0'
        ? snprintf(
            fallback,
            sizeof(fallback),
            locale == LOIM_LOCALE_ZH_CN ? "合并图片-%s" : "Merged Images-%s",
            timestamp)
        : snprintf(
            fallback,
            sizeof(fallback),
            "%s",
            locale == LOIM_LOCALE_ZH_CN ? "合并图片" : "Merged Images");
    if (written < 0 || (size_t)written >= sizeof(fallback)) {
        return 0;
    }
    selected.data = fallback;
    selected.length = (size_t)written;
    return export_sanitize_view(selected, output, output_capacity);
}

static int export_middle_truncate(
    const char *content,
    size_t maximum_bytes,
    size_t maximum_characters,
    char *output,
    size_t output_capacity)
{
    size_t offsets[1025];
    size_t content_length = strlen(content);
    size_t character_count = 0U;
    size_t offset = 0U;
    size_t head;
    size_t tail;
    size_t tail_offset;
    size_t selected_bytes;
    static const char ellipsis[] = "…";

    while (offset < content_length && character_count + 1U <
           sizeof(offsets) / sizeof(offsets[0])) {
        offsets[character_count++] = offset;
        offset += export_utf8_unit_length(content + offset, content_length - offset);
    }
    offsets[character_count] = offset;
    content_length = offset;
    if (content_length <= maximum_bytes && character_count <= maximum_characters) {
        if (content_length + 1U > output_capacity) {
            return 0;
        }
        memcpy(output, content, content_length + 1U);
        return 1;
    }
    if (maximum_characters < 2U || maximum_bytes < sizeof(ellipsis) - 1U) {
        return 0;
    }
    tail = character_count < 12U ? character_count : 12U;
    if (tail + 1U > maximum_characters) {
        tail = maximum_characters - 1U;
    }
    head = maximum_characters - tail - 1U;
    if (head + tail > character_count) {
        head = character_count - tail;
    }
    tail_offset = offsets[character_count - tail];
    selected_bytes = offsets[head] + (sizeof(ellipsis) - 1U) +
        content_length - tail_offset;
    while (selected_bytes > maximum_bytes && head > 0U) {
        head -= 1U;
        selected_bytes = offsets[head] + (sizeof(ellipsis) - 1U) +
            content_length - tail_offset;
    }
    while (selected_bytes > maximum_bytes && tail > 0U) {
        tail -= 1U;
        tail_offset = offsets[character_count - tail];
        selected_bytes = offsets[head] + (sizeof(ellipsis) - 1U) +
            content_length - tail_offset;
    }
    if (selected_bytes > maximum_bytes || selected_bytes + 1U > output_capacity) {
        return 0;
    }
    memcpy(output, content, offsets[head]);
    memcpy(output + offsets[head], ellipsis, sizeof(ellipsis) - 1U);
    memcpy(
        output + offsets[head] + sizeof(ellipsis) - 1U,
        content + tail_offset,
        content_length - tail_offset);
    output[selected_bytes] = '\0';
    return 1;
}

static int export_build_filename(
    const char *content,
    size_t columns,
    loim_locale locale,
    unsigned collision_index,
    char *output,
    size_t output_capacity)
{
    static const char *const chinese_columns[] = {"单栏", "双栏", "三栏"};
    static const char *const english_columns[] = {
        "1 Column", "2 Columns", "3 Columns"
    };
    char suffix[64];
    char truncated[1025];
    const char *column_label;
    size_t suffix_length;
    size_t suffix_characters;
    int written;

    columns = loim_columns_normalize(columns);
    column_label = locale == LOIM_LOCALE_ZH_CN
        ? chinese_columns[columns - 1U]
        : english_columns[columns - 1U];
    written = collision_index >= 2U
        ? snprintf(suffix, sizeof(suffix), "-%s (%u).pdf", column_label, collision_index)
        : snprintf(suffix, sizeof(suffix), "-%s.pdf", column_label);
    if (written < 0 || (size_t)written >= sizeof(suffix)) {
        return 0;
    }
    suffix_length = (size_t)written;
    suffix_characters = export_utf8_character_count(suffix, suffix_length);
    if (suffix_length >= LOIM_EXPORT_FILENAME_MAX_BYTES ||
        suffix_characters >= LOIM_EXPORT_FILENAME_MAX_CHARACTERS ||
        !export_middle_truncate(
            content,
            LOIM_EXPORT_FILENAME_MAX_BYTES - suffix_length,
            LOIM_EXPORT_FILENAME_MAX_CHARACTERS - suffix_characters,
            truncated,
            sizeof(truncated))) {
        return 0;
    }
    written = snprintf(output, output_capacity, "%s%s", truncated, suffix);
    return written >= 0 && (size_t)written < output_capacity &&
        (size_t)written <= LOIM_EXPORT_FILENAME_MAX_BYTES;
}

static int export_join_path(
    loim_text_view directory,
    const char *filename,
    char *output,
    size_t output_capacity)
{
    char separator = '/';
    int has_separator;
    size_t filename_length = strlen(filename);
    size_t index;
    size_t required;

    if (directory.length == 0U) {
        directory.data = ".";
        directory.length = 1U;
    }
    for (index = 0U; index < directory.length; ++index) {
        if (directory.data[index] == '/' || directory.data[index] == '\\') {
            separator = directory.data[index];
        }
    }
    has_separator = export_path_separator(directory.data[directory.length - 1U]);
    if (directory.length > SIZE_MAX - filename_length - 2U) {
        return 0;
    }
    required = directory.length + (has_separator ? 0U : 1U) +
        filename_length + 1U;
    if (required > output_capacity) {
        return 0;
    }
    memcpy(output, directory.data, directory.length);
    index = directory.length;
    if (!has_separator) {
        output[index++] = separator;
    }
    memcpy(output + index, filename, filename_length + 1U);
    return 1;
}

static int export_path_exists(const char *path)
{
#if defined(_WIN32)
    int wide_length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    wchar_t *wide_path;
    DWORD attributes;

    if (wide_length <= 0 ||
        (size_t)wide_length > SIZE_MAX / sizeof(*wide_path)) {
        return 0;
    }
    wide_path = malloc((size_t)wide_length * sizeof(*wide_path));
    if (wide_path == NULL ||
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide_path, wide_length) == 0) {
        free(wide_path);
        return 0;
    }
    attributes = GetFileAttributesW(wide_path);
    free(wide_path);
    return attributes != INVALID_FILE_ATTRIBUTES;
#else
    return access(path, F_OK) == 0;
#endif
}

bool loim_export_suggested_filename(
    const char *const *image_paths,
    size_t image_count,
    size_t columns,
    loim_locale locale,
    const char *timestamp,
    char *output,
    size_t output_capacity)
{
    char content[1025];

    if (output == NULL || output_capacity == 0U) {
        return false;
    }
    output[0] = '\0';
    if (!export_choose_content_name(
            image_paths,
            image_count,
            locale,
            timestamp,
            content,
            sizeof(content))) {
        return false;
    }
    return export_build_filename(
        content, columns, locale, 0U, output, output_capacity) != 0;
}

bool loim_export_suggested_path(
    const char *documents_path,
    const char *const *image_paths,
    size_t image_count,
    size_t columns,
    loim_locale locale,
    const char *timestamp,
    char *output,
    size_t output_capacity)
{
    loim_text_view directory = {NULL, 0U};
    char content[1025];
    char filename[LOIM_EXPORT_FILENAME_MAX_BYTES + 1U];
    unsigned collision_index;

    if (output == NULL || output_capacity == 0U) {
        return false;
    }
    output[0] = '\0';
    if (!export_choose_content_name(
            image_paths,
            image_count,
            locale,
            timestamp,
            content,
            sizeof(content))) {
        return false;
    }
    if (documents_path != NULL && documents_path[0] != '\0') {
        directory.data = documents_path;
        directory.length = strlen(documents_path);
    } else if (image_paths != NULL && image_count > 0U && image_paths[0] != NULL) {
        directory = export_path_parent(image_paths[0]);
    }
    for (collision_index = 0U; collision_index <= 9999U; ++collision_index) {
        unsigned filename_index = collision_index < 2U ? 0U : collision_index;

        if (collision_index == 1U) {
            continue;
        }
        if (!export_build_filename(
                content,
                columns,
                locale,
                filename_index,
                filename,
                sizeof(filename)) ||
            !export_join_path(directory, filename, output, output_capacity)) {
            return false;
        }
        if (!export_path_exists(output)) {
            return true;
        }
    }
    output[0] = '\0';
    return false;
}
