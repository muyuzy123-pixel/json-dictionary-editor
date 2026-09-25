# 后续版本的本地打包与验证

本仓库已经公开，`preview-1` 是独立的历史发布。新版本从当时最新的 `main` 增量提交；更新代码或文档不移动旧标签，也不替换旧附件。创建远端标签、推送、发布预发布和晋升正式版均由维护者另行授权执行。

## 从干净源码提交制备候选

当前目标版本为 macOS **1.1.1 (3)**、Windows **1.1.1**，目标标签 `v1.1.1`。两端的 `SOURCE.json` 都写入同一完整 Git 提交、标签、应用版本和对应架构。

1. 在新分支完成源码、资源、许可证、两端说明和脚本适配；确认图标字节、`LICENSE`、`tests/` 与旧发布记录未误被旧交付覆盖。运行文案校验、核心与共享测试。建立本地提交，确认工作树干净。
2. Mac 在独立输出目录运行 `macos/scripts/build.sh` 与 `verify.sh`；普通构建保持**无签名**。`Localizable.strings`、`InfoPlist.strings` 与 bundle 元数据必须进入构建输入 SHA、无签名 App、暂存副本和最终 ZIP。`package_preview.sh` 仅对暂存 App 做 ad-hoc 签名，然后从 ZIP 解压，检查签名、两架构、1.1.1 (3) CLI、自检、样例、资源及固定 ICNS。
3. Windows 显式指定已核对的 LLVM-MinGW 20260616，从同一提交交叉编译 x64/ARM64。语言检查先于构建；新版英文说明和文案校验器纳入 `build-inputs.sha256`。`package_preview.sh` 检查 EXE、实际 `Loaded archive(member)`、资源、许可证与 Git 对象，再封装中英文说明及 `SOURCE.json`。普通 Windows 构建直接使用仓库 ICO。
4. 生成三个 ZIP 与一个 `SHA256SUMS.txt`；逐项检查 ZIP 路径、UTF-8 标志、许可和源码提交。候选产物、原始构建日志及验证记录都保存在本地工作区之外，不作为本轮远端发布。

参考命令从仓库根运行。路径仅是独立输出示例；正式打包时先用最终本地提交替换 `SOURCE_COMMIT`，不得使用有未提交改动的检出：

```sh
export SOURCE_COMMIT="$(git rev-parse HEAD)"
export RELEASE_TAG=v1.1.1
export BUILD_DIR=/tmp/json-editor-111/macos-build
export DIST_DIR=/tmp/json-editor-111/assets
./macos/scripts/build.sh
./macos/scripts/package_preview.sh

export LLVM_MINGW_ROOT=/path/to/llvm-mingw-20260616-ucrt-macos-universal
export LLVM_MINGW_ARCHIVE=/path/to/llvm-mingw-20260616-ucrt-macos-universal.tar.xz
export BUILD_DIR=/tmp/json-editor-111/windows-build
sh windows/scripts/build_cross_macos.sh
sh windows/scripts/package_preview.sh
```

产物文件名保持 `JSONDictionaryEditor-macOS-Universal.zip`、`JSONDictionaryEditor-Windows-x64.zip`、`JSONDictionaryEditor-Windows-arm64.zip`、`SHA256SUMS.txt`。版本由新标签和包内 `SOURCE.json` 区分。`SOURCE_COMMIT` 必须是实际构建输入提交；构建完成后若更改被打包的源码、资源、许可、平台说明或脚本，应重新冻结并重建相关附件。

## 后续获授权的远端步骤

将已验收的源码提交和 `v1.1.1` 标签推送到现有公开仓库，**保留 `preview-1`**。先以预发布状态上传四个新附件，输入新标签、版本、预发布状态和最终 x64 ZIP 的独立 SHA-256，手动运行 `verify-preview.yml`。它下载实际附件做 Windows 诊断，不 checkout、不重建、不写入 Release。

正式版晋升需要最终 Mac 包的**正常输入、保存、关闭重开及独立内容核对**通过；如正常保存挂起、损坏或静默覆盖，就停留在预发布。历史 AX 直接赋值后立即保存的挂起单列记录；未复现不能写成已修复。正式版状态下可对**同一最终 x64 附件**用工作流的 `release_state=release` 再核验；GUI 与 ARM64 实机范围依实际证据声明。

旧版的 Windows 1.0.1 分隔条源码修复已包含在 `preview-1`；新发行说明只比较 `preview-1 → v1.1.1` 的双语功能、1.1.1 搜索和中文资源自检修复、英文示例，不把分隔条再次列为新修复。
