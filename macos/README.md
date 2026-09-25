# JSON 字典编辑器 · macOS 1.1.1

原生 SwiftUI/AppKit 实现，版本 **1.1.1 (3)**，目标为 macOS 13 或更高版本。MIT，Copyright © 2026 muyuzy123-pixel。本目录是基于现有公开仓库的 1.1.1 本地候选；[preview-1](https://github.com/muyuzy123-pixel/json-dictionary-editor/releases/tag/preview-1) 仍是历史 1.0.0 (1) 下载，不代表此候选已经发布。

1.1.0 加入简体中文、英语与跟随系统的即时界面切换，选择会记忆。1.1.1 修复非空对象/数组误命中“空对象/空数组”搜索，强化中文翻译确实加载的自检，示例改用英文键名。这次没有改语言切换行为。详见 [English guide](README.en.md) 与 [版本记录](CHANGELOG.md)。

## 使用

打开 `.json` 或按 `⌘N` 新建空对象。左侧树选择节点，右侧修改键名、类型和值；键名和数字草稿需要应用，原始 JSON 编辑先检查再提交。搜索、复制、移动、排序和四种保存格式保持原有操作。文档根必须为对象；数字保留原文，非法 UTF-8、重复键与无效 JSON 会被拒绝。语言切换不会保存文档或提交编辑草稿。系统文件窗口的语言由 macOS 决定。

Mac 与 Windows 在 Unicode 等价键、BOM、CRLF、排序和容量限制方面存在已记录差异，见[共享数据规则](../docs/data-rules.md)。Mac 保存使用 SwiftUI 的系统文档接口。

## 从仓库源码构建

安装提供 Swift 编译器、macOS SDK 的 Xcode 或 Command Line Tools。仓库以直接 `swiftc` 分别编译 arm64/x86_64，`Package.swift` 仍可用于 SwiftPM；普通构建通过 `-D DIRECT_SWIFTC_BUILD` 找到随包语言目录。`xcrun` 发现当前工具链，可用 `DEVELOPER_DIR` 或 `MACOS_SDK_PATH` 选择已安装版本。

在仓库根运行：

```sh
BUILD_DIR=/tmp/json-editor-111-build ./macos/scripts/build.sh
BUILD_DIR=/tmp/json-editor-111-build ./macos/scripts/verify.sh
```

构建先校验双语目录，生成无签名 Universal App，将 `Localizable.strings`、应用名称的 `InfoPlist.strings` 和固定原图标放入 App，并记录所有资源与脚本输入摘要。自检与资源比较通过后，单独的预览打包脚本才在独立暂存副本中执行 ad-hoc 签名；它不改变无签名构建入口。Apple 芯片主机用 Rosetta 验证 x86_64 诊断，脚本不会安装它。源码直编与签名打包的参数见[发布流程](../docs/RELEASING.md)。

Mac 固定图标资源的 SHA-256 仍是 `e0a3b35e53ee2fe28bd95b988e6d9555a4902361c3338051c3667e92245ea53d`；`IconGenerator.swift` 保留为维护工具，不在普通构建中重新渲染。

## 候选验证边界

CLI 提供 `--version`、`--self-test` 和 `--validate-json FILE`。直接运行自检会读取随 App 放入的双语资源，缺少中文条目时应失败。最终候选实际执行结果以[1.1.1 发布记录](../docs/releases/1.1.1.md)为准；编译和局部 GUI 操作均不代表最低系统、真实 Intel 或完整 GUI 的全面验收。

历史 `preview-1` 曾记录一次辅助功能直接赋值后立即保存的挂起，根因未确定。1.1.1 未复现不等于修复；正式版晋升前，正常输入后的保存与重新打开必须通过，旧 AX 路径单独记载。临时测试只编辑合成样例副本。

预览包计划使用 ad-hoc 签名，未经 Developer ID 签名和 Apple 公证。下载后先核对 SHA-256，再按系统安全提示决定是否打开，不关闭全局 Gatekeeper。原始构建日志可能包含本机路径，不直接上传。
