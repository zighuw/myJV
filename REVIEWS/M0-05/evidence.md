# M0-05 证据包：MIDI 子块切分框架

- 日期：2026-09-22
- 分支：`feature/M0-05-midi-subblocks`
- 审查级别：R2（RT 路径；受保护区域：无（未改 `CMakeLists.txt`）；人类裁决）
- 状态：待人类审查

## 1. 环境

同 M0-01（`REVIEWS/M0-01/evidence.md`）。

## 2. 交付物

| 文件 | 内容 |
| --- | --- |
| `Source/Engine/SynthEngine.h` | `MidiEventSink` 虚接口（零拷贝 `MidiMessageMetadata`）；`setMidiEventSink()`；`process(buses, midi, numSamples)`；私有 `renderSegment` |
| `Source/Engine/SynthEngine.cpp` | 按事件 `samplePosition` 切分子块：渲染 `[pos, eventPos)` → 分发 → 末尾渲染剩余；越界事件跳过；测试音相位跨子块连续 |
| `Source/Plugin/MyJVProcessor.cpp` | `processBlock` 将 MIDI 传入引擎（MIDI 参数启用） |

设计要点（架构 5.3 第 3–4 步框架）：
- 分发零拷贝：`MidiMessage` 构造会堆分配（`allocateSpace`），故接口传递 `const juce::MidiMessageMetadata&`；**处理器不得跨回调持有数据指针**。
- 同位置多事件：按缓冲区顺序全部先分发。
- 无 sink 时仍按子块渲染，保持未来声部渲染的切分结构。
- RT 安全：无分配、无锁、无异常（`// RT-safe`）。

## 3. 验证

### 3.1 配置与构建

- `build.log`（清空全量重建，202 行）末行：`BUILD_EXIT=0`；error **0**；warning **0**

### 3.2 测试

- `myJV_tests`：**All tests passed（25 test cases / 72872 assertions）**（`tests.log`）
- `ctest -C Release`：1/1 Passed（`ctest.log`）
- 新增覆盖：
  1. 采样精度分发（事件位置 0 / 12345 / 块末，含状态字节与数据字节校验）
  2. 同位置多事件保持缓冲区顺序（CC 值 10/20/30）
  3. 越界事件跳过（位置 = 块长、块长 + 100）
  4. 零采样块不渲染不分发
  5. 未配置 sink 时事件被忽略且音频正常
  6. 事件切分不改变音频（与无事件渲染逐采样一致，前 4096 采样 × 3 总线）
- **变异抽查**：分发位置改为固定 0 → 2 断言失败（`mutation-check.log`）；恢复后全绿
  - 备注：首次尝试的等价变异（用 `currentSample` 替代事件位置，分发前两者相等）被测试正确放行，说明断言不过度敏感。

### 3.3 冒烟

`smoke.log`：Standalone 存活 5s（pid=15364）。

### 3.4 产物

| 产物 | SHA-256 |
| --- | --- |
| `build/myJV_artefacts/Release/Standalone/myJV.exe` | `89CE0775B7637ECF33460FC3DD5191D103C4178661AC2CD1DC98815409518923` |
| `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` | `A515D2ED37AF8FC12A403F51820DA3CD257284A9B6DEF0106F690A301727FC5C` |

## 4. 发现与修复

| # | 问题 | 根因 | 修复 |
| --- | --- | --- | --- |
| F1 | 首次变异抽查未被捕获 | 所选变异语义等价（分发前 `currentSample` 已等于事件位置） | 改用固定位置 0 的变异；测试正确变红 |

> 实现过程中无构建/测试失败。

## 5. 待补证据

- [ ] macOS/Linux 构建与测试（M0-06 CI）
- [ ] ASan/UBSan（随 M0-06）
- [x] 变异抽查（`mutation-check.log`）

## 6. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `build.log` | 最终清空全量构建（202 行，`BUILD_EXIT=0`，0 error / 0 warning） |
| `tests.log` | 测试全绿（25 用例 / 72872 断言） |
| `ctest.log` | ctest 1/1 通过 |
| `mutation-check.log` | 变异抽查（分发位置固定 0，2 断言失败） |
| `smoke.log` | Standalone 冒烟 |
