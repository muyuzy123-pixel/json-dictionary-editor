#pragma once

#include "json_core.hpp"

#include <string>

namespace jsondict {

// Type aliases are language-independent search terms. Empty-container terms
// describe the value only when it has no children.
inline std::wstring search_aliases(const Node& node) {
    switch (node.kind()) {
        case Kind::String: return L"字符串 string text";
        case Kind::Number: return L"数字 number";
        case Kind::Boolean: return L"布尔值 boolean bool";
        case Kind::Null: return L"null";
        case Kind::Object:
            return std::wstring(L"对象 object 键 key keys") +
                (node.child_count() == 0 ? L" 空对象 empty object" : L"");
        case Kind::Array:
            return std::wstring(L"数组 array 元素 element elements") +
                (node.child_count() == 0 ? L" 空数组 empty array" : L"");
    }
    return {};
}

}  // namespace jsondict
