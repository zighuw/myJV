# M0-05 证据包：MIDI 子块切分框架

- 日期：2026-09-22
- 分支：`feature/M0-05-midi-subblocks`
- 审查级别：R2（RT 路径；未改 `CMakeLists.txt`；人类裁决）
- 状态：已完成（人类已合并至 main `a592715`）

## 1. 环境

同 M0-01（`REVIEWS/M0-01/evidence.md`）。

## 2. 交付物

| 文件 | 内容 |
| --- | --- |
| `Source/Engine/SynthEngine.h` | `MidiEventSink` 虚接口（零拷贝 `MidiMessageMetadata`）；`std::atomic<MidiEventSink*>`；`setMidiEventSink()`；`process(buses, midi, numSamples)`；私有 `renderSegment`；JUCE 类型前向声明 |
| `Source/Engine/SynthEngine.cpp` | 按事件 `samplePosition` 切分子块：渲染 `[pos, eventPos)` → 分发 → 末尾渲染剩余；越界事件（<0 或 ≥numSamples）跳过；测试音相位跨子块连续 |
| `Source/Plugin/MyJVProcessor.cpp` | `processBlock` 将 MIDI 传入引擎（MIDI 参数启用） |

设计要点（架构 5.3 第 3–4 步框架，ADR-011）：
- 分发零拷贝：接口传递 `const juce::MidiMessageMetadata&`（`juce::MidiMessage` 长消息构造会堆分配；3 字节通道消息为内联存储，>8 字节才分配）。
- sink 指针为 `std::atomic`（release/acquire），满足架构 3.5 原子指针交换约束。
- 契约：`event.data` 仅在回调期间有效，处理器不得跨回调持有。
- 同位置多事件：按缓冲区顺序全部先分发；无 sink 时仍按子块渲染。

## 3. 验证

### 3.1 配置与构建

- `build.log`（含命令/日期头注，清空全量重建）末行：`BUILD_EXIT=0`；error **0**；warning **0**

### 3.2 测试

- `myJV_tests`：**All tests passed（26 test cases / 60590 assertions）**（`tests.log`）
- `ctest -C Release`：1/1 Passed（`ctest.log`）
- 新增覆盖：
  1. 采样精度分发（事件位置 0 / 12345 / 块末，含状态字节与数据字节校验）
  2. 同位置多事件保持缓冲区顺序（CC 值 10/20/30）
  3. 越界事件跳过（位置 −1、块长、块长 + 100）且同块内合法事件仍分发（对照事件位置 100）
  4. 零采样块不渲染不分发
  5. 未配置 sink / sink 被清空（nullptr）时事件被忽略且音频正常
  6. 事件切分不改变音频（**全块**逐采样最大差 = 0，3 总线）
- **变异抽查**：分发位置改为固定 0 → 3 断言失败（`mutation-check.log`）；恢复后全绿
  - 备注：首次尝试的等价变异（用 `currentSample` 替代事件位置，分发前两者相等）被测试正确放行，说明断言不过度敏感。

### 3.3 冒烟

`smoke.log`：Standalone 存活 5s（pid=25992）。

### 3.4 产物

| 产物 | SHA-256 |
| --- | --- |
| `build/myJV_artefacts/Release/Standalone/myJV.exe` | `6F35E4E4485A5F37D091CC8A1F90F6F6DC80BF023CDA74D0D80B106C0C3A7EDA` |
| `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` | `32BE3FD88945DC916DD537892821AB829EB7CFF74E7A5EB946F520FB366777D4` |

## 4. 修复记录

### 4.1 实现过程中

| # | 问题 | 根因 | 修复 |
| --- | --- | --- | --- |
| F1 | 首次变异抽查未被捕获 | 所选变异语义等价（分发前 `currentSample` 已等于事件位置） | 改用固定位置 0 的变异；测试正确变红 |

> 实现过程中无构建/测试失败。

### 4.2 终审修复轮（AI 交叉审查，2026-09-22）

终审结论：0 Critical / 2 Important / 7 Minor。

| # | 发现 | 级别 | 处置 |
| --- | --- | --- | --- |
| R1 | `midiSink` 跨线程指针无同步（架构 3.5 要求原子指针交换） | Important | 改为 `std::atomic<MidiEventSink*>`（release/acquire） |
| R2 | 音频一致性测试仅覆盖前 4096 采样，段边界回归可漏过 | Important | 改为全块逐采样最大差断言（= 0） |
| R3 | sink 契约仅存于证据文档 | Minor | 契约写入 ADR-011（项目决策记录） |
| R4 | "MidiMessage 构造会堆分配"表述不精确 | Minor | 更正为"长消息（>8 字节）才堆分配，3 字节通道消息内联" |
| R5 | 越界测试无对照事件、负位置未覆盖、无 sink 清空用例 | Minor | 均已补（负位置、块长、块长+100 + 对照事件；sink 置空） |
| R6 | 头文件重新引入 JUCE 依赖未记录 | Minor | 改为前向声明 `juce::MidiBuffer` / `juce::MidiMessageMetadata` |
| R7 | `build.log` 与 M0-04 字节相同（确定性重建），证据不可区分 | Minor | 重建日志加命令/日期头注 |
| R8 | 无 `MidiEventSink` 的 ADR | Minor | 新增 ADR-011 |
| R9 | 无测试先行的红灯记录（计划 5.3） | Minor | 流程记录：本会话实现与测试同批完成（非严格先失败测试），以变异抽查补偿；后续任务尽量先写失败测试 |

## 5. 待补证据

- [ ] macOS/Linux 构建与测试（M0-06 CI）
- [ ] ASan/UBSan（随 M0-06）
- [x] 变异抽查（`mutation-check.log`）

## 6. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `build.log` | 最终清空全量构建（含命令/日期头注，`BUILD_EXIT=0`，0 error / 0 warning） |
| `tests.log` | 测试全绿（26 用例 / 60590 断言） |
| `ctest.log` | ctest 1/1 通过 |
| `mutation-check.log` | 变异抽查（分发位置固定 0，3 断言失败） |
| `smoke.log` | Standalone 冒烟 |
