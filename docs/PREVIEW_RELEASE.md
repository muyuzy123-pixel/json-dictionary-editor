# preview-1 发布记录

状态：准备中，尚未公开。目标仓库 `muyuzy123-pixel/json-dictionary-editor`；先建立私有预览并核验，再公开同一仓库和同一批附件。

| 项目 | 当前状态 |
|---|---|
| 源码提交与标签 | 待冻结；标签 `preview-1` |
| Mac 1.0.0 (1) Universal | 待最终打包；ad-hoc，未公证 |
| Windows 1.0.1 x64/ARM64 | 待最终构建、资源及运行库核对；未签名 |
| 最终 Mac CLI 与局部 GUI | 待执行 |
| 最终 Windows x64 CI | 待执行；固定 windows-2025，仅诊断与内置文件测试 |
| 远端历史、文件与日志 | 待核验 |
| 公开及匿名下载 | 待执行 |

Windows ARM64 实机、Windows 图标与分隔条视觉、输入法、DPI、完整 GUI、macOS 最低系统与真正 Intel 设备验收仍未完成。以上待执行项目不会因历史核心或交叉构建记录而自动通过。

发布包通过 `SOURCE.json` 标明源码提交、tag、app_version 和 architecture；三个 ZIP 的摘要独立列在 Release 的 `SHA256SUMS.txt`。本仓库 `SHA256SUMS` 用于源码文件，不能替代下载包摘要。
