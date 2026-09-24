import SwiftUI

struct NodeInspectorView: View {
    @Binding var document: JSONDictionaryDocument
    let nodeID: UUID
    let onAdd: (JSONKind) -> Void
    let onEditRaw: () -> Void
    let onDelete: () -> Void

    @State private var pendingKind: JSONKind?
    @State private var showsKindConfirmation = false

    private var node: JSONNode? { document.node(withID: nodeID) }
    private var location: JSONNodeLocation? { document.location(of: nodeID) }
    private var isRoot: Bool { nodeID == document.root.id }

    var body: some View {
        ScrollView {
            if let node {
                VStack(alignment: .leading, spacing: 18) {
                    inspectorHeader(node)

                    Divider()

                    if isRoot {
                        LabeledContent("节点", value: "根对象（字典）")
                    } else if location?.isObjectMember == true {
                        ValidatedKeyField(document: $document, nodeID: nodeID)
                            .id("key-\(nodeID)")
                    } else if let index = location?.index {
                        LabeledContent("数组索引", value: "[\(index)]")
                    }

                    typePicker(for: node)

                    Divider()

                    valueEditor(for: node)

                    Divider()

                    metadataSection(node)
                }
                .padding(20)
                .frame(maxWidth: .infinity, alignment: .leading)
            } else {
                Text("未选择节点")
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .padding(40)
            }
        }
        .background(Color(nsColor: .windowBackgroundColor))
        .alert("更改类型会移除子项", isPresented: $showsKindConfirmation) {
            Button("取消", role: .cancel) { pendingKind = nil }
            Button("更改", role: .destructive) {
                if let kind = pendingKind { _ = document.changeKind(of: nodeID, to: kind) }
                pendingKind = nil
            }
        } message: {
            Text("所选对象或数组当前包含子项。更改为其他类型后，这些子项将被移除。")
        }
    }

    private func inspectorHeader(_ node: JSONNode) -> some View {
        HStack(spacing: 12) {
            ZStack {
                RoundedRectangle(cornerRadius: 9, style: .continuous)
                    .fill(TypeAppearance.color(for: node.kind).opacity(0.13))
                Image(systemName: node.kind.symbolName)
                    .font(.system(size: 20, weight: .medium))
                    .foregroundStyle(TypeAppearance.color(for: node.kind))
            }
            .frame(width: 42, height: 42)

            VStack(alignment: .leading, spacing: 2) {
                Text(isRoot ? "根对象" : (location?.key ?? location.map { "[\($0.index ?? 0)]" } ?? "节点"))
                    .font(.title3.weight(.semibold))
                    .lineLimit(1)
                Text(node.kind.title)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            Menu {
                Button("编辑原始 JSON…", action: onEditRaw)
                if case .object = node.value {
                    Button("按键名排序") { _ = document.sortObject(at: nodeID) }
                }
                if !isRoot {
                    Divider()
                    Button("删除", role: .destructive, action: onDelete)
                }
            } label: {
                Image(systemName: "ellipsis.circle")
                    .font(.title3)
            }
            .menuStyle(.borderlessButton)
            .fixedSize()
        }
    }

    private func typePicker(for node: JSONNode) -> some View {
        VStack(alignment: .leading, spacing: 7) {
            Text("值类型")
                .font(.caption)
                .foregroundStyle(.secondary)
            Picker("值类型", selection: Binding(
                get: { node.kind },
                set: { requested in requestKindChange(from: node, to: requested) }
            )) {
                ForEach(JSONKind.allCases) { kind in
                    Label(kind.title, systemImage: kind.symbolName).tag(kind)
                }
            }
            .labelsHidden()
            .disabled(isRoot)

            if isRoot {
                Text("JSON 字典的根节点固定为对象。")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    @ViewBuilder
    private func valueEditor(for node: JSONNode) -> some View {
        switch node.value {
        case .string:
            StringValueEditor(document: $document, nodeID: nodeID)
        case .number(let value):
            ValidatedNumberField(document: $document, nodeID: nodeID, initialValue: value)
                .id("number-\(nodeID)")
        case .boolean(let value):
            VStack(alignment: .leading, spacing: 8) {
                Text("布尔值")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Toggle(value ? "True（真）" : "False（假）", isOn: Binding(
                    get: { value },
                    set: { _ = document.setBoolean($0, for: nodeID) }
                ))
                .toggleStyle(.switch)
            }
        case .null:
            VStack(alignment: .leading, spacing: 10) {
                Label("此值为空（null）", systemImage: "nosign")
                    .foregroundStyle(.secondary)
                Text("Null 与空字符串、数字 0 和 false 不相同。")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        case .object(let members):
            containerEditor(
                title: members.isEmpty ? "空对象" : "包含 \(members.count) 个键",
                explanation: "对象中的每个子项都有唯一键名。",
                isObject: true
            )
        case .array(let values):
            containerEditor(
                title: values.isEmpty ? "空数组" : "包含 \(values.count) 个元素",
                explanation: "数组元素按顺序保存，索引会随移动自动更新。",
                isObject: false
            )
        }
    }

    private func containerEditor(title: String, explanation: String, isObject: Bool) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Label(title, systemImage: isObject ? "curlybraces" : "square.stack.3d.up")
                .font(.headline)
            Text(explanation)
                .font(.caption)
                .foregroundStyle(.secondary)

            Menu {
                ForEach(JSONKind.allCases) { kind in
                    Button {
                        onAdd(kind)
                    } label: {
                        Label(kind.title, systemImage: kind.symbolName)
                    }
                }
            } label: {
                Label("添加子项", systemImage: "plus")
            }
            .buttonStyle(.borderedProminent)

            Button("编辑此节点的原始 JSON…", action: onEditRaw)
                .buttonStyle(.link)
        }
    }

    private func metadataSection(_ node: JSONNode) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("节点信息")
                .font(.headline)

            if let path = location?.path {
                VStack(alignment: .leading, spacing: 4) {
                    Text("JSON 路径")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Text(path)
                        .font(.system(.caption, design: .monospaced))
                        .textSelection(.enabled)
                }
            }

            LabeledContent("类型", value: node.kind.title)
            if node.isContainer {
                LabeledContent("直接子项", value: "\(node.childCount)")
            }
        }
    }

    private func requestKindChange(from node: JSONNode, to requested: JSONKind) {
        guard requested != node.kind else { return }
        if node.isContainer && node.childCount > 0 {
            pendingKind = requested
            showsKindConfirmation = true
        } else {
            _ = document.changeKind(of: nodeID, to: requested)
        }
    }
}

private struct ValidatedKeyField: View {
    @Binding var document: JSONDictionaryDocument
    let nodeID: UUID
    @State private var draft: String
    @FocusState private var focused: Bool

    init(document: Binding<JSONDictionaryDocument>, nodeID: UUID) {
        self._document = document
        self.nodeID = nodeID
        self._draft = State(initialValue: document.wrappedValue.key(for: nodeID) ?? "")
    }

    private var isAvailable: Bool {
        document.isKeyAvailable(draft, for: nodeID)
    }

    private var hasChanges: Bool {
        draft != document.key(for: nodeID)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 7) {
            Text("键名")
                .font(.caption)
                .foregroundStyle(.secondary)
            HStack {
                TextField("键名", text: $draft)
                    .focused($focused)
                    .onSubmit(apply)
                Button("应用", action: apply)
                    .disabled(!hasChanges || !isAvailable)
            }
            if !isAvailable {
                Label("同一对象中已经存在此键名。", systemImage: "exclamationmark.triangle.fill")
                    .font(.caption)
                    .foregroundStyle(.red)
            } else if draft.isEmpty {
                Text("空字符串可以作为 JSON 键，但通常不便于维护。")
                    .font(.caption)
                    .foregroundStyle(.orange)
            }
        }
    }

    private func apply() {
        guard hasChanges, isAvailable else { return }
        _ = document.renameNode(nodeID, to: draft)
    }
}

private struct StringValueEditor: View {
    @Binding var document: JSONDictionaryDocument
    let nodeID: UUID

    var body: some View {
        VStack(alignment: .leading, spacing: 7) {
            Text("字符串值")
                .font(.caption)
                .foregroundStyle(.secondary)
            TextEditor(text: Binding(
                get: {
                    guard let node = document.node(withID: nodeID), case .string(let value) = node.value else { return "" }
                    return value
                },
                set: { _ = document.setString($0, for: nodeID) }
            ))
            .font(.body)
            .frame(minHeight: 150)
            .padding(5)
            .background(Color(nsColor: .textBackgroundColor))
            .clipShape(RoundedRectangle(cornerRadius: 6, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: 6, style: .continuous)
                    .stroke(Color(nsColor: .separatorColor), lineWidth: 1)
            }
            Text("换行、引号和反斜杠会在保存时自动转义。")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }
}

private struct ValidatedNumberField: View {
    @Binding var document: JSONDictionaryDocument
    let nodeID: UUID
    @State private var draft: String

    init(document: Binding<JSONDictionaryDocument>, nodeID: UUID, initialValue: String) {
        self._document = document
        self.nodeID = nodeID
        self._draft = State(initialValue: initialValue)
    }

    private var isValid: Bool { JSONNumberValidator.isValid(draft) }

    var body: some View {
        VStack(alignment: .leading, spacing: 7) {
            Text("数字值")
                .font(.caption)
                .foregroundStyle(.secondary)
            HStack {
                TextField("例如 42、-1.5 或 6.02e23", text: $draft)
                    .font(.system(.body, design: .monospaced))
                    .onSubmit(apply)
                Button("应用", action: apply)
                    .disabled(!isValid || currentValue == draft)
            }
            if !isValid {
                Label("请输入有效的 JSON 数字；不支持 NaN、Infinity 或前导零。", systemImage: "exclamationmark.triangle.fill")
                    .font(.caption)
                    .foregroundStyle(.red)
            } else {
                Text("数字会原样保存，不会因浮点转换而丢失精度。")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var currentValue: String {
        guard let node = document.node(withID: nodeID), case .number(let value) = node.value else { return "" }
        return value
    }

    private func apply() {
        guard isValid else { return }
        _ = document.setNumber(draft, for: nodeID)
    }
}
