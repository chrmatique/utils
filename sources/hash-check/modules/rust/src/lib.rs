//! HashCheck -- a no-std, dependency-free SHA-256 implementation for Rust.
//!
//! # Example
//! ```
//! use hashcheck::{sha256, verify};
//!
//! let expected: [u8; 32] = [
//!     0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
//!     0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
//!     0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
//!     0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
//! ];
//!
//! assert!(verify(b"abc", &expected));
//! ```

#![no_std]

/// A raw 32-byte SHA-256 digest.
pub type Digest = [u8; 32];

/// Compute the SHA-256 digest of a byte slice.
pub fn sha256(data: &[u8]) -> Digest {
    let mut ctx = Context::new();
    ctx.update(data);
    ctx.finish()
}

/// Compare two digests in constant time.
pub fn constant_time_equal(a: &Digest, b: &Digest) -> bool {
    let mut diff: u8 = 0;
    for i in 0..32 {
        diff |= a[i] ^ b[i];
    }
    diff == 0
}

/// Verify that `data` hashes to `expected`.
pub fn verify(data: &[u8], expected: &Digest) -> bool {
    constant_time_equal(&sha256(data), expected)
}

// ---------------------------------------------------------------------------
// Internal implementation
// ---------------------------------------------------------------------------

struct Context {
    state: [u32; 8],
    bit_count: u64,
    buffer: [u8; 64],
    buffer_len: usize,
}

impl Context {
    fn new() -> Self {
        Self {
            state: [
                0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
            ],
            bit_count: 0,
            buffer: [0; 64],
            buffer_len: 0,
        }
    }

    fn update(&mut self, data: &[u8]) {
        self.bit_count += data.len() as u64 * 8;

        let mut offset = 0;
        while offset < data.len() {
            let avail = 64 - self.buffer_len;
            let to_copy = data.len() - offset;
            let copy = if to_copy < avail { to_copy } else { avail };

            self.buffer[self.buffer_len..self.buffer_len + copy]
                .copy_from_slice(&data[offset..offset + copy]);
            self.buffer_len += copy;
            offset += copy;

            if self.buffer_len == 64 {
                transform(&mut self.state, &self.buffer);
                self.buffer_len = 0;
            }
        }
    }

    fn finish(&mut self) -> Digest {
        self.buffer[self.buffer_len] = 0x80;
        self.buffer_len += 1;

        if self.buffer_len > 56 {
            self.buffer[self.buffer_len..64].fill(0);
            transform(&mut self.state, &self.buffer);
            self.buffer_len = 0;
        }

        self.buffer[self.buffer_len..56].fill(0);

        let bit_count_bytes = self.bit_count.to_be_bytes();
        self.buffer[56..64].copy_from_slice(&bit_count_bytes);

        transform(&mut self.state, &self.buffer);

        let mut digest = [0u8; 32];
        for i in 0..8 {
            digest[i * 4..i * 4 + 4].copy_from_slice(&self.state[i].to_be_bytes());
        }
        digest
    }
}

fn transform(state: &mut [u32; 8], block: &[u8; 64]) {
    let mut m = [0u32; 64];
    for i in 0..16 {
        m[i] = u32::from_be_bytes([
            block[i * 4],
            block[i * 4 + 1],
            block[i * 4 + 2],
            block[i * 4 + 3],
        ]);
    }
    for i in 16..64 {
        m[i] = sig1(m[i - 2])
            .wrapping_add(m[i - 7])
            .wrapping_add(sig0(m[i - 15]))
            .wrapping_add(m[i - 16]);
    }

    const K: [u32; 64] = [
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
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    ];

    let mut a = state[0];
    let mut b = state[1];
    let mut c = state[2];
    let mut d = state[3];
    let mut e = state[4];
    let mut f = state[5];
    let mut g = state[6];
    let mut h = state[7];

    for i in 0..64 {
        let t1 = h
            .wrapping_add(ep1(e))
            .wrapping_add(ch(e, f, g))
            .wrapping_add(K[i])
            .wrapping_add(m[i]);
        let t2 = ep0(a).wrapping_add(maj(a, b, c));
        h = g;
        g = f;
        f = e;
        e = d.wrapping_add(t1);
        d = c;
        c = b;
        b = a;
        a = t1.wrapping_add(t2);
    }

    state[0] = state[0].wrapping_add(a);
    state[1] = state[1].wrapping_add(b);
    state[2] = state[2].wrapping_add(c);
    state[3] = state[3].wrapping_add(d);
    state[4] = state[4].wrapping_add(e);
    state[5] = state[5].wrapping_add(f);
    state[6] = state[6].wrapping_add(g);
    state[7] = state[7].wrapping_add(h);
}

#[inline]
fn ch(x: u32, y: u32, z: u32) -> u32 {
    (x & y) ^ (!x & z)
}

#[inline]
fn maj(x: u32, y: u32, z: u32) -> u32 {
    (x & y) ^ (x & z) ^ (y & z)
}

#[inline]
fn ep0(x: u32) -> u32 {
    x.rotate_right(2) ^ x.rotate_right(13) ^ x.rotate_right(22)
}

#[inline]
fn ep1(x: u32) -> u32 {
    x.rotate_right(6) ^ x.rotate_right(11) ^ x.rotate_right(25)
}

#[inline]
fn sig0(x: u32) -> u32 {
    x.rotate_right(7) ^ x.rotate_right(18) ^ (x >> 3)
}

#[inline]
fn sig1(x: u32) -> u32 {
    x.rotate_right(17) ^ x.rotate_right(19) ^ (x >> 10)
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn nist_abc_vector() {
        const EXPECTED: Digest = [
            0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
            0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
            0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
            0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
        ];

        assert_eq!(sha256(b"abc"), EXPECTED);
        assert!(verify(b"abc", &EXPECTED));
        assert!(!verify(b"wrong", &EXPECTED));
    }

    #[test]
    fn constant_time_equal_test() {
        let a = [0xAAu8; 32];
        let b = [0xAAu8; 32];
        let c = [0xABu8; 32];

        assert!(constant_time_equal(&a, &b));
        assert!(!constant_time_equal(&a, &c));
    }
}
