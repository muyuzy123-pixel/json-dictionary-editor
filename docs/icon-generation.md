# Windows 图标生成与来源

2026-09-20 的图标替换仅作用于独立候选 Windows 资源。Mac 图标源码、Mac 图标资源及历史交付保持不变；两端产品版本保持 macOS 1.0.0、Windows 1.0.1。

## 定向来源核对

对照 `macos/Sources/JSONDictionaryEditor/IconGenerator.swift` 的实际绘制代码：

| 元素 | 已核对来源 | Windows 候选处理 |
|---|---|---|
| 蓝青圆角渐变背景 | 项目代码给出的矩形、圆角、颜色与 -45° 渐变 | 直接沿用几何参数与颜色 |
| 白色圆角卡片 | 项目代码定义的矩形、圆角与透明度 | 直接沿用 |
| 青色圆点和蓝色横线 | 项目代码定义的路径、坐标与颜色 | 沿用；16/24/32 像素省略第三行，减少与花括号粘连 |
| 花括号 | 原 Mac 通过 `NSFont.monospacedSystemFont(..., weight: .bold)` 渲染 | 仅此元素改为项目自行定义的 Bézier 笔画，保持 `{ }` 意图和位置 |
| 其他素材 | 该生成器没有外部图片、图标包、字体文件或 SF Symbols 调用 | 不增加其他素材 |

本机无签名字体探针查询到 `.AppleSystemUIFontMonospaced-Bold`（显示名 `.SF NS Mono Light Bold`）。[AppKit NSFont 文档](https://developer.apple.com/documentation/appkit/nsfont)说明系统字体 API 的用途；[macOS Tahoe 软件许可第 2E 条](https://www.apple.com/legal/sla/docs/macOSTahoe.pdf)涉及系统字体的使用及嵌入。此次没有确认该特定字形作为独立 Windows 图标资产分发的许可范围，因此只处理花括号，没有把字体条款扩大成“所有栅格图标输出均被禁止”的结论。

新花括号是手工定义的对称曲线和笔画；没有读取、提取、描摹或嵌入原系统字体轮廓。维护工具保留 AppKit 渲染器，以直接复用现有几何绘制代码；不修改 Mac 生成器，也不引入外部字体或第三方绘图库。

## 文件与许可

- [`windows/tools/GenerateIcon.swift`](../windows/tools/GenerateIcon.swift)：完整绘制及 ICO 写入源码。
- [`windows/tools/generate_icon.sh`](../windows/tools/generate_icon.sh)：可选维护者生成入口。
- [`windows/resources/app.ico`](../windows/resources/app.ico)：普通 Windows 构建直接使用的已入库资源。

项目自有几何设计、上述新生成源码/脚本及其 ICO 输出均由 **muyuzy123-pixel** 按根目录标准 [MIT LICENSE](../LICENSE) 授权。AppKit、系统 SDK 和原 Mac 使用的系统字体不被本项目重新授权；新 Windows ICO 不包含字体软件或系统字形轮廓。旧 Windows ICO 仍只保存在原历史交付及候选外本地审计备份中，没有被附加 MIT，也不再出现在待公开候选中。

## 可选重新生成

只在维护图标时使用 macOS、Xcode/Command Line Tools 和已有 Rosetta（Apple 芯片主机）。脚本显式禁用链接器临时签名，临时生成器及缓存写入独立临时目录并在退出时清理；不签名、不发布。**Windows 日常构建不需要运行此命令，不需要 Swift、AppKit 或 macOS。**

在仓库根执行，替换候选 ICO：

```sh
sh windows/tools/generate_icon.sh
```

或生成到独立路径并导出审阅图：

```sh
sh windows/tools/generate_icon.sh /tmp/jsondict-preview.ico /tmp/jsondict-icon-preview
```

ICO 包含 **16、24、32、48、64、128、256** 像素七档独立绘制的 PNG 帧，各为 8 位 RGBA。透明画布、圆角边缘和 alpha 随帧保留；ICO 目录使用小端尺寸/偏移，256 在宽高字节中以 0 表示，planes=1、bitCount=32。不是把旧 Windows 图标改格式，也不是从 Mac ICNS 中提取字体图块。

预览目录包含每档 PNG 和 `icon-preview.png`：从左到右对应上述七档，上排浅色背景、下排深色背景；前五档放大 3 倍并关闭插值，后两档原尺寸，便于查看实际像素。预览不写入仓库资源，不作为 Windows 任务栏、标题栏或资源管理器的实机显示证据。

同一工具链上重复生成应得到相同字节，本轮实际对照结果见[验证记录](verification.md)。不同 macOS/AppKit/SDK 版本可能改变 PNG 编码或渲染细节；不宣称跨系统逐字节可复现。

## 检查边界

本轮检查 ICO 目录、PNG 尺寸/CRC/alpha、生成可复核性及预览可读性，并重新交叉构建 Windows x64/ARM64，核对嵌入资源与 ICO 一致。Windows CMake 和交叉构建仍只引用 `resources/app.rc` → `app.ico`，没有添加图标再生成步骤。

这些结果只证明资源与构建范围。**未运行 Windows 程序；未完成图标的 Windows 实机视觉验收、1.0.1 分隔条修复后的实机视觉确认或完整 GUI 验收。** 实际执行结果和资源/产物摘要见 [icon-update-evidence.json](icon-update-evidence.json)。

## Mac 固定图标资源

首次公开预览把历史 Mac ICNS 原字节作为 `macos/Resources/AppIcon.icns` 纳入必要资源，普通构建不再重新渲染。这样避免系统字体/SDK变化使同一生成器产生不同字节。原Mac图标设计、原IconGenerator及历史交付保持不变；Windows ICO也不因当前署名变化重新生成。
