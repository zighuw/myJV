# M0-04 证据包：引擎空壳 + BusBuffers + 测试音

- 日期：2026-09-20
- 分支：`feature/M0-04-engine-shell`
- 审查级别：R2（首次触达 RT 路径约束；受保护区域：`CMakeLists.txt`；人类裁决）
- 状态：已完成（人类已合并至 main `f48786a` 并补入宿主多输出截图 `REVIEWS/M0-04/2026-09-22 003308.png`）

## 1. 环境

同 M0-01（`REVIEWS/M0-01/evidence.md`）。测试目标新增 `juce_audio_formats`（WAV 写出）。

## 2. 交付物

| 文件 | 内容 |
| --- | --- |
| `Source/Engine/SynthEngine.h/.cpp` | `kNumOutputBuses` + `BusBuffers`（附录 B）+ `prepare/releaseResources/process`（`// RT-safe`）+ 临时测试音（ADR-010）；头文件无 JUCE 依赖 |
| `Source/Plugin/MyJVProcessor.h/.cpp` | `createBuses()`、`isLayoutSupported()`、`buildBusBuffers()`（静态可测）；`processBlock` `noexcept` 并委托引擎 |
| `Tests/engine_tests.cpp` | 11 个用例（布局属性、布局谓词、总线映射、禁用辅助总线渲染、逐总线测试音、44.1 kHz、空/半空总线、零采样、跨块相位、WAV） |
| `CMakeLists.txt` | `SynthEngine.cpp` 入 `MYJV_CORE_SOURCES`；测试目标增加 engine_tests + `juce_audio_formats` |
| `DECISIONS.md` | 新增 ADR-010（临时测试音，M1-04 移除） |

> `process` 契约（供 M1+ 混音改造）：当前对每个非空总线**覆盖写入**全部采样；未来引擎引入声部混音时需改为每块先清空一次。

## 3. 验证

### 3.1 配置与构建

- `build.log`（清空全量重建，202 行）末行：`BUILD_EXIT=0`；error **0**；warning **0**

### 3.2 测试

- `myJV_tests`：**All tests passed（19 test cases / 60566 assertions）**（`tests.log`）
- `ctest -C Release`：1/1 Passed（`ctest.log`）
- 覆盖：
  1. 插件输出总线布局（3 路立体声、名称、默认启用）
  2. 布局谓词（Main 必需立体声；Out1/Out2 立体声或禁用；Main 禁用/单声道/辅助单声道拒绝）
  3. **处理器级总线映射**（全启用 / Out1 禁用 / Out1+Out2 禁用 → `getBusBuffer` 通道偏移正确）
  4. 禁用辅助总线布局下的实际渲染（Main=440 Hz、Out2=880 Hz，且 Out2 ≠ 660 Hz）
  5. 逐总线测试音：频率 440/660/880 Hz ±2 Hz、RMS ≈ -23 dBFS（峰值 -20 dBFS）、L/R 逐采样一致
  6. 44.1 kHz 采样率下频率正确（防硬编码 48 kHz）
  7. 空辅助总线 / 半空总线（仅 L 或仅 R）/ 零采样安全
  8. 跨块相位连续（两块渲染 = 单块渲染，逐采样一致）
  9. 隐藏用例 `[.wav]`：离线渲染 0.5 s × 3 总线 WAV
- **变异抽查 ×2**：
  - 映射：`buses.r[bus]` 误指向通道 0 → 3 断言失败（`mutation-check2-bus-mapping.log`）
  - 频率：Out1 660→700 Hz → 2 断言失败（`mutation-check.log`）

### 3.3 测试音 WAV（证据文件）

| 文件 | 格式 | 大小 |
| --- | --- | --- |
| `test-tone-main.wav` | PCM / 2ch / 48 kHz / 16-bit / 0.5 s / 440 Hz | 96,104 字节 |
| `test-tone-out1.wav` | 同上（660 Hz） | 96,104 字节 |
| `test-tone-out2.wav` | 同上（880 Hz） | 96,104 字节 |

> 头部已校验：audioFormat=1、channels=2、sampleRate=48000、bits=16、data=96000 字节；RMS 0.07071（-23.01 dBFS）、峰值 0.1（-20 dBFS）。

### 3.4 冒烟

`smoke.log`：Standalone 存活 5s（pid=31384）。

### 3.5 产物

| 产物 | SHA-256 |
| --- | --- |
| `build/myJV_artefacts/Release/Standalone/myJV.exe` | `51CE5D9F4F9F0C77EEF1F888B42BD91B4B38AF239B1DE1D215937C89D55EE929` |
| `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` | `9B2701B5974BE291FA586F71BBD1B3F9E11DA006589B295EE345AAACFBFB8A6C` |

## 4. 修复记录

### 4.1 实现过程中

| # | 问题 | 根因 | 修复 | 证据 |
| --- | --- | --- | --- | --- |
| F1 | 编译失败：临时 `BusBuffers` 不能绑定非 const 引用 | `process(BusBuffers&)` 签名不允许临时对象 | 改为 `process(const BusBuffers&)` | `build-attempt1-const-ref.log` |
| F2 | 测试失败：辅助缓冲出现垃圾值 | `AudioBuffer::setSize` 不初始化内存 | 夹具中 `clear()` | 失败日志被后续成功构建覆盖 |
| F3 | 编译失败：JUCE 8 弃用旧 `createWriterFor`，新 API 参数不匹配 | 新重载接受 `std::unique_ptr<OutputStream>&` + `AudioFormatWriterOptions` | 改用新 API（先经 FileOutputStream 校验 `openedOk` 再上转型） | 失败日志被后续成功构建覆盖 |

### 4.2 终审修复轮（AI 交叉审查，2026-09-20）

终审结论：0 Critical / 2 Important / 10 Minor。

| # | 发现 | 级别 | 处置 |
| --- | --- | --- | --- |
| R1 | 处理器级总线映射与布局谓词无测试（"总线布局验证"未覆盖真实映射） | Important | 提取静态 `isLayoutSupported` / `buildBusBuffers`；新增 4 个用例（布局谓词、三种映射组合、禁用辅助布局渲染） |
| R2 | 证据把峰值 -20 dBFS 误写为 RMS | Important | 修正证据与 ADR-010：峰值 -20 dBFS / RMS ≈ -23 dBFS |
| R3 | `getBusBuffer` 视图未做通道数防御 | Minor | 增加 `getNumChannels() < 2` 跳过 |
| R4 | `processBlock` 未标 `noexcept`（架构 3.5） | Minor | 已加 `noexcept` |
| R5 | 相位回绕单次 `if`（采样率 < 880 Hz 才出错） | Minor | 改为 `while` |
| R6 | 频率/RMS 仅测 48 kHz | Minor | 新增 44.1 kHz 用例 |
| R7 | L/R 一致性用 `getMagnitude` 不够强 | Minor | 改为逐采样比较（前 1024 采样） |
| R8 | 头文件引入重型 JUCE 头 | Minor | `SynthEngine.h` 去 JUCE 依赖（`.cpp` 引入 juce_core） |
| R9 | 总线数 3 在多处重复 | Minor | 统一为 `kNumOutputBuses` |
| R10 | 边界用例缺失（半空总线/零采样） | Minor | 已补两个用例 |
| R11 | F2/F3 日志被覆盖；`[.wav]` 隐藏用例与 R0"无跳过用例"的关系 | Minor | 已在本文档披露与说明 |

## 5. 待补证据

- [x] 宿主多输出截图（人类已补：`REVIEWS/M0-04/2026-09-22 003308.png`，宿主 pin connector 显示 6 路输出）
- [ ] macOS/Linux 构建与测试（M0-06 CI）
- [ ] ASan/UBSan（首个 RT 路径；MSVC ASan 环境复杂，建议随 M0-06 CI 覆盖）
- [x] 变异抽查 ×2（`mutation-check.log`、`mutation-check2-bus-mapping.log`）
- [x] 测试音 WAV ×3

## 6. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `build.log` | 最终清空全量构建（202 行，`BUILD_EXIT=0`，0 error / 0 warning） |
| `build-attempt1-const-ref.log` | F1（const 引用） |
| `build-fix.log` | 修复轮编译诊断（BusesLayout 别名，已解决） |
| `tests.log` | 测试全绿（19 用例 / 60566 断言） |
| `ctest.log` | ctest 1/1 通过 |
| `mutation-check.log` | 变异抽查：Out1 660→700 Hz（2 断言失败） |
| `mutation-check2-bus-mapping.log` | 变异抽查：R 通道误映射（3 断言失败） |
| `wav-run.log` | `[.wav]` 运行输出 |
| `smoke.log` | Standalone 冒烟 |
| `test-tone-*.wav` | 测试音证据（3 路总线） |
