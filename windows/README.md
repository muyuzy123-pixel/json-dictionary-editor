# JSON 字典编辑器 · Windows 1.1.1 本地候选

这是一个原生 Win32 图形化 JSON 字典编辑器。程序用于在树形界面中安全地查看和修改 JSON 对象，同时保留对象键顺序和数字的原始文本；不会把高精度数字先转换为浮点数，也不会把重复键静默覆盖。

1.1.0 加入简体中文、英文及跟随系统的界面切换；1.1.1 修复非空容器误匹配空对象/数组搜索、校验英文文案，并改用英文示例。语言切换行为不变。**分隔条修复已存在于 `preview-1` 的 Windows 1.0.1 中，不是本次新修复。** 其真实 Windows 视觉与交互确认仍未完成。

本页描述尚未远端发布的 1.1.1 本地候选。[preview-1](https://github.com/muyuzy123-pixel/json-dictionary-editor/releases/tag/preview-1) 仍提供历史 Windows 1.0.1 下载。新版实际构建和验收结果见[1.1.1 发布记录](../docs/releases/1.1.1.md)。[English guide](README.en.md) 说明相同产品操作。

源码不含编译产物、缓存、工具链或原始录像。预览 ZIP 提供对应架构程序、合成样例、许可和验证脚本；实际发布状态见[发布记录](https://github.com/muyuzy123-pixel/json-dictionary-editor/blob/main/docs/PREVIEW_RELEASE.md)。Windows 预览程序没有 Authenticode 签名。

## 双语界面

主窗口和原始 JSON 编辑器的“语言 / Language”菜单提供“跟随系统 / System”“简体中文”“English”。首次启动随 Windows 界面语言；手动选择记在当前用户的 `%LOCALAPPDATA%\JSONDictionaryEditor\settings.ini`，切换时不提交检查器草稿、不保存文档、不替换原始 JSON 文本。文件选择窗口随 Windows 语言。英文/中文界面展示的键名、值、路径和诊断内容与实际文件数据严格分开。

## 使用预览包

获得对应版本的 x64 或 ARM64 ZIP 后，先核对 SHA-256，再解压到可写目录，双击 `JSONDictionaryEditor.exe`，通过“文件 → 打开”选择 `SampleDictionary.json` 的副本。版本、架构与源码归属写在 `SOURCE.json`；包内包含中文/英文说明、MIT、第三方声明和对应许可全文。未知发布者/SmartScreen 提示不能由 SHA 校验消除；不要关闭全局安全设置。

## 图标与许可

项目自有内容采用根目录标准 MIT，版权署名为 `muyuzy123-pixel`。本候选的 `resources/app.ico` 已替换为可由项目源码生成的七档资源，沿用 Mac 图标的几何设计；生成器和 ICO 输出均适用项目 MIT。普通 Windows 构建直接使用此 ICO，不要求 macOS、Swift 或 AppKit；本次图标字节保持不变。可选维护步骤及第三方边界见 [图标生成说明](https://github.com/muyuzy123-pixel/json-dictionary-editor/blob/main/docs/icon-generation.md) 与 [许可说明](https://github.com/muyuzy123-pixel/json-dictionary-editor/blob/main/docs/licensing.md)。

## 目标平台与产物

- Win32 GUI 的目标系统为 Windows 10/11，目标架构为 x64、ARM64。两种 PE 已能交叉构建；原生 Visual Studio 构建与完整 GUI 尚未验收；Windows runner 中的最终 x64 诊断结果单独记录。
- LLVM-MinGW 路径生成静态 C++ 运行库的便携 EXE。Visual Studio 路径使用其默认运行库策略，两条路径的依赖不能混为一谈。
- 默认执行权限为 `asInvoker`，不会请求管理员权限。
- 原生 ARM64 构建脚本路径已提供，仍属待目标设备验证；没有 Windows x86 32 位目标。

候选脚本不进行 Authenticode 签名，不建立 SmartScreen 信誉。源码公开、正式二进制分发和完整 GUI 验收是不同事项。

## 主要功能

- 左侧树形浏览 JSON 层级，右侧检查器按类型编辑。
- 支持字符串、文本数字、布尔值、`null`、对象、数组六种 JSON 类型。
- 新增、复制、删除、上移、下移、对象键排序和键名修改。
- 按键名、路径、类型或值摘要搜索。
- 原始 JSON 编辑器支持检查、格式化和事务性应用；解析失败时不修改当前文档。
- 2 空格、4 空格、Tab 和紧凑四种输出格式，可控制文件末尾换行。
- 打开、保存、另存为、命令行路径打开和 `.json` 文件拖放。
- 使用 Windows 系统配色，适配高对比度和 Per-Monitor V2 DPI 布局。
- 检查器修改只是草稿；切换节点、保存或执行结构操作前会明确询问“应用 / 放弃 / 取消”。
- 未保存状态提示；新建、打开和退出前可选择保存、不保存或取消。
- 同目录临时文件、完整写入、磁盘刷新和原子替换；替换时会先保留实际被替换的版本，只在新旧内容均通过指纹核对后删除安全备份。
- 打开和保存后记录文件身份与 SHA-256 内容指纹；检测到外部更改时不会静默覆盖。

## 严格数据规则

- 整个文档的根必须是 JSON 对象，不能是数组或标量。
- 同一对象中的键名必须唯一；转义解码后相同的键也视为重复，例如 `"a"` 与 `"\u0061"`。
- 对象键顺序和数组顺序都会保留。
- 数字以原始词法文本保存，例如 `1.2300e+04` 不会变成 `12300`。
- 严格拒绝 `01`、`+1`、`.5`、`1.`、`NaN`、`Infinity`、尾随逗号、注释和多余内容。
- 文件必须是有效 UTF-8。UTF-8 BOM 会在保存时保留；UTF-16 BOM 会明确拒绝，不会静默转码。
- 单个 UTF-8 文件的图形编辑安全上限为 16 MiB、50000 个节点和 512 层嵌套；单次原始 JSON 编辑上限为 4 MiB。
- 保存不会上传文件或调用网络服务；打开或保存成功后，路径会按 Windows 常规行为加入系统“最近使用的文档”。

## 常用快捷键

| 操作 | 快捷键 |
|---|---|
| 新建 / 打开 / 保存 / 另存为 | `Ctrl+N` / `Ctrl+O` / `Ctrl+S` / `Ctrl+Shift+S` |
| 搜索 | `Ctrl+F` |
| 添加 / 复制 / 删除 | `Insert` / `Ctrl+D` / `Delete` |
| 上移 / 下移 | `Alt+↑` / `Alt+↓` |
| 编辑原始 JSON | `Ctrl+E` |
| 修改键名 | `F2` |
| 应用键名或数字 | `Enter` |
| 应用多行字符串 | `Ctrl+Enter` |
| 关闭窗口 | `Ctrl+W` |

原始 JSON 窗口中可用 `F5` 检查、`Ctrl+Shift+F` 格式化、`Ctrl+Enter` 应用、`Esc` 关闭；存在未应用草稿时，`Esc`、“取消”和窗口关闭按钮都会要求确认。文本框获得焦点时，`Delete` 和 `Backspace` 只编辑文字，不会删除整个节点。

## 命令行自检

在 Windows 上先按下文构建，然后运行：

```powershell
powershell -NoProfile -File .\scripts\verify_on_windows.ps1 `
  -Exe .\build-windows-x64\Release\JSONDictionaryEditor.exe
```

该脚本通过 `Start-Process -Wait -PassThru` 等待 GUI 子系统 EXE，检查 `--version`、`--self-test`、独立 UTF-8 临时样例的 `--validate-json` 退出码，再短暂启动窗口确认进程未立即退出。它会关闭测试窗口并清理临时样例。

如果单独执行诊断，也应明确等待进程并读取它的退出码，避免在 PowerShell 中直接调用 GUI EXE 后误读旧的 `$LASTEXITCODE`：

```powershell
$p = Start-Process -FilePath .\build-windows-x64\Release\JSONDictionaryEditor.exe `
  -ArgumentList '--self-test' -Wait -PassThru
if ($p.ExitCode -ne 0) { throw "Self-test failed: $($p.ExitCode)" }
```

产品诊断成功为 `0`，数据或自测失败为 `1`，命令用法错误为 `2`。`verify_on_windows.ps1` 只证明诊断结果和进程存活，不能证明完整 GUI、输入法、保存流程或分隔条视觉效果通过。集成自测中的 Win32 文件保存与冲突测试必须在 Windows 上运行；macOS 上的可移植核心测试不覆盖它们。

## 真实 Windows 窗口验收清单

交叉编译只能证明 Windows 程序格式和链接结果，不能替代真实 Windows 操作。正式二进制发布前请在对应架构的 Windows 10/11 上完成以下检查：

1. 双击程序，确认主窗口、菜单、图标和中文文字正常，进程不会无响应。
2. 打开 `resources\SampleDictionary.json`，展开对象与数组，确认键顺序和类型正确。
3. 依次新增六种类型；修改键名、字符串、数字和布尔值；复制、移动、排序、删除并保存。
4. 输入 `01`、`NaN` 和重复键，确认界面拒绝应用且现有文档不变。
5. 在原始 JSON 窗口制造语法错误，确认“检查/格式化/应用”均显示错误且不会部分替换。
6. 保存后重新打开，确认 `1.2300e+04`、Unicode、数组顺序、缩进和末尾换行均按设置保留。
7. 修改检查器内容但不点“应用”，分别测试切换节点、保存、删除和退出时的“应用 / 放弃 / 取消”；特别确认取消另存为后不会继续关闭或新建。
8. 把 `.json` 文件拖入窗口，测试带空格和中文的长路径。
9. 用另一个编辑器修改已打开的文件，再保存，确认程序提示外部更改，且“另存为 / 覆盖 / 取消”均按选择执行。
10. 在 100%、150%、200% 缩放，以及系统浅色、深色、高对比度设置下检查布局与可读性。
    检查中间分隔条只显示竖线和中央拖动柄，无竖排文字；拖动、Tab 聚焦后按左右键、窗口缩放和切换焦点后均能正确重绘。
11. 运行 `--self-test` 和 `verify_on_windows.ps1`，记录退出码与 Windows 版本。

## 从源码构建

构建目录可为空，不依赖旧项目的 `work/`、个人主目录或已经生成的 `.res`。构建产物留在忽略目录或通过参数指定的目录；不需下载项目外的源码依赖。工具链由使用者预先安装，脚本不会自行下载或安装。

### Windows + Visual Studio 2022（本候选尚未实机执行）

要求 CMake 3.22+、Visual Studio 2022 的「使用 C++ 的桌面开发」、Windows SDK；ARM64 还需安装相应 C++ ARM64 工具；构建后的测试需在能运行目标架构程序的 Windows 设备上执行。脚本明确选择 `Visual Studio 17 2022`，分别使用独立架构目录：

```powershell
powershell -NoProfile -File .\scripts\build_on_windows.ps1 -Architecture x64
powershell -NoProfile -File .\scripts\build_on_windows.ps1 -Architecture ARM64
```

可加 `-BuildDir C:\build\jsondict-x64` 指定输出。脚本对配置、编译和 CTest 逐项检查退出码，失败返回非零；成功生成 Release EXE 和 `build-info-Release.txt`（CMake、编译器、Windows SDK、架构、编译器/工具/产物 SHA-256）。构建脚本运行 82 项断言的可移植核心测试；完整 Win32 诊断与 GUI 验收按上文单独执行。

### macOS 交叉构建（已执行双架构）

本次实际使用 LLVM-MinGW `20260616` UCRT macOS Universal 归档，Clang `22.1.8`。经过本次读取的归档 SHA-256 为 `2cab02a2e964bd4aae981150a45985d07c657cfa8d244959eb9e2dcc5eedd7b1`；本地完整归档已通过目录读取检查，其摘要与上游 GitHub Release API 所列 digest 一致；这不等于签名验证。

从 [LLVM-MinGW 上游 releases](https://github.com/mstorsjo/llvm-mingw/releases) 获取并自行核验工具链，然后指定提取目录：

```sh
LLVM_MINGW_ROOT=/path/to/llvm-mingw \
LLVM_MINGW_ARCHIVE=/path/to/toolchain.tar.xz \
sh scripts/build_cross_macos.sh
```

`LLVM_MINGW_ROOT` 必填；不再自动回溯历史目录。`LLVM_MINGW_ARCHIVE` 可选，用于记录归档 SHA-256；未提供则明确记为 `not supplied`。默认 `ARCHS="x64 arm64"`，也可只指定其一。`BUILD_DIR`、`DIST_DIR` 可指定空输出目录；默认 `build-cross/`、`dist/`。脚本预检目标编译器、资源编译器和 strip，采用 C++17、UTF-8 资源、Unicode Win32 及警告即错误；任何失败均返回非零。`build-info.txt` 记录编译器版本、目标与工具/产物摘要；区分目标命令包装器与 `clang`、`ld.lld`、`llvm-rc` 底层二进制摘要，完整工具链由归档摘要标识。每次重建应保存新摘要，旧摘要不能代表新产物。

LLVM-MinGW 工具本身不导入仓库；静态链接的运行库代码会进入 EXE；预览包附实际组件声明与原始许可文本，打包入口核对最终链接映射。

### macOS / Linux 可移植核心测试

使用 C++17 编译器与 `shasum`：

```sh
sh scripts/build_core.sh
RUN_SANITIZERS=1 sh scripts/build_core.sh
```

可通过 `CXX` 指定编译器命令或完整路径、`BUILD_DIR` 指定输出目录。macOS 路径为避免链接器隐式 ad-hoc 签名，生成未签名 x86_64 测试程序；Apple Silicon 上需要已有 Rosetta。Linux 路径尚未执行验证。此脚本不构建 Win32 UI。

另提供仅用于 Windows / Linux 的 CMake 核心入口；macOS 请使用上面的无签名 `build_core.sh`，避免 CMake 编译器探测触发隐式签名：

```sh
cmake -S . -B build-core-cmake -DJDE_BUILD_GUI=OFF
cmake --build build-core-cmake
ctest --test-dir build-core-cmake --output-on-failure
```

该 CMake 路径已静态整理，本候选环境没有 CMake，因此本轮未执行。macOS 的本轮验证使用上述无签名的直接编译脚本。

## 源码结构

- `src/json_core.hpp`、`src/json_core.cpp`：有序 JSON 模型、严格解析、验证、写入和树操作。
- `src/windows_app.cpp`：Unicode Win32 主窗口、文件流程、原始 JSON 编辑器和诊断入口。
- `src/core_self_test.cpp`：可在 macOS、Linux 或 Windows 上运行的确定性核心测试。
- `resources/app.rc`、`app.manifest`、`app.ico`：版本、图标、Common Controls v6、DPI、长路径与执行权限声明。
- `CMakeLists.txt`：Visual Studio、clang-cl 或 MinGW 构建入口。
- `scripts/`：交叉构建、Windows 本机构建和 Windows 验收辅助脚本。

## 验证状态与边界

- **1.1.1 本轮已执行**：见[1.1.1 发布记录](../docs/releases/1.1.1.md)；核心、语言静态检查与两架构交叉编译结果均按本次提交列示，不扩大为 GUI 验收。
- **历史记录**：既有交付记录称核心优化与 ASan/UBSan 各通过 82 项断言、双架构 LLVM-MinGW 交叉编译成功。1.0.1 修复目录保有空诊断数组的 Clang 静态分析输出；这属于代码分析记录，不是 Windows 显示结果。
- **未验证**：原生 Visual Studio 构建、Windows ARM64 实机、原生文件对话框、拖放、中文输入法、DPI、多显示器、高对比度、SmartScreen/Defender，以及 1.0.1 分隔条修复后的视觉和交互。1.1.1 最终 x64 包仍需在后续授权上传后由附件 CI 检查 CLI 与内置文件自测；本次本地准备不能借用旧版 CI 通过记录。附件 CI 不调用启动窗口的 `verify_on_windows.ps1`。
- **计划支持、待验收**：Windows 10/11 x64 与 ARM64；Linux 仅有便携核心测试路径，不提供 GUI。

## 当前边界

- 这是免安装便携版，没有自动更新、文件关联、开始菜单项目或卸载器。
- 产物未做 Authenticode 签名，也没有承诺 SmartScreen 信誉。
- 没有网络、云同步、JSON Schema、撤销/重做或多文档标签页。
- 对象排序采用确定的无符号 UTF-8 字节顺序，不做自然语言排序。macOS 现有实现使用本地化自然排序，排序结果可能不同；见共享平台差异表。
