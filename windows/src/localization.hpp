#pragma once

#include <windows.h>
#include <shlobj.h>

#include <string>
#include <string_view>
#include <unordered_map>

namespace app_l10n {

enum class Preference { System, Chinese, English };

inline Preference preference = Preference::System;
inline bool english = false;

inline const std::unordered_map<std::wstring_view, std::wstring_view>& catalog() {
    static const std::unordered_map<std::wstring_view, std::wstring_view> entries{
        {L"JSON 字典编辑器", L"JSON Dictionary Editor"},
        {L"未知系统错误", L"Unknown system error"},
        {L"未知错误", L"Unknown error"},
        {L"字符串", L"String"}, {L"数字", L"Number"},
        {L"布尔值", L"Boolean"}, {L"对象", L"Object"},
        {L"数组", L"Array"}, {L"未知", L"Unknown"},
        {L"空对象", L"Empty Object"}, {L"空数组", L"Empty Array"},
        {L"新建(&N)\tCtrl+N", L"New(&N)\tCtrl+N"},
        {L"打开(&O)…\tCtrl+O", L"Open(&O)…\tCtrl+O"},
        {L"保存(&S)\tCtrl+S", L"Save(&S)\tCtrl+S"},
        {L"另存为(&A)…\tCtrl+Shift+S", L"Save As(&A)…\tCtrl+Shift+S"},
        {L"退出(&X)", L"Exit(&X)"},
        {L"文件(&F)", L"File(&F)"},
        {L"搜索(&F)\tCtrl+F", L"Find(&F)\tCtrl+F"},
        {L"编辑(&E)", L"Edit(&E)"},
        {L"添加(&A)", L"Add(&A)"},
        {L"复制(&D)\tCtrl+D", L"Duplicate(&D)\tCtrl+D"},
        {L"删除\tDelete", L"Delete\tDelete"},
        {L"上移\tAlt+↑", L"Move Up\tAlt+↑"},
        {L"下移\tAlt+↓", L"Move Down\tAlt+↓"},
        {L"按键名排序", L"Sort by Key"},
        {L"重命名键\tF2", L"Rename Key\tF2"},
        {L"编辑原始 JSON…\tCtrl+E", L"Edit Raw JSON…\tCtrl+E"},
        {L"节点(&N)", L"Node(&N)"},
        {L"全部展开", L"Expand All"}, {L"全部折叠", L"Collapse All"},
        {L"查看(&V)", L"View(&V)"},
        {L"关于 JSON 字典编辑器", L"About JSON Dictionary Editor"},
        {L"帮助(&H)", L"Help(&H)"},
        {L"跟随系统 / System", L"跟随系统 / System"},
        {L"简体中文", L"简体中文"},
        {L"语言 / Language", L"语言 / Language"},
        {L"语言设置", L"Language Preference"},
        {L"添加 ▾", L"Add ▾"}, {L"复制", L"Duplicate"},
        {L"删除", L"Delete"}, {L"上移", L"Move Up"},
        {L"下移", L"Move Down"}, {L"排序", L"Sort"},
        {L"原始 JSON", L"Raw JSON"},
        {L"搜索键名、路径、类型或值", L"Search keys, paths, types, or values"},
        {L"键 / 索引          类型          值", L"Key / Index          Type          Value"},
        {L"调整左右面板宽度", L"Resize left and right panels"},
        {L"根对象", L"Root Object"},
        {L"JSON 路径", L"JSON Path"}, {L"键名", L"Key"},
        {L"应用", L"Apply"}, {L"值类型", L"Value Type"},
        {L"值", L"Value"}, {L"True（真）", L"True"},
        {L"False（假）", L"False"},
        {L"添加子项 ▾", L"Add Child ▾"},
        {L"编辑此节点的原始 JSON…", L"Edit This Node as Raw JSON…"},
        {L"有效 JSON 字典", L"Valid JSON dictionary"},
        {L"2 个空格", L"2 Spaces"}, {L"4 个空格", L"4 Spaces"},
        {L"制表符", L"Tabs"}, {L"紧凑", L"Compact"},
        {L"末尾换行", L"Final Newline"},
        {L"树视图刷新失败（请重试搜索或重新打开）", L"Tree refresh failed (retry search or reopen)"},
        {L"此键名包含控制字符或过长，请使用原始 JSON 编辑器。",
         L"This key contains control characters or is too long; use the raw JSON editor."},
        {L"字符串值", L"String Value"},
        {L"此字符串包含回车/控制字符或过长，请使用原始 JSON 编辑器。",
         L"This string contains control characters or is too long; use the raw JSON editor."},
        {L"数字值", L"Number Value"},
        {L"数字文本过长，请使用原始 JSON 编辑器。",
         L"Number text is too long; use the raw JSON editor."},
        {L"此值为空（null）。Null 与空字符串、数字 0 和 false 不相同。",
         L"This value is null. Null differs from an empty string, zero, or false."},
        {L"同一对象中已经存在此键名。", L"This key already exists in the object."},
        {L"空字符串是合法 JSON 键，但通常不便维护。",
         L"An empty string is a valid JSON key, but is often hard to maintain."},
        {L"键名包含无效的 Unicode 文本。", L"The key contains invalid Unicode text."},
        {L"请输入有效 JSON 数字；不支持前导零、NaN 或 Infinity。",
         L"Enter a valid JSON number; leading zeros, NaN, and Infinity are not allowed."},
        {L"数字只能包含 JSON 数字语法中的 ASCII 字符。",
         L"Numbers may contain only ASCII characters from JSON number syntax."},
        {L"字符串包含无效的 Unicode 文本。", L"The string contains invalid Unicode text."},
        {L"草稿尚未应用；点击“应用”后才会写入文档。",
         L"Draft not applied; click Apply to update the document."},
        {L"未命名.json", L"Untitled.json"},
        {L"JSON 字典包含未保存或未应用的更改。",
         L"The JSON dictionary has unsaved or unapplied changes."},
        {L"更改已应用", L"Changes Applied"},
        {L"更改已应用，但窗口状态刷新失败。请尽快保存。\n\n",
         L"Changes were applied, but the window did not refresh. Save soon.\n\n"},
        {L"更改已应用到文档，但界面刷新失败。请立即保存并重新打开程序。\n\n",
         L"Changes were applied, but the view did not refresh. Save and reopen the app.\n\n"},
        {L"草稿已经应用到文档，但界面刷新失败。请立即保存并重新打开程序。\n\n",
         L"The draft was applied, but the view did not refresh. Save and reopen the app.\n\n"},
        {L"数据已经应用，但主窗口刷新失败。请立即保存并重新打开程序。\n\n",
         L"Data was applied, but the main window did not refresh. Save and reopen the app.\n\n"},
        {L"数据已应用，界面刷新失败", L"Data Applied; View Refresh Failed"},
        {L"文档已达到 50000 个节点的图形编辑安全上限。",
         L"The document has reached the 50,000-node editor safety limit."},
        {L"无法添加节点", L"Cannot Add Node"},
        {L"复制后会超过 50000 个节点的图形编辑安全上限。",
         L"Duplicating would exceed the 50,000-node editor safety limit."},
        {L"无法复制节点", L"Cannot Duplicate Node"},
        {L"所选容器包含子项。删除后，其中的所有内容也会被删除。\n\n确定继续吗？",
         L"The selected container has children. Deleting it also removes all of them.\n\nContinue?"},
        {L"删除所选容器？", L"Delete Selected Container?"},
        {L"更改类型会移除当前对象或数组中的全部子项。\n\n确定继续吗？",
         L"Changing the type removes all children in this object or array.\n\nContinue?"},
        {L"更改类型会移除子项", L"Changing Type Removes Children"},
        {L"无法应用检查器草稿", L"Cannot Apply Inspector Draft"},
        {L"文档在应用草稿前发生变化；草稿没有提交。",
         L"The document changed before the draft was applied; nothing was committed."},
        {L"检查器中有尚未应用的键名或值。\n\n选择“是”应用草稿，选择“否”放弃草稿，选择“取消”返回编辑。",
         L"The inspector has an unapplied key or value.\n\nChoose Apply, Discard, or Cancel to keep editing."},
        {L"处理尚未应用的草稿", L"Resolve Unapplied Draft"},
        {L"当前 JSON 字典包含尚未保存的更改。\n\n是否现在保存？",
         L"The current JSON dictionary has unsaved changes.\n\nSave now?"},
        {L"保存更改？", L"Save Changes?"},
        {L"无法显示打开对话框", L"Cannot Show Open Dialog"},
        {L"文档过大", L"Document Too Large"},
        {L"磁盘上的文件自打开或上次保存后已被其他程序修改、替换或删除。\n\n"
         L"选择“是”覆盖当前磁盘版本；选择“否”改为另存为；选择“取消”停止保存。",
         L"The file on disk was changed, replaced, or deleted by another program.\n\n"
         L"Choose Overwrite, Save As, or Cancel."},
        {L"程序无法确认当前路径是否仍与上次保存后一致。\n\n"
         L"选择“是”以此刻的磁盘文件为基准覆盖；选择“否”改为另存为；选择“取消”停止保存。",
         L"The program cannot confirm that the target matches the previous save.\n\n"
         L"Choose Overwrite using the current file, Save As, or Cancel."},
        {L"程序已完成原子写入，但随后无法确认磁盘内容仍与当前文档一致。"
         L"当前文档仍保持“未保存”状态，程序不会继续关闭或新建。\n\n",
         L"The file was written, but the program could not verify its contents. "
         L"The document remains unsaved and the program will not close or create another.\n\n"},
        {L"按当前格式写出的 UTF-8 文件会超过 16 MiB。请减少内容或改用紧凑格式。",
         L"The UTF-8 file would exceed 16 MiB. Reduce its contents or use compact formatting."},
        {L"无法打开 JSON 字典", L"Cannot Open JSON Dictionary"},
        {L"树视图尚未完整刷新，为避免操作到不可见节点，节点修改已暂时停用。"
         L"请在搜索框中重试搜索，或重新打开文件。",
         L"The tree did not finish refreshing, so node editing is disabled. "
         L"Retry the search or reopen the file."},
        {L"检测到外部文件更改", L"External File Change Detected"},
        {L"无法确认保存基准", L"Cannot Verify Save Baseline"},
        {L"当前文档仍保持“未保存”状态，程序不会继续关闭或新建。\n\n",
         L"The document remains unsaved; the program will not close or create a new document.\n\n"},
        {L"保存后验证失败", L"Post-save Verification Failed"},
        {L"保存失败", L"Save Failed"},
        {L"无法显示另存为对话框", L"Cannot Show Save As Dialog"},
        {L"无法检查另存为目标", L"Cannot Check Save As Target"},
        {L"添加子项", L"Add Child"}, {L"添加同级项", L"Add Sibling"},
        {L"编辑原始 JSON…", L"Edit Raw JSON…"},
        {L"树视图尚未完整刷新，为避免操作到不可见节点，节点修改已暂时停用。\n",
         L"The tree did not finish refreshing, so editing hidden nodes is disabled.\n"},
        {L"请在搜索框中重试搜索，或重新打开文件。",
         L"Retry the search or reopen the file."},
        {L"树视图不可用", L"Tree Unavailable"},
        {L"JSON 字典编辑器 Windows 版\n\n原生 Win32 图形化编辑器。文件仅在本机以 UTF-8 处理，不上传网络。",
         L"JSON Dictionary Editor for Windows\n\nNative Win32 editor. Files stay on this device as UTF-8; nothing is uploaded."},
        {L"关于", L"About"},
        {L"无法创建应用界面", L"Cannot Create Application Interface"},
        {L"操作失败", L"Operation Failed"},
        {L"树操作失败", L"Tree Operation Failed"},
        {L"搜索失败", L"Search Failed"},
        {L"树刷新失败", L"Tree Refresh Failed"},
        {L"DPI 布局更新失败", L"DPI Layout Update Failed"},
        {L"请一次只拖入一个 JSON 文件。", L"Drop one JSON file at a time."},
        {L"无法打开多个文件", L"Cannot Open Multiple Files"},
        {L"无法创建原始 JSON 编辑器", L"Cannot Create Raw JSON Editor"},
        {L"内容已更改，尚未应用", L"Content changed; not applied"},
        {L"原始 JSON 编辑器包含未应用的草稿。", L"The raw JSON editor has an unapplied draft."},
        {L"原始 JSON 操作失败", L"Raw JSON Operation Failed"},
        {L"原始 JSON 草稿尚未应用。关闭窗口会放弃这些更改。\n\n确定关闭吗？",
         L"The raw JSON draft has not been applied. Closing will discard it.\n\nClose?"},
        {L"放弃原始 JSON 草稿？", L"Discard Raw JSON Draft?"},
        {L"尚未检查", L"Not checked yet"},
        {L"检查", L"Check"}, {L"格式化", L"Format"}, {L"取消", L"Cancel"},
        {L"正在编辑完整 JSON 字典；根节点必须是对象。",
         L"Editing the entire JSON dictionary; the root must be an object."},
        {L"正在编辑所选节点；应用前会完整验证。",
         L"Editing the selected node; it will be fully validated before applying."},
        {L"✓ 原草稿已从内存恢复，尚未应用", L"✓ Original draft restored from memory; not applied"},
        {L"原始 JSON 编辑器在内存中保留着待恢复草稿。",
         L"The raw JSON editor holds a draft in memory awaiting recovery."},
        {L"✓ 已格式化并通过检查", L"✓ Formatted and validated"},
        {L"错误：目标节点已不存在", L"Error: target node no longer exists"},
        {L"错误：应用后会超过 50000 个节点的图形编辑安全上限",
         L"Error: applying would exceed the 50,000-node editor safety limit"},
        {L"错误：替换会违反根对象、重复键或 512 层嵌套限制",
         L"Error: replacement violates the object root, unique keys, or 512-level depth limit"},
        {L"错误：应用后会超过图形编辑安全上限",
         L"Error: applying would exceed the editor safety limit"},
        {L"数据已应用，界面刷新失败", L"Data Applied; View Refresh Failed"},
        {L"无法注册原始 JSON 编辑器窗口。", L"Cannot register the raw JSON editor window."},
        {L"错误", L"Error"},
        {L"编辑完整 JSON 字典", L"Edit Entire JSON Dictionary"},
        {L"编辑所选节点的原始 JSON", L"Edit Selected Node as Raw JSON"},
        {L"无法创建原始 JSON 编辑器窗口。", L"Cannot create the raw JSON editor window."},
        {L"无法创建 JSON 字典编辑器主窗口。", L"Cannot create the main JSON Dictionary Editor window."},
        {L"启动失败", L"Startup Failed"},
        {L"读取文件身份失败", L"Cannot read file identity"},
        {L"读取文件失败", L"Cannot read file"},
        {L"检查文件外部更改失败", L"Cannot check external file changes"},
        {L"打开文件失败", L"Cannot open file"},
        {L"清理临时文件失败", L"Cannot clean temporary file"},
        {L"创建安全保存临时文件失败", L"Cannot create safe-save temporary file"},
        {L"写入临时文件失败", L"Cannot write temporary file"},
        {L"刷新临时文件失败", L"Cannot flush temporary file"},
        {L"原子替换目标文件失败", L"Cannot atomically replace target file"},
        {L"原子创建目标文件失败", L"Cannot atomically create target file"},
        {L"创建界面控件失败", L"Cannot create interface control"},
        {L"创建 DPI 字体失败", L"Cannot create DPI font"},
        {L"创建原始 JSON 编辑器失败", L"Cannot create raw JSON editor"},
        {L"创建原始 JSON 编辑器字体失败", L"Cannot create raw JSON editor font"},
        {L"文件超过 16 MiB 的安全编辑上限", L"File exceeds the 16 MiB editor safety limit"},
        {L"读取文件时意外到达末尾", L"Unexpected end of file while reading"},
        {L"文件在检查过程中被其他程序修改，请重试",
         L"Another program changed the file during verification; retry."},
        {L"检测到 UTF-16 文件；当前版本仅支持 UTF-8 JSON",
         L"UTF-16 file detected; only UTF-8 JSON is supported."},
        {L"文件在读取过程中被其他程序修改，请重试",
         L"Another program changed the file while reading; retry."},
        {L"无法为安全保存创建唯一临时文件",
         L"Cannot create a unique safe-save temporary file"},
        {L"写入临时文件时发生短写", L"Temporary file write was incomplete"},
        {L"无法初始化面板分隔条", L"Cannot initialize pane divider"},
        {L"无法创建树节点；系统内存或控件资源不足",
         L"Cannot create tree node; memory or control resources are exhausted"},
        {L"树视图无法同步当前选中节点", L"Tree selection could not be synchronized"},
        {L"树视图没有可选中的根节点", L"Tree has no selectable root node"},
        {L"树节点文字刷新失败", L"Cannot refresh tree node text"},
        {L"同一对象中已经存在此键名", L"This key already exists in the object"},
        {L"请输入有效 JSON 数字；不支持前导零、NaN 或 Infinity",
         L"Enter a valid JSON number; leading zeros, NaN, and Infinity are not allowed"},
        {L"原始 JSON 文本无法完整载入编辑控件",
         L"Raw JSON text could not be loaded completely"},
        {L"所选 JSON 超过 4 MiB 的原始文本编辑安全上限",
         L"Selected JSON exceeds the 4 MiB raw-editor safety limit"},
        {L"原草稿正在程序内存中等待恢复；请先点击“格式化”恢复原文本",
         L"A draft is waiting in memory; click Format to restore the original text first"},
        {L"原始 JSON 草稿超过 4 MiB 的 UTF-8 安全上限",
         L"Raw JSON draft exceeds the 4 MiB UTF-8 safety limit"},
        {L"格式化结果超过 4 MiB 的 UTF-8 原始编辑安全上限",
         L"Formatted output exceeds the 4 MiB raw-editor UTF-8 safety limit"},
        {L"格式化结果超过原始编辑器的字符安全上限",
         L"Formatted output exceeds the raw-editor character limit"},
        {L"编辑控件无法完整装载格式化结果；原草稿已保留",
         L"The edit control could not load the formatted output; the original draft was kept"},
        {L"格式化后的 UTF-8 文件超过 16 MiB；请减少内容或改用紧凑格式",
         L"Formatted UTF-8 file exceeds 16 MiB; reduce content or use compact formatting"},
        {L"目标文件在原子替换后立即被删除或改写",
         L"Target file was deleted or modified immediately after atomic replacement"},
        {L"目标文件在保存过程中又被其他程序修改；原文件没有被覆盖",
         L"Another program changed the file during save; the original was not overwritten"},
        {L"目标文件在最终替换前又被其他程序修改；程序已恢复该外部版本，未覆盖它",
         L"Another program changed the target before replacement; its version was restored"},
        {L"新目标内容与本次写入的 SHA-256 不一致",
         L"Target SHA-256 differs from the content written this time"},
        {L"原草稿仍保留在程序内存中，但编辑控件暂时无法恢复；请释放系统内存后再次点击“格式化”",
         L"The draft is still in memory, but the editor cannot restore it yet; free memory and click Format again"},
        {L"编辑控件无法恢复原草稿；原文本仍保留在程序内存中。请释放系统内存后再次点击“格式化”恢复",
         L"The editor cannot restore the draft; the text remains in memory. Free memory and click Format again"},
    };
    return entries;
}

inline bool resolves_to_english(Preference choice, LANGID system_language) {
    if (choice == Preference::Chinese) return false;
    if (choice == Preference::English) return true;
    return PRIMARYLANGID(system_language) != LANG_CHINESE;
}

inline void resolve() {
    english = resolves_to_english(preference, GetUserDefaultUILanguage());
}

inline std::wstring text(std::wstring_view key) {
    if (!english) {
        if (key == L"检查器中有尚未应用的键名或值。\n\n"
                   L"选择“是”应用草稿，选择“否”放弃草稿，选择“取消”返回编辑。") {
            return L"检查器中有尚未应用的键名或值。\n\n"
                   L"选择“应用”提交草稿，“放弃”丢弃草稿，或“取消”返回编辑。";
        }
        if (key == L"磁盘上的文件自打开或上次保存后已被其他程序修改、替换或删除。\n\n"
                   L"选择“是”覆盖当前磁盘版本；选择“否”改为另存为；选择“取消”停止保存。") {
            return L"磁盘上的文件自打开或上次保存后已被其他程序修改、替换或删除。\n\n"
                   L"选择“覆盖”“另存为”或“取消”。";
        }
        if (key == L"程序无法确认当前路径是否仍与上次保存后一致。\n\n"
                   L"选择“是”以此刻的磁盘文件为基准覆盖；选择“否”改为另存为；选择“取消”停止保存。") {
            return L"程序无法确认当前路径是否仍与上次保存后一致。\n\n"
                   L"选择“覆盖”“另存为”或“取消”。";
        }
        return std::wstring(key);
    }
    const auto entry = catalog().find(key);
    return entry == catalog().end() ? std::wstring(key) : std::wstring(entry->second);
}

inline std::wstring settings_path() {
    PWSTR local = nullptr;
    const HRESULT result_code = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local);
    if (FAILED(result_code) || !local) {
        if (local) CoTaskMemFree(local);
        return {};
    }
    std::wstring result(local);
    CoTaskMemFree(local);
    return result + L"\\JSONDictionaryEditor\\settings.ini";
}

inline void load() {
    const std::wstring path = settings_path();
    if (!path.empty()) {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            char bytes[32]{};
            DWORD length = 0;
            if (ReadFile(file, bytes, sizeof(bytes) - 1, &length, nullptr)) {
                const std::string_view value(bytes, length);
                if (value == "zh-Hans") preference = Preference::Chinese;
                else if (value == "en") preference = Preference::English;
            }
            CloseHandle(file);
        }
    }
    resolve();
}

inline bool save(Preference selected) {
    preference = selected;
    resolve();
    const std::wstring path = settings_path();
    if (path.empty()) return false;
    const auto separator = path.find_last_of(L'\\');
    const std::wstring directory = path.substr(0, separator);
    if (!CreateDirectoryW(directory.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) return false;
    const std::wstring temporary = path + L".tmp." + std::to_wstring(GetCurrentProcessId());
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    const char* value = selected == Preference::Chinese ? "zh-Hans" :
                        selected == Preference::English ? "en" : "system";
    const DWORD size = static_cast<DWORD>(std::char_traits<char>::length(value));
    DWORD written = 0;
    const bool okay = WriteFile(file, value, size, &written, nullptr) && written == size &&
                      FlushFileBuffers(file);
    CloseHandle(file);
    if (!okay || !MoveFileExW(temporary.c_str(), path.c_str(),
                              MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

}  // namespace app_l10n
