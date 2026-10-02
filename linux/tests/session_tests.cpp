#include "editor_session.hpp"
#include "json_search_aliases.hpp"
#include <iostream>
#include <stdexcept>

using namespace jsondict_linux;
int assertions = 0;
void check(bool value, const char* message) {
    ++assertions;
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejected(F action, const char* message) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    check(failed, message);
}

int main() {
    try {
        EditorSession session;
        session.load("{\"n\":1.2300e+04,\"other\":0,\"s\":\"\\r\\n🙂\",\"empty\":{},\"filled\":{\"x\":1}}");
        const auto n = session.document.root().as_object()[0].value.id();
        session.selected = n;
        const auto before = session.encoded();
        rejected([&] { session.apply_draft({"renamed", "01", jsondict::Kind::Number}); },
                 "invalid combined draft was accepted");
        check(session.encoded() == before && !session.dirty, "failed draft partially committed");
        rejected([&] { session.apply_draft({"other", "2", jsondict::Kind::Number}); },
                 "duplicate combined draft was accepted");
        check(session.encoded() == before, "duplicate draft changed numeric value");
        check(session.apply_draft({"renamed", "-0", jsondict::Kind::Number}), "valid draft did not commit");
        check(session.document.find(n)->as_number().text == "-0", "numeric text changed");
        const auto committed = session.encoded();
        rejected([&] { session.apply_raw(session.document.root_id(), "[1]"); }, "array root accepted");
        check(session.encoded() == committed, "raw failure partially replaced document");
        check(session.apply_raw(n, "{\"unicode\":\"\\uD83D\\uDE42\"}"), "valid raw failed");
        check(session.document.find(n)->id() == n, "raw replacement lost identity");
        const auto& members = session.document.root().as_object();
        check(members[0].key == "renamed" && members[1].key == "other", "key order changed");
        check(members[2].value.as_string() == "\r\n🙂", "CR/Unicode string changed");
        check(jsondict::search_aliases(members[3].value).find(L"empty object") != std::wstring::npos,
              "empty object lost alias");
        check(jsondict::search_aliases(members[4].value).find(L"empty object") == std::wstring::npos,
              "filled object matched empty alias");
        EditorSession bom;
        bom.load("\xef\xbb\xbf{\"n\":9007199254740993}\n");
        check(bom.encoded().substr(0, 3) == "\xef\xbb\xbf", "BOM not retained");
        rejected([&] { bom.load(std::string("\xff\xfe{}", 4)); }, "UTF-16 BOM accepted");
        rejected([&] { session.apply_raw(n, std::string(kMaximumRawBytes + 1, ' ')); }, "raw byte limit missing");
        rejected([&] { session.load(std::string(kMaximumFileBytes + 1, ' ')); }, "file byte limit missing");
        std::string nodes = "{\"items\":[";
        for (std::size_t i = 0; i < kMaximumDocumentNodes; ++i) {
            if (i) nodes += ',';
            nodes += '0';
        }
        nodes += "]}";
        rejected([&] { session.load(nodes); }, "GUI node limit missing");

        EditorSession limits;
        limits.load("{\"k\":\"old\",\"b\":false,\"huge\":90071992547409931234567890,\"exp\":1.2300e+04}");
        limits.selected = limits.document.root().as_object()[0].value.id();
        const auto clean = limits.encoded();
        rejected([&] { limits.apply_draft({std::string(kMaximumKeyCharacters + 1, 'k'), {}, jsondict::Kind::String}); }, "inspector key limit missing");
        rejected([&] { limits.apply_draft({{}, std::string(kMaximumInspectorCharacters + 1, 'v'), jsondict::Kind::String}); }, "inspector value limit missing");
        check(limits.encoded() == clean && !limits.dirty, "oversize inspector mutated document");
        rejected([&] { limits.apply_draft({{}, std::string("\xc0\xaf", 2), jsondict::Kind::String}); }, "invalid draft UTF-8 accepted");
        check(limits.encoded() == clean, "invalid UTF-8 mutated document");
        check(!limits.mutate([](jsondict::Document& doc) { doc.add_child(); return false; }), "false callback committed");
        check(limits.encoded() == clean && !limits.dirty, "discarded candidate leaked into session");
        check(!limits.apply_draft({"k", "old", jsondict::Kind::String}), "no-op draft reported changed");
        check(!limits.dirty, "no-op marked clean document dirty");
        check(limits.encoded().find("90071992547409931234567890") != std::string::npos &&
              limits.encoded().find("1.2300e+04") != std::string::npos, "high precision/exponent changed");
        limits.selected = limits.document.root().as_object()[1].value.id();
        check(limits.apply_draft({"flag", {}, jsondict::Kind::Boolean, true}), "boolean transaction failed");
        check(limits.document.find(limits.selected)->as_boolean(), "boolean value not committed");
        const auto boolean_committed = limits.encoded();
        rejected([&] { limits.apply_draft({"k", {}, jsondict::Kind::Boolean, false}); }, "duplicate boolean key accepted");
        check(limits.encoded() == boolean_committed, "duplicate boolean draft partially committed");
        rejected([&] { limits.apply_raw(limits.document.root_id(), "{\"a\":0,\"\\u0061\":1}"); }, "decoded duplicate keys accepted");
        rejected([&] { limits.apply_raw(limits.document.root_id(), "{\"s\":\"\\uD800\"}"); }, "isolated surrogate accepted");
        check(limits.encoded() == boolean_committed, "invalid raw modified document");
        limits.selected = 0;
        rejected([&] { limits.apply_draft({}); }, "stale selection accepted");
        EditorSession boundary;
        std::string exact = "{\"items\":[";
        for (std::size_t i = 0; i < kMaximumDocumentNodes - 2; ++i) { if (i) exact += ','; exact += '0'; }
        exact += "]}";
        boundary.load(exact);
        check(boundary.document.node_count() == kMaximumDocumentNodes, "50,000-node boundary rejected");
        const auto node_boundary = boundary.encoded();
        rejected([&] { boundary.mutate([](jsondict::Document& doc) { return doc.add_child().has_value(); }); }, "50,001st node accepted");
        check(boundary.encoded() == node_boundary && !boundary.dirty, "node overflow partially committed");
        boundary.load("{\"s\":\"\"}"); boundary.selected = boundary.document.root().as_object()[0].value.id();
        std::string emoji;
        for (std::size_t i = 0; i < kMaximumInspectorCharacters / 2; ++i) emoji += "🙂";
        check(boundary.apply_draft({{}, emoji, jsondict::Kind::String}), "UTF-16 unit boundary rejected");
        rejected([&] { boundary.apply_draft({{}, emoji + "x", jsondict::Kind::String}); }, "UTF-16 unit overflow accepted");
        check(boundary.apply_draft({std::string(kMaximumKeyCharacters, 'k'), {}, jsondict::Kind::String}), "exact key boundary rejected");
        boundary.load("{}");
        std::string depth = "{\"x\":" + std::string(511, '[') + '0' + std::string(511, ']') + '}';
        boundary.load(depth); check(boundary.document.node_count() == 513, "512-container nesting rejected");
        rejected([&] { boundary.load("{\"x\":" + std::string(512, '[') + '0' + std::string(512, ']') + '}'); }, "513-container nesting accepted");
        boundary.load("{}");
        const std::string raw_boundary = "{\"s\":\"" + std::string(kMaximumRawBytes - 8, 'x') + "\"}";
        check(raw_boundary.size() == kMaximumRawBytes, "raw fixture is not exact boundary");
        check(boundary.apply_raw(boundary.document.root_id(), raw_boundary), "4 MiB raw boundary rejected");
        const auto raw_committed = boundary.encoded();
        rejected([&] { boundary.apply_raw(boundary.document.root_id(), raw_boundary + ' '); }, "4 MiB plus one accepted");
        check(boundary.encoded() == raw_committed, "raw overflow mutated document");
        const std::string file_boundary = "{\"s\":\"" + std::string(kMaximumFileBytes - 8, 'x') + "\"}";
        boundary.load(file_boundary); check(boundary.encoded().size() == kMaximumFileBytes, "16 MiB final boundary rejected");
        const auto file_clean = boundary.encoded();
        rejected([&] { boundary.mutate([](jsondict::Document& doc) { doc.set_trailing_newline(true); return true; }); }, "final newline escaped file byte limit");
        check(boundary.encoded() == file_clean && !boundary.dirty, "file overflow partially committed");
        boundary.load("{\"s\":\"\"}");
        const auto small = boundary.encoded();
        rejected([&] { boundary.mutate([&](jsondict::Document& doc) { return doc.set_string(doc.root().as_object()[0].value.id(), std::string(3 * 1024 * 1024, '\0')); }); }, "escaped output size limit missing");
        check(boundary.encoded() == small && !boundary.dirty, "escaped overflow partially committed");
        check(jsondict::parse("[" + std::string("0,") + "0]").child_count() == 2, "core parser changed");
        std::string parser_boundary = "[";
        for (std::size_t i = 0; i < 249999; ++i) { if (i) parser_boundary += ','; parser_boundary += '0'; }
        parser_boundary += ']';
        check(jsondict::parse(parser_boundary).child_count() == 249999, "250,000 core nodes rejected");
        parser_boundary.insert(parser_boundary.size() - 1, ",0");
        rejected([&] { jsondict::parse(parser_boundary); }, "250,001 core nodes accepted");
        std::cout << "LINUX_SESSION_OK: " << assertions << " assertions\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "LINUX_SESSION_FAILED: " << error.what() << '\n';
        return 1;
    }
}
