import SwiftUI

@main
struct JSONDictionaryEditorApp: App {
    @StateObject private var language = LanguageStore.shared

    init() {
        CommandLineRouter.handleIfNeeded()
    }

    var body: some Scene {
        DocumentGroup(newDocument: JSONDictionaryDocument()) { file in
            DocumentEditorView(document: file.$document)
                .environmentObject(language)
                .environment(\.locale, language.locale)
        }
        .defaultSize(width: 1080, height: 700)
        .commands {
            CommandMenu("语言 / Language") {
                ForEach(LanguagePreference.allCases) { option in
                    Button {
                        language.preference = option
                    } label: {
                        if language.preference == option {
                            Label(option.menuTitle, systemImage: "checkmark")
                        } else {
                            Text(option.menuTitle)
                        }
                    }
                }
            }
        }
    }
}
