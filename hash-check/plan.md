<!-- 1ab97489-a7c5-4a2f-a476-a0574ca535f4 -->
---
todos:
  - id: "create-sha256-hpp"
    content: "Create sha256.hpp with self-contained SHA256 implementation and verify() API"
    status: pending
  - id: "create-test-cpp"
    content: "Create test.cpp with a NIST SHA256 vector sanity check and verify() example"
    status: pending
  - id: "verify-build"
    content: "Compile and run test.cpp with C++11 to confirm the header works"
    status: pending
isProject: false
---
# Self-contained SHA256 verification header

Create a portable, header-only C++ library in `/Users/croyer/Documents/Cursor Repos/hash-check` that computes SHA256 hashes and verifies data against an expected 32-byte raw checksum, with no external dependencies.

## Files to create

1. `[sha256.hpp]` — Single, self-contained header implementing SHA256 and a small verification API.
2. `[test.cpp]` — Minimal buildable sanity check using a known NIST SHA256 test vector and the `verify()` API.

## Proposed public API (C++11)

```cpp
namespace hashcheck {
    using Sha256Digest = std::array<uint8_t, 32>;

    Sha256Digest sha256(const void* data, size_t size);
    bool verify(const void* data, size_t size, const Sha256Digest& expected);
    bool constant_time_equal(const Sha256Digest& a, const Sha256Digest& b);
}
```

## Implementation details

- Implement the full SHA256 algorithm inline (initial hash values, round constants, 512-bit block processing, padding, and final digest). No OpenSSL or OS-specific APIs are used.
- Keep all code in a single header file guarded by include guards; no compilation step is required for the library itself.
- `sha256()` accepts a `const void*` and length, so it works for any contiguous binary data (`std::string`, `std::vector`, raw buffers, etc.).
- `verify()` computes the digest and compares it to the expected raw 32-byte checksum using a constant-time comparison to mitigate timing attacks.
- Target C++11 for broad portability; use only `<cstdint>`, `<cstddef>`, `<cstring>`, `<array>`, and `<stdexcept>` (optional). `std::array` is C++11, so the header will require `-std=c++11` or newer.
- Because the user selected raw bytes as the checksum format, `verify()` will take `std::array<uint8_t, 32>` (or `uint8_t[32]`) as the expected checksum. A small convenience overload taking a `const uint8_t*` pointer can be added if helpful.

## Sanity check

`test.cpp` will hash the string `"abc"` and assert the result matches the NIST SHA256 vector:

```
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
```

It will also call `hashcheck::verify()` with the same vector to exercise the verification path. Build example:

```bash
c++ -std=c++11 -Wall -Wextra test.cpp -o test && ./test
```

## Optional follow-up (not in scope unless requested)

- Hex encoder/decoder helpers for users who store checksums as hex strings.
- C++17 `std::string_view` overload for zero-copy hashing of string-like inputs.
- Additional NIST test vectors for broader validation.
