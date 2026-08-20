#define _POSIX_C_SOURCE 200809L

#include "linux_shortcut.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool loim_path_join(
    char *output,
    size_t output_capacity,
    const char *directory,
    const char *name)
{
    size_t directory_length;
    int written;

    if (output == NULL || output_capacity == 0U || directory == NULL ||
        directory[0] == '\0' || name == NULL || name[0] == '\0') {
        return false;
    }
    directory_length = strlen(directory);
    written = snprintf(
        output,
        output_capacity,
        "%s%s%s",
        directory,
        directory[directory_length - 1U] == '/' ? "" : "/",
        name);
    return written >= 0 && (size_t)written < output_capacity;
}

static bool loim_copy_expanded_desktop(
    const char *value,
    const char *home_directory,
    char *output,
    size_t output_capacity)
{
    const char *suffix = NULL;
    int written;

    if (strncmp(value, "$HOME", 5U) == 0 &&
        (value[5] == '\0' || value[5] == '/')) {
        suffix = value + 5U;
    } else if (strncmp(value, "${HOME}", 7U) == 0 &&
               (value[7] == '\0' || value[7] == '/')) {
        suffix = value + 7U;
    }
    if (suffix != NULL) {
        written = snprintf(output, output_capacity, "%s%s", home_directory, suffix);
    } else if (value[0] == '/') {
        written = snprintf(output, output_capacity, "%s", value);
    } else {
        return false;
    }
    return written >= 0 && (size_t)written < output_capacity;
}

static bool loim_parse_desktop_setting(
    FILE *file,
    const char *home_directory,
    char *output,
    size_t output_capacity)
{
    char line[4096];

    while (fgets(line, (int)sizeof(line), file) != NULL) {
        static const char key[] = "XDG_DESKTOP_DIR";
        char decoded[4096];
        const char *cursor = line;
        char quote = '\0';
        size_t decoded_length = 0U;

        while (*cursor == ' ' || *cursor == '\t') cursor += 1;
        if (strncmp(cursor, key, sizeof(key) - 1U) != 0) continue;
        cursor += sizeof(key) - 1U;
        while (*cursor == ' ' || *cursor == '\t') cursor += 1;
        if (*cursor != '=') continue;
        cursor += 1;
        while (*cursor == ' ' || *cursor == '\t') cursor += 1;
        if (*cursor == '\'' || *cursor == '"') {
            quote = *cursor;
            cursor += 1;
        }
        while (*cursor != '\0' && *cursor != '\n' && *cursor != '\r' &&
               (quote == '\0' || *cursor != quote)) {
            if (*cursor == '\\' && cursor[1] != '\0') cursor += 1;
            if (decoded_length + 1U >= sizeof(decoded)) return false;
            decoded[decoded_length++] = *cursor++;
        }
        while (decoded_length > 0U && quote == '\0' &&
               (decoded[decoded_length - 1U] == ' ' ||
                decoded[decoded_length - 1U] == '\t')) {
            decoded_length -= 1U;
        }
        decoded[decoded_length] = '\0';
        return loim_copy_expanded_desktop(
            decoded, home_directory, output, output_capacity);
    }
    return false;
}

bool loim_linux_resolve_desktop_directory(
    const char *home_directory,
    const char *config_directory,
    char *output,
    size_t output_capacity)
{
    char settings_path[4096];
    FILE *file = NULL;
    bool resolved = false;

    if (output == NULL || output_capacity == 0U) return false;
    output[0] = '\0';
    if (home_directory == NULL || home_directory[0] != '/') return false;
    if (config_directory != NULL && config_directory[0] == '/' &&
        loim_path_join(
            settings_path,
            sizeof(settings_path),
            config_directory,
            "user-dirs.dirs")) {
        file = fopen(settings_path, "rb");
        if (file != NULL) {
            resolved = loim_parse_desktop_setting(
                file, home_directory, output, output_capacity);
            (void)fclose(file);
        }
    }
    return resolved || loim_path_join(
        output, output_capacity, home_directory, "Desktop");
}

static loim_shortcut_result loim_existing_shortcut_result(
    const char *shortcut_path,
    const char *installed_entry,
    const struct stat *metadata)
{
    char target[4096];
    ssize_t target_length;

    if (!S_ISLNK(metadata->st_mode)) return LOIM_SHORTCUT_CONFLICT;
    target_length = readlink(shortcut_path, target, sizeof(target) - 1U);
    if (target_length < 0) return LOIM_SHORTCUT_ERROR;
    target[target_length] = '\0';
    return strcmp(target, installed_entry) == 0
        ? LOIM_SHORTCUT_ALREADY_PRESENT
        : LOIM_SHORTCUT_CONFLICT;
}

static bool loim_entry_is_managed(const char *path)
{
    static const char marker[] = "X-LoimReader-Managed=true";
    char line[512];
    FILE *file = fopen(path, "rb");
    bool managed = false;

    if (file == NULL) return false;
    while (fgets(line, (int)sizeof(line), file) != NULL) {
        if (strncmp(line, marker, sizeof(marker) - 1U) == 0 &&
            (line[sizeof(marker) - 1U] == '\n' ||
             line[sizeof(marker) - 1U] == '\r' ||
             line[sizeof(marker) - 1U] == '\0')) {
            managed = true;
            break;
        }
    }
    (void)fclose(file);
    return managed;
}

static bool loim_files_equal(const char *left_path, const char *right_path)
{
    unsigned char left[4096];
    unsigned char right[4096];
    FILE *left_file = fopen(left_path, "rb");
    FILE *right_file = fopen(right_path, "rb");
    bool equal = left_file != NULL && right_file != NULL;

    while (equal) {
        size_t left_count = fread(left, 1U, sizeof(left), left_file);
        size_t right_count = fread(right, 1U, sizeof(right), right_file);

        if (left_count != right_count ||
            memcmp(left, right, left_count) != 0) {
            equal = false;
            break;
        }
        if (left_count < sizeof(left)) {
            equal = !ferror(left_file) && !ferror(right_file);
            break;
        }
    }
    if (left_file != NULL) (void)fclose(left_file);
    if (right_file != NULL) (void)fclose(right_file);
    return equal;
}

static bool loim_copy_executable_entry(
    const char *installed_entry,
    const char *shortcut_path)
{
    char temporary_path[4096];
    unsigned char buffer[16384];
    FILE *source = NULL;
    int destination = -1;
    bool success = false;
    int written;

    written = snprintf(
        temporary_path,
        sizeof(temporary_path),
        "%s.tmp.XXXXXX",
        shortcut_path);
    if (written < 0 || (size_t)written >= sizeof(temporary_path)) return false;
    source = fopen(installed_entry, "rb");
    if (source == NULL) return false;
    destination = mkstemp(temporary_path);
    if (destination < 0) goto cleanup;
    for (;;) {
        size_t count = fread(buffer, 1U, sizeof(buffer), source);
        size_t offset = 0U;

        while (offset < count) {
            ssize_t result = write(destination, buffer + offset, count - offset);

            if (result <= 0) goto cleanup;
            offset += (size_t)result;
        }
        if (count < sizeof(buffer)) {
            if (ferror(source)) goto cleanup;
            break;
        }
    }
    if (fchmod(destination, 0755) != 0 || fsync(destination) != 0 ||
        close(destination) != 0) {
        destination = -1;
        goto cleanup;
    }
    destination = -1;
    if (rename(temporary_path, shortcut_path) != 0) goto cleanup;
    success = true;

cleanup:
    if (destination >= 0) (void)close(destination);
    if (source != NULL) (void)fclose(source);
    if (!success) (void)unlink(temporary_path);
    return success;
}

loim_shortcut_result loim_linux_desktop_shortcut_ensure(
    const char *desktop_directory,
    const char *installed_entry,
    const char *shortcut_name)
{
    struct stat metadata;
    char shortcut_path[4096];

    if (desktop_directory == NULL || installed_entry == NULL ||
        shortcut_name == NULL || strchr(shortcut_name, '/') != NULL ||
        !loim_path_join(
            shortcut_path,
            sizeof(shortcut_path),
            desktop_directory,
            shortcut_name)) {
        return LOIM_SHORTCUT_ERROR;
    }
    if (stat(desktop_directory, &metadata) != 0 || !S_ISDIR(metadata.st_mode) ||
        stat(installed_entry, &metadata) != 0 || S_ISDIR(metadata.st_mode)) {
        return LOIM_SHORTCUT_UNAVAILABLE;
    }
    if (lstat(shortcut_path, &metadata) == 0) {
        if (S_ISLNK(metadata.st_mode)) {
            loim_shortcut_result existing = loim_existing_shortcut_result(
                shortcut_path, installed_entry, &metadata);

            if (existing != LOIM_SHORTCUT_ALREADY_PRESENT) return existing;
            if (unlink(shortcut_path) != 0) return LOIM_SHORTCUT_ERROR;
        } else if (S_ISREG(metadata.st_mode) &&
                   loim_entry_is_managed(shortcut_path)) {
            if (loim_files_equal(shortcut_path, installed_entry) &&
                (metadata.st_mode & 0111) != 0) {
                return LOIM_SHORTCUT_ALREADY_PRESENT;
            }
            /* Refresh an older LoimReader-owned copy during package upgrade. */
        } else {
            return LOIM_SHORTCUT_CONFLICT;
        }
    }
    else if (errno != ENOENT) return LOIM_SHORTCUT_ERROR;
    if (loim_copy_executable_entry(installed_entry, shortcut_path)) {
        return LOIM_SHORTCUT_CREATED;
    }
    if (errno == EEXIST && lstat(shortcut_path, &metadata) == 0) {
        return loim_existing_shortcut_result(
            shortcut_path, installed_entry, &metadata);
    }
    return LOIM_SHORTCUT_ERROR;
}
