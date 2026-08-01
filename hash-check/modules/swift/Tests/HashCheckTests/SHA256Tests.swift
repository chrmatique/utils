import XCTest
@testable import HashCheck

final class SHA256Tests: XCTestCase {

    func testNistVector() {
        let expected: [UInt8] = [
            0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
            0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
            0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
            0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
        ]

        XCTAssertEqual(SHA256.hash("abc"), expected)
        XCTAssertTrue(SHA256.verify("abc", expected: expected))
        XCTAssertFalse(SHA256.verify("wrong", expected: expected))
    }

    func testConstantTimeEqual() {
        let a = [UInt8](repeating: 0xAA, count: 32)
        let b = [UInt8](repeating: 0xAA, count: 32)
        let c = [UInt8](repeating: 0xAB, count: 32)
        let d = [UInt8](repeating: 0xAA, count: 31)

        XCTAssertTrue(SHA256.constantTimeEqual(a, b))
        XCTAssertFalse(SHA256.constantTimeEqual(a, c))
        XCTAssertFalse(SHA256.constantTimeEqual(a, d))
    }
}
