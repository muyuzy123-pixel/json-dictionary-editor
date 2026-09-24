# 预览打包与核验

此流程用于人工执行预览发布，不自动创建仓库、上传、改变公开性或购买签名服务。普通构建入口保持原有行为。

1. 将准备发布的源码提交到独立仓库，并固定 `SOURCE_COMMIT` 和 `RELEASE_TAG=preview-1`。应用版本仍分别取 Mac 1.0.0 (1)、Windows 1.0.1。
2. Mac 先运行无签名构建与验证，再调用独立 `macos/scripts/package_preview.sh`。该步骤在暂存副本内放入 MIT 与 SOURCE.json，执行 ad-hoc 签名并打包；不执行 Developer ID 签名或公证。
3. Windows 显式指定已核对的 LLVM-MinGW 20260616，运行交叉构建，再调用 `windows/scripts/package_preview.sh`。记录链接映射，装入实际需要的运行库许可。普通 Windows 构建无需 Mac 图标生成器。
4. 为三个最终 ZIP 生成 `SHA256SUMS.txt`。核对 ZIP 路径、内容、源码归属、许可证、签名状态及解压后资源；将原始构建路径和 link map 留在本地审计。
5. 私有 Release 保持 prerelease。触发唯一的 Windows CI，输入 tag 与期望的最终 x64 ZIP SHA；它下载同一附件并运行诊断。替换附件后必须重新验证。
6. 全部前置检查通过后再公开仓库，独立匿名下载三个 ZIP 复算摘要。更新发布状态不重建或替换已核验附件。

源码与最终附件分别有校验清单。Tag 必须指向实际构建输入提交；后续仅更新发布记录的提交不改变标签和产物归属。历史交付、图标设计及产品解析器不因发布步骤改变。

从干净源码提交运行（下列路径为示例，构建目录须全新且位于仓库之外）：

```sh
export SOURCE_COMMIT="$(git rev-parse HEAD)"
export RELEASE_TAG=preview-1
export BUILD_DIR=/tmp/jsondict-preview/macos/build
export DIST_DIR=/tmp/jsondict-preview/dist
./macos/scripts/build.sh
./macos/scripts/package_preview.sh

export LLVM_MINGW_ROOT=/path/to/llvm-mingw-20260616-ucrt-macos-universal
export LLVM_MINGW_ARCHIVE=/path/to/llvm-mingw-20260616-ucrt-macos-universal.tar.xz
export BUILD_DIR=/tmp/jsondict-preview/windows/build
sh windows/scripts/build_cross_macos.sh
sh windows/scripts/package_preview.sh
```

Mac 包支持 `PACKAGE_WORK_DIR`、`AUDIT_DIR`、`PACKAGE_README` 指定暂存、证据和说明文件；默认说明来自 `macos/README.md`，HEAD 必须等于 SOURCE_COMMIT 且干净。Windows 打包核对构建输入与该提交的实际 Git 对象；其工具链声明集只适用于上述已记录版本。

生成三个 ZIP 后，在 DIST_DIR 内对这三个明确文件生成 `SHA256SUMS.txt`，不要把原始 EXE、link map 或构建报告误传为 Release 附件。私有 Release 附件名固定为 `JSONDictionaryEditor-macOS-Universal.zip`、`JSONDictionaryEditor-Windows-x64.zip`、`JSONDictionaryEditor-Windows-arm64.zip` 和 `SHA256SUMS.txt`。

失败时保持私有，不把缺项填成“通过”。
