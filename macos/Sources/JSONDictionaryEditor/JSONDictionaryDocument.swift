import Foundation
import SwiftUI
import UniformTypeIdentifiers

struct JSONDictionaryDocument: FileDocument {
    static var readableContentTypes: [UTType] { [.json] }
    static var writableContentTypes: [UTType] { [.json] }

    var root: JSONNode
    var formatting: JSONFormatting
    var trailingNewline: Bool

    init() {
        root = JSONNode(value: .object([]))
        formatting = .twoSpaces
        trailingNewline = true
    }

    init(configuration: ReadConfiguration) throws {
        guard let data = configuration.file.regularFileContents else {
            throw JSONModelError.fileHasNoData
        }
        guard let text = String(data: data, encoding: .utf8) else {
            throw JSONModelError.invalidUTF8
        }
        var parser = try OrderedJSONParser(data: data)
        let parsed = try parser.parse()
        guard parsed.kind == .object else {
            throw JSONModelError.rootMustBeObject
        }
        try OrderedJSONWriter(formatting: .compact).validate(parsed, requireRootObject: true)

        root = parsed
        formatting = JSONFormatting.detect(in: text)
        trailingNewline = text.hasSuffix("\n")
    }

    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        let writer = OrderedJSONWriter(formatting: formatting)
        try writer.validate(root, requireRootObject: true)
        let text = try writer.encode(root, trailingNewline: trailingNewline)
        guard let data = text.data(using: .utf8) else {
            throw JSONModelError.invalidUTF8
        }
        return FileWrapper(regularFileWithContents: data)
    }

    func node(withID id: UUID) -> JSONNode? {
        Self.findNode(root, id: id)
    }

    func location(of id: UUID) -> JSONNodeLocation? {
        if root.id == id {
            return JSONNodeLocation(nodeID: id, parentID: nil, key: nil, index: nil, path: "$")
        }
        return Self.findLocation(in: root, targetID: id, path: "$")
    }

    func allRows() -> [JSONFlatRow] {
        var rows = [JSONFlatRow(
            node: root,
            name: "根对象",
            path: "$",
            depth: 0,
            parentID: nil,
            key: nil,
            index: nil
        )]
        Self.appendRows(from: root, path: "$", depth: 1, into: &rows)
        return rows
    }

    func visibleRows(expanded: Set<UUID>, searchText: String) -> [JSONFlatRow] {
        let all = allRows()
        let query = searchText.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        if !query.isEmpty {
            return all.filter { row in
                row.name.lowercased().contains(query) ||
                row.path.lowercased().contains(query) ||
                row.node.kind.title.lowercased().contains(query) ||
                row.node.summary.lowercased().contains(query)
            }
        }

        var result: [JSONFlatRow] = []
        var hiddenDepth: Int?
        for row in all {
            if let depth = hiddenDepth {
                if row.depth > depth { continue }
                hiddenDepth = nil
            }
            result.append(row)
            if row.node.isContainer, !expanded.contains(row.id) {
                hiddenDepth = row.depth
            }
        }
        return result
    }

    var nodeCount: Int {
        allRows().count
    }

    var rootKeyCount: Int {
        root.childCount
    }

    func key(for nodeID: UUID) -> String? {
        location(of: nodeID)?.key
    }

    func isKeyAvailable(_ candidate: String, for nodeID: UUID) -> Bool {
        guard let location = location(of: nodeID), let parentID = location.parentID,
              let parent = node(withID: parentID), case .object(let members) = parent.value else {
            return false
        }
        return !members.contains { $0.value.id != nodeID && $0.key == candidate }
    }

    func canMove(_ nodeID: UUID, offset: Int) -> Bool {
        guard offset != 0, let location = location(of: nodeID), let index = location.index ?? objectIndex(for: nodeID) else {
            return false
        }
        guard let parentID = location.parentID, let parent = node(withID: parentID) else { return false }
        return (0..<parent.childCount).contains(index + offset)
    }

    @discardableResult
    mutating func renameNode(_ nodeID: UUID, to newKey: String) -> Bool {
        guard isKeyAvailable(newKey, for: nodeID) else { return false }
        return Self.rename(in: &root, nodeID: nodeID, newKey: newKey)
    }

    @discardableResult
    mutating func setString(_ value: String, for nodeID: UUID) -> Bool {
        mutateNode(nodeID) { node in node.value = .string(value) }
    }

    @discardableResult
    mutating func setNumber(_ value: String, for nodeID: UUID) -> Bool {
        guard JSONNumberValidator.isValid(value) else { return false }
        return mutateNode(nodeID) { node in node.value = .number(value) }
    }

    @discardableResult
    mutating func setBoolean(_ value: Bool, for nodeID: UUID) -> Bool {
        mutateNode(nodeID) { node in node.value = .boolean(value) }
    }

    @discardableResult
    mutating func changeKind(of nodeID: UUID, to kind: JSONKind) -> Bool {
        guard nodeID != root.id || kind == .object else { return false }
        return mutateNode(nodeID) { node in
            guard node.kind != kind else { return }
            switch kind {
            case .string:
                switch node.value {
                case .number(let value): node.value = .string(value)
                case .boolean(let value): node.value = .string(value ? "true" : "false")
                case .null: node.value = .string("")
                default: node.value = .string("")
                }
            case .number:
                switch node.value {
                case .string(let value) where JSONNumberValidator.isValid(value): node.value = .number(value)
                case .boolean(let value): node.value = .number(value ? "1" : "0")
                default: node.value = .number("0")
                }
            case .boolean:
                switch node.value {
                case .string(let value): node.value = .boolean(value.lowercased() == "true")
                case .number(let value): node.value = .boolean(value != "0")
                default: node.value = .boolean(false)
                }
            case .null:
                node.value = .null
            case .object:
                node.value = .object([])
            case .array:
                node.value = .array([])
            }
        }
    }

    @discardableResult
    mutating func replaceNode(_ nodeID: UUID, with replacement: JSONNode) -> Bool {
        guard nodeID != root.id || replacement.kind == .object else { return false }
        return mutateNode(nodeID) { node in
            var copy = replacement
            copy.id = node.id
            node = copy
        }
    }

    @discardableResult
    mutating func addChild(to preferredNodeID: UUID?) -> UUID? {
        let selectedID = preferredNodeID ?? root.id
        let containerID: UUID
        if let node = node(withID: selectedID), node.isContainer {
            containerID = selectedID
        } else if let parentID = location(of: selectedID)?.parentID {
            containerID = parentID
        } else {
            containerID = root.id
        }

        var createdID: UUID?
        _ = mutateNode(containerID) { node in
            switch node.value {
            case .object(var members):
                let child = JSONNode(value: .string(""))
                let key = Self.uniqueKey(base: "新键", in: members)
                members.append(JSONMember(key: key, value: child))
                node.value = .object(members)
                createdID = child.id
            case .array(var values):
                let child = JSONNode(value: .string(""))
                values.append(child)
                node.value = .array(values)
                createdID = child.id
            default:
                break
            }
        }
        return createdID
    }

    @discardableResult
    mutating func deleteNode(_ nodeID: UUID) -> Bool {
        guard nodeID != root.id else { return false }
        return Self.delete(from: &root, nodeID: nodeID)
    }

    @discardableResult
    mutating func duplicateNode(_ nodeID: UUID) -> UUID? {
        guard nodeID != root.id else { return nil }
        return Self.duplicate(in: &root, nodeID: nodeID)
    }

    @discardableResult
    mutating func moveNode(_ nodeID: UUID, offset: Int) -> Bool {
        guard nodeID != root.id, offset != 0 else { return false }
        return Self.move(in: &root, nodeID: nodeID, offset: offset)
    }

    @discardableResult
    mutating func sortObject(at nodeID: UUID) -> Bool {
        mutateNode(nodeID) { node in
            guard case .object(let members) = node.value else { return }
            node.value = .object(members.sorted {
                $0.key.localizedStandardCompare($1.key) == .orderedAscending
            })
        }
    }

    func encodedText(for nodeID: UUID? = nil, formatting requestedFormatting: JSONFormatting? = nil) -> String? {
        let target = nodeID.flatMap { node(withID: $0) } ?? root
        let writer = OrderedJSONWriter(formatting: requestedFormatting ?? formatting)
        return try? writer.encode(target, trailingNewline: false)
    }

    @discardableResult
    private mutating func mutateNode(_ nodeID: UUID, transform: (inout JSONNode) -> Void) -> Bool {
        Self.mutate(&root, targetID: nodeID, transform: transform)
    }

    private func objectIndex(for nodeID: UUID) -> Int? {
        guard let location = location(of: nodeID), let parentID = location.parentID,
              let parent = node(withID: parentID), case .object(let members) = parent.value else {
            return nil
        }
        return members.firstIndex { $0.value.id == nodeID }
    }

    private static func findNode(_ node: JSONNode, id: UUID) -> JSONNode? {
        if node.id == id { return node }
        switch node.value {
        case .object(let members):
            for member in members {
                if let found = findNode(member.value, id: id) { return found }
            }
        case .array(let values):
            for value in values {
                if let found = findNode(value, id: id) { return found }
            }
        default:
            break
        }
        return nil
    }

    private static func findLocation(in node: JSONNode, targetID: UUID, path: String) -> JSONNodeLocation? {
        switch node.value {
        case .object(let members):
            for member in members {
                let childPath = pathForKey(member.key, base: path)
                if member.value.id == targetID {
                    return JSONNodeLocation(
                        nodeID: targetID,
                        parentID: node.id,
                        key: member.key,
                        index: nil,
                        path: childPath
                    )
                }
                if let found = findLocation(in: member.value, targetID: targetID, path: childPath) {
                    return found
                }
            }
        case .array(let values):
            for (index, value) in values.enumerated() {
                let childPath = "\(path)[\(index)]"
                if value.id == targetID {
                    return JSONNodeLocation(
                        nodeID: targetID,
                        parentID: node.id,
                        key: nil,
                        index: index,
                        path: childPath
                    )
                }
                if let found = findLocation(in: value, targetID: targetID, path: childPath) {
                    return found
                }
            }
        default:
            break
        }
        return nil
    }

    private static func appendRows(from node: JSONNode, path: String, depth: Int, into rows: inout [JSONFlatRow]) {
        switch node.value {
        case .object(let members):
            for member in members {
                let childPath = pathForKey(member.key, base: path)
                rows.append(JSONFlatRow(
                    node: member.value,
                    name: member.key,
                    path: childPath,
                    depth: depth,
                    parentID: node.id,
                    key: member.key,
                    index: nil
                ))
                appendRows(from: member.value, path: childPath, depth: depth + 1, into: &rows)
            }
        case .array(let values):
            for (index, value) in values.enumerated() {
                let childPath = "\(path)[\(index)]"
                rows.append(JSONFlatRow(
                    node: value,
                    name: "[\(index)]",
                    path: childPath,
                    depth: depth,
                    parentID: node.id,
                    key: nil,
                    index: index
                ))
                appendRows(from: value, path: childPath, depth: depth + 1, into: &rows)
            }
        default:
            break
        }
    }

    private static func mutate(_ node: inout JSONNode, targetID: UUID, transform: (inout JSONNode) -> Void) -> Bool {
        if node.id == targetID {
            transform(&node)
            return true
        }
        switch node.value {
        case .object(var members):
            for index in members.indices {
                if mutate(&members[index].value, targetID: targetID, transform: transform) {
                    node.value = .object(members)
                    return true
                }
            }
        case .array(var values):
            for index in values.indices {
                if mutate(&values[index], targetID: targetID, transform: transform) {
                    node.value = .array(values)
                    return true
                }
            }
        default:
            break
        }
        return false
    }

    private static func rename(in node: inout JSONNode, nodeID: UUID, newKey: String) -> Bool {
        switch node.value {
        case .object(var members):
            if let index = members.firstIndex(where: { $0.value.id == nodeID }) {
                members[index].key = newKey
                node.value = .object(members)
                return true
            }
            for index in members.indices {
                if rename(in: &members[index].value, nodeID: nodeID, newKey: newKey) {
                    node.value = .object(members)
                    return true
                }
            }
        case .array(var values):
            for index in values.indices {
                if rename(in: &values[index], nodeID: nodeID, newKey: newKey) {
                    node.value = .array(values)
                    return true
                }
            }
        default:
            break
        }
        return false
    }

    private static func delete(from node: inout JSONNode, nodeID: UUID) -> Bool {
        switch node.value {
        case .object(var members):
            if let index = members.firstIndex(where: { $0.value.id == nodeID }) {
                members.remove(at: index)
                node.value = .object(members)
                return true
            }
            for index in members.indices {
                if delete(from: &members[index].value, nodeID: nodeID) {
                    node.value = .object(members)
                    return true
                }
            }
        case .array(var values):
            if let index = values.firstIndex(where: { $0.id == nodeID }) {
                values.remove(at: index)
                node.value = .array(values)
                return true
            }
            for index in values.indices {
                if delete(from: &values[index], nodeID: nodeID) {
                    node.value = .array(values)
                    return true
                }
            }
        default:
            break
        }
        return false
    }

    private static func duplicate(in node: inout JSONNode, nodeID: UUID) -> UUID? {
        switch node.value {
        case .object(var members):
            if let index = members.firstIndex(where: { $0.value.id == nodeID }) {
                let copy = members[index].value.deepCopy()
                let key = uniqueKey(base: members[index].key, in: members)
                members.insert(JSONMember(key: key, value: copy), at: index + 1)
                node.value = .object(members)
                return copy.id
            }
            for index in members.indices {
                if let copyID = duplicate(in: &members[index].value, nodeID: nodeID) {
                    node.value = .object(members)
                    return copyID
                }
            }
        case .array(var values):
            if let index = values.firstIndex(where: { $0.id == nodeID }) {
                let copy = values[index].deepCopy()
                values.insert(copy, at: index + 1)
                node.value = .array(values)
                return copy.id
            }
            for index in values.indices {
                if let copyID = duplicate(in: &values[index], nodeID: nodeID) {
                    node.value = .array(values)
                    return copyID
                }
            }
        default:
            break
        }
        return nil
    }

    private static func move(in node: inout JSONNode, nodeID: UUID, offset: Int) -> Bool {
        switch node.value {
        case .object(var members):
            if let index = members.firstIndex(where: { $0.value.id == nodeID }) {
                let target = index + offset
                guard members.indices.contains(target) else { return false }
                members.swapAt(index, target)
                node.value = .object(members)
                return true
            }
            for index in members.indices {
                if move(in: &members[index].value, nodeID: nodeID, offset: offset) {
                    node.value = .object(members)
                    return true
                }
            }
        case .array(var values):
            if let index = values.firstIndex(where: { $0.id == nodeID }) {
                let target = index + offset
                guard values.indices.contains(target) else { return false }
                values.swapAt(index, target)
                node.value = .array(values)
                return true
            }
            for index in values.indices {
                if move(in: &values[index], nodeID: nodeID, offset: offset) {
                    node.value = .array(values)
                    return true
                }
            }
        default:
            break
        }
        return false
    }

    private static func uniqueKey(base: String, in members: [JSONMember]) -> String {
        let used = Set(members.map(\.key))
        if !used.contains(base) { return base }
        var number = 2
        while used.contains("\(base) \(number)") { number += 1 }
        return "\(base) \(number)"
    }

    private static func pathForKey(_ key: String, base: String) -> String {
        let pattern = #"^[A-Za-z_$][A-Za-z0-9_$]*$"#
        if key.range(of: pattern, options: .regularExpression) != nil {
            return "\(base).\(key)"
        }
        let escaped = key
            .replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "\"", with: "\\\"")
        return "\(base)[\"\(escaped)\"]"
    }
}
