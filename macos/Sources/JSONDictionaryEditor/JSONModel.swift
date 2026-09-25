import Foundation

enum JSONKind: String, CaseIterable, Identifiable {
    case string
    case number
    case boolean
    case null
    case object
    case array

    var id: String { rawValue }

    var localizationKey: String {
        switch self {
        case .string: return "字符串"
        case .number: return "数字"
        case .boolean: return "布尔值"
        case .null: return "Null"
        case .object: return "对象"
        case .array: return "数组"
        }
    }

    var title: String {
        switch self {
        case .string: return tr("字符串")
        case .number: return tr("数字")
        case .boolean: return tr("布尔值")
        case .null: return "Null"
        case .object: return tr("对象")
        case .array: return tr("数组")
        }
    }

    var shortTitle: String {
        switch self {
        case .string: return tr("文本")
        case .number: return tr("数字")
        case .boolean: return tr("布尔")
        case .null: return "Null"
        case .object: return tr("对象")
        case .array: return tr("数组")
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
            return members.isEmpty ? tr("空对象") :
                LanguageStore.shared.count(members.count, one: "个键", other: "个键复数", chinese: "个键")
        case .array(let values):
            return values.isEmpty ? tr("空数组") :
                LanguageStore.shared.count(values.count, one: "个元素", other: "个元素复数", chinese: "个元素")
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
        case .twoSpaces: return tr("2 个空格")
        case .fourSpaces: return tr("4 个空格")
        case .tabs: return tr("制表符")
        case .compact: return tr("紧凑")
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

enum JSONParseIssue: Equatable {
    case message(String)
    case duplicateKey(String)
    case invalidNumber(String)
    case expectedLiteral(String)
    case invalidLiteralSuffix(String)

    var description: String {
        let english = LanguageStore.shared.effectiveIdentifier == "en"
        switch self {
        case .message(let key): return tr(key)
        case .duplicateKey(let key):
            return english ? "Duplicate object key: \(key)" : "对象中存在重复键“\(key)”"
        case .invalidNumber(let value):
            return english ? "Invalid JSON number: \(value)" : "“\(value)”不是有效的 JSON 数字"
        case .expectedLiteral(let value):
            return english ? "Invalid literal; expected \(value)" : "无效的字面量，预期为 \(value)"
        case .invalidLiteralSuffix(let value):
            return english ? "Invalid character after literal \(value)" : "字面量 \(value) 后包含无效字符"
        }
    }
}

enum JSONModelError: LocalizedError, Equatable {
    case parse(issue: JSONParseIssue, line: Int, column: Int)
    case rootMustBeObject
    case invalidNumber(String)
    case duplicateKey(String, path: String)
    case invalidUTF8
    case fileHasNoData

    var errorDescription: String? {
        switch self {
        case .parse(let issue, let line, let column):
            return LanguageStore.shared.effectiveIdentifier == "zh-Hans"
                ? "第 \(line) 行、第 \(column) 列：\(issue.description)"
                : "Line \(line), column \(column): \(issue.description)"
        case .rootMustBeObject:
            return tr("JSON 根节点必须是对象（字典），不能是数组或标量。")
        case .invalidNumber(let value):
            return LanguageStore.shared.effectiveIdentifier == "zh-Hans"
                ? "“\(value)”不是有效的 JSON 数字。"
                : "“\(value)” is not a valid JSON number."
        case .duplicateKey(let key, let path):
            return LanguageStore.shared.effectiveIdentifier == "zh-Hans"
                ? "\(path) 中存在重复键“\(key)”。"
                : "Duplicate key “\(key)” at \(path)."
        case .invalidUTF8:
            return tr("文件不是有效的 UTF-8 文本。")
        case .fileHasNoData:
            return tr("文件没有可读取的数据。")
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
