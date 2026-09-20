# M0-04 证据包：引擎空壳 + BusBuffers + 测试音

- 日期：2026-09-20
- 分支：`feature/M0-04-engine-shell`
- 审查级别：R2（首次触达 RT 路径约束；受保护区域：`CMakeLists.txt`；人类裁决）
- 状态：待人类审查

## 1. 环境

同 M0-01（`REVIEWS/M0-01/evidence.md`）。测试目标新增 `juce_audio_formats`（WAV 写出）。

## 2. 交付物

| 文件 | 内容 |
| --- | --- |
| `Source/Engine/SynthEngine.h/.cpp` | `BusBuffers`（附录 B）+ `prepare/releaseResources/process`（`// RT-safe`）+ 临时测试音（ADR-010） |
| `Source/Plugin/MyJVProcessor.h/.cpp` | `createBuses()` 静态（3 立体声输出）；`isBusesLayoutSupported`（Main 必需，Out1/Out2 可禁用）；`processBlock` 构建 BusBuffers 调引擎 |
| `Tests/engine_tests.cpp` | 5 个用例（总线布局、测试音、空总线、跨块相位连续、WAV 渲染） |
| `CMakeLists.txt` | `SynthEngine.cpp` 入 `MYJV_CORE_SOURCES`；测试目标增加 engine_tests + `juce_audio_formats` |
| `DECISIONS.md` | 新增 ADR-010（临时测试音，M1-04 移除） |

## 3. 验证

### 3.1 配置与构建

- `build.log`（清空全量重建，202 行）末行：`BUILD_EXIT=0`；error **0**；warning **0**

### 3.2 测试

- `myJV_tests`：**All tests passed（13 test cases / 57458 assertions）**（`tests.log`）
- `ctest -C Release`：1/1 Passed（`ctest.log`）
- 覆盖：
  1. 插件输出总线布局（3 路立体声、名称 Main/Out1/Out2、默认启用）
  2. 测试音频率（过零计数 440/660/880 Hz ±2 Hz）、RMS ≈ -20 dBFS、L/R 一致
  3. 空辅助总线安全（Out1/Out2 为空指针时 Main 正常、辅助缓冲不被写入）
  4. 跨块相位连续（两块渲染 = 单块渲染，逐采样一致）
  5. 隐藏用例 `[.wav]`：离线渲染 0.5 s × 3 总线 WAV
- **变异抽查**：Out1 频率 660→700 Hz → 1 断言失败（exit 42，`mutation-check.log`）；恢复后全绿

### 3.3 测试音 WAV（证据文件）

| 文件 | 格式 | 大小 |
| --- | --- | --- |
| `test-tone-main.wav` | PCM / 2ch / 48 kHz / 16-bit / 0.5 s | 96,104 字节 |
| `test-tone-out1.wav` | 同上（660 Hz） | 96,104 字节 |
| `test-tone-out2.wav` | 同上（880 Hz） | 96,104 字节 |

> 头部已校验：audioFormat=1、channels=2、sampleRate=48000、bits=16、data=96000 字节。

### 3.4 冒烟

`smoke.log`：Standalone 存活 5s（pid=30096）。

### 3.5 产物

| 产物 | SHA-256 |
| --- | --- |
| `build/myJV_artefacts/Release/Standalone/myJV.exe` | `0E994B24CB39B7B7813A253AD664D526D5F15752D0C732060E6761720B5D54BA` |
| `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` | `EC13FD9E8F3350A2552E4F55641355B65E14A341F40CC3B408CC681EA98AFA0C` |

## 4. 发现与修复

| # | 问题 | 根因 | 修复 | 证据 |
| --- | --- | --- | --- | --- |
| F1 | 编译失败：临时 `BusBuffers` 不能绑定非 const 引用 | `process(BusBuffers&)` 签名不允许临时对象 | 改为 `process(const BusBuffers&)`（结构只读、指向数据写入） | `build-attempt1-const-ref.log` |
| F2 | 测试失败：辅助缓冲出现垃圾值 | `AudioBuffer::setSize` 不初始化内存 | 夹具中 `clear()` | 失败日志已被后续成功构建覆盖；见修复提交 |
| F3 | 编译失败：JUCE 8 弃用旧 `createWriterFor`，新 API 参数不匹配 | 新重载接受 `std::unique_ptr<OutputStream>&` + `AudioFormatWriterOptions` | 改用新 API；流以 `unique_ptr<OutputStream>` 持有（先经 FileOutputStream 校验 `openedOk`） | 失败日志已被后续成功构建覆盖；见修复提交 |
| F4 | 警告 C4996（旧 API 弃用） | 同上 | 采用新 API 后警告归零 | `build.log`（0 warning） |

## 5. 待补证据

- [ ] 宿主多输出截图（人类：宿主中插件显示 3 路输出；并确认可听到测试音）
- [ ] macOS/Linux 构建与测试（M0-06 CI）
- [x] 变异抽查（`mutation-check.log`）
- [x] 测试音 WAV ×3

## 6. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `build.log` | 最终清空全量构建（202 行，`BUILD_EXIT=0`，0 error / 0 warning） |
| `build-attempt1-const-ref.log` | F1（const 引用） |
| `tests.log` | 测试全绿（13 用例 / 57458 断言） |
| `ctest.log` | ctest 1/1 通过 |
| `mutation-check.log` | 变异抽查（660→700 Hz，1 断言失败） |
| `wav-run.log` | `[.wav]` 运行输出 |
| `smoke.log` | Standalone 冒烟 |
| `test-tone-*.wav` | 测试音证据（3 路总线） |
