import Foundation

struct OrderedJSONParser {
    private let bytes: [UInt8]
    private var index = 0

    init(data: Data) throws {
        guard String(data: data, encoding: .utf8) != nil else {
            throw JSONModelError.invalidUTF8
        }
        self.bytes = Array(data)
    }

    init(text: String) {
        self.bytes = Array(text.utf8)
    }

    mutating func parse() throws -> JSONNode {
        skipWhitespace()
        guard index < bytes.count else {
            throw error("JSON 内容为空")
        }
        let node = try parseValue()
        skipWhitespace()
        guard index == bytes.count else {
            throw error("值结束后还有多余内容")
        }
        return node
    }

    private mutating func parseValue() throws -> JSONNode {
        guard let byte = currentByte else { throw error("缺少 JSON 值") }
        switch byte {
        case 0x22:
            return JSONNode(value: .string(try parseString()))
        case 0x7B:
            return try parseObject()
        case 0x5B:
            return try parseArray()
        case 0x74:
            try consumeLiteral("true")
            return JSONNode(value: .boolean(true))
        case 0x66:
            try consumeLiteral("false")
            return JSONNode(value: .boolean(false))
        case 0x6E:
            try consumeLiteral("null")
            return JSONNode(value: .null)
        case 0x2D, 0x30...0x39:
            return JSONNode(value: .number(try parseNumber()))
        default:
            throw error("无法识别的 JSON 值")
        }
    }

    private mutating func parseObject() throws -> JSONNode {
        try expect(0x7B, message: "缺少“{”")
        skipWhitespace()
        var members: [JSONMember] = []
        var keys = Set<String>()

        if consumeIf(0x7D) {
            return JSONNode(value: .object(members))
        }

        while true {
            guard currentByte == 0x22 else {
                throw error("对象键必须是双引号字符串")
            }
            let key = try parseString()
            guard !keys.contains(key) else {
                throw error(.duplicateKey(key))
            }
            keys.insert(key)
            skipWhitespace()
            try expect(0x3A, message: "对象键后缺少冒号")
            skipWhitespace()
            let value = try parseValue()
            members.append(JSONMember(key: key, value: value))
            skipWhitespace()

            if consumeIf(0x7D) { break }
            try expect(0x2C, message: "对象成员之间缺少逗号")
            skipWhitespace()
        }

        return JSONNode(value: .object(members))
    }

    private mutating func parseArray() throws -> JSONNode {
        try expect(0x5B, message: "缺少“[”")
        skipWhitespace()
        var values: [JSONNode] = []

        if consumeIf(0x5D) {
            return JSONNode(value: .array(values))
        }

        while true {
            values.append(try parseValue())
            skipWhitespace()
            if consumeIf(0x5D) { break }
            try expect(0x2C, message: "数组元素之间缺少逗号")
            skipWhitespace()
        }

        return JSONNode(value: .array(values))
    }

    private mutating func parseString() throws -> String {
        try expect(0x22, message: "缺少字符串起始引号")
        var result = ""
        var segmentStart = index

        while index < bytes.count {
            let byte = bytes[index]
            if byte == 0x22 {
                try appendUTF8Segment(from: segmentStart, to: index, into: &result)
                index += 1
                return result
            }

            if byte == 0x5C {
                try appendUTF8Segment(from: segmentStart, to: index, into: &result)
                index += 1
                guard let escaped = currentByte else { throw error("字符串转义不完整") }
                index += 1
                switch escaped {
                case 0x22: result.append("\"")
                case 0x5C: result.append("\\")
                case 0x2F: result.append("/")
                case 0x62: result.append("\u{08}")
                case 0x66: result.append("\u{0C}")
                case 0x6E: result.append("\n")
                case 0x72: result.append("\r")
                case 0x74: result.append("\t")
                case 0x75:
                    let first = try parseHexCodeUnit()
                    if (0xD800...0xDBFF).contains(first) {
                        guard currentByte == 0x5C,
                              index + 1 < bytes.count,
                              bytes[index + 1] == 0x75 else {
                            throw error("高位代理项后缺少低位代理项")
                        }
                        index += 2
                        let second = try parseHexCodeUnit()
                        guard (0xDC00...0xDFFF).contains(second) else {
                            throw error("Unicode 低位代理项无效")
                        }
                        let scalarValue = 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00)
                        guard let scalar = UnicodeScalar(scalarValue) else {
                            throw error("Unicode 转义无效")
                        }
                        result.unicodeScalars.append(scalar)
                    } else if (0xDC00...0xDFFF).contains(first) {
                        throw error("出现了孤立的 Unicode 低位代理项")
                    } else if let scalar = UnicodeScalar(first) {
                        result.unicodeScalars.append(scalar)
                    } else {
                        throw error("Unicode 转义无效")
                    }
                default:
                    throw error("不支持的字符串转义")
                }
                segmentStart = index
                continue
            }

            if byte < 0x20 {
                throw error("字符串中不能直接包含控制字符")
            }
            index += 1
        }

        throw error("字符串缺少结束引号")
    }

    private mutating func appendUTF8Segment(from start: Int, to end: Int, into result: inout String) throws {
        guard end > start else { return }
        let data = Data(bytes[start..<end])
        guard let segment = String(data: data, encoding: .utf8) else {
            throw error("字符串包含无效的 UTF-8")
        }
        result.append(segment)
    }

    private mutating func parseHexCodeUnit() throws -> UInt32 {
        guard index + 4 <= bytes.count else { throw error("Unicode 转义不完整") }
        var value: UInt32 = 0
        for _ in 0..<4 {
            let byte = bytes[index]
            index += 1
            value <<= 4
            switch byte {
            case 0x30...0x39: value += UInt32(byte - 0x30)
            case 0x41...0x46: value += UInt32(byte - 0x41 + 10)
            case 0x61...0x66: value += UInt32(byte - 0x61 + 10)
            default: throw error("Unicode 转义必须包含 4 位十六进制数字")
            }
        }
        return value
    }

    private mutating func parseNumber() throws -> String {
        let start = index
        while index < bytes.count, !isDelimiter(bytes[index]) {
            index += 1
        }
        let token = String(decoding: bytes[start..<index], as: UTF8.self)
        guard JSONNumberValidator.isValid(token) else {
            throw error(.invalidNumber(token))
        }
        return token
    }

    private mutating func consumeLiteral(_ literal: String) throws {
        let expected = Array(literal.utf8)
        guard index + expected.count <= bytes.count,
              Array(bytes[index..<(index + expected.count)]) == expected else {
            throw error(.expectedLiteral(literal))
        }
        index += expected.count
        if let next = currentByte, !isDelimiter(next) {
            throw error(.invalidLiteralSuffix(literal))
        }
    }

    private mutating func expect(_ byte: UInt8, message: String) throws {
        guard currentByte == byte else { throw error(message) }
        index += 1
    }

    private mutating func consumeIf(_ byte: UInt8) -> Bool {
        guard currentByte == byte else { return false }
        index += 1
        return true
    }

    private mutating func skipWhitespace() {
        while let byte = currentByte, byte == 0x20 || byte == 0x09 || byte == 0x0A || byte == 0x0D {
            index += 1
        }
    }

    private var currentByte: UInt8? {
        index < bytes.count ? bytes[index] : nil
    }

    private func isDelimiter(_ byte: UInt8) -> Bool {
        byte == 0x20 || byte == 0x09 || byte == 0x0A || byte == 0x0D ||
        byte == 0x2C || byte == 0x5D || byte == 0x7D
    }

    private func error(_ message: String) -> JSONModelError {
        error(.message(message))
    }

    private func error(_ issue: JSONParseIssue) -> JSONModelError {
        var line = 1
        var column = 1
        for byte in bytes.prefix(index) {
            if byte == 0x0A {
                line += 1
                column = 1
            } else {
                column += 1
            }
        }
        return .parse(issue: issue, line: line, column: column)
    }
}

enum JSONNumberValidator {
    private static let pattern = #"^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$"#

    static func isValid(_ value: String) -> Bool {
        guard !value.isEmpty else { return false }
        return value.range(of: pattern, options: .regularExpression) != nil
    }
}

struct OrderedJSONWriter {
    let formatting: JSONFormatting

    func encode(_ node: JSONNode, trailingNewline: Bool = false) throws -> String {
        try validate(node, path: "$", requireRootObject: false)
        var result = write(node, level: 0)
        if trailingNewline { result.append("\n") }
        return result
    }

    func validate(_ node: JSONNode, path: String = "$", requireRootObject: Bool = false) throws {
        if requireRootObject, node.kind != .object {
            throw JSONModelError.rootMustBeObject
        }

        switch node.value {
        case .number(let number):
            guard JSONNumberValidator.isValid(number) else {
                throw JSONModelError.invalidNumber(number)
            }
        case .object(let members):
            var keys = Set<String>()
            for member in members {
                guard keys.insert(member.key).inserted else {
                    throw JSONModelError.duplicateKey(member.key, path: path)
                }
                try validate(member.value, path: pathForKey(member.key, base: path))
            }
        case .array(let values):
            for (index, value) in values.enumerated() {
                try validate(value, path: "\(path)[\(index)]")
            }
        default:
            break
        }
    }

    private func write(_ node: JSONNode, level: Int) -> String {
        switch node.value {
        case .string(let text):
            return quote(text)
        case .number(let number):
            return number
        case .boolean(let value):
            return value ? "true" : "false"
        case .null:
            return "null"
        case .object(let members):
            guard !members.isEmpty else { return "{}" }
            if formatting == .compact {
                return "{" + members.map { quote($0.key) + ":" + write($0.value, level: level + 1) }.joined(separator: ",") + "}"
            }
            let prefix = indent(level + 1)
            let body = members.map {
                prefix + quote($0.key) + ": " + write($0.value, level: level + 1)
            }.joined(separator: ",\n")
            return "{\n" + body + "\n" + indent(level) + "}"
        case .array(let values):
            guard !values.isEmpty else { return "[]" }
            if formatting == .compact {
                return "[" + values.map { write($0, level: level + 1) }.joined(separator: ",") + "]"
            }
            let prefix = indent(level + 1)
            let body = values.map { prefix + write($0, level: level + 1) }.joined(separator: ",\n")
            return "[\n" + body + "\n" + indent(level) + "]"
        }
    }

    private func indent(_ level: Int) -> String {
        guard let unit = formatting.indentUnit else { return "" }
        return String(repeating: unit, count: level)
    }

    private func quote(_ value: String) -> String {
        var result = "\""
        for scalar in value.unicodeScalars {
            switch scalar.value {
            case 0x22: result += "\\\""
            case 0x5C: result += "\\\\"
            case 0x08: result += "\\b"
            case 0x0C: result += "\\f"
            case 0x0A: result += "\\n"
            case 0x0D: result += "\\r"
            case 0x09: result += "\\t"
            case 0x00...0x1F:
                result += String(format: "\\u%04X", scalar.value)
            default:
                result.unicodeScalars.append(scalar)
            }
        }
        result.append("\"")
        return result
    }

    private func pathForKey(_ key: String, base: String) -> String {
        let identifierPattern = #"^[A-Za-z_$][A-Za-z0-9_$]*$"#
        if key.range(of: identifierPattern, options: .regularExpression) != nil {
            return "\(base).\(key)"
        }
        return "\(base)[\(quote(key))]"
    }
}
