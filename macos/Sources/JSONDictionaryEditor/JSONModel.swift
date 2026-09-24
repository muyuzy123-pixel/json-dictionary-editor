import Foundation

enum JSONKind: String, CaseIterable, Identifiable {
    case string
    case number
    case boolean
    case null
    case object
    case array

    var id: String { rawValue }

    var title: String {
        switch self {
        case .string: return "字符串"
        case .number: return "数字"
        case .boolean: return "布尔值"
        case .null: return "Null"
        case .object: return "对象"
        case .array: return "数组"
        }
    }

    var shortTitle: String {
        switch self {
        case .string: return "文本"
        case .number: return "数字"
        case .boolean: return "布尔"
        case .null: return "Null"
        case .object: return "对象"
        case .array: return "数组"
        }
    }

    var symbolName: String {
        switch self {
        case .string: return "text.quote"
        case .number: return "number"
        case .boolean: return "switch.2"
        case .null: return "nosign"
        case .object: return "curlybraces"
        case .array: return "square.stack.3d.up"
        }
    }
}

struct JSONMember: Identifiable, Equatable {
    var key: String
    var value: JSONNode

    var id: UUID { value.id }
}

struct JSONNode: Identifiable, Equatable {
    var id: UUID
    var value: JSONValue

    init(id: UUID = UUID(), value: JSONValue) {
        self.id = id
        self.value = value
    }

    var kind: JSONKind {
        switch value {
        case .string: return .string
        case .number: return .number
        case .boolean: return .boolean
        case .null: return .null
        case .object: return .object
        case .array: return .array
        }
    }

    var isContainer: Bool {
        kind == .object || kind == .array
    }

    var childCount: Int {
        switch value {
        case .object(let members): return members.count
        case .array(let values): return values.count
        default: return 0
        }
    }

    var summary: String {
        switch value {
        case .string(let text):
            let singleLine = text
                .replacingOccurrences(of: "\n", with: " ↩ ")
                .replacingOccurrences(of: "\r", with: "")
            return singleLine.truncated(to: 72)
        case .number(let number):
            return number
        case .boolean(let value):
            return value ? "true" : "false"
        case .null:
            return "null"
        case .object(let members):
            return members.isEmpty ? "空对象" : "\(members.count) 个键"
        case .array(let values):
            return values.isEmpty ? "空数组" : "\(values.count) 个元素"
        }
    }

    func deepCopy() -> JSONNode {
        switch value {
        case .string(let text):
            return JSONNode(value: .string(text))
        case .number(let number):
            return JSONNode(value: .number(number))
        case .boolean(let boolean):
            return JSONNode(value: .boolean(boolean))
        case .null:
            return JSONNode(value: .null)
        case .object(let members):
            return JSONNode(value: .object(members.map {
                JSONMember(key: $0.key, value: $0.value.deepCopy())
            }))
        case .array(let values):
            return JSONNode(value: .array(values.map { $0.deepCopy() }))
        }
    }
}

indirect enum JSONValue: Equatable {
    case string(String)
    case number(String)
    case boolean(Bool)
    case null
    case object([JSONMember])
    case array([JSONNode])
}

enum JSONFormatting: String, CaseIterable, Identifiable {
    case twoSpaces
    case fourSpaces
    case tabs
    case compact

    var id: String { rawValue }

    var title: String {
        switch self {
        case .twoSpaces: return "2 个空格"
        case .fourSpaces: return "4 个空格"
        case .tabs: return "制表符"
        case .compact: return "紧凑"
        }
    }

    var indentUnit: String? {
        switch self {
        case .twoSpaces: return "  "
        case .fourSpaces: return "    "
        case .tabs: return "\t"
        case .compact: return nil
        }
    }

    static func detect(in text: String) -> JSONFormatting {
        guard text.contains("\n") else { return .compact }

        for line in text.split(separator: "\n", omittingEmptySubsequences: false).dropFirst() {
            let prefix = line.prefix { $0 == " " || $0 == "\t" }
            guard !prefix.isEmpty else { continue }
            if prefix.first == "\t" { return .tabs }
            return prefix.count >= 4 ? .fourSpaces : .twoSpaces
        }
        return .twoSpaces
    }
}

struct JSONNodeLocation: Equatable {
    let nodeID: UUID
    let parentID: UUID?
    let key: String?
    let index: Int?
    let path: String

    var isObjectMember: Bool { key != nil }
    var isArrayElement: Bool { index != nil }
}

struct JSONFlatRow: Identifiable, Equatable {
    let node: JSONNode
    let name: String
    let path: String
    let depth: Int
    let parentID: UUID?
    let key: String?
    let index: Int?

    var id: UUID { node.id }
}

enum JSONModelError: LocalizedError, Equatable {
    case parse(message: String, line: Int, column: Int)
    case rootMustBeObject
    case invalidNumber(String)
    case duplicateKey(String, path: String)
    case invalidUTF8
    case fileHasNoData

    var errorDescription: String? {
        switch self {
        case .parse(let message, let line, let column):
            return "第 \(line) 行、第 \(column) 列：\(message)"
        case .rootMustBeObject:
            return "JSON 根节点必须是对象（字典），不能是数组或标量。"
        case .invalidNumber(let value):
            return "“\(value)”不是有效的 JSON 数字。"
        case .duplicateKey(let key, let path):
            return "\(path) 中存在重复键“\(key)”。"
        case .invalidUTF8:
            return "文件不是有效的 UTF-8 文本。"
        case .fileHasNoData:
            return "文件没有可读取的数据。"
        }
    }
}

extension String {
    func truncated(to limit: Int) -> String {
        guard count > limit else { return self }
        let end = index(startIndex, offsetBy: max(0, limit - 1))
        return String(self[..<end]) + "…"
    }
}
