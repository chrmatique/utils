#include "sha256.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    // NIST SHA256 test vector for the string "abc".
    const uint8_t expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };

    const char* data = "abc";
    hashcheck::Sha256Digest digest = hashcheck::sha256(data, 3);

    for (size_t i = 0; i < digest.size(); ++i) {
        std::printf("%02x", digest[i]);
    }
    std::printf("\n");

    assert(std::memcmp(digest.data(), expected, 32) == 0);
    assert(hashcheck::verify(data, 3, expected));
    assert(!hashcheck::verify("wrong", 5, expected));

    std::printf("All tests passed.\n");
    return 0;
}
