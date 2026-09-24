# 许可、版权署名与资源来源

项目授权主体为所有者本人（自然人），当前公开版权署名为 **muyuzy123-pixel**。项目自有内容采用根目录标准 [MIT LICENSE](../LICENSE)，第三方内容保留其适用许可与版权声明。本轮署名与 [macos-terminal-settings 项目许可证](https://github.com/muyuzy123-pixel/macos-terminal-settings/blob/main/LICENSE)一致。

## 项目内容

MIT 覆盖项目自有的两端源码、配置、合成样例、测试、构建/打包脚本、文档及自有图标设计。它不重新授权 Apple 系统字体、系统框架、SDK、工具链或第三方运行库。

两端版权资源字段采用同一公开署名；Mac Bundle ID `com.codex.JSONDictionaryEditor`、Windows 内部窗口/manifest 标识保持原样，它们是程序标识，不是版权主体。旧署名及完整旧构建记录保留在候选之外的本地审计中，公开历史摘要不伪造当时验证结果。

## 图标来源

Mac `IconGenerator.swift` 使用项目定义的圆角、渐变、白卡、圆点和横线，并通过系统等宽字体 API 绘制花括号，没有加载外部图片或字体文件。图标生成器源码采用项目 MIT；系统字体软件及其字形不被本项目重新授权。

预览包使用 `macos/Resources/AppIcon.icns` 的固定资源，逐字节取自既有 Mac 交付（SHA-256 `e0a3b35e53ee2fe28bd95b988e6d9555a4902361c3338051c3667e92245ea53d`）。它保留原图标，不随构建主机字体或渲染器变化重新生成；原生成器及命令行入口仍保留供维护。

Windows 图标直接沿用项目几何设计，只将系统字体花括号改成独立 Bézier 笔画，不提取字体轮廓；小尺寸简化第三行细节。生成器、脚本及新 ICO 输出均属于项目自有 MIT 内容。Windows 普通构建直接使用已入库 ICO，不调用 Mac 维护工具。旧来源未明 ICO 不在本仓库中，也不因此获得新许可。详细过程见[图标生成说明](icon-generation.md)。

[Apple macOS 软件许可](https://www.apple.com/legal/sla/docs/macOSTahoe.pdf)的字体条款不应被扩大解释为所有栅格化输出都禁止分发；本项目没有把系统字体软件纳入 MIT，也没有在 Windows 资源中提取其轮廓。

## 第三方运行库与预览分发

Mac 依赖系统提供的 SwiftUI、AppKit、Foundation 和 Swift 运行环境；本仓库不导入 Apple SDK、系统框架或字体文件。

Windows 使用 LLVM-MinGW 20260616 的静态运行库。预览分发按最终链接映射和实际 Loaded 归档记录核对 libc++（包含 libc++abi 成员）、libunwind、compiler-rt、MinGW 启动/运行库及系统导入库；不能以没有额外 DLL 推断没有第三方代码。

完整许可文本与组件索引见 [THIRD_PARTY_NOTICES.txt](../THIRD_PARTY_NOTICES.txt)。LLVM 与 MinGW 声明原样保留；是否需要其他组件声明由实际链接内容决定，不将工具链中的工具许可误套到应用，也不将第三方内容标为项目 MIT。

源码公开的署名与图标来源两项既有阻断已解决。预览包仍须完成声明装包、签名/归档、Windows CI 和下载摘要核验，实际结果见 [发布记录](PREVIEW_RELEASE.md)。许可和资源检查不构成 Windows 实机视觉或完整 GUI 验收。
