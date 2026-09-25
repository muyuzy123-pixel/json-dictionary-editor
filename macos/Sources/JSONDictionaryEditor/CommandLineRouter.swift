import Darwin
import Foundation

enum CommandLineRouter {
    static func handleIfNeeded() {
        let arguments = CommandLine.arguments
        guard arguments.count > 1 else { return }

        switch arguments[1] {
        case "--self-test":
            exit(SelfTest.run())
        case "--validate-json":
            guard arguments.count == 3 else {
                fputs((LanguageStore.shared.effectiveIdentifier == "en"
                    ? "Usage: JSONDictionaryEditor --validate-json <file-path>\n"
                    : "用法：JSONDictionaryEditor --validate-json <文件路径>\n"), stderr)
                exit(2)
            }
            exit(validateFile(at: arguments[2]))
        case "--generate-icon":
            guard arguments.count == 3 else {
                fputs((LanguageStore.shared.effectiveIdentifier == "en"
                    ? "Usage: JSONDictionaryEditor --generate-icon <output.icns>\n"
                    : "用法：JSONDictionaryEditor --generate-icon <输出.icns>\n"), stderr)
                exit(2)
            }
            do {
                try IconGenerator.writeICNS(to: URL(fileURLWithPath: arguments[2]))
                print("ICON_OK: \(arguments[2])")
                exit(0)
            } catch {
                fputs("ICON_FAILED: \(error.localizedDescription)\n", stderr)
                exit(1)
            }
        case "--version":
            print("JSON Dictionary Editor 1.1.1 (3)")
            exit(0)
        case "--help":
            if LanguageStore.shared.effectiveIdentifier == "en" {
                print("""
                JSON Dictionary Editor 1.1.1

                  --self-test              run deterministic core self-tests
                  --validate-json <path>   validate a UTF-8 JSON dictionary
                  --generate-icon <path>   generate the application ICNS icon
                  --version                display version
                """)
            } else {
                print("""
                JSON 字典编辑器 1.1.1

                  --self-test              运行确定性核心自检
                  --validate-json <路径>   验证文件是否为有效 JSON 字典
                  --generate-icon <路径>   生成应用 ICNS 图标
                  --version                显示版本
                """)
            }
            exit(0)
        default:
            return
        }
    }

    private static func validateFile(at path: String) -> Int32 {
        do {
            let data = try Data(contentsOf: URL(fileURLWithPath: path))
            var parser = try OrderedJSONParser(data: data)
            let root = try parser.parse()
            try OrderedJSONWriter(formatting: .compact).validate(root, requireRootObject: true)
            print("VALID_JSON_DICTIONARY: \(countNodes(root)) nodes")
            return 0
        } catch {
            fputs("INVALID_JSON_DICTIONARY: \(error.localizedDescription)\n", stderr)
            return 1
        }
    }

    private static func countNodes(_ node: JSONNode) -> Int {
        switch node.value {
        case .object(let members): return 1 + members.reduce(0) { $0 + countNodes($1.value) }
        case .array(let values): return 1 + values.reduce(0) { $0 + countNodes($1) }
        default: return 1
        }
    }
}
