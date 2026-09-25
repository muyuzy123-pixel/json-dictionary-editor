import Foundation

enum SelfTestFailure: LocalizedError {
    case assertion(String)

    var errorDescription: String? {
        switch self {
        case .assertion(let message): return message
        }
    }
}

enum SelfTest {
    static func run() -> Int32 {
        do {
            try localizedResourcesAreAvailable()
            try languagePreferenceRoundTrips()
            try parserPreservesTypesAndOrder()
            try unicodeAndEscapesRoundTrip()
            try numberGrammarIsStrict()
            try malformedInputsAreRejected()
            try writerFormatsAndRoundTrips()
            try documentOperationsRemainValid()
            try rawReplacementKeepsSelectionIdentity()
            try searchAndPathsWork()
            try deepCopyRegeneratesIdentifiers()
            print("SELF_TEST_OK: parser, Unicode, number grammar, validation, formatting, tree operations, identity, search paths, deep copy")
            return 0
        } catch {
            fputs("SELF_TEST_FAILED: \(error.localizedDescription)\n", stderr)
            return 1
        }
    }

    private static func localizedResourcesAreAvailable() throws {
        let language = LanguageStore.shared
        try expect(language.hasTranslation("字符串", language: "en"),
                   "English localization entry was not loaded")
        try expect(language.hasTranslation("字符串", language: "zh-Hans"),
                   "Simplified Chinese localization entry was not loaded")
        try expect(language.text("字符串", language: "en") == "String",
                   "English localization resource is unavailable")
        try expect(language.text("字符串", language: "zh-Hans") == "字符串",
                   "Simplified Chinese localization resource is unavailable")
        try expect(!language.hasTranslation("__missing_localization_probe__", language: "zh-Hans"),
                   "missing localization key was treated as loaded")
    }

    private static func languagePreferenceRoundTrips() throws {
        try expect(LanguageStore.resolve(.system, systemLanguage: "zh-Hans") == "zh-Hans",
                   "Chinese system preference did not resolve to Chinese")
        try expect(LanguageStore.resolve(.english, systemLanguage: "zh-Hans") == "en",
                   "manual English preference did not override Chinese system")
        try expect(LanguageStore.resolve(.system, systemLanguage: "zh-Hans") == "zh-Hans",
                   "returning to Chinese system preference kept English")
        try expect(LanguageStore.resolve(.system, systemLanguage: "en-US") == "en",
                   "English system preference did not resolve to English")
        try expect(LanguageStore.resolve(.chinese, systemLanguage: "en-US") == "zh-Hans",
                   "manual Chinese preference did not override English system")
        try expect(LanguageStore.resolve(.system, systemLanguage: "en-US") == "en",
                   "returning to English system preference kept Chinese")
    }

    private static func parserPreservesTypesAndOrder() throws {
        let text = #"{"z":1,"a":"文字","enabled":true,"nothing":null,"nested":{"x":2},"list":[1,"2",false]}"#
        var parser = OrderedJSONParser(text: text)
        let root = try parser.parse()
        guard case .object(let members) = root.value else {
            throw failure("根节点未解析为对象")
        }
        try expect(members.map(\.key) == ["z", "a", "enabled", "nothing", "nested", "list"], "对象键顺序未保留")
        try expect(members[0].value.kind == .number, "数字类型错误")
        try expect(members[1].value.kind == .string, "字符串类型错误")
        try expect(members[2].value.kind == .boolean, "布尔类型错误")
        try expect(members[3].value.kind == .null, "null 类型错误")
        try expect(members[4].value.kind == .object, "嵌套对象类型错误")
        try expect(members[5].value.kind == .array, "数组类型错误")
    }

    private static func unicodeAndEscapesRoundTrip() throws {
        let text = #"{"中文":"你好\n世界","emoji":"\uD83D\uDE80","quote":"\"\\\/"}"#
        var parser = OrderedJSONParser(text: text)
        let root = try parser.parse()
        guard case .object(let members) = root.value,
              case .string(let emoji) = members[1].value.value else {
            throw failure("Unicode 测试结构错误")
        }
        try expect(emoji == "🚀", "Unicode 代理项未正确组合")
        let encoded = try OrderedJSONWriter(formatting: .twoSpaces).encode(root)
        var roundTripParser = OrderedJSONParser(text: encoded)
        let roundTrip = try roundTripParser.parse()
        try expect(equivalentJSON(root, roundTrip), "Unicode/转义往返后内容变化")
    }

    private static func numberGrammarIsStrict() throws {
        let valid = ["0", "-0", "12", "-12.50", "6.02e23", "1E-9"]
        let invalid = ["", "+1", "01", "1.", ".5", "NaN", "Infinity", "1e", "--2"]
        try expect(valid.allSatisfy(JSONNumberValidator.isValid), "有效 JSON 数字被拒绝")
        try expect(invalid.allSatisfy { !JSONNumberValidator.isValid($0) }, "无效 JSON 数字被接受")
    }

    private static func malformedInputsAreRejected() throws {
        let invalidTexts = [
            #"{"a":1,"a":2}"#,
            #"{"a":01}"#,
            #"{"a":[1,]}"#,
            #"{"a":"\uD800"}"#,
            #"{"a" 1}"#
        ]
        for text in invalidTexts {
            do {
                var parser = OrderedJSONParser(text: text)
                _ = try parser.parse()
                throw failure("无效 JSON 被接受：\(text)")
            } catch is SelfTestFailure {
                throw errorFromInvalidAcceptance(text)
            } catch {
                // Expected.
            }
        }

        var arrayParser = OrderedJSONParser(text: "[1,2]")
        let array = try arrayParser.parse()
        do {
            try OrderedJSONWriter(formatting: .compact).validate(array, requireRootObject: true)
            throw failure("数组根节点被当作字典接受")
        } catch JSONModelError.rootMustBeObject {
            // Expected.
        }
    }

    private static func writerFormatsAndRoundTrips() throws {
        var parser = OrderedJSONParser(text: #"{"b":[1,2],"a":{"c":true}}"#)
        let root = try parser.parse()
        let pretty = try OrderedJSONWriter(formatting: .fourSpaces).encode(root, trailingNewline: true)
        try expect(pretty.contains("\n    \"b\""), "4 空格缩进未生效")
        try expect(pretty.hasSuffix("\n"), "末尾换行未保留")
        let compact = try OrderedJSONWriter(formatting: .compact).encode(root)
        try expect(compact == #"{"b":[1,2],"a":{"c":true}}"#, "紧凑格式不符合预期")
        var reparsed = OrderedJSONParser(text: pretty)
        let reparsedRoot = try reparsed.parse()
        try expect(equivalentJSON(root, reparsedRoot), "格式化往返后内容变化")
    }

    private static func documentOperationsRemainValid() throws {
        var document = JSONDictionaryDocument()
        guard let first = document.addChild(to: document.root.id) else { throw failure("无法添加顶层键") }
        try expect(document.renameNode(first, to: "名称"), "无法重命名键")
        try expect(document.setString("编辑器", for: first), "无法设置字符串")

        guard let object = document.addChild(to: document.root.id) else { throw failure("无法添加对象") }
        try expect(document.renameNode(object, to: "设置"), "无法重命名对象键")
        try expect(document.changeKind(of: object, to: .object), "无法更改为对象")
        guard let nested = document.addChild(to: object) else { throw failure("无法添加嵌套键") }
        try expect(document.renameNode(nested, to: "启用"), "无法重命名嵌套键")
        try expect(document.changeKind(of: nested, to: .boolean), "无法更改为布尔值")
        try expect(document.setBoolean(true, for: nested), "无法设置布尔值")

        try expect(!document.renameNode(object, to: "名称"), "重复键名未被阻止")
        guard let duplicate = document.duplicateNode(object) else { throw failure("无法复制对象") }
        try expect(document.key(for: duplicate) == "设置 2", "复制对象未生成唯一键名")
        try expect(document.moveNode(duplicate, offset: -1), "无法移动节点")
        try expect(document.deleteNode(duplicate), "无法删除节点")
        try expect(document.nodeCount == 4, "节点计数错误")

        try OrderedJSONWriter(formatting: .twoSpaces).validate(document.root, requireRootObject: true)
    }

    private static func rawReplacementKeepsSelectionIdentity() throws {
        var document = JSONDictionaryDocument()
        guard let id = document.addChild(to: document.root.id) else { throw failure("无法添加替换目标") }
        var parser = OrderedJSONParser(text: #"{"nested":[1,2,3]}"#)
        let replacement = try parser.parse()
        try expect(document.replaceNode(id, with: replacement), "无法替换节点")
        try expect(document.node(withID: id)?.kind == .object, "替换后节点 ID 未保留")
        try expect(document.location(of: id) != nil, "替换后选择路径失效")
    }

    private static func searchAndPathsWork() throws {
        var parser = OrderedJSONParser(text: #"{"普通键":1,"settings":{"theme":"dark"},"items":[true]}"#)
        var document = JSONDictionaryDocument()
        document.root = try parser.parse()
        let rows = document.allRows()
        try expect(rows.contains { $0.path == #"$["普通键"]"# }, "中文键路径错误")
        try expect(rows.contains { $0.path == "$.settings.theme" }, "标识符键路径错误")
        try expect(rows.contains { $0.path == "$.items[0]" }, "数组路径错误")
        let matches = document.visibleRows(expanded: [], searchText: "dark")
        try expect(matches.count == 1 && matches[0].path == "$.settings.theme", "值搜索结果错误")

        var containers = OrderedJSONParser(text:
            #"{"filledObject":{"x":1},"emptyObject":{},"filledArray":[1],"emptyArray":[]}"#)
        document.root = try containers.parse()
        for (term, path) in [
            ("empty object", "$.emptyObject"), ("空对象", "$.emptyObject"),
            ("empty array", "$.emptyArray"), ("空数组", "$.emptyArray")
        ] {
            let hits = document.visibleRows(expanded: [], searchText: term)
            try expect(hits.map(\.path) == [path], "空容器搜索误匹配非空容器：\(term)")
        }
        for (term, paths) in [
            ("object", ["$.filledObject", "$.emptyObject"]),
            ("array", ["$.filledArray", "$.emptyArray"])
        ] {
            let hits = document.visibleRows(expanded: [], searchText: term)
            try expect(Set(hits.map(\.path)).isSuperset(of: paths),
                       "普通类型别名丢失：\(term)")
        }
    }

    private static func deepCopyRegeneratesIdentifiers() throws {
        var parser = OrderedJSONParser(text: #"{"a":{"b":[1,2]}}"#)
        let original = try parser.parse()
        let copy = original.deepCopy()
        let originalIDs = Set(flattenIDs(original))
        let copyIDs = Set(flattenIDs(copy))
        try expect(originalIDs.isDisjoint(with: copyIDs), "深复制复用了节点 ID")
        try expect(equivalentJSON(original, copy), "深复制改变了 JSON 内容")
    }

    private static func flattenIDs(_ node: JSONNode) -> [UUID] {
        switch node.value {
        case .object(let members): return [node.id] + members.flatMap { flattenIDs($0.value) }
        case .array(let values): return [node.id] + values.flatMap(flattenIDs)
        default: return [node.id]
        }
    }

    private static func equivalentJSON(_ lhs: JSONNode, _ rhs: JSONNode) -> Bool {
        switch (lhs.value, rhs.value) {
        case (.string(let a), .string(let b)): return a == b
        case (.number(let a), .number(let b)): return a == b
        case (.boolean(let a), .boolean(let b)): return a == b
        case (.null, .null): return true
        case (.object(let a), .object(let b)):
            return a.count == b.count && zip(a, b).allSatisfy { left, right in
                left.key == right.key && equivalentJSON(left.value, right.value)
            }
        case (.array(let a), .array(let b)):
            return a.count == b.count && zip(a, b).allSatisfy(equivalentJSON)
        default: return false
        }
    }

    private static func expect(_ condition: @autoclosure () -> Bool, _ message: String) throws {
        guard condition() else { throw failure(message) }
    }

    private static func failure(_ message: String) -> SelfTestFailure {
        .assertion(message)
    }

    private static func errorFromInvalidAcceptance(_ text: String) -> SelfTestFailure {
        failure("无效 JSON 被接受：\(text)")
    }
}
