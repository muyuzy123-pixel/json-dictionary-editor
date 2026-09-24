# 验证记录与证据边界

当前预览下载包以 [PREVIEW_RELEASE.md](PREVIEW_RELEASE.md) 的实际记录为准。这里保留本地候选的历史摘要；完整原始记录留在未上传的本地审计目录，未将旧版权字段或旧产物摘要改写为新的检查结果。

## 2026-09-20：首次源码候选

- Mac 无签名 arm64/x86_64 构建和 Universal 合并通过；x86_64 经 Rosetta 自检及 13 节点样例通过；未运行 arm64 或候选 GUI。
- C++ 核心普通与 ASan/UBSan 各 82 项断言通过；失败传播负例通过。
- Windows x64/ARM64 交叉编译与 PE 静态检查通过，未运行 Windows EXE。
- 共享样例共 70 项检查通过，明确保留 Unicode 规范等价键和 CRLF 等平台差异。
- 原历史交付 52 个文件内容与导入前基线一致。

摘要见 [build-evidence.json](build-evidence.json)。这些核心记录不能代替后续最终包检查。

## 2026-09-20：Windows 图标替换

七档 PNG-in-ICO（16–256 像素）的 CRC、RGBA、透明度和边界通过；重复生成字节一致。双架构 EXE 的嵌入图标逐帧匹配新 ICO。未运行 Windows 图标/分隔条视觉验收。

图标 SHA-256 为 `e635dbe42f45fb5321707439e05db95aae9c02f11c595ee73dd9873353cccae9`，本轮署名调整不重新生成它。摘要见 [icon-update-evidence.json](icon-update-evidence.json)。

## 2026-09-25：公开预览准备

统一公开署名，Mac 使用既有固定 ICNS，更新版权资源并加入独立签名/打包步骤。应用版本、解析器、图标设计及 Windows 1.0.1 分隔条实现不变。

最终包的签名、资源、归档、Mac 诊断与局部 GUI、Windows x64 CI 及匿名下载结果，逐项记入[发布记录](PREVIEW_RELEASE.md)。工具链、构建输入、实际源码提交和产物 SHA-256 分别留存，不把旧证据换名后充作新证据。

## 持续未验证范围

Windows ARM64 实机；Windows 图标、1.0.1 分隔条修复后视觉、输入法、DPI/多显示器和完整 GUI；macOS 最低系统与真正 Intel 设备的完整验收。Windows CI 的自动化诊断不是 Windows 10/11 桌面操作验收。

最低系统和架构是实现/部署目标，不能仅由成功编译推导为全部已支持并验收。无关代码未变时不重复原始核心测试，最终二进制仍须运行其适用诊断。
