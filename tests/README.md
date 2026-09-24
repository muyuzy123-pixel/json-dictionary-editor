# 共享核心样例

只使用 Python 3 标准库和现有平台编译器。`adapters/` 是测试入口，直接调用原 Swift / C++ 解析器和写入器，不实现第三套解析器，也不进入 GUI。

在仓库根执行，macOS 上编译并测试两个核心：

```sh
bash tests/build_adapters_macos.sh .build/shared-tests
python3 tests/run_shared.py \
  --swift-adapter .build/shared-tests/swift-adapter \
  --cpp-adapter .build/shared-tests/cpp-adapter \
  --require-both --report .build/shared-tests/results.json
```

脚本明确生成 x86_64、macOS 13 目标测试程序，并向链接器传递 `-no_adhoc_codesign`，不会调用签名或打包。Apple Silicon 主机需已有 Rosetta；缺少 Rosetta 时测试执行会失败，不自动安装。所有测试产物和 Swift 缓存位于指定输出目录，可以把参数替换为临时目录。

Windows 或其他 C++17 环境可以单独编译 C++ 适配器。例如在安装了 Clang 的开发终端、仓库根执行：

```sh
clang++ -std=c++17 -O2 -I windows/src windows/src/json_core.cpp tests/adapters/cpp_adapter.cpp -o cpp-adapter.exe
python tests/run_shared.py --cpp-adapter ./cpp-adapter.exe --report cpp-results.json
```

这里的单平台命令只证明该次 C++ 核心测试，不代表同时测试了 Swift 或 Windows UI。也可以使用平台构建提供的适配器路径。Windows 原生命令是准备复现的用法，本次没有在 Windows 实机执行。

`fixtures/cases.json` 包含 36 条合成样例：34 条在两端运行，2 条 C++ 深度边界只针对已有上限，因此双端完整运行应得到 **70/70**。没有私人数据。无效 UTF-8 用 `input_hex` 保存，避免编辑器替换非法字节；Unicode 规范等价键与 CRLF 格式识别明确保存两端不同期望。

每次成功解析都核对输出字节，可以检出数字文本、类型、顺序、Unicode 或格式化漂移。拒绝样例必须退出 1、无标准输出且有错误消息；崩溃、超时、缺少可执行文件及适配器错误不会被算成正确拒绝。适配器使用 0 表示数据接受，1 表示产品核心拒绝，2 表示参数或输入读取等适配器错误。runner 使用 0 表示全部通过，1 表示测试失败，2 表示用法错误；缺少任一实现时 `--require-both` 会失败。

报告记录样例摘要、适配器 SHA-256、每项退出码及错误诊断，不收集环境变量或完整终端会话。工具链版本和完整构建命令由候选验证记录单独记载。

这些测试不覆盖 Windows 文件读取层对 BOM 的处理、文件保存、GUI 交互、输入法、DPI、50000/250000 节点边界或分隔条视觉效果；也不能将一次成功构建视作目标操作系统完整兼容验收。
