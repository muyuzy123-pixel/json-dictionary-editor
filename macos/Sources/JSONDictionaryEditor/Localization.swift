import Foundation
import SwiftUI

enum LanguagePreference: String, CaseIterable, Identifiable {
    case system
    case chinese = "zh-Hans"
    case english = "en"

    var id: String { rawValue }
    var menuTitle: String {
        switch self {
        case .system: return "跟随系统 / System"
        case .chinese: return "简体中文"
        case .english: return "English"
        }
    }
}

final class LanguageStore: ObservableObject {
    static let shared = LanguageStore()
    static let preferenceKey = "uiLanguagePreference"

    @Published var preference: LanguagePreference {
        didSet { UserDefaults.standard.set(preference.rawValue, forKey: Self.preferenceKey) }
    }

    private var catalogues: [String: [String: String]] = [:]

    private init() {
        preference = LanguagePreference(rawValue:
            UserDefaults.standard.string(forKey: Self.preferenceKey) ?? "system") ?? .system
        let installedBundleURL = Bundle.main.resourceURL?
            .appendingPathComponent("JSONDictionaryEditor_JSONDictionaryEditor.bundle")
        // A packaged app must use its own sealed resources. Do not let a local
        // SwiftPM build directory conceal a missing language in the shipped app.
        let resourceBundle: Bundle? = Bundle.main.bundleURL.pathExtension.lowercased() == "app"
            ? installedBundleURL.flatMap { Bundle(url: $0) } : Bundle.module
        for identifier in ["zh-Hans", "en"] {
            let resourceDirectory = identifier == "zh-Hans" ? "zh-hans.lproj" : "en.lproj"
            if let path = resourceBundle?.path(forResource: "Localizable", ofType: "strings",
                                               inDirectory: resourceDirectory),
               let entries = NSDictionary(contentsOfFile: path) as? [String: String] {
                catalogues[identifier] = entries
            }
        }
    }

    var effectiveIdentifier: String {
        Self.resolve(preference, systemLanguage: Locale.preferredLanguages.first ?? "en")
    }

    static func resolve(_ preference: LanguagePreference, systemLanguage: String) -> String {
        switch preference {
        case .chinese: return "zh-Hans"
        case .english: return "en"
        case .system:
            return systemLanguage.lowercased().hasPrefix("zh") ? "zh-Hans" : "en"
        }
    }

    var locale: Locale { Locale(identifier: effectiveIdentifier) }

    func text(_ key: String) -> String {
        text(key, language: effectiveIdentifier)
    }

    func text(_ key: String, language: String) -> String {
        catalogues[language]?[key] ?? key
    }

    func hasTranslation(_ key: String, language: String) -> Bool {
        catalogues[language]?[key] != nil
    }

    func count(_ number: Int, one: String, other: String, chinese: String) -> String {
        if effectiveIdentifier == "zh-Hans" { return "\(number) \(text(chinese))" }
        return "\(number) \(text(number == 1 ? one : other))"
    }
}

func tr(_ key: String) -> String { LanguageStore.shared.text(key) }
