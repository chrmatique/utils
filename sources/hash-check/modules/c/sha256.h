/* HASHCHECK_SHA256_H -- portable, header-only SHA-256 implementation for ANSI C. */
#ifndef HASHCHECK_SHA256_H
#define HASHCHECK_SHA256_H

#include <stddef.h>
#include <limits.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Portable integer types                                                    */
/* ------------------------------------------------------------------------- */

/* We use unsigned char for bytes. */
typedef unsigned char hc_u8;

/*
 * Select a 32-bit (or wider) unsigned type.  UINT_MAX is guaranteed by
 * <limits.h>.  If unsigned int is exactly 32 bits we use it; otherwise we
 * fall back to unsigned long, which is at least 32 bits in every ANSI C
 * implementation.  All results are masked back to 32 bits after every
 * operation, so a wider type is harmless.
 */
#if (UINT_MAX == 0xFFFFFFFFU)
typedef unsigned int hc_u32;
#else
typedef unsigned long hc_u32;
#endif

#define HC_SHA256_MASK ((hc_u32)0xFFFFFFFFU)

/* Context for incremental hashing. */
typedef struct HcSha256Context {
    hc_u32 state[8];       /* current hash state */
    hc_u32 bit_count_low;  /* low 32 bits of the total bit length */
    hc_u32 bit_count_high; /* high 32 bits of the total bit length */
    hc_u8  buffer[64];     /* pending input bytes */
    size_t buffer_size;    /* number of valid bytes in buffer */
} HcSha256Context;

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

static hc_u32 hc_sha256_rotr(hc_u32 x, int n) {
    return (((x >> n) | (x << (32 - n))) & HC_SHA256_MASK);
}

static hc_u32 hc_sha256_ch(hc_u32 x, hc_u32 y, hc_u32 z) {
    return (((x & y) ^ (~x & z)) & HC_SHA256_MASK);
}

static hc_u32 hc_sha256_maj(hc_u32 x, hc_u32 y, hc_u32 z) {
    return (((x & y) ^ (x & z) ^ (y & z)) & HC_SHA256_MASK);
}

static hc_u32 hc_sha256_ep0(hc_u32 x) {
    return ((hc_sha256_rotr(x, 2) ^ hc_sha256_rotr(x, 13) ^ hc_sha256_rotr(x, 22)) & HC_SHA256_MASK);
}

static hc_u32 hc_sha256_ep1(hc_u32 x) {
    return ((hc_sha256_rotr(x, 6) ^ hc_sha256_rotr(x, 11) ^ hc_sha256_rotr(x, 25)) & HC_SHA256_MASK);
}

static hc_u32 hc_sha256_sig0(hc_u32 x) {
    return ((hc_sha256_rotr(x, 7) ^ hc_sha256_rotr(x, 18) ^ (x >> 3)) & HC_SHA256_MASK);
}

static hc_u32 hc_sha256_sig1(hc_u32 x) {
    return ((hc_sha256_rotr(x, 17) ^ hc_sha256_rotr(x, 19) ^ (x >> 10)) & HC_SHA256_MASK);
}

/* Add len*8 bits to the 64-bit bit counter stored as two 32-bit words. */
static void hc_sha256_add_bits(HcSha256Context* ctx, size_t len) {
    hc_u32 low_add  = (((hc_u32)(len & HC_SHA256_MASK)) << 3) & HC_SHA256_MASK;
    hc_u32 high_add = (hc_u32)((len >> 29) & HC_SHA256_MASK);
    hc_u32 old_low  = ctx->bit_count_low;

    ctx->bit_count_low = (old_low + low_add) & HC_SHA256_MASK;
    if (ctx->bit_count_low < old_low) {
        high_add += 1U;
    }
    ctx->bit_count_high = (ctx->bit_count_high + high_add) & HC_SHA256_MASK;
}

/* Process one 64-byte block. */
static void hc_sha256_transform(HcSha256Context* ctx, const hc_u8 block[64]) {
    hc_u32 m[64];
    int i;
    hc_u32 a, b, c, d, e, f, g, h;
    hc_u32 t1, t2;

    static const hc_u32 k[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
        0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
        0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
        0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
        0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
        0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
        0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
        0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
        0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
        0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
    };

    for (i = 0; i < 16; i++) {
        m[i] = ((((hc_u32)block[i * 4])     << 24) |
                (((hc_u32)block[i * 4 + 1]) << 16) |
                (((hc_u32)block[i * 4 + 2]) <<  8) |
                (((hc_u32)block[i * 4 + 3]))) & HC_SHA256_MASK;
    }

    for (i = 16; i < 64; i++) {
        m[i] = (hc_sha256_sig1(m[i - 2]) + m[i - 7] +
                hc_sha256_sig0(m[i - 15]) + m[i - 16]) & HC_SHA256_MASK;
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        t1 = (h + hc_sha256_ep1(e) + hc_sha256_ch(e, f, g) + k[i] + m[i]) & HC_SHA256_MASK;
        t2 = (hc_sha256_ep0(a) + hc_sha256_maj(a, b, c)) & HC_SHA256_MASK;
        h = g;
        g = f;
        f = e;
        e = (d + t1) & HC_SHA256_MASK;
        d = c;
        c = b;
        b = a;
        a = (t1 + t2) & HC_SHA256_MASK;
    }

    ctx->state[0] = (ctx->state[0] + a) & HC_SHA256_MASK;
    ctx->state[1] = (ctx->state[1] + b) & HC_SHA256_MASK;
    ctx->state[2] = (ctx->state[2] + c) & HC_SHA256_MASK;
    ctx->state[3] = (ctx->state[3] + d) & HC_SHA256_MASK;
    ctx->state[4] = (ctx->state[4] + e) & HC_SHA256_MASK;
    ctx->state[5] = (ctx->state[5] + f) & HC_SHA256_MASK;
    ctx->state[6] = (ctx->state[6] + g) & HC_SHA256_MASK;
    ctx->state[7] = (ctx->state[7] + h) & HC_SHA256_MASK;
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

static void hc_sha256_init(HcSha256Context* ctx) {
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
    ctx->bit_count_low  = 0U;
    ctx->bit_count_high = 0U;
    ctx->buffer_size    = 0U;
}

static void hc_sha256_update(HcSha256Context* ctx, const void* data, size_t len) {
    const hc_u8* p = (const hc_u8*)data;
    size_t remaining = len;

    hc_sha256_add_bits(ctx, len);

    while (remaining > 0) {
        size_t avail = 64 - ctx->buffer_size;
        size_t copy  = remaining < avail ? remaining : avail;

        memcpy(ctx->buffer + ctx->buffer_size, p, copy);
        ctx->buffer_size += copy;
        p += copy;
        remaining -= copy;

        if (ctx->buffer_size == 64) {
            hc_sha256_transform(ctx, ctx->buffer);
            ctx->buffer_size = 0;
        }
    }
}

static void hc_sha256_finish(HcSha256Context* ctx, hc_u8 digest[32]) {
    int i;

    ctx->buffer[ctx->buffer_size++] = 0x80U;

    if (ctx->buffer_size > 56) {
        memset(ctx->buffer + ctx->buffer_size, 0, 64 - ctx->buffer_size);
        hc_sha256_transform(ctx, ctx->buffer);
        ctx->buffer_size = 0;
    }

    memset(ctx->buffer + ctx->buffer_size, 0, 56 - ctx->buffer_size);

    ctx->buffer[56] = (hc_u8)(ctx->bit_count_high >> 24);
    ctx->buffer[57] = (hc_u8)(ctx->bit_count_high >> 16);
    ctx->buffer[58] = (hc_u8)(ctx->bit_count_high >>  8);
    ctx->buffer[59] = (hc_u8)(ctx->bit_count_high);
    ctx->buffer[60] = (hc_u8)(ctx->bit_count_low  >> 24);
    ctx->buffer[61] = (hc_u8)(ctx->bit_count_low  >> 16);
    ctx->buffer[62] = (hc_u8)(ctx->bit_count_low  >>  8);
    ctx->buffer[63] = (hc_u8)(ctx->bit_count_low);

    hc_sha256_transform(ctx, ctx->buffer);

    for (i = 0; i < 8; i++) {
        digest[i * 4]     = (hc_u8)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (hc_u8)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (hc_u8)(ctx->state[i] >>  8);
        digest[i * 4 + 3] = (hc_u8)(ctx->state[i]);
    }
}

/* One-shot SHA-256: hash data and write the 32-byte digest. */
static void hc_sha256(const void* data, size_t len, hc_u8 digest[32]) {
    HcSha256Context ctx;
    hc_sha256_init(&ctx);
    hc_sha256_update(&ctx, data, len);
    hc_sha256_finish(&ctx, digest);
}

/* Constant-time digest comparison.  Returns 1 if equal, 0 otherwise. */
static int hc_sha256_constant_time_equal(const hc_u8 a[32], const hc_u8 b[32]) {
    hc_u8 diff = 0U;
    int i;
    for (i = 0; i < 32; i++) {
        diff |= a[i] ^ b[i];
    }
    return (diff == 0U) ? 1 : 0;
}

/* Hash data and compare to a 32-byte expected checksum.  Returns 1 on match. */
static int hc_sha256_verify(const void* data, size_t len, const hc_u8 expected[32]) {
    hc_u8 digest[32];
    hc_sha256(data, len, digest);
    return hc_sha256_constant_time_equal(digest, expected);
}

#ifdef __cplusplus
}
#endif

#endif /* HASHCHECK_SHA256_H */
