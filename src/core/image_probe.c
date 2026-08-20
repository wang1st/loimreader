#include "loim/image_probe.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static FILE *loim_open_binary_read(const char *path)
{
#if defined(_WIN32)
    int wide_count;
    wchar_t *wide_path;
    FILE *file = NULL;

    wide_count = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        path,
        -1,
        NULL,
        0);
    if (wide_count <= 0) {
        return NULL;
    }
    wide_path = malloc((size_t)wide_count * sizeof(*wide_path));
    if (wide_path == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            path,
            -1,
            wide_path,
            wide_count) == 0) {
        free(wide_path);
        return NULL;
    }
    if (_wfopen_s(&file, wide_path, L"rb") != 0) {
        file = NULL;
    }
    free(wide_path);
    return file;
#else
    return fopen(path, "rb");
#endif
}

static uint16_t loim_read_be16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8U) | (uint16_t)bytes[1]);
}

static uint32_t loim_read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24U) |
           ((uint32_t)bytes[1] << 16U) |
           ((uint32_t)bytes[2] << 8U) |
           (uint32_t)bytes[3];
}

static uint16_t loim_read_le16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static uint32_t loim_read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static loim_status loim_validate_dimensions(loim_image_info *info)
{
    if (info->width_px == 0U || info->height_px == 0U) {
        return LOIM_ERROR_CORRUPT_IMAGE;
    }
    return LOIM_OK;
}

static loim_status loim_probe_png(
    FILE *file,
    const uint8_t *header,
    size_t header_size,
    loim_image_info *out_info)
{
    (void)file;
    if (header_size < 24U || memcmp(header + 12U, "IHDR", 4U) != 0) {
        return LOIM_ERROR_CORRUPT_IMAGE;
    }
    out_info->format = LOIM_IMAGE_FORMAT_PNG;
    out_info->width_px = loim_read_be32(header + 16U);
    out_info->height_px = loim_read_be32(header + 20U);
    return loim_validate_dimensions(out_info);
}

static loim_status loim_probe_gif(
    FILE *file,
    const uint8_t *header,
    size_t header_size,
    loim_image_info *out_info)
{
    (void)file;
    if (header_size < 10U) {
        return LOIM_ERROR_CORRUPT_IMAGE;
    }
    out_info->format = LOIM_IMAGE_FORMAT_GIF;
    out_info->width_px = (uint32_t)loim_read_le16(header + 6U);
    out_info->height_px = (uint32_t)loim_read_le16(header + 8U);
    return loim_validate_dimensions(out_info);
}

static loim_status loim_probe_bmp(
    FILE *file,
    const uint8_t *header,
    size_t header_size,
    loim_image_info *out_info)
{
    uint32_t dib_size;
    uint32_t width;
    uint32_t raw_height;
    (void)file;

    if (header_size < 26U) {
        return LOIM_ERROR_CORRUPT_IMAGE;
    }
    dib_size = loim_read_le32(header + 14U);
    if (dib_size == 12U) {
        width = (uint32_t)loim_read_le16(header + 18U);
        raw_height = (uint32_t)loim_read_le16(header + 20U);
    } else if (dib_size >= 40U) {
        int32_t signed_width = (int32_t)loim_read_le32(header + 18U);
        int32_t signed_height = (int32_t)loim_read_le32(header + 22U);

        if (signed_width <= 0 || signed_height == 0 || signed_height == INT32_MIN) {
            return LOIM_ERROR_CORRUPT_IMAGE;
        }
        width = (uint32_t)signed_width;
        raw_height = signed_height < 0
            ? (uint32_t)(-signed_height)
            : (uint32_t)signed_height;
    } else {
        return LOIM_ERROR_UNSUPPORTED_FORMAT;
    }
    out_info->format = LOIM_IMAGE_FORMAT_BMP;
    out_info->width_px = width;
    out_info->height_px = raw_height;
    return loim_validate_dimensions(out_info);
}

static int loim_jpeg_is_start_of_frame(uint8_t marker)
{
    return marker == 0xC0U || marker == 0xC1U || marker == 0xC2U ||
           marker == 0xC3U || marker == 0xC5U || marker == 0xC6U ||
           marker == 0xC7U || marker == 0xC9U || marker == 0xCAU ||
           marker == 0xCBU || marker == 0xCDU || marker == 0xCEU ||
           marker == 0xCFU;
}

static loim_status loim_probe_jpeg(FILE *file, loim_image_info *out_info)
{
    int value;

    if (fseek(file, 2L, SEEK_SET) != 0) {
        return LOIM_ERROR_IO;
    }
    for (;;) {
        uint8_t marker;
        uint8_t length_bytes[2];
        uint16_t segment_length;

        do {
            value = fgetc(file);
            if (value == EOF) {
                return LOIM_ERROR_CORRUPT_IMAGE;
            }
        } while (value != 0xFF);
        do {
            value = fgetc(file);
            if (value == EOF) {
                return LOIM_ERROR_CORRUPT_IMAGE;
            }
        } while (value == 0xFF);
        marker = (uint8_t)value;

        if (marker == 0xD9U || marker == 0xDAU) {
            return LOIM_ERROR_CORRUPT_IMAGE;
        }
        if (marker == 0x01U || (marker >= 0xD0U && marker <= 0xD7U)) {
            continue;
        }
        if (fread(length_bytes, 1U, sizeof(length_bytes), file) != sizeof(length_bytes)) {
            return LOIM_ERROR_CORRUPT_IMAGE;
        }
        segment_length = loim_read_be16(length_bytes);
        if (segment_length < 2U) {
            return LOIM_ERROR_CORRUPT_IMAGE;
        }
        if (loim_jpeg_is_start_of_frame(marker)) {
            uint8_t dimensions[5];

            if (segment_length < 7U ||
                fread(dimensions, 1U, sizeof(dimensions), file) != sizeof(dimensions)) {
                return LOIM_ERROR_CORRUPT_IMAGE;
            }
            out_info->format = LOIM_IMAGE_FORMAT_JPEG;
            out_info->height_px = (uint32_t)loim_read_be16(dimensions + 1U);
            out_info->width_px = (uint32_t)loim_read_be16(dimensions + 3U);
            return loim_validate_dimensions(out_info);
        }
        if (fseek(file, (long)segment_length - 2L, SEEK_CUR) != 0) {
            return LOIM_ERROR_CORRUPT_IMAGE;
        }
    }
}

loim_status loim_image_probe_file(const char *path, loim_image_info *out_info)
{
    static const uint8_t png_signature[8] = {
        0x89U, 0x50U, 0x4EU, 0x47U, 0x0DU, 0x0AU, 0x1AU, 0x0AU
    };
    uint8_t header[32];
    size_t bytes_read;
    FILE *file;
    loim_status status;

    if (path == NULL || path[0] == '\0' || out_info == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    memset(out_info, 0, sizeof(*out_info));
    file = loim_open_binary_read(path);
    if (file == NULL) {
        return LOIM_ERROR_NOT_FOUND;
    }
    bytes_read = fread(header, 1U, sizeof(header), file);
    if (ferror(file) != 0) {
        fclose(file);
        return LOIM_ERROR_IO;
    }

    if (bytes_read >= sizeof(png_signature) &&
        memcmp(header, png_signature, sizeof(png_signature)) == 0) {
        status = loim_probe_png(file, header, bytes_read, out_info);
    } else if (bytes_read >= 6U &&
               (memcmp(header, "GIF87a", 6U) == 0 ||
                memcmp(header, "GIF89a", 6U) == 0)) {
        status = loim_probe_gif(file, header, bytes_read, out_info);
    } else if (bytes_read >= 2U && header[0] == 'B' && header[1] == 'M') {
        status = loim_probe_bmp(file, header, bytes_read, out_info);
    } else if (bytes_read >= 2U && header[0] == 0xFFU && header[1] == 0xD8U) {
        status = loim_probe_jpeg(file, out_info);
    } else {
        status = LOIM_ERROR_UNSUPPORTED_FORMAT;
    }
    fclose(file);
    return status;
}

const char *loim_image_format_name(loim_image_format format)
{
    switch (format) {
    case LOIM_IMAGE_FORMAT_PNG:
        return "PNG";
    case LOIM_IMAGE_FORMAT_JPEG:
        return "JPEG";
    case LOIM_IMAGE_FORMAT_GIF:
        return "GIF";
    case LOIM_IMAGE_FORMAT_BMP:
        return "BMP";
    default:
        return "unknown";
    }
}
