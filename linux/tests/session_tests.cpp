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
        std::cout << "LINUX_SESSION_OK: " << assertions << " assertions\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "LINUX_SESSION_FAILED: " << error.what() << '\n';
        return 1;
    }
}
