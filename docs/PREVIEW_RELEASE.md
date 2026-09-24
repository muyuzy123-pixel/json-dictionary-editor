# preview-1 发布记录

状态：**已公开的预览版**。2026-09-25 07:48（Asia/Shanghai）完成公开切换，07:49 完成匿名访问与下载复核。

- 仓库：`muyuzy123-pixel/json-dictionary-editor`（ID `1386476275`）。
- 构建源码与标签：`8631981691f74086ce7f517143a1f66b1032cbd3`，`preview-1`。
- Mac 1.0.0 (1)，Windows 1.0.1；署名 `muyuzy123-pixel`，项目自有内容 MIT。
- 三个 ZIP 的 SOURCE.json 均指向以上同一提交。后续发布记录提交不改变标签、构建归属或附件。

## 最终包核验

| 附件 | SHA-256 |
|---|---|
| JSONDictionaryEditor-macOS-Universal.zip | `ba63a5317d8607f400f919088e136c07e45034469efc3e3fbf10d67cf6a999fc` |
| JSONDictionaryEditor-Windows-x64.zip | `c99861579be8d2e6c075e12a75da976713ca4dc780591c2bcc398fd68f6fd2d2` |
| JSONDictionaryEditor-Windows-arm64.zip | `a41c4c7f4ce13aff2673e2d6d99fd2ff48e960f4b525969eceb3c82192244851` |

已检查三个 ZIP 的 CRC、路径、UTF-8 文件名、权限、许可和 SOURCE.json；GitHub 上传摘要匹配，私有阶段下载的四个附件与本地逐字节一致。

Mac：macOS 26.6.2 (25G83) Apple 芯片，Swift 6.3.3、SDK 26.5；最终解压 App 严格 ad-hoc 签名检查及 arm64/x86_64 自检、样例校验通过。未使用 Developer ID 或 Apple 公证，固定历史图标字节保持不变。

Windows：两架构 PE、版本版权、七档嵌入图标和 manifest 通过；实际 Loaded 静态归档及许可装包通过。包含 libc++/libc++abi、libunwind、compiler-rt 和 MinGW 运行库，未加载 winpthreads/winstorecompat；原始 LLVM 与完整 MinGW runtime 声明随包保留。EXE 未做 Authenticode 签名。

## Mac 局部 GUI 与已知异常

从最终 ZIP 解压的 App 完成：启动、打开独立样例副本、聚焦并粘贴中文多行字符串、确认树模型更新、保存、关闭重开。独立 JSON 解析与应用校验确认 13 个节点、其余字段和键顺序保持，中文与换行正确保存。

同一产品代码的一次早期自动化尝试（AX 直接赋值后立即保存）曾挂起，采样显示 NSDocument 序列化信号量等待；测试文件未因此被改写。**根因尚未确认，也未声称已修复；该路径不计为通过。** 后续正常聚焦、粘贴、保存、重开流程通过与这个异常同时保留。原始采样仅保存在本地审计，不将局部成功扩大成全部保存路径或完整 GUI 验收。

## Windows 最终附件 CI

[运行 36073962567](https://github.com/muyuzy123-pixel/json-dictionary-editor/actions/runs/36073962567) 成功。工作流固定标签 `windows-2025`；实际镜像为 `windows-2025-vs2026` / `20260907.229.1`，系统 Microsoft Windows Server 2025 Datacenter 10.0.26100。任务只有 contents:read 权限，没有 checkout、重建、GUI 启动或额外发布动作。

任务下载最终 x64 ZIP，核对指定 SHA-256 和 tag/source_commit 后运行：版本 0、自检 0、合法 JSON 0、重复键 1、缺少参数 2。自检覆盖 Win32 原子文件往返与旧指纹冲突保护。这不是 Windows 10/11 桌面操作验收，也不覆盖 ARM64。

## 仍未验证

Windows ARM64 实机；Windows 图标、1.0.1 分隔条修复后视觉、输入法、DPI、多显示器和完整 GUI；macOS 最低系统与真正 Intel 设备完整验收。Windows CI 和 Mac 局部流程不能代替这些结果。

## 准备过程与保全

打包过程中先后修正元数据复制受拒和 ZIP 中文路径标志；旧 Documents 工具链读取停滞后，改从同一已验证归档提取到本地临时目录。LLD map 缺少归档来源时改用其真实 Loaded 记录并保留 map。失败尝试及原始日志均保留在候选外，最终结果只对应上述提交和附件。应用解析器、功能、版本及图标设计没有改变。

构建与机器可读摘要见 [release-evidence.json](release-evidence.json)。原历史交付和本地审计不会上传；公开前复查 Git 可达对象、noreply 提交身份、附件与 CI 日志。

## 公开后复核

同一仓库 ID `1386476275` 已为 Public，Release 保持 prerelease；源码标签及四个附件的 ID/字节均未因公开改变。未登录请求访问仓库、发布页、README、MIT 均返回 200；三个 ZIP 与校验清单匿名下载后摘要均匹配本地和 GitHub 服务端记录。

当前源文件清单及摘要重新生成；发布记录的更新未重打包或替换附件。原历史交付 52 个文件摘要与导入前基线一致。
