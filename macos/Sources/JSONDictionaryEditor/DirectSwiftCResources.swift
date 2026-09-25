import Foundation

// SwiftPM synthesizes Bundle.module. Direct swiftc builds do not, so locate the
// same checked-in resource bundle beside a standalone diagnostic executable.
// A packaged .app always uses its sealed bundle via LanguageStore instead.
#if DIRECT_SWIFTC_BUILD
extension Bundle {
    static var module: Bundle {
        let executable = URL(fileURLWithPath: CommandLine.arguments[0]).standardizedFileURL
        let adjacent = executable.deletingLastPathComponent()
            .appendingPathComponent("JSONDictionaryEditor_JSONDictionaryEditor.bundle", isDirectory: true)
        return Bundle(url: adjacent) ?? .main
    }
}
#endif
