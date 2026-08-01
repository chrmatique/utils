/*
 * HashCheck -- a pure-Swift, dependency-free SHA-256 implementation.
 */

public typealias SHA256Digest = [UInt8]

public enum SHA256 {

    /// Hash a raw byte buffer and return a 32-byte digest.
    public static func hash(_ data: [UInt8]) -> SHA256Digest {
        var context = Context()
        context.update(data)
        return context.finish()
    }

    /// Hash a UTF-8 string and return a 32-byte digest.
    public static func hash(_ string: String) -> SHA256Digest {
        hash(Array(string.utf8))
    }

    /// Verify that `data` hashes to the 32-byte `expected` checksum.
    public static func verify(_ data: [UInt8], expected: SHA256Digest) -> Bool {
        constantTimeEqual(hash(data), expected)
    }

    /// Verify that `string` hashes to the 32-byte `expected` checksum.
    public static func verify(_ string: String, expected: SHA256Digest) -> Bool {
        verify(Array(string.utf8), expected: expected)
    }

    /// Constant-time equality check for two byte arrays.
    public static func constantTimeEqual(_ a: [UInt8], _ b: [UInt8]) -> Bool {
        guard a.count == b.count else { return false }
        var diff: UInt8 = 0
        for i in 0..<a.count {
            diff |= a[i] ^ b[i]
        }
        return diff == 0
    }

    // ------------------------------------------------------------------------
    // Internal implementation
    // ------------------------------------------------------------------------

    private struct Context {
        private var state: [UInt32]
        private var buffer: [UInt8]
        private var bitCount: UInt64

        init() {
            state = [
                0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
            ]
            buffer = []
            bitCount = 0
        }

        mutating func update(_ data: [UInt8]) {
            bitCount += UInt64(data.count) * 8
            for byte in data {
                buffer.append(byte)
                if buffer.count == 64 {
                    transform(buffer)
                    buffer.removeAll(keepingCapacity: true)
                }
            }
        }

        mutating func finish() -> SHA256Digest {
            buffer.append(0x80)
            if buffer.count > 56 {
                buffer.append(contentsOf: [UInt8](repeating: 0, count: 64 - buffer.count))
                transform(buffer)
                buffer.removeAll(keepingCapacity: true)
            }
            buffer.append(contentsOf: [UInt8](repeating: 0, count: 56 - buffer.count))

            for i in (0..<8).reversed() {
                buffer.append(UInt8((bitCount >> UInt64(i * 8)) & 0xFF))
            }
            transform(buffer)

            var digest = [UInt8](repeating: 0, count: 32)
            for i in 0..<8 {
                for j in 0..<4 {
                    digest[i * 4 + j] = UInt8((state[i] >> UInt32(24 - j * 8)) & 0xFF)
                }
            }
            return digest
        }

        private mutating func transform(_ block: [UInt8]) {
            var m = [UInt32](repeating: 0, count: 64)
            for i in 0..<16 {
                m[i] = (UInt32(block[i * 4]) << 24) |
                       (UInt32(block[i * 4 + 1]) << 16) |
                       (UInt32(block[i * 4 + 2]) << 8) |
                        UInt32(block[i * 4 + 3])
            }
            for i in 16..<64 {
                m[i] = sig1(m[i - 2]) &+ m[i - 7] &+ sig0(m[i - 15]) &+ m[i - 16]
            }

            let k: [UInt32] = [
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
            ]

            var a = state[0], b = state[1], c = state[2], d = state[3],
                e = state[4], f = state[5], g = state[6], h = state[7]

            for i in 0..<64 {
                let t1 = h &+ ep1(e) &+ ch(e, f, g) &+ k[i] &+ m[i]
                let t2 = ep0(a) &+ maj(a, b, c)
                h = g
                g = f
                f = e
                e = d &+ t1
                d = c
                c = b
                b = a
                a = t1 &+ t2
            }

            state[0] = state[0] &+ a
            state[1] = state[1] &+ b
            state[2] = state[2] &+ c
            state[3] = state[3] &+ d
            state[4] = state[4] &+ e
            state[5] = state[5] &+ f
            state[6] = state[6] &+ g
            state[7] = state[7] &+ h
        }
    }

    private static func rotr(_ x: UInt32, _ n: UInt32) -> UInt32 {
        (x >> n) | (x << (32 - n))
    }

    private static func ch(_ x: UInt32, _ y: UInt32, _ z: UInt32) -> UInt32 {
        (x & y) ^ (~x & z)
    }

    private static func maj(_ x: UInt32, _ y: UInt32, _ z: UInt32) -> UInt32 {
        (x & y) ^ (x & z) ^ (y & z)
    }

    private static func ep0(_ x: UInt32) -> UInt32 {
        rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22)
    }

    private static func ep1(_ x: UInt32) -> UInt32 {
        rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25)
    }

    private static func sig0(_ x: UInt32) -> UInt32 {
        rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3)
    }

    private static func sig1(_ x: UInt32) -> UInt32 {
        rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10)
    }
}
