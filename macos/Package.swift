// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "JSONDictionaryEditor",
    defaultLocalization: "en",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(name: "JSONDictionaryEditor", targets: ["JSONDictionaryEditor"])
    ],
    targets: [
        .executableTarget(
            name: "JSONDictionaryEditor",
            path: "Sources/JSONDictionaryEditor",
            resources: [.process("Resources")]
        )
    ]
)
