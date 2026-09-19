# M0-02 证据包：目录与模块骨架

- 日期：2026-09-20
- 分支：`feature/M0-02-module-skeleton`
- 审查级别：R2（受保护区域 `CMakeLists.txt`，人类裁决）
- 状态：待人类审查

## 1. 环境

同 M0-01（`REVIEWS/M0-01/evidence.md`）：便携 CMake 4.4.3 + VS 18 2026 生成器 + SDK 10.0.26100 + JUCE 8.0.9。

## 2. 交付物

### 2.1 目录结构（按架构 9.2）

| 目录 | 内容 |
| --- | --- |
| `Docs/` | 三份项目文档（本任务由根目录 `git mv` 归位） |
| `Source/Plugin/` | 既有（M0-01） |
| `Source/Params/`、`Source/Model/`、`Source/Engine/`、`Source/DSP/`、`Source/IO/` | 骨架头文件 + 模块编译锚 |
| `Source/UI/Components/`、`Source/UI/Pages/` | 占位（M5 填充） |
| `Tests/` | 占位（Catch2 接入留给首个测试任务） |
| `Resources/fonts/`、`Resources/images/`、`Resources/default_patches/` | 占位 |
| `cmake/` | 占位（JUCE 集成工具链） |

### 2.2 接口头文件清单（最小空声明，成员由各任务卡填充）

| 头文件 | 声明 |
| --- | --- |
| `Source/Params/ParameterIDs.h` | `namespace ParameterIDs`（空） |
| `Source/Params/ParamSnapshot.h` | `ToneSnapshot`、`PatchSnapshot` |
| `Source/Params/Calibration.h` | `namespace Calibration`（空） |
| `Source/Model/Patch.h` | `Patch` |
| `Source/Model/Tone.h` | `Tone` |
| `Source/Model/ZoneSet.h` | `Zone`、`ZoneSet` |
| `Source/Model/Sample.h` | `Sample` |
| `Source/Model/SampleLibrary.h` | `SampleLibrary` |
| `Source/Engine/SynthEngine.h` | `SynthEngine` |
| `Source/Engine/VoiceManager.h` | `VoiceManager` |
| `Source/Engine/Note.h` | `Note` |
| `Source/Engine/ToneVoice.h` | `ToneVoice` |
| `Source/Engine/TonePairProcessor.h` | `TonePairProcessor` |
| `Source/Engine/AssetReclaimer.h` | `AssetReclaimer` |
| `Source/Engine/ModulationMatrix.h` | `ModulationMatrix` |
| `Source/DSP/SamplePlayer.h` | `SamplePlayer` |
| `Source/DSP/Envelope.h` | `Envelope` |
| `Source/DSP/LFO.h` | `LFO` |
| `Source/DSP/SVF.h` | `SVF` |
| `Source/DSP/Biquad.h` | `Biquad` |
| `Source/DSP/Booster.h` | `Booster` |
| `Source/DSP/FXM.h` | `FXM` |
| `Source/DSP/RingMod.h` | `RingMod` |
| `Source/DSP/Smoother.h` | `Smoother` |
| `Source/DSP/DeterministicRandom.h` | `DeterministicRandom` |
| `Source/IO/SampleImporter.h` | `SampleImporter` |
| `Source/IO/PatchSerializer.h` | `PatchSerializer` |
| `Source/IO/LibraryIndex.h` | `LibraryIndex` |

模块编译锚（仅 include 本模块头文件，真实实现落地后移除/吸收）：
`Source/Params/Params.cpp`、`Source/Model/Model.cpp`、`Source/Engine/Engine.cpp`、`Source/DSP/DSP.cpp`、`Source/IO/IO.cpp`。

### 2.3 构建结构

- 新增静态库 `myJV_core`（上述 5 个编译锚），include 根为 `Source/`
- 插件目标 `myJV` 链接 `myJV_core`
- 不实现任何接口行为；不引入 JUCE 依赖（见 4.1 F1 与 ADR-007）

## 3. 构建证据

### 3.1 配置

命令：`D:\Program\CMake\bin\cmake.exe -B build -G "Visual Studio 18 2026" -A x64`

`configure.log` 末行：`CONFIGURE_EXIT=0`

### 3.2 构建

命令：`D:\Program\CMake\bin\cmake.exe --build build --config Release`

`build.log` 末行：`BUILD_EXIT=0`

| 指标 | 数量 |
| --- | --- |
| error（C/LNK/MSB） | **0** |
| warning（全部来源） | **0** |
| 模块编译锚 | 5/5 编译（Params/Model/Engine/DSP/IO） |
| `myJV_core.lib` | 3,166 字节（仅 5 个锚对象） |

### 3.3 产物（插件未变更功能，链接结构变更后重生成）

| 产物 | SHA-256 |
| --- | --- |
| `build/myJV_artefacts/Release/Standalone/myJV.exe` | `688FB05CC4E0E9567F04D0FF71013223A8739F2D43BA8A725896394F66E50334` |
| `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` | `8C7403CFF32841B3F59417EFD26243F4FB3E03A1C1AFC829BA3CA793B7FC7039` |

### 3.4 Standalone 启动冒烟

`smoke.log` 原始记录：`result: ALIVE after 5s (pid=19740)`。

## 4. 发现与修复

### 4.1 F1：核心库链接 JUCE 导致重复编译与 100MB 静态库

| 项 | 内容 |
| --- | --- |
| 现象 | 首次构建后 `myJV_core.lib` 为 105,667,142 字节；`build-attempt1-core-linked-juce.log` 显示 JUCE 模块源码（如 harfbuzz）在 `myJV_core.vcxproj` 中编译 |
| 根因 | JUCE 模块是 INTERFACE 库（`JUCEModuleSupport.cmake:97-100` 的 `target_sources(INTERFACE ...)`），任何链接它们的 target 都会编译全部模块源码；核心库与插件目标因此各编译一份 JUCE |
| 修复 | `myJV_core` 不再链接 `juce_audio_utils`（骨架无 JUCE 依赖）；库从 105 MB 降至 3,166 字节；构建 0 error / 0 warning |
| 后续 | JUCE 链接策略（共享代码再导出 vs 源列表）决策点延后至 M1-01，见 ADR-007 |

## 5. 待补证据

- [x] 参数核对表：不适用（无自动化参数）
- [x] 变异抽查：不适用（无行为可测；本任务为目录/编译骨架，TDD 明示例外，已随设计批准）
- [ ] macOS/Linux 构建日志（M0-06 CI）
- [ ] 宿主加载截图：沿用 M0-01（插件功能未变，二进制已重生成）

## 6. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `configure.log` | 配置（原始输出 + `CONFIGURE_EXIT=0`） |
| `build.log` | 最终构建（原始输出 + `BUILD_EXIT=0`） |
| `build-attempt1-core-linked-juce.log` | F1 现象（JUCE 编入 `myJV_core.vcxproj`） |
| `smoke.log` | Standalone 冒烟原始记录 |
