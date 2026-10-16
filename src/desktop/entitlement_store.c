#include "entitlement_store.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <Security/SecRandom.h>
#include <sys/stat.h>
#else
#include <sys/random.h>
#include <errno.h>
#include <sys/stat.h>
#endif

bool loim_entitlement_nonce(char output[LOIM_ENTITLEMENT_NONCE_CAPACITY])
{
    uint8_t bytes[32];
    static const char hex[] = "0123456789abcdef";
    size_t i;
    output[0] = '\0';
#if defined(_WIN32)
    if (BCryptGenRandom(NULL, bytes, (ULONG)sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return false;
#elif defined(__APPLE__)
    if (SecRandomCopyBytes(kSecRandomDefault, sizeof(bytes), bytes) != errSecSuccess) return false;
#else
    size_t received = 0U;
    while (received < sizeof(bytes)) {
        ssize_t count = getrandom(bytes + received, sizeof(bytes) - received, 0U);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            FILE *source = fopen("/dev/urandom", "rb");
            bool read_ok = source != NULL && fread(bytes, 1U, sizeof(bytes), source) == sizeof(bytes);
            if (source != NULL) fclose(source);
            if (!read_ok) return false;
            break;
        }
        received += (size_t)count;
    }
#endif
    for (i = 0U; i < sizeof(bytes); ++i) {
        output[i * 2U] = hex[bytes[i] >> 4U];
        output[i * 2U + 1U] = hex[bytes[i] & 15U];
    }
    output[64] = '\0';
    return true;
}

static char *cache_path(void)
{
#if defined(LOIM_INTERNAL_TESTS)
    char *directory = SDL_GetPrefPath("ctdy123", "LoimReader-tests");
#else
    char *directory = SDL_GetPrefPath("ctdy123", "LoimReader");
#endif
    char *path = NULL;
    if (directory != NULL) (void)SDL_asprintf(&path, "%sentitlement-v1.cache", directory);
    SDL_free(directory);
    return path;
}

bool loim_entitlement_cache_save(const char *payload, const char *signature)
{
    char nonce[65];
    char *path;
    char *temporary = NULL;
    SDL_IOStream *file = NULL;
    bool success = false;
    size_t length;
    if (payload == NULL || signature == NULL) return false;
    path = cache_path();
    length = strlen(payload);
    if (path == NULL || length == 0U || length >= LOIM_ENTITLEMENT_PAYLOAD_CAPACITY ||
        strlen(signature) != 128U || !loim_entitlement_nonce(nonce)) goto cleanup;
    if (SDL_asprintf(&temporary, "%s.%s.tmp", path, nonce) < 0) goto cleanup;
    file = SDL_IOFromFile(temporary, "wb");
    if (file == NULL) goto cleanup;
#if !defined(_WIN32)
    if (chmod(temporary, S_IRUSR | S_IWUSR) != 0) goto cleanup;
#endif
    success = SDL_WriteIO(file, payload, length) == length &&
        SDL_WriteIO(file, signature, 128U) == 128U;
    if (!SDL_CloseIO(file)) success = false;
    file = NULL;
    if (success) success = SDL_RenamePath(temporary, path);
cleanup:
    if (file != NULL) (void)SDL_CloseIO(file);
    if (temporary != NULL) (void)SDL_RemovePath(temporary);
    SDL_free(temporary);
    SDL_free(path);
    return success;
}

bool loim_entitlement_cache_load(const char *device, const char *account,
    uint64_t now, loim_entitlement *out)
{
    char *path;
    SDL_PathInfo info;
    char *bytes = NULL;
    size_t length = 0U;
    char signature[129];
    bool success = false;
    if (out == NULL) return false;
    path = cache_path();
    memset(out, 0, sizeof(*out));
    if (path == NULL || !SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE ||
        info.size <= 128U || info.size >= LOIM_ENTITLEMENT_PAYLOAD_CAPACITY + 128U) goto cleanup;
    {
        SDL_IOStream *file = SDL_IOFromFile(path, "rb");
        if (file == NULL) goto cleanup;
        bytes = SDL_malloc(LOIM_ENTITLEMENT_PAYLOAD_CAPACITY + 129U);
        if (bytes != NULL) {
            length = SDL_ReadIO(file, bytes, LOIM_ENTITLEMENT_PAYLOAD_CAPACITY + 128U);
            bytes[length] = '\0';
        }
        (void)SDL_CloseIO(file);
    }
    if (bytes == NULL || length <= 128U || length >= LOIM_ENTITLEMENT_PAYLOAD_CAPACITY + 128U) goto cleanup;
    memcpy(signature, bytes + length - 128U, 128U);
    signature[128] = '\0';
    bytes[length - 128U] = '\0';
    if (strlen(bytes) != length - 128U) goto cleanup;
    success = loim_entitlement_verify(bytes, signature, device, account, NULL, now, out) == LOIM_OK;
cleanup:
    SDL_free(bytes);
    SDL_free(path);
    return success;
}

void loim_entitlement_cache_clear(void)
{
    char *path = cache_path();
    if (path != NULL) (void)SDL_RemovePath(path);
    SDL_free(path);
}
