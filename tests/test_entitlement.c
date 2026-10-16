#include "loim/entitlement.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LOIM_ENTITLEMENT_VECTOR_DATA 1
#include "entitlement_vectors.h"
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "failed line %d: %s\n", __LINE__, #condition); return EXIT_FAILURE; } } while (0)
int main(void)
{
    const char *device = "9001-5098-3CD2-4FB0";
    const char *account = "paid@example.com";
    const char *nonce = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    loim_entitlement result;
    size_t i;
    (void)loim_entitlement_public_key;
    CHECK(loim_entitlement_verify(test_payloads[0], test_signatures[0], device,
        " Paid@Example.com ", nonce, 0U, &result) == LOIM_OK);
    CHECK(result.licensed);
    CHECK(loim_entitlement_verify(test_payloads[1], test_signatures[1], device,
        account, nonce, 0U, &result) == LOIM_OK && !result.licensed);
    for (i = 2U; i < sizeof(test_payloads) / sizeof(test_payloads[0]); ++i) {
        CHECK(loim_entitlement_verify(test_payloads[i], test_signatures[i], device,
            account, nonce, 0U, &result) != LOIM_OK);
    }
    CHECK(loim_entitlement_verify(test_payloads[0], test_signatures[1], device,
        account, nonce, 0U, &result) != LOIM_OK);
    CHECK(loim_entitlement_verify(test_payloads[0], test_signatures[0], "another-device",
        account, nonce, 0U, &result) != LOIM_OK);
    CHECK(loim_entitlement_verify(test_payloads[0], test_signatures[0], device,
        "other@example.com", nonce, 0U, &result) != LOIM_OK);
    CHECK(loim_entitlement_verify(test_payloads[0], test_signatures[0], device,
        account, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", 0U, &result) != LOIM_OK);
    CHECK(loim_entitlement_verify(test_payloads[0], test_signatures[0], device,
        account, NULL, 1700000001U, &result) == LOIM_OK);
    CHECK(loim_entitlement_verify(test_payloads[0], test_signatures[0], device,
        account, NULL, 1699999999U, &result) != LOIM_OK);
    CHECK(loim_entitlement_verify(test_payloads[0], test_signatures[0], device,
        account, NULL, 1700604800U, &result) != LOIM_OK);
    CHECK(loim_entitlement_verify(test_payloads[0], "", device,
        account, nonce, 0U, &result) != LOIM_OK);
    puts("all entitlement verification tests passed");
    return EXIT_SUCCESS;
}
