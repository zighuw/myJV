# myJV 架构决策记录（ADR）

> 审查中形成的架构决策与例外批准（开发计划 3.7）。格式：编号 / 日期 / 状态 / 决策 / 理由 / 影响。

## ADR-001：JUCE 8.0.9 经 FetchContent 引入

- 日期：2026-09-20
- 状态：已批准（人类，M0-01 会话）
- 决策：CMake FetchContent 固定 tag `8.0.9`（浅克隆）；不使用 submodule；不使用 JUCE 7.x。
- 理由：本机无本地 JUCE；CMake 4.x（本机 4.4.3）与 JUCE 8 兼容性最好；tag 固定保证可复现构建。
- 影响：首次配置需联网下载（数百 MB）；CI 缓存策略在 M0-06 决定。

## ADR-002：M0 阶段使用本地 git 仓库，暂无远程与 CI

- 日期：2026-09-20
- 状态：已批准
- 决策：本地 `git init`（main + feature 分支）；不建远程；CI（M0-06）推迟到远程仓库就绪后接入。
- 理由：先完成工程骨架；远程/CI 依赖外部平台账号与仓库。
- 影响：R0 门禁在本阶段以本地构建/测试替代；三平台构建日志由 M0-06 的 CI 补全。

## ADR-003：M0-01 证据口径（Windows 本地）

- 日期：2026-09-20
- 状态：已批准
- 决策：M0-01 验收证据 = 本机 Windows 配置/构建日志 + 产物存在性检查 + Standalone 启动冒烟；宿主加载截图由人类补入；macOS/Linux 构建日志由 M0-06 CI 补全。
- 理由：本机仅 Windows（MSVC BuildTools），无 macOS/Linux 环境。
- 影响：M0-01 的"三平台"证据分两阶段收齐，M0-06 收口时核对。

## ADR-004：本机工具链为便携版 CMake + VS 18 2026 BuildTools

- 日期：2026-09-20
- 状态：已批准（人类，M0-01 会话）
- 决策：PATH 上的 MSYS2 CMake 不含 VS 生成器且本机无原生 CMake；下载便携版 CMake 4.4.3 至 `D:\Program\CMake`（不写系统 PATH）。本机实际安装的是 VS 18 2026 BuildTools（18.10.1，MSVC 14.51），生成器使用 `Visual Studio 18 2026`（文档基线的 MSVC 2022 在 CI/发布保留，见架构 9.1）。
- 理由：不引入旧版 VS 大型安装；便携 CMake 免管理员；VS 18 生成器与 CMake 4.4.3 兼容。
- 影响：构建命令与 AGENTS.md 第 5 条按本机环境更新；CI（M0-06）仍按三平台标准工具链配置。

## ADR-005：Windows SDK 缺失，由人类安装（M0-01 构建阻塞项）

- 日期：2026-09-20
- 状态：已完成（人类已安装 10.0.26100；M0-01 配置/构建通过，证据见 `REVIEWS/M0-01/evidence.md`）
- 决策：本机注册表登记 SDK 10.0.26100 但文件缺失（`Windows Kits\10` 下无 Include/Lib），链接器报 `LNK1181: 无法打开 kernel32.lib`；由人类安装 Windows SDK 10.0.26100 后继续 M0-01 构建。
- 理由：SDK 为系统级组件，安装需 UAC；AI 不擅自安装。
- 影响：阻塞已解除；若后续更换 SDK 版本，需同步核对注册表与 vcvars 解析。

## ADR-006：VST3 不启用 VST2 替代兼容（`JUCE_VST3_CAN_REPLACE_VST2=0`）

- 日期：2026-09-20
- 状态：已批准（M0-01 终审后记录）
- 决策：`CMakeLists.txt` 定义 `JUCE_VST3_CAN_REPLACE_VST2=0`（与 JUCE 官方 CMake 示例一致）；本项目从未发布 VST2 版本。
- 理由：避免 VST2/VST3 参数自动化 ID 冲突的编译期保护（C1189）；该宏同时决定 VST3 类 ID 与 VST2 兼容行为。
- 影响：**发布后不得更改**（更改会破坏宿主工程中的插件识别与自动化数据）；若未来需要 VST2 迁移，必须实现 `VST3ClientExtensions::getCompatibleParameterIds()` 并另开 ADR。

## ADR-007：`myJV_core` 暂不链接 JUCE 模块（构建结构决策点延后至 M1-01）

- 日期：2026-09-20
- 状态：已取代（M0-03 采纳选项 ②，见 ADR-008）
- 背景：JUCE 模块是 INTERFACE 库（`JUCEModuleSupport.cmake` 的 `target_sources(INTERFACE ...)`），任何链接它们的 target 都会编译全部模块源码。`myJV_core` 若链接 `juce_audio_utils`，会把约 100 MB 的 JUCE 目标文件编入静态库，且与插件目标重复编译。
- 决策：M0-02 中 `myJV_core` 不链接 JUCE（骨架仅含模块编译锚，无 JUCE 依赖）；插件与后续 Tests 各自链接 JUCE 模块（JUCE 原生模型）。
- 影响：M1-01（首次在核心代码中使用 JUCE 类型）需二选一：① 核心库链接 JUCE，并按 JUCE 共享代码模式为消费方再导出 include/defines（JUCE 只编译一次，复杂度高）；② 放弃静态库，改用 `MYJV_CORE_SOURCES` 源列表由各目标自行编译（简单、符合 JUCE 惯例）。届时按 R2 记录新 ADR。

## ADR-008：核心代码改用 `MYJV_CORE_SOURCES` 源列表（取代 ADR-007 的结构选择）

- 日期：2026-09-20
- 状态：已批准（M0-03 设计批准，人类）
- 背景：M0-03 的 `ParameterLayout.cpp` 是首个需要 JUCE 类型的核心源文件；`myJV_core` 静态库不链接 JUCE 则无法编译该文件，链接 JUCE 则导致模块重复编译与约 100 MB 静态库（ADR-007 背景）。
- 决策：移除 `myJV_core` 静态库与 5 个模块编译锚；定义 `set(MYJV_CORE_SOURCES ...)`，由插件目标与 `myJV_tests` 目标各自编译（连同各自链接的 JUCE 模块，符合 JUCE 原生模型）。
- 理由：简单、无重复静态库、无需再导出 include/defines；JUCE 模块本就按消费目标编译。
- 影响：插件与测试各自编译核心源与 JUCE 模块（本机约 2×5 分钟构建）；后续核心源文件加入 `MYJV_CORE_SOURCES` 即可被两目标共享。
- 备注：移除 5 个编译锚后，`Source/Engine|Model|DSP|IO` 的骨架头文件暂不被任何目标编译（骨架完整性由 M0-02 证据归档）；各任务加入真实源文件时纳入 `MYJV_CORE_SOURCES`。

## ADR-009：依赖记录——Catch2 v3.16.0（单元测试）

- 日期：2026-09-20
- 状态：已批准（架构 2.2 已列依赖 + M0-03 设计批准）
- 用途：C++ 单元测试框架（参数注册表、自动化往返等）
- 许可证：BSL-1.0
- 体积：源码 tarball 约 1.2 MB（FetchContent URL + SHA256 固定；git 协议不可达时的替代获取方式）
- 替代方案：JUCE UnitTest（无成熟断言宏/报告生态）

## ADR-010：临时测试音（M0-04 引入，M1-04 移除）

- 日期：2026-09-20
- 状态：已批准（M0-04 设计批准，人类）
- 背景：架构 1.3 原则 6"采样器为唯一波形源"；M0 里程碑目标包含 Standalone/VST3 出声。
- 决策：`SynthEngine` 内置临时测试音（Main 440 Hz、Out1 660 Hz、Out2 880 Hz，**峰值** -20 dBFS（RMS ≈ -23 dBFS）正弦），插件加载即发声，用于验证音频通路与总线路由；总线指针为空时跳过。
- 影响：**M1-04 `SamplePlayer` 落地后必须移除测试音分支**；移除前不得进入 v1.0。

## ADR-011：MIDI 事件分发接口（MidiEventSink，零拷贝）

- 日期：2026-09-22
- 状态：已批准（M0-05 设计批准，人类）
- 决策：引擎经 `MidiEventSink` 虚接口分发事件，参数为 `const juce::MidiMessageMetadata&`（零拷贝）；sink 指针为 `std::atomic<MidiEventSink*>`（release 存 / acquire 取），符合架构 3.5 的原子指针交换约束。
- 理由：`juce::MidiMessage` 的长消息构造会堆分配（3 字节通道消息使用内联存储，>8 字节才分配），RT 路径不可逐事件构造；虚调用本身无分配。
- 契约：`event.data` 仅在回调期间有效，处理器不得跨回调持有；实现不得在 `handleMidiEvent` 内分配/加锁。
- 影响：M3-01 的 `VoiceManager` 将实现该接口；若需跨块保留事件，必须复制到预分配缓冲。
