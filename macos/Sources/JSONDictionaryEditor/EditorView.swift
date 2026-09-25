import SwiftUI

private struct RawEditorTarget: Identifiable {
    let id: UUID
}

struct DocumentEditorView: View {
    @Binding var document: JSONDictionaryDocument
    @EnvironmentObject private var language: LanguageStore

    @State private var selection: UUID?
    @State private var expanded = Set<UUID>()
    @State private var searchText = ""
    @State private var rawEditorTarget: RawEditorTarget?
    @State private var pendingDeleteID: UUID?
    @State private var showsDeleteConfirmation = false

    private var selectedID: UUID {
        selection ?? document.root.id
    }

    private var rows: [JSONFlatRow] {
        document.visibleRows(expanded: expanded, searchText: searchText)
    }

    var body: some View {
        let _ = language.preference
        VStack(spacing: 0) {
            HSplitView {
                treePane
                    .frame(minWidth: 440, idealWidth: 650)

                NodeInspectorView(
                    document: $document,
                    nodeID: selectedID,
                    onAdd: { kind in addNode(kind, relativeTo: selectedID) },
                    onEditRaw: { rawEditorTarget = RawEditorTarget(id: selectedID) },
                    onDelete: { requestDelete() }
                )
                .id(selectedID)
                .frame(minWidth: 320, idealWidth: 390, maxWidth: 500)
            }

            statusBar
        }
        .frame(minWidth: 820, minHeight: 540)
        .toolbar { toolbarContent }
        .sheet(item: $rawEditorTarget) { target in
            RawJSONEditorSheet(document: $document, nodeID: target.id)
        }
        .alert(tr("删除所选容器？"), isPresented: $showsDeleteConfirmation) {
            Button(tr("取消"), role: .cancel) { pendingDeleteID = nil }
            Button(tr("删除"), role: .destructive) {
                if let id = pendingDeleteID { deleteImmediately(id) }
                pendingDeleteID = nil
            }
        } message: {
            Text(tr("其中的所有子项也会被删除。此更改会标记在文档中，保存前请确认内容。"))
        }
        .onAppear {
            expanded.insert(document.root.id)
            if selection == nil { selection = document.root.id }
        }
        .onChange(of: document.root.id) { newRootID in
            expanded = [newRootID]
            selection = newRootID
        }
    }

    private var treePane: some View {
        VStack(spacing: 0) {
            HStack(spacing: 10) {
                Image(systemName: "magnifyingglass")
                    .foregroundStyle(.secondary)
                TextField(tr("搜索键名、路径、类型或值"), text: $searchText)
                    .textFieldStyle(.plain)
                if !searchText.isEmpty {
                    Button {
                        searchText = ""
                    } label: {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundStyle(.secondary)
                    }
                    .buttonStyle(.plain)
                    .help(tr("清除搜索"))
                }
            }
            .padding(.horizontal, 12)
            .frame(height: 38)
            .background(Color(nsColor: .controlBackgroundColor))

            Divider()

            HStack(spacing: 10) {
                Text(tr("键 / 索引"))
                    .frame(maxWidth: .infinity, alignment: .leading)
                Text(tr("类型"))
                    .frame(width: 66, alignment: .leading)
                Text(tr("值"))
                    .frame(width: 150, alignment: .leading)
            }
            .font(.caption)
            .foregroundStyle(.secondary)
            .padding(.horizontal, 14)
            .frame(height: 28)
            .background(Color(nsColor: .windowBackgroundColor))

            Divider()

            ZStack {
                List(selection: $selection) {
                    ForEach(rows) { row in
                        NodeTreeRow(
                            row: row,
                            isExpanded: expanded.contains(row.id),
                            isSearching: !searchText.isEmpty,
                            toggleExpanded: { toggleExpanded(row.id) }
                        )
                        .tag(row.id)
                        .contextMenu {
                            nodeContextMenu(for: row)
                        }
                    }
                }
                .listStyle(.inset)

                if rows.isEmpty {
                    VStack(spacing: 10) {
                        Image(systemName: "magnifyingglass")
                            .font(.system(size: 30))
                            .foregroundStyle(.tertiary)
                        Text(tr("没有匹配项"))
                            .font(.headline)
                        Text(tr("尝试缩短关键词或搜索 JSON 路径。"))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
            }
        }
        .background(Color(nsColor: .textBackgroundColor))
    }

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItemGroup(placement: .primaryAction) {
            Menu {
                addMenuItems()
            } label: {
                Label(tr("添加"), systemImage: "plus")
            }
            .help(tr("向所选容器添加子项；若选择的是值，则添加同级项"))

            Button {
                duplicateSelected()
            } label: {
                Label(tr("复制"), systemImage: "plus.square.on.square")
            }
            .disabled(selectedID == document.root.id)
            .help(tr("复制所选项"))

            Button {
                requestDelete()
            } label: {
                Label(tr("删除"), systemImage: "trash")
            }
            .disabled(selectedID == document.root.id)
            .help(tr("删除所选项"))

            Divider()

            Button {
                moveSelected(by: -1)
            } label: {
                Label(tr("上移"), systemImage: "arrow.up")
            }
            .disabled(!document.canMove(selectedID, offset: -1))
            .help(tr("上移所选项"))

            Button {
                moveSelected(by: 1)
            } label: {
                Label(tr("下移"), systemImage: "arrow.down")
            }
            .disabled(!document.canMove(selectedID, offset: 1))
            .help(tr("下移所选项"))

            Divider()

            Button {
                rawEditorTarget = RawEditorTarget(id: selectedID)
            } label: {
                Label(tr("原始 JSON"), systemImage: "chevron.left.forwardslash.chevron.right")
            }
            .help(tr("以原始 JSON 编辑所选节点"))
        }
    }

    private var statusBar: some View {
        HStack(spacing: 12) {
            Label(tr("有效 JSON 字典"), systemImage: "checkmark.circle.fill")
                .foregroundStyle(.green)

            Text(LanguageStore.shared.count(document.rootKeyCount,
                                            one: "个顶层键", other: "个顶层键复数", chinese: "个顶层键") +
                " · " + LanguageStore.shared.count(document.nodeCount,
                    one: "个节点", other: "个节点复数", chinese: "个节点"))
                .foregroundStyle(.secondary)

            Spacer()

            Menu {
                Picker(tr("缩进"), selection: $document.formatting) {
                    ForEach(JSONFormatting.allCases) { style in
                        Text(style.title).tag(style)
                    }
                }
                Divider()
                Toggle(tr("文件末尾保留换行"), isOn: $document.trailingNewline)
            } label: {
                HStack(spacing: 5) {
                    Image(systemName: "text.alignleft")
                    Text(document.formatting.title)
                }
            }
            .menuStyle(.borderlessButton)
            .fixedSize()
            .help(tr("保存格式"))
        }
        .font(.caption)
        .padding(.horizontal, 12)
        .frame(height: 30)
        .background(Color(nsColor: .windowBackgroundColor))
        .overlay(alignment: .top) { Divider() }
    }

    @ViewBuilder
    private func addMenuItems(relativeTo nodeID: UUID? = nil) -> some View {
        Button { addNode(.string, relativeTo: nodeID) } label: { Label(tr("字符串"), systemImage: JSONKind.string.symbolName) }
        Button { addNode(.number, relativeTo: nodeID) } label: { Label(tr("数字"), systemImage: JSONKind.number.symbolName) }
        Button { addNode(.boolean, relativeTo: nodeID) } label: { Label(tr("布尔值"), systemImage: JSONKind.boolean.symbolName) }
        Button { addNode(.null, relativeTo: nodeID) } label: { Label("Null", systemImage: JSONKind.null.symbolName) }
        Divider()
        Button { addNode(.object, relativeTo: nodeID) } label: { Label(tr("对象"), systemImage: JSONKind.object.symbolName) }
        Button { addNode(.array, relativeTo: nodeID) } label: { Label(tr("数组"), systemImage: JSONKind.array.symbolName) }
    }

    @ViewBuilder
    private func nodeContextMenu(for row: JSONFlatRow) -> some View {
        if row.node.isContainer {
            Menu(tr("添加子项")) { addMenuItems(relativeTo: row.id) }
            Divider()
        }
        Button(tr("编辑原始 JSON…")) {
            selection = row.id
            rawEditorTarget = RawEditorTarget(id: row.id)
        }
        if case .object = row.node.value {
            Button(tr("按键名排序")) {
                _ = document.sortObject(at: row.id)
            }
        }
        if row.id != document.root.id {
            Divider()
            Button(tr("复制")) {
                selection = row.id
                duplicateSelected()
            }
            Button(tr("删除"), role: .destructive) {
                selection = row.id
                requestDelete(row.id)
            }
        }
    }

    private func toggleExpanded(_ id: UUID) {
        if expanded.contains(id) {
            expanded.remove(id)
        } else {
            expanded.insert(id)
        }
    }

    private func addNode(_ kind: JSONKind, relativeTo nodeID: UUID? = nil) {
        let selected = nodeID ?? selectedID
        let targetContainer: UUID
        if document.node(withID: selected)?.isContainer == true {
            targetContainer = selected
        } else {
            targetContainer = document.location(of: selected)?.parentID ?? document.root.id
        }

        guard let newID = document.addChild(to: selected) else { return }
        if kind != .string { _ = document.changeKind(of: newID, to: kind) }
        expanded.insert(targetContainer)
        selection = newID
    }

    private func duplicateSelected() {
        guard selectedID != document.root.id,
              let copyID = document.duplicateNode(selectedID) else { return }
        if let parentID = document.location(of: copyID)?.parentID { expanded.insert(parentID) }
        selection = copyID
    }

    private func requestDelete(_ explicitID: UUID? = nil) {
        let id = explicitID ?? selectedID
        guard id != document.root.id, let node = document.node(withID: id) else { return }
        if node.isContainer && node.childCount > 0 {
            pendingDeleteID = id
            showsDeleteConfirmation = true
        } else {
            deleteImmediately(id)
        }
    }

    private func deleteImmediately(_ id: UUID) {
        let parent = document.location(of: id)?.parentID ?? document.root.id
        guard document.deleteNode(id) else { return }
        expanded.remove(id)
        selection = parent
    }

    private func moveSelected(by offset: Int) {
        _ = document.moveNode(selectedID, offset: offset)
    }
}

private struct NodeTreeRow: View {
    @EnvironmentObject private var language: LanguageStore
    let row: JSONFlatRow
    let isExpanded: Bool
    let isSearching: Bool
    let toggleExpanded: () -> Void

    var body: some View {
        let _ = language.preference
        HStack(spacing: 10) {
            HStack(spacing: 4) {
                Color.clear
                    .frame(width: CGFloat(max(0, row.depth)) * 14)

                if row.node.isContainer && !isSearching {
                    Button(action: toggleExpanded) {
                        Image(systemName: isExpanded ? "chevron.down" : "chevron.right")
                            .font(.caption.weight(.semibold))
                            .frame(width: 12, height: 18)
                    }
                    .buttonStyle(.plain)
                } else {
                    Color.clear.frame(width: 12, height: 18)
                }

                Image(systemName: row.node.kind.symbolName)
                    .foregroundStyle(TypeAppearance.color(for: row.node.kind))
                    .frame(width: 18)

                VStack(alignment: .leading, spacing: 1) {
                    Text(row.name)
                        .lineLimit(1)
                    if isSearching {
                        Text(row.path)
                            .font(.caption2.monospaced())
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                    }
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            Text(row.node.kind.shortTitle)
                .font(.caption.weight(.medium))
                .foregroundStyle(TypeAppearance.color(for: row.node.kind))
                .frame(width: 66, alignment: .leading)

            Text(row.node.summary)
                .font(.callout)
                .foregroundStyle(.secondary)
                .lineLimit(1)
                .frame(width: 150, alignment: .leading)
        }
        .padding(.vertical, 3)
        .contentShape(Rectangle())
    }
}

enum TypeAppearance {
    static func color(for kind: JSONKind) -> Color {
        switch kind {
        case .string: return .blue
        case .number: return .purple
        case .boolean: return .orange
        case .null: return .secondary
        case .object: return .indigo
        case .array: return .teal
        }
    }
}
