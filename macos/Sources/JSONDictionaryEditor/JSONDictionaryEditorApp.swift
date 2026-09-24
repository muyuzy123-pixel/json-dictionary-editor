import SwiftUI

@main
struct JSONDictionaryEditorApp: App {
    init() {
        CommandLineRouter.handleIfNeeded()
    }

    var body: some Scene {
        DocumentGroup(newDocument: JSONDictionaryDocument()) { file in
            DocumentEditorView(document: file.$document)
        }
        .defaultSize(width: 1080, height: 700)
    }
}
