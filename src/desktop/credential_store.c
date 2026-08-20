#if !defined(_WIN32) && !defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "credential_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#else
#include <stdlib.h>
#include <sys/wait.h>
#endif

static void wipe_bytes(void *data, size_t length)
{
    volatile uint8_t *bytes = data;
    size_t index;

    for (index = 0U; index < length; ++index) {
        bytes[index] = 0U;
    }
}

#if defined(_WIN32)

static wchar_t credential_target[] = L"com.ctdy123.loimreader/login";
static wchar_t credential_username[] = L"LoimReader";

bool loim_credential_store_load(loim_credentials *out_credentials)
{
    PCREDENTIALW credential = NULL;
    loim_status status;

    if (out_credentials == NULL ||
        !CredReadW(credential_target, CRED_TYPE_GENERIC, 0U, &credential)) {
        return false;
    }
    status = loim_credentials_decode(
        credential->CredentialBlob,
        (size_t)credential->CredentialBlobSize,
        out_credentials);
    CredFree(credential);
    return status == LOIM_OK;
}

bool loim_credential_store_save(const loim_credentials *credentials)
{
    uint8_t encoded[LOIM_CREDENTIALS_ENCODED_CAPACITY];
    size_t encoded_length = 0U;
    CREDENTIALW credential;
    bool saved;

    if (loim_credentials_encode(
            credentials,
            encoded,
            sizeof(encoded),
            &encoded_length) != LOIM_OK || encoded_length > UINT32_MAX) {
        return false;
    }
    memset(&credential, 0, sizeof(credential));
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = credential_target;
    credential.CredentialBlobSize = (DWORD)encoded_length;
    credential.CredentialBlob = encoded;
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = credential_username;
    saved = CredWriteW(&credential, 0U) != 0;
    wipe_bytes(encoded, sizeof(encoded));
    return saved;
}

bool loim_credential_store_clear(void)
{
    return CredDeleteW(credential_target, CRED_TYPE_GENERIC, 0U) != 0 ||
        GetLastError() == ERROR_NOT_FOUND;
}

#elif defined(__APPLE__)

static void keychain_add_identity(CFMutableDictionaryRef dictionary)
{
    CFDictionarySetValue(dictionary, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(
        dictionary, kSecAttrService, CFSTR("com.ctdy123.loimreader"));
    CFDictionarySetValue(dictionary, kSecAttrAccount, CFSTR("login"));
}

bool loim_credential_store_load(loim_credentials *out_credentials)
{
    CFMutableDictionaryRef query;
    CFTypeRef result = NULL;
    const UInt8 *bytes;
    CFIndex length;
    loim_status status;

    if (out_credentials == NULL) {
        return false;
    }
    query = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (query == NULL) {
        return false;
    }
    keychain_add_identity(query);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    if (SecItemCopyMatching(query, &result) != errSecSuccess || result == NULL ||
        CFGetTypeID(result) != CFDataGetTypeID()) {
        if (result != NULL) {
            CFRelease(result);
        }
        CFRelease(query);
        return false;
    }
    length = CFDataGetLength((CFDataRef)result);
    bytes = CFDataGetBytePtr((CFDataRef)result);
    status = length >= 0
        ? loim_credentials_decode(bytes, (size_t)length, out_credentials)
        : LOIM_ERROR_INVALID_DATA;
    CFRelease(result);
    CFRelease(query);
    return status == LOIM_OK;
}

bool loim_credential_store_save(const loim_credentials *credentials)
{
    uint8_t encoded[LOIM_CREDENTIALS_ENCODED_CAPACITY];
    size_t encoded_length = 0U;
    CFMutableDictionaryRef query;
    CFMutableDictionaryRef attributes;
    CFDataRef data;
    OSStatus status;

    if (loim_credentials_encode(
            credentials,
            encoded,
            sizeof(encoded),
            &encoded_length) != LOIM_OK) {
        return false;
    }
    data = CFDataCreate(kCFAllocatorDefault, encoded, (CFIndex)encoded_length);
    query = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    attributes = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (data == NULL || query == NULL || attributes == NULL) {
        if (data != NULL) CFRelease(data);
        if (query != NULL) CFRelease(query);
        if (attributes != NULL) CFRelease(attributes);
        wipe_bytes(encoded, sizeof(encoded));
        return false;
    }
    keychain_add_identity(query);
    CFDictionarySetValue(attributes, kSecValueData, data);
    status = SecItemUpdate(query, attributes);
    if (status == errSecItemNotFound) {
        CFDictionarySetValue(query, kSecValueData, data);
        status = SecItemAdd(query, NULL);
    }
    CFRelease(attributes);
    CFRelease(query);
    CFRelease(data);
    wipe_bytes(encoded, sizeof(encoded));
    return status == errSecSuccess;
}

bool loim_credential_store_clear(void)
{
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    OSStatus status;

    if (query == NULL) {
        return false;
    }
    keychain_add_identity(query);
    status = SecItemDelete(query);
    CFRelease(query);
    return status == errSecSuccess || status == errSecItemNotFound;
}

#else

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static void encode_hex(
    const uint8_t *bytes,
    size_t length,
    char *output)
{
    static const char digits[] = "0123456789abcdef";
    size_t index;

    for (index = 0U; index < length; ++index) {
        output[index * 2U] = digits[bytes[index] >> 4U];
        output[index * 2U + 1U] = digits[bytes[index] & 0x0FU];
    }
    output[length * 2U] = '\0';
}

bool loim_credential_store_load(loim_credentials *out_credentials)
{
    static const char command[] =
        "secret-tool lookup application com.ctdy123.loimreader kind credentials "
        "2>/dev/null";
    char hex[LOIM_CREDENTIALS_ENCODED_CAPACITY * 2U + 2U];
    uint8_t encoded[LOIM_CREDENTIALS_ENCODED_CAPACITY];
    FILE *pipe;
    size_t length;
    size_t byte_count;
    size_t index;
    int process_status;
    bool loaded = false;

    if (out_credentials == NULL) {
        return false;
    }
    pipe = popen(command, "r");
    if (pipe == NULL || fgets(hex, sizeof(hex), pipe) == NULL) {
        if (pipe != NULL) (void)pclose(pipe);
        goto cleanup;
    }
    process_status = pclose(pipe);
    if (!WIFEXITED(process_status) || WEXITSTATUS(process_status) != 0) {
        goto cleanup;
    }
    length = strcspn(hex, "\r\n");
    hex[length] = '\0';
    if (length == 0U || (length & 1U) != 0U) {
        goto cleanup;
    }
    byte_count = length / 2U;
    if (byte_count > sizeof(encoded)) {
        goto cleanup;
    }
    for (index = 0U; index < byte_count; ++index) {
        int high = hex_value(hex[index * 2U]);
        int low = hex_value(hex[index * 2U + 1U]);

        if (high < 0 || low < 0) {
            goto cleanup;
        }
        encoded[index] = (uint8_t)((unsigned)high << 4U | (unsigned)low);
    }
    loaded = loim_credentials_decode(
        encoded, byte_count, out_credentials) == LOIM_OK;
cleanup:
    wipe_bytes(encoded, sizeof(encoded));
    wipe_bytes(hex, sizeof(hex));
    return loaded;
}

bool loim_credential_store_save(const loim_credentials *credentials)
{
    static const char command[] =
        "secret-tool store --label='LoimReader login' "
        "application com.ctdy123.loimreader kind credentials 2>/dev/null";
    uint8_t encoded[LOIM_CREDENTIALS_ENCODED_CAPACITY];
    char hex[LOIM_CREDENTIALS_ENCODED_CAPACITY * 2U + 1U];
    size_t encoded_length = 0U;
    FILE *pipe;
    int process_status;
    bool saved;

    if (loim_credentials_encode(
            credentials,
            encoded,
            sizeof(encoded),
            &encoded_length) != LOIM_OK) {
        return false;
    }
    encode_hex(encoded, encoded_length, hex);
    pipe = popen(command, "w");
    if (pipe == NULL) {
        wipe_bytes(encoded, sizeof(encoded));
        wipe_bytes(hex, sizeof(hex));
        return false;
    }
    if (fprintf(pipe, "%s\n", hex) < 0) {
        (void)pclose(pipe);
        wipe_bytes(encoded, sizeof(encoded));
        wipe_bytes(hex, sizeof(hex));
        return false;
    }
    process_status = pclose(pipe);
    saved = WIFEXITED(process_status) && WEXITSTATUS(process_status) == 0;
    wipe_bytes(encoded, sizeof(encoded));
    wipe_bytes(hex, sizeof(hex));
    return saved;
}

bool loim_credential_store_clear(void)
{
    static const char command[] =
        "secret-tool clear application com.ctdy123.loimreader "
        "kind credentials >/dev/null 2>&1";
    int process_status = system(command);

    return WIFEXITED(process_status) && WEXITSTATUS(process_status) == 0;
}

#endif
