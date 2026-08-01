// swift-tools-version:5.7
import PackageDescription

let package = Package(
    name: "HashCheck",
    products: [
        .library(name: "HashCheck", targets: ["HashCheck"]),
    ],
    targets: [
        .target(name: "HashCheck"),
        .testTarget(name: "HashCheckTests", dependencies: ["HashCheck"]),
    ]
)
