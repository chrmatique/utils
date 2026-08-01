#ifndef HASHCHECK_SHA256_HPP
#define HASHCHECK_SHA256_HPP

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace hashcheck {

using Sha256Digest = std::array<uint8_t, 32>;

namespace detail {

inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline uint32_t ep0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline uint32_t ep1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline uint32_t sig0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline uint32_t sig1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

struct Sha256Context {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[64];
    size_t buffer_size;

    Sha256Context() : bit_count(0), buffer_size(0) {
        state[0] = 0x6a09e667;
        state[1] = 0xbb67ae85;
        state[2] = 0x3c6ef372;
        state[3] = 0xa54ff53a;
        state[4] = 0x510e527f;
        state[5] = 0x9b05688c;
        state[6] = 0x1f83d9ab;
        state[7] = 0x5be0cd19;
    }

    void transform(const uint8_t block[64]) {
        uint32_t m[64];
        for (int i = 0; i < 16; ++i) {
            m[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];
        uint32_t f = state[5];
        uint32_t g = state[6];
        uint32_t h = state[7];

        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
            0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
            0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
            0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
            0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
            0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
            0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
            0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
            0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
            0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + ep1(e) + ch(e, f, g) + k[i] + m[i];
            uint32_t t2 = ep0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    void update(const uint8_t* data, size_t size) {
        bit_count += static_cast<uint64_t>(size) * 8;
        while (size > 0) {
            size_t available = 64 - buffer_size;
            size_t to_copy = size < available ? size : available;
            std::memcpy(buffer + buffer_size, data, to_copy);
            buffer_size += to_copy;
            data += to_copy;
            size -= to_copy;

            if (buffer_size == 64) {
                transform(buffer);
                buffer_size = 0;
            }
        }
    }

    void finish(uint8_t digest[32]) {
        buffer[buffer_size++] = 0x80;
        if (buffer_size > 56) {
            std::memset(buffer + buffer_size, 0, 64 - buffer_size);
            transform(buffer);
            buffer_size = 0;
        }
        std::memset(buffer + buffer_size, 0, 56 - buffer_size);

        uint64_t bit_count_be = bit_count;
        uint8_t len_bytes[8] = {
            static_cast<uint8_t>(bit_count_be >> 56),
            static_cast<uint8_t>(bit_count_be >> 48),
            static_cast<uint8_t>(bit_count_be >> 40),
            static_cast<uint8_t>(bit_count_be >> 32),
            static_cast<uint8_t>(bit_count_be >> 24),
            static_cast<uint8_t>(bit_count_be >> 16),
            static_cast<uint8_t>(bit_count_be >> 8),
            static_cast<uint8_t>(bit_count_be)
        };
        std::memcpy(buffer + 56, len_bytes, 8);
        transform(buffer);

        for (int i = 0; i < 8; ++i) {
            digest[i * 4] = static_cast<uint8_t>(state[i] >> 24);
            digest[i * 4 + 1] = static_cast<uint8_t>(state[i] >> 16);
            digest[i * 4 + 2] = static_cast<uint8_t>(state[i] >> 8);
            digest[i * 4 + 3] = static_cast<uint8_t>(state[i]);
        }
    }
};

} // namespace detail

inline Sha256Digest sha256(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    detail::Sha256Context ctx;
    ctx.update(bytes, size);
    Sha256Digest digest;
    ctx.finish(digest.data());
    return digest;
}

inline bool constant_time_equal(const Sha256Digest& a, const Sha256Digest& b) {
    uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}

inline bool verify(const void* data, size_t size, const Sha256Digest& expected) {
    return constant_time_equal(sha256(data, size), expected);
}

inline bool verify(const void* data, size_t size, const uint8_t* expected) {
    Sha256Digest expected_array;
    std::memcpy(expected_array.data(), expected, expected_array.size());
    return verify(data, size, expected_array);
}

inline std::string digest_to_hex(const Sha256Digest& digest) {
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(digest.size() * 2);
    for (uint8_t byte : digest) {
        out.push_back(hex[byte >> 4]);
        out.push_back(hex[byte & 0x0f]);
    }
    return out;
}

inline bool digest_from_hex(const char* hex, Sha256Digest& out) {
    if (hex == nullptr) {
        return false;
    }

    size_t len = 0;
    while (hex[len] != '\0') {
        ++len;
    }
    if (len != 64) {
        return false;
    }

    for (size_t i = 0; i < 32; ++i) {
        char hi = hex[i * 2];
        char lo = hex[i * 2 + 1];
        if (!std::isxdigit(static_cast<unsigned char>(hi)) ||
            !std::isxdigit(static_cast<unsigned char>(lo))) {
            return false;
        }
        auto nibble = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
            return 0;
        };
        out[i] = static_cast<uint8_t>((nibble(hi) << 4) | nibble(lo));
    }
    return true;
}

inline bool verify_hex(const void* data, size_t size, const char* expected_hex) {
    Sha256Digest expected;
    if (!digest_from_hex(expected_hex, expected)) {
        return false;
    }
    return verify(data, size, expected);
}

} // namespace hashcheck

#endif // HASHCHECK_SHA256_HPP
