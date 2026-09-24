# JSON 字典编辑器 · macOS

原生 SwiftUI/AppKit 实现，版本 **1.0.0 (1)**，目标为 macOS 13 或更高版本。MIT，Copyright © 2026 muyuzy123-pixel。

## 使用预览包

1. 在[发布页](https://github.com/muyuzy123-pixel/json-dictionary-editor/releases/tag/preview-1)下载 Universal ZIP 与校验清单，复算 ZIP SHA-256。
2. 解压后可将 `JSON字典编辑器.app` 放到“应用程序”，或先从解压目录运行。
3. 选择 JSON 文件或按 `⌘N` 新建对象；首次操作建议使用 `SampleDictionary.json` 的副本。
4. 在树中选中条目，修改类型、键名和值；通过系统菜单保存或另存为。键名和数字先校验再应用；字符串编辑行为不同于 Windows 检查器的整体草稿决议。

下载包为 **ad-hoc 签名、未公证** 的预览版，不含 Developer ID 签名。macOS 可能阻止首次打开；先确认来源和校验值，再按系统“隐私与安全性”中的提示决定是否允许，不需要关闭全局 Gatekeeper。严格签名校验通过只证明包内完整性，不表示 Apple 公证或 Gatekeeper 信誉。

`SOURCE.json` 记录源码提交、预览标签、版本和架构。最新实际验证结果见[发布记录](https://github.com/muyuzy123-pixel/json-dictionary-editor/blob/main/docs/PREVIEW_RELEASE.md)。最低 macOS 版本、真实 Intel 设备及完整 GUI 的全部场景仍未完成验收；Universal、CLI 和局部编辑保存检查不等于这些全面结论。

## 数据规则与功能

文档根必须为对象。支持六种 JSON 类型、树形浏览、搜索、增删/复制/移动/排序、原始 JSON 编辑和四种格式；数字保留原始文本，对象保留成员顺序。拒绝重复键、非法 UTF-8/Unicode 与非法 JSON。

Mac 与 Windows 在 Unicode 等价键、BOM、CRLF、排序和容量限制方面并非完全相同，见[共享数据规则](https://github.com/muyuzy123-pixel/json-dictionary-editor/blob/main/docs/data-rules.md)。保存采用系统文档接口；不要将 Windows 自定义保存流程的验证套用到 Mac。

## 从源码构建（无签名）

安装提供 Swift 编译器和 macOS SDK 的 Xcode 或 Command Line Tools；通过 `xcrun` 发现工具，可用 `DEVELOPER_DIR` 或 `MACOS_SDK_PATH` 选择已安装版本。从仓库根运行：

```sh
BUILD_DIR=/tmp/json-editor-build ./macos/scripts/build.sh
BUILD_DIR=/tmp/json-editor-build ./macos/scripts/verify.sh
```

默认输出为仓库 `build/macos/`；已有完整构建目录不会被覆盖。编译采用 Swift 5 语言模式、arm64/x86_64 两架构、显式禁用 linker ad-hoc 签名。构建本身不要求 Rosetta；验证 x86_64 在 Apple 芯片上需要已有 Rosetta，脚本不安装它。`Package.swift` 保留，但本次发行采用直接 swiftc 路径。

普通构建复制固定 `Resources/AppIcon.icns`，保留历史图标字节；原 `IconGenerator.swift` 和 `--generate-icon` 维护入口不改。生成器调用系统字体和 AppKit，不分发字体文件。

构建信息、输入及输出摘要写入构建目录；工具文件摘要不等于整个 SDK 摘要，不承诺跨主机逐字节复现。原始日志可能包含本机路径，不应直接上传。

## 维护者预览打包

预览归档需要 Python 3 标准库，用于正确写入 UTF-8 文件名。签名通过独立 `macos/scripts/package_preview.sh` 进行，只处理暂存副本，不改变无签名构建入口。需要候选自身干净 Git 仓库、显式源码提交、已通过的无签名构建，以及 Apple 芯片+Rosetta 用于最终双架构诊断；具体参数见[发布流程](https://github.com/muyuzy123-pixel/json-dictionary-editor/blob/main/docs/RELEASING.md)。

程序也提供 `--version`、`--self-test`、`--validate-json FILE`。合法数据返回 0，数据错误返回 1，缺少校验参数返回 2；未知参数沿用 GUI 启动行为。
