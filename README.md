# JSON 字典编辑器

原生图形化 JSON 对象编辑器，包含独立的 macOS SwiftUI/AppKit 和 Windows Win32/C++ 实现。树形浏览与编辑六种 JSON 类型，保留对象键顺序和数字原始文本，拒绝重复键及非法 JSON。应用在本机处理文件，不依赖在线服务。

项目自有内容采用 [MIT](LICENSE)，Copyright © 2026 **muyuzy123-pixel**。第三方组件适用各自许可，见 [许可说明](docs/licensing.md) 和 [第三方声明](THIRD_PARTY_NOTICES.txt)。

## 首次预览 `preview-1`

`preview-1` 提供以下下载包；当前发布状态、实际验收及已知限制见 [发布记录](docs/PREVIEW_RELEASE.md)。

| 下载 | 应用版本 | 平台与分发状态 |
|---|---|---|
| [macOS Universal ZIP](https://github.com/muyuzy123-pixel/json-dictionary-editor/releases/download/preview-1/JSONDictionaryEditor-macOS-Universal.zip) | 1.0.0 (1) | Apple 芯片/Intel；部署目标 macOS 13+；ad-hoc 签名，未公证 |
| [Windows x64 ZIP](https://github.com/muyuzy123-pixel/json-dictionary-editor/releases/download/preview-1/JSONDictionaryEditor-Windows-x64.zip) | 1.0.1 | Windows 10/11 x64 为目标；无 Authenticode 签名 |
| [Windows ARM64 ZIP](https://github.com/muyuzy123-pixel/json-dictionary-editor/releases/download/preview-1/JSONDictionaryEditor-Windows-arm64.zip) | 1.0.1 | Windows on ARM 为目标；无 Authenticode 签名，实机未验收 |
| [SHA256SUMS.txt](https://github.com/muyuzy123-pixel/json-dictionary-editor/releases/download/preview-1/SHA256SUMS.txt) | — | 上述三个 ZIP 的 SHA-256 |

解压到可写目录，打开对应应用，再选择 JSON 文件或新建对象。首次使用建议在副本上操作。macOS 未使用 Developer ID 或 Apple 公证，可能触发系统安全提示；先核对来源和摘要，再按照系统“隐私与安全性”的提示决定是否打开，不需要关闭全局安全检查。Windows 可能显示未知发布者或 SmartScreen 提示；摘要证明下载内容一致，不保证系统信誉。

macOS 可用 `shasum -a 256 下载文件.zip`，Windows 可用 `Get-FileHash -Algorithm SHA256 -LiteralPath .\下载文件.zip`，将结果与清单对应项比较。包内 `SOURCE.json` 记录构建源码提交、预览标签和应用版本；不要混用历史候选的产物摘要。

## 使用和数据边界

- 支持字符串、文本数字、布尔值、null、对象和数组；编辑器文档根必须是对象。
- 支持增删、复制、移动、键名修改、排序、搜索、原始 JSON 编辑及四种输出格式。
- 数字保留词法文本，避免转换成浮点数造成精度损失。
- 两端在 Unicode 规范等价键、BOM、CRLF 格式识别、显式排序及容量限制方面存在差异，见 [共享规则](docs/data-rules.md) 和 [平台差异](docs/platform-differences.md)。
- 保存方式和操作细节见 [Mac 说明](macos/README.md) 与 [Windows 说明](windows/README.md)。

## 验证范围

最终 Mac 包的签名、两架构诊断及正常输入/保存/重开局部流程通过；Windows x64 最终 ZIP 已通过 Windows CI 下载校验、诊断和内置文件自测。详见[发布记录](docs/PREVIEW_RELEASE.md)。

已知异常：一次辅助功能直接赋值后立即保存的自动化尝试曾挂起，根因未明、未确认修复，该路径未计为通过。后续正常输入流程的局部成功不代表所有保存路径均已验收。

**Windows ARM64 实机、Windows 图标与 1.0.1 分隔条修复后视觉、输入法、DPI 和完整 GUI 验收仍未完成。** Windows CI 运行环境不是 Windows 10/11 的完整桌面验收。macOS 最低系统版本和真实 Intel 设备也不能从一次 Universal 构建推导为已通过。

首次本地候选曾通过 x86_64/Rosetta Mac 核心自检、C++ 普通与 sanitizer 各 82 项断言，以及共享样例 70 项检查；历史摘要见 [验证记录](docs/verification.md)，不把旧记录当作新下载包的检查。

## 从源码构建

仓库根直接包含 `macos/`、`windows/`、`tests/`。两套解析器保持独立，没有统一重写或新增产品功能。

- [Mac 无签名构建](macos/README.md)：通过 `xcrun` 使用已安装 SDK；与预览包签名步骤分开。
- [Windows 构建](windows/README.md)：原生 Visual Studio/CMake 或显式指定 LLVM-MinGW 交叉构建。
- [共享测试](tests/README.md)：使用现有实现和合成样例。
- [预览打包与核验](docs/RELEASING.md)：源码提交、第三方声明、签名和下载包检查。
- [图标来源和生成](docs/icon-generation.md)：Windows 普通构建直接读取仓库 ICO，不要求 macOS；Mac 固定图标资源保留历史字节。

仓库不包含构建缓存、工具链、私人数据、原始桌面录像或完整会话日志。当前候选文件与摘要见 [清单](docs/file-manifest.tsv) 和 [SHA256SUMS](SHA256SUMS)。
