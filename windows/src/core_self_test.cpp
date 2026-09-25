#include "json_core.hpp"
#include "json_search_aliases.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace jsondict;

int g_assertions = 0;

void expect(bool condition, const std::string& message) {
    ++g_assertions;
    if (!condition) throw std::runtime_error(message);
}

template <typename Function>
void expect_error(Function&& function, ErrorCode code, const std::string& message) {
    ++g_assertions;
    try {
        function();
    } catch (const Error& error) {
        if (error.code() == code) return;
        throw std::runtime_error(message + " (wrong error code)");
    }
    throw std::runtime_error(message + " (no error)");
}

void collect_ids(const Node& node, std::set<NodeId>& output) {
    output.insert(node.id());
    if (node.kind() == Kind::Object) {
        for (const auto& member : node.as_object()) collect_ids(member.value, output);
    } else if (node.kind() == Kind::Array) {
        for (const auto& value : node.as_array()) collect_ids(value, output);
    }
}

void parser_preserves_types_order_and_number_text() {
    const Node root = parse(
        R"({"z":1.2300e+04,"a":"文字","enabled":true,"nothing":null,"nested":{"x":2},"list":[1,"2",false]})");
    expect(root.kind() == Kind::Object, "root was not parsed as an object");
    const auto& members = root.as_object();
    expect(members.size() == 6, "wrong root member count");
    expect(members[0].key == "z" && members[1].key == "a" &&
           members[2].key == "enabled" && members[3].key == "nothing" &&
           members[4].key == "nested" && members[5].key == "list",
           "object member order changed");
    expect(members[0].value.kind() == Kind::Number &&
           members[0].value.as_number().text == "1.2300e+04",
           "number text was normalized");
    expect(members[1].value.kind() == Kind::String, "string type changed");
    expect(members[2].value.kind() == Kind::Boolean, "boolean type changed");
    expect(members[3].value.kind() == Kind::Null, "null type changed");
    expect(members[4].value.kind() == Kind::Object, "nested object type changed");
    expect(members[5].value.kind() == Kind::Array, "array type changed");
}

void unicode_and_escapes_round_trip() {
    const Node root = parse(
        R"({"中文":"你好\n世界","emoji":"\uD83D\uDE80","quote":"\"\\\/","nul":"\u0000"})");
    const auto& members = root.as_object();
    expect(members[0].key == u8"中文", "Unicode key decoded incorrectly");
    expect(members[1].value.as_string() == u8"🚀", "surrogate pair decoded incorrectly");
    expect(members[2].value.as_string() == "\"\\/", "string escapes decoded incorrectly");
    expect(members[3].value.as_string().size() == 1 && members[3].value.as_string()[0] == '\0',
           "escaped NUL decoded incorrectly");

    const std::string encoded = write(root, WriteOptions{Formatting::TwoSpaces, false, false});
    expect(encoded.find("\\u0000") != std::string::npos, "control byte was not escaped by writer");
    const Node round_trip = parse(encoded);
    expect(equivalent(root, round_trip), "Unicode/escape round trip changed JSON data");
}

void number_grammar_is_strict() {
    const std::vector<std::string> valid = {
        "0", "-0", "12", "-12.50", "6.02e23", "1E-9", "0.0", "10e+2"
    };
    const std::vector<std::string> invalid = {
        "", "+1", "01", "-01", "1.", ".5", "NaN", "Infinity", "1e", "--2", " 1", "1 "
    };
    expect(std::all_of(valid.begin(), valid.end(), is_valid_number),
           "valid JSON number was rejected");
    expect(std::all_of(invalid.begin(), invalid.end(),
                       [](const std::string& value) { return !is_valid_number(value); }),
           "invalid JSON number was accepted");
}

void search_aliases_respect_empty_containers() {
    const Node filled_object = parse(R"({"x":1})");
    const Node empty_object = parse("{}");
    const Node filled_array = parse("[1]");
    const Node empty_array = parse("[]");
    auto has = [](const Node& node, std::wstring_view term) {
        return search_aliases(node).find(term) != std::wstring::npos;
    };
    expect(!has(filled_object, L"empty object") && !has(filled_object, L"空对象"),
           "nonempty object matched an empty-object alias");
    expect(has(empty_object, L"empty object") && has(empty_object, L"空对象"),
           "empty object did not match bilingual empty-object aliases");
    expect(!has(filled_array, L"empty array") && !has(filled_array, L"空数组"),
           "nonempty array matched an empty-array alias");
    expect(has(empty_array, L"empty array") && has(empty_array, L"空数组"),
           "empty array did not match bilingual empty-array aliases");
    expect(has(filled_object, L"object") && has(empty_object, L"object") &&
           has(filled_array, L"array") && has(empty_array, L"array"),
           "ordinary type aliases were removed");
}

void malformed_inputs_are_rejected() {
    const std::vector<std::string> invalid = {
        R"({"a":1,"a":2})",
        R"({"a":1,"\u0061":2})",
        R"({"a":01})",
        R"({"a":[1,]})",
        R"({"a":"\uD800"})",
        R"({"a":"\uDC00"})",
        R"({"a":"\uD800\u0041"})",
        std::string("{\"a\":\"line\nfeed\"}"),
        R"({"a" 1})",
        R"({"a":truex})",
        R"({"a":1} trailing)",
        R"(/*comment*/{})",
        "",
    };
    for (std::size_t i = 0; i < invalid.size(); ++i) {
        const ErrorCode expected = i < 2 ? ErrorCode::DuplicateKey : ErrorCode::Parse;
        expect_error([&] { (void)parse(invalid[i]); }, expected,
                     "malformed JSON was accepted: " + invalid[i]);
    }
    try {
        (void)parse(R"({"a":1,"\u0061":2})");
        throw std::runtime_error("structured duplicate-key error was not thrown");
    } catch (const Error& error) {
        expect(error.reason() == ErrorReason::DuplicateKey && error.argument() == "a",
               "decoded duplicate key was not preserved in structured error");
        expect(error.line() == 1 && error.column() > 0,
               "structured duplicate-key location was lost");
    }
    try {
        (void)parse(R"({"n":01})");
        throw std::runtime_error("structured number error was not thrown");
    } catch (const Error& error) {
        expect(error.reason() == ErrorReason::InvalidNumber && error.argument() == "01",
               "original invalid number was not preserved in structured error");
    }

    std::string invalid_utf8 = "{\"a\":\"";
    invalid_utf8.push_back(static_cast<char>(0xC0));
    invalid_utf8.push_back(static_cast<char>(0xAF));
    invalid_utf8 += "\"}";
    expect_error([&] { (void)parse(invalid_utf8); }, ErrorCode::InvalidUtf8,
                 "overlong UTF-8 was accepted");

    std::string surrogate_utf8 = "{\"a\":\"";
    surrogate_utf8.push_back(static_cast<char>(0xED));
    surrogate_utf8.push_back(static_cast<char>(0xA0));
    surrogate_utf8.push_back(static_cast<char>(0x80));
    surrogate_utf8 += "\"}";
    expect_error([&] { (void)parse(surrogate_utf8); }, ErrorCode::InvalidUtf8,
                 "UTF-8-encoded surrogate was accepted");

    try {
        (void)parse("{\n  \"a\": 1,\n  nope\n}");
        expect(false, "invalid line/column input was accepted");
    } catch (const Error& error) {
        expect(error.line() == 3 && error.column() == 3,
               "parse error line/column was not deterministic");
    }

    const std::string too_deep = std::string(513, '[') + "null" + std::string(513, ']');
    expect_error([&] { (void)parse(too_deep); }, ErrorCode::Parse,
                 "excessive JSON nesting bypassed the safety limit");
}

void writer_formats_validates_and_round_trips() {
    const Node root = parse(R"({"b":[1,2],"a":{"c":true}})");
    const std::string pretty = write(root, WriteOptions{Formatting::FourSpaces, true, true});
    expect(pretty.find("\n    \"b\"") != std::string::npos,
           "four-space indentation was not applied");
    expect(!pretty.empty() && pretty.back() == '\n', "trailing newline was not emitted");
    expect(equivalent(root, parse(pretty)), "pretty output failed round trip");

    const std::string compact = write(root, WriteOptions{Formatting::Compact, false, true});
    expect(compact == R"({"b":[1,2],"a":{"c":true}})", "compact output changed ordering");
    expect(detect_formatting(pretty) == Formatting::FourSpaces,
           "four-space format detection failed");
    expect(detect_formatting(compact) == Formatting::Compact,
           "compact format detection failed");
    expect(detect_formatting("{\n\t\"a\": 1\n}") == Formatting::Tabs,
           "tab format detection failed");

    expect_error([&] { validate(parse("[1,2]"), true); }, ErrorCode::RootMustBeObject,
                 "array root was accepted as a dictionary");

    Node duplicate = Node::object({
        Node::Member{"same", Node::number("1")},
        Node::Member{"same", Node::number("2")},
    });
    expect_error([&] { validate(duplicate); }, ErrorCode::DuplicateKey,
                 "programmatically duplicated key was not rejected");

    Node invalid_number = Node::number("01");
    expect_error([&] { validate(invalid_number); }, ErrorCode::InvalidNumber,
                 "programmatically invalid number was not rejected");

    std::string bad_text(1, static_cast<char>(0xFF));
    Node invalid_string = Node::string(bad_text);
    expect_error([&] { validate(invalid_string); }, ErrorCode::InvalidUtf8,
                 "programmatically invalid UTF-8 string was not rejected");
}

void document_operations_remain_valid() {
    Document document;
    const auto first = document.add_child();
    expect(first.has_value(), "could not add root member");
    expect(document.rename_node(*first, u8"名称"), "could not rename root member");
    expect(document.set_string(*first, u8"编辑器"), "could not set string value");

    const auto object = document.add_child();
    expect(object.has_value(), "could not add object target");
    expect(document.rename_node(*object, u8"设置"), "could not rename object member");
    expect(document.change_kind(*object, Kind::Object), "could not change kind to object");
    const auto nested = document.add_child(*object);
    expect(nested.has_value(), "could not add nested member");
    expect(document.rename_node(*nested, u8"启用"), "could not rename nested member");
    expect(document.change_kind(*nested, Kind::Boolean), "could not change kind to boolean");
    expect(document.set_boolean(*nested, true), "could not set boolean");

    expect(!document.rename_node(*object, u8"名称"), "duplicate sibling key was accepted");
    const auto duplicate = document.duplicate_node(*object);
    expect(duplicate.has_value(), "could not duplicate object");
    expect(document.location(*duplicate)->key == std::optional<std::string>(u8"设置 2"),
           "duplicate did not receive a unique key");
    expect(document.can_move(*duplicate, -1) && document.move_node(*duplicate, -1),
           "could not move duplicated node");
    expect(document.delete_node(*duplicate), "could not delete duplicated node");
    expect(document.node_count() == 4, "document node count is wrong");
    validate(document.root(), true);

    const auto array_member = document.add_child();
    expect(array_member && document.rename_node(*array_member, "items") &&
           document.change_kind(*array_member, Kind::Array),
           "could not make array member");
    const auto array_child = document.add_child(*array_member);
    expect(array_child.has_value(), "could not add array element");
    expect(document.location(*array_child)->path == "$.items[0]", "array path is wrong");
    expect(!document.rename_node(*array_child, "not-allowed"),
           "array element was incorrectly treated as an object member");

    expect(!document.delete_node(document.root_id()), "root deletion was allowed");
    expect(!document.change_kind(document.root_id(), Kind::Array), "root kind changed from object");
    expect(!document.set_string(document.root_id(), "invalid"),
           "scalar setter changed the dictionary root");
}

void replacement_paths_and_deep_copy_identity_work() {
    Document document = Document::from_json(
        R"({"普通键":1,"settings":{"theme":"dark"},"items":[true]})");
    const auto& root_members = document.root().as_object();
    const NodeId chinese_id = root_members[0].value.id();
    const NodeId theme_id = root_members[1].value.as_object()[0].value.id();
    const NodeId item_id = root_members[2].value.as_array()[0].id();
    expect(document.location(chinese_id)->path == u8"$[\"普通键\"]",
           "non-identifier object path is wrong");
    expect(document.location(theme_id)->path == "$.settings.theme",
           "identifier object path is wrong");
    expect(document.location(item_id)->path == "$.items[0]", "array path is wrong");

    const NodeId selected = theme_id;
    const Node replacement = parse(R"({"nested":[1,2,3]})");
    expect(document.replace_node(selected, replacement), "node replacement failed");
    expect(document.find(selected) && document.find(selected)->kind() == Kind::Object,
           "replacement did not preserve selected node ID");
    expect(document.location(selected).has_value(), "replacement invalidated selection path");

    Document depth_document = Document::from_json(R"({"x":null})");
    const NodeId depth_target = depth_document.root().as_object()[0].value.id();
    Node too_deep = Node::null();
    for (std::size_t level = 0; level < 512; ++level) {
        Node::Array values;
        values.push_back(std::move(too_deep));
        too_deep = Node::array(std::move(values));
    }
    expect(!depth_document.replace_node(depth_target, too_deep),
           "replacement exceeded the document nesting invariant");
    expect(depth_document.find(depth_target)->kind() == Kind::Null,
           "failed deep replacement did not roll back atomically");

    const Node original = parse(R"({"a":{"b":[1,2]}})");
    const Node copy = original.deep_copy();
    Node reassigned = original;
    reassigned.regenerate_ids();
    std::set<NodeId> original_ids;
    std::set<NodeId> copy_ids;
    collect_ids(original, original_ids);
    collect_ids(copy, copy_ids);
    std::vector<NodeId> intersection;
    std::set_intersection(original_ids.begin(), original_ids.end(),
                          copy_ids.begin(), copy_ids.end(),
                          std::back_inserter(intersection));
    expect(intersection.empty(), "deep copy reused a node ID");
    expect(equivalent(original, copy), "deep copy changed JSON content");
    std::set<NodeId> reassigned_ids;
    collect_ids(reassigned, reassigned_ids);
    intersection.clear();
    std::set_intersection(original_ids.begin(), original_ids.end(),
                          reassigned_ids.begin(), reassigned_ids.end(),
                          std::back_inserter(intersection));
    expect(intersection.empty(), "regenerate_ids reused an existing node ID");
    expect(equivalent(original, reassigned), "regenerate_ids changed JSON content");
}

void deterministic_sort_and_serialization_work() {
    Document document = Document::from_json(R"({"z":1,"A":2,"a":3,"10":4,"2":5})");
    document.set_formatting(Formatting::TwoSpaces);
    expect(document.sort_object(document.root_id()), "root object sort failed");
    expect(document.encoded_text() == R"({
  "10": 4,
  "2": 5,
  "A": 2,
  "a": 3,
  "z": 1
})", "portable bytewise sort was not deterministic");
}

}  // namespace

int main() {
    try {
        parser_preserves_types_order_and_number_text();
        unicode_and_escapes_round_trip();
        number_grammar_is_strict();
        search_aliases_respect_empty_containers();
        malformed_inputs_are_rejected();
        writer_formats_validates_and_round_trips();
        document_operations_remain_valid();
        replacement_paths_and_deep_copy_identity_work();
        deterministic_sort_and_serialization_work();
        std::cout << "SELF_TEST_OK: " << g_assertions
                  << " assertions; ordered model, strict UTF-8/parser, Unicode, numbers, "
                     "writer, validation, tree mutations, paths, identity\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SELF_TEST_FAILED after " << g_assertions
                  << " assertions: " << error.what() << '\n';
        return 1;
    }
}
