// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "JSONDictionaryEditor",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(name: "JSONDictionaryEditor", targets: ["JSONDictionaryEditor"])
    ],
    targets: [
        .executableTarget(
            name: "JSONDictionaryEditor",
            path: "Sources/JSONDictionaryEditor"
        )
    ]
)
