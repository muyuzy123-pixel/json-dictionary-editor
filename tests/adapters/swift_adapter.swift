import Foundation
import Darwin

// Test-only bridge to the existing Swift implementation. No JSON parser lives here.
@main
enum SharedFixtureAdapter {
    static func main() {
        let args = CommandLine.arguments
        guard args.count == 5,
              ["compact", "twoSpaces", "fourSpaces", "tabs", "detect"].contains(args[2]),
              ["object", "any"].contains(args[3]),
              ["true", "false", "preserve"].contains(args[4]) else {
            fputs("usage: swift-adapter INPUT FORMAT object|any true|false|preserve\n", stderr)
            exit(2)
        }
        let data: Data
        do {
            data = args[1] == "-" ? FileHandle.standardInput.readDataToEndOfFile()
                : try Data(contentsOf: URL(fileURLWithPath: args[1]))
        } catch {
            fputs("adapter input read failed\n", stderr)
            exit(2)
        }
        do {
            var parser = try OrderedJSONParser(data: data)
            let root = try parser.parse()
            let text = String(data: data, encoding: .utf8)!
            let formatting = args[2] == "detect"
                ? JSONFormatting.detect(in: text)
                : JSONFormatting(rawValue: args[2])!
            let writer = OrderedJSONWriter(formatting: formatting)
            try writer.validate(root, requireRootObject: args[3] == "object")
            let trailing = args[4] == "preserve" ? text.hasSuffix("\n") : args[4] == "true"
            let encoded = try writer.encode(root, trailingNewline: trailing)
            FileHandle.standardOutput.write(Data(encoded.utf8))
        } catch let error as JSONModelError {
            fputs("\(error.localizedDescription)\n", stderr)
            exit(1)
        } catch {
            fputs("unexpected adapter failure\n", stderr)
            exit(2)
        }
    }
}
