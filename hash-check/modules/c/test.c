/* Minimal sanity check for modules/c/sha256.h */
#include "sha256.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    /* NIST SHA-256 test vector for the string "abc". */
    static const hc_u8 expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };

    hc_u8 digest[32];
    int i;

    hc_sha256("abc", 3, digest);

    for (i = 0; i < 32; i++) {
        printf("%02x", digest[i]);
    }
    printf("\n");

    if (memcmp(digest, expected, 32) != 0) {
        printf("FAILED: digest does not match NIST vector.\n");
        return 1;
    }
    if (!hc_sha256_verify("abc", 3, expected)) {
        printf("FAILED: verify() rejected the correct vector.\n");
        return 1;
    }
    if (hc_sha256_verify("wrong", 5, expected)) {
        printf("FAILED: verify() accepted a wrong vector.\n");
        return 1;
    }

    printf("All tests passed.\n");
    return 0;
}
