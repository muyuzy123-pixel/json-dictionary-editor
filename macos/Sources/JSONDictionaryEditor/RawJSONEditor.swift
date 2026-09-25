import SwiftUI

struct RawJSONEditorSheet: View {
    @Binding var document: JSONDictionaryDocument
    let nodeID: UUID

    @Environment(\.dismiss) private var dismiss
    @EnvironmentObject private var language: LanguageStore
    @State private var draft: String
    @State private var status = RawStatus.unchecked

    private enum RawStatus {
        case unchecked
        case edited
        case valid(JSONKind, Int)
        case formatted
        case failure(Error)
    }

    private var validationSucceeded: Bool {
        switch status {
        case .valid, .formatted: return true
        default: return false
        }
    }

    private var validationMessage: String {
        let _ = language.preference
        switch status {
        case .unchecked: return tr("尚未检查")
        case .edited: return tr("内容已更改，尚未检查")
        case .formatted: return tr("已格式化并通过检查")
        case .failure(let error): return error.localizedDescription
        case .valid(let kind, let count):
            if language.effectiveIdentifier == "zh-Hans" {
                return "有效的 \(kind.title) · \(count) 个节点"
            }
            return "Valid \(kind.title.lowercased()) · " +
                language.count(count, one: "个节点", other: "个节点复数", chinese: "个节点")
        }
    }

    private var isRoot: Bool { nodeID == document.root.id }

    init(document: Binding<JSONDictionaryDocument>, nodeID: UUID) {
        self._document = document
        self.nodeID = nodeID
        self._draft = State(initialValue:
            document.wrappedValue.encodedText(for: nodeID, formatting: .twoSpaces) ?? "{}"
        )
    }

    var body: some View {
        let _ = language.preference
        VStack(spacing: 0) {
            HStack(spacing: 12) {
                ZStack {
                    RoundedRectangle(cornerRadius: 8, style: .continuous)
                        .fill(Color.indigo.opacity(0.13))
                    Image(systemName: "chevron.left.forwardslash.chevron.right")
                        .foregroundStyle(.indigo)
                }
                .frame(width: 38, height: 38)

                VStack(alignment: .leading, spacing: 2) {
                    Text(tr(isRoot ? "编辑完整 JSON 字典" : "编辑所选节点的原始 JSON"))
                        .font(.headline)
                    Text(document.location(of: nodeID)?.path ?? "$")
                        .font(.caption.monospaced())
                        .foregroundStyle(.secondary)
                }
                Spacer()
            }
            .padding(16)

            Divider()

            TextEditor(text: $draft)
                .font(.system(.body, design: .monospaced))
                .disableAutocorrection(true)
                .padding(10)
                .frame(minWidth: 660, minHeight: 420)
                .onChange(of: draft) { _ in
                    status = .edited
                }

            Divider()

            HStack {
                Label(validationMessage, systemImage: validationSucceeded ? "checkmark.circle.fill" : "info.circle")
                    .font(.caption)
                    .foregroundStyle(validationSucceeded ? .green : .secondary)
                    .lineLimit(2)

                Spacer()

                Button(tr("格式化")) { formatDraft() }
                Button(tr("检查")) { validateDraft() }
                Button(tr("取消"), role: .cancel) { dismiss() }
                    .keyboardShortcut(.cancelAction)
                Button(tr("应用")) { applyDraft() }
                    .buttonStyle(.borderedProminent)
                    .keyboardShortcut(.defaultAction)
            }
            .padding(14)
        }
    }

    private func parseDraft() throws -> JSONNode {
        var parser = OrderedJSONParser(text: draft)
        let node = try parser.parse()
        if isRoot, node.kind != .object {
            throw JSONModelError.rootMustBeObject
        }
        try OrderedJSONWriter(formatting: .compact).validate(node, requireRootObject: isRoot)
        return node
    }

    private func validateDraft() {
        do {
            let node = try parseDraft()
            status = .valid(node.kind, countNodes(node))
        } catch {
            status = .failure(error)
        }
    }

    private func formatDraft() {
        do {
            let node = try parseDraft()
            draft = try OrderedJSONWriter(formatting: .twoSpaces).encode(node)
            status = .formatted
        } catch {
            status = .failure(error)
        }
    }

    private func applyDraft() {
        do {
            let node = try parseDraft()
            guard document.replaceNode(nodeID, with: node) else {
                throw JSONModelError.rootMustBeObject
            }
            dismiss()
        } catch {
            status = .failure(error)
        }
    }

    private func countNodes(_ node: JSONNode) -> Int {
        switch node.value {
        case .object(let members):
            return 1 + members.reduce(0) { $0 + countNodes($1.value) }
        case .array(let values):
            return 1 + values.reduce(0) { $0 + countNodes($1) }
        default:
            return 1
        }
    }
}
