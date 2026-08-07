#include "loim/machine_code.h"

#include <string.h>

typedef struct md5_context {
    uint32_t state[4];
    uint64_t byte_count;
    uint8_t block[64];
    size_t block_size;
} md5_context;

static uint32_t rotate_left(uint32_t value, uint8_t count)
{
    return (value << count) | (value >> (32U - count));
}

static void md5_transform(uint32_t state[4], const uint8_t block[64])
{
    static const uint32_t constants[64] = {
        0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU,
        0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
        0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU,
        0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,
        0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU,
        0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
        0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU,
        0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,
        0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU,
        0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
        0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U,
        0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
        0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U,
        0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
        0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
        0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U
    };
    static const uint8_t shifts[64] = {
        7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U,
        7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U,
        5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U,
        5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U,
        4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U,
        4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U,
        6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U,
        6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U
    };
    uint32_t words[16];
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t index;

    for (index = 0U; index < 16U; ++index) {
        size_t offset = (size_t)index * 4U;

        words[index] = (uint32_t)block[offset] |
            ((uint32_t)block[offset + 1U] << 8U) |
            ((uint32_t)block[offset + 2U] << 16U) |
            ((uint32_t)block[offset + 3U] << 24U);
    }
    for (index = 0U; index < 64U; ++index) {
        uint32_t function;
        uint32_t word_index;
        uint32_t previous_d = d;

        if (index < 16U) {
            function = (b & c) | ((~b) & d);
            word_index = index;
        } else if (index < 32U) {
            function = (d & b) | ((~d) & c);
            word_index = (5U * index + 1U) % 16U;
        } else if (index < 48U) {
            function = b ^ c ^ d;
            word_index = (3U * index + 5U) % 16U;
        } else {
            function = c ^ (b | (~d));
            word_index = (7U * index) % 16U;
        }
        d = c;
        c = b;
        b += rotate_left(
            a + function + constants[index] + words[word_index],
            shifts[index]);
        a = previous_d;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static void md5_initialize(md5_context *context)
{
    context->state[0] = 0x67452301U;
    context->state[1] = 0xefcdab89U;
    context->state[2] = 0x98badcfeU;
    context->state[3] = 0x10325476U;
    context->byte_count = 0U;
    context->block_size = 0U;
}

static void md5_update(md5_context *context, const uint8_t *data, size_t size)
{
    context->byte_count += (uint64_t)size;
    while (size > 0U) {
        size_t available = sizeof(context->block) - context->block_size;
        size_t copied = size < available ? size : available;

        memcpy(context->block + context->block_size, data, copied);
        context->block_size += copied;
        data += copied;
        size -= copied;
        if (context->block_size == sizeof(context->block)) {
            md5_transform(context->state, context->block);
            context->block_size = 0U;
        }
    }
}

static void md5_finish(md5_context *context, uint8_t digest[16])
{
    uint64_t bit_count = context->byte_count * 8U;
    uint8_t padding[64] = {0x80U};
    uint8_t length_bytes[8];
    size_t padding_size = context->block_size < 56U
        ? 56U - context->block_size
        : 120U - context->block_size;
    uint32_t index;

    for (index = 0U; index < 8U; ++index) {
        length_bytes[index] = (uint8_t)(bit_count >> (index * 8U));
    }
    md5_update(context, padding, padding_size);
    md5_update(context, length_bytes, sizeof(length_bytes));
    for (index = 0U; index < 4U; ++index) {
        uint32_t value = context->state[index];
        size_t offset = (size_t)index * 4U;

        digest[offset] = (uint8_t)value;
        digest[offset + 1U] = (uint8_t)(value >> 8U);
        digest[offset + 2U] = (uint8_t)(value >> 16U);
        digest[offset + 3U] = (uint8_t)(value >> 24U);
    }
}

loim_status loim_machine_code_from_bytes(
    const uint8_t *machine_id,
    size_t machine_id_size,
    char *output,
    size_t output_capacity)
{
    static const char digits[] = "0123456789ABCDEF";
    md5_context context;
    uint8_t digest[16];
    size_t index;

    if (machine_id == NULL || machine_id_size == 0U || output == NULL ||
        output_capacity < LOIM_MACHINE_CODE_CAPACITY) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    md5_initialize(&context);
    md5_update(&context, machine_id, machine_id_size);
    md5_finish(&context, digest);
    for (index = 0U; index < 10U; ++index) {
        size_t output_index = (index / 2U) * 5U + (index % 2U) * 2U;

        output[output_index] = digits[digest[index] >> 4U];
        output[output_index + 1U] = digits[digest[index] & 0x0FU];
        if (index % 2U != 0U && index < 9U) {
            output[output_index + 2U] = '-';
        }
    }
    output[LOIM_MACHINE_CODE_CAPACITY - 1U] = '\0';
    return LOIM_OK;
}
