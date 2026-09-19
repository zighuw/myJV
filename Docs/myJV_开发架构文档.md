# myJV 合成器软件开发架构文档

| 项目 | 内容 |
| --- | --- |
| 项目代号 | **myJV**（JV-1080 Patch 参数体系 + 自定义采样器） |
| 文档版本 | v1.4 |
| 技术栈 | C++20 + JUCE（VST3 / AU / 独立程序） |
| 采样器规格 | 多采样 + 键区/力度层 + 循环点 |
| 不包含 | EFX / Chorus / Reverb 等效果器；Performance 多声部模式；GM；Rhythm Set（接口预留） |

### v1.1 修订说明

本版相对 v1.0 的变更：

1. **移除与参考实现的功能对比内容**：删除差异对比表、实现状态映射表、外部校准流程等章节与措辞，全部改为自洽的产品规格与架构描述。
2. **声部池修正**：v1.0 的 32 个 `NoteVoice` 池无法覆盖"1 Tone/音符 × 64 声部"的最坏情况；改为 **64 个 Tone 级声部（`ToneVoice`）固定池 + Note 持有 1–4 个 `ToneVoice` 索引**。
3. **实时内存回收修正**：音频线程不再使用 `std::atomic<std::shared_ptr>` 或任何引用计数操作；改为**不可变资产（AssetId）+ 退休队列（消息线程回收）+ 裸指针快照**。
4. **结构渲染修正**：v1.0 的 `ToneVoice::render()` 直接输出，无法实现 Structure 2–4 的跨 Tone 路由；改为**分阶段接口**（`updateModulators / processWG / processTVF / processTVA / addToBus`），由 `TonePairProcessor` 按结构编排。
5. **快照所有权修正**：明确"**活动 Runtime 的快照由音频线程每块刷新；退休 Runtime 的快照冻结**"，解决实时编辑与 Patch Remain 的冲突。
6. **力度层修正**：Zone 在 Note-On 时一次性选定（力度在音符生命周期内固定），删除不成立的"运行期力度层切换"。
7. **输出总线修正**：JUCE 总线布局在编译期固定（Main + Out1 + Out2），不支持运行时增删。
8. **包络速率修正**：A-ENV 逐采样推进（避免 zipper noise）；P-ENV/F-ENV/LFO 允许后续优化为控制率 + 插值。
9. **MIDI 处理修正**：按事件采样位置切分子块渲染，保证采样精度。
10. **移除外部校准流程**：改为应用内听感/频谱调试的**映射常量表**，并可随时调整。

### v1.2 审查修订说明

本版为严谨性审查修订：

1. **资产生命周期补全**：`AssetId` 明确仅用于 `PatchRuntime`；`ZoneSet`/`Sample` 通过 `shared_ptr` 链由 Runtime 保活，物理释放在最后一个引用（消息线程）消失时发生；`oldestAssetInUse` 仅比较 Runtime id，且活动 Runtime 永不被回收。
2. **性能预算修正**：声部上限为 64 个 Tone 级声部，修正原"64 声 × 4 Tone"的重复计算与 GFlops 算术错误。
3. **参数计数修正**：附录 A 去掉分组重叠（F-ENV/A-ENV 被重复计数），总量与正文一致（约 515）。
4. **滤波器更新频率修正**：Cutoff 被逐采样调制，SVF 系数按控制率/逐采样更新（TPT 支持），与 4.3 的平滑策略一致。
5. **声部分配补全**：新增 `kMaxNotes`；抢占引入 KillFading 状态，淡出未完成的声部不计为空闲。
6. **总线接口修正**：`Note::render` 接收全部三组立体声总线；明确 Tone 输出分配与 Patch Level/Pan 的作用范围。
7. **命名统一**：`Voice` 与 `ToneVoice` 统一为 `ToneVoice`，消除文档内两种称呼。
8. **每声部 DSP 状态补全**：明确 FXM 状态属于 `ToneVoice`，Booster/RingMod 状态属于 `TonePairProcessor`。
9. **Tone Delay 机制明确**：Normal/Hold/Key Interval 的引擎状态与 Note-Off 取消逻辑。
10. **其他**：Ext Sync 在 Standalone 下回退到 Default Tempo；Tone Switch 在 Note-On 时确定；程序切换后台加载期间旧补丁继续发声；补充 `prepare()` 与采样率变化处理。

### v1.3 审查修订说明

本版为跨文档一致性审查修订：

1. **采样回收描述统一**：6.1 中"采样移除走退休队列"的旧表述与 3.3 规则 6 的 `shared_ptr` 引用链方案统一。
2. **访问规则表修正**：`Sample` 的回收条件改为"最后一个引用（消息线程）释放后"，不再限定于 Runtime。
3. 其余内容与《myJV 开发计划文档 v1.1》交叉核对一致（术语、参数计数、声部模型、性能预算）。

### v1.4 最终校对说明

本版为发布前最终校对：

1. **命名残留清理**：3.5、5.4、5.10、9.2 中残留的 `Voice` 表述统一为 `ToneVoice`；`kill()` 注释与 KillFading 语义对齐。
2. **里程碑时点对齐**：13.1 中"多输出总线 AU 兼容性"的验证时点由 M5 修正为 M6（与计划 M6-05 一致）。
3. 表格完整性、章节结构与交叉引用已自动化校验通过。

---

## 目录

1. [项目概述](#1-项目概述)
2. [技术选型](#2-技术选型)
3. [总体架构](#3-总体架构)
4. [参数系统](#4-参数系统)
5. [声音引擎](#5-声音引擎)
6. [采样器子系统](#6-采样器子系统)
7. [用户界面](#7-用户界面)
8. [文件与数据格式](#8-文件与数据格式)
9. [开发环境与目录结构](#9-开发环境与目录结构)
10. [测试与验证](#10-测试与验证)
11. [里程碑计划](#11-里程碑计划)
12. [性能预算](#12-性能预算)
13. [风险与对策](#13-风险与对策)
14. [附录 A：参数组总览](#附录-a参数组总览)
15. [附录 B：关键数据结构草案](#附录-b关键数据结构草案)
16. [附录 C：映射常量](#附录-c映射常量)
17. [附录 D：参考资料](#附录-d参考资料)

---

## 1. 项目概述

### 1.1 目标

开发一款软件合成器，完整实现 **JV-1080 Patch 模块的参数体系与声音行为**：

- Patch Common（公共参数：电平、声像、滑音、结构、Voice Priority、Analog Feel、Stretch Tune 等）
- 最多 4 个 Tone，每个 Tone 包含 WG / TVF / TVA / 2×LFO / 3×包络 / Control 调制矩阵
- Structure 1–4 信号结构（含 Booster 与环形调制）
- 64 声部动态分配、Mono/Poly、Legato、Portamento、Patch Remain
- **波形源使用自定义多采样器**，取代固定的波形选择：
  - 每个 Tone 挂载一个 **Zone Set**（采样映射表）
  - 支持键区（Key Range）、力度层（Velocity Layer）、根音（Root Key）、循环点（Loop）、微调、增益、反向

### 1.2 范围

**包含（v1.0）**

| 模块 | 内容 |
| --- | --- |
| Patch Common | 名称、电平、声像、Analog Feel、Bend Range、Key Assign、Legato、Portamento、Octave Shift、Stretch Tune、Voice Priority、Structure/Booster、Control Source 2/3、Control Hold/Peak |
| Tone ×4 | WG（采样器版本）、TVF、TVA、LFO1/2、P-ENV/F-ENV/A-ENV、Control 1/2/3 矩阵、Tone Delay、FXM、Bias |
| Structure | Type 1–4（含 Booster、Structure 4 的环形调制） |
| 声部管理 | 64 声部、Last/Loudest 抢占、Mono/Poly、Legato、Portamento、Patch Remain |
| 采样器 | 多 Zone、键区/力度层、根音、循环点/循环交叉淡化、WAV/AIFF/FLAC 导入、采样库管理 |
| MIDI | Note、Velocity、Pitch Bend、CC（Mod/Volume/Pan/Expression/Hold-1 等）、Channel/Poly Aftertouch、Program Change |
| 输出 | 固定总线布局：Main L/R + Out1 L/R + Out2 L/R（宿主不支持多输出时仅 Main） |
| 插件格式 | VST3、AU（macOS）、Standalone |

**不包含（v1.0）**

- EFX / Chorus / Reverb 等效果器（数据模型中保留预留字段，不接线）
- Performance（16 Part 多音色）、GM 模式、Rhythm Set
- SysEx 全量数据兼容（仅提供可选的参数导入，见 8.4）

### 1.3 设计原则

1. **参数即规格**：所有可编辑量都有稳定的参数 ID、类型、范围与默认值，可序列化、可自动化。
2. **实时安全优先**：音频线程无分配、无锁、无文件 IO、无引用计数操作。
3. **不可变资产**：采样、Zone Set、补丁运行数据在发布后只读，通过原子指针交换与退休队列管理生命周期。
4. **数据与 DSP 分离**：模型层（Patch/Tone/ZoneSet/Sample）不依赖 DSP；引擎层只读模型。
5. **可扩展**：效果器、多 Patch 实例、Rhythm Set 等均以接口预留方式设计，不破坏现有结构。
6. **采样器为唯一波形源**：所有发声都经过采样器子系统，不保留内置波形分支。

---

## 2. 技术选型

### 2.1 语言与框架

| 项 | 选择 | 理由 |
| --- | --- | --- |
| 语言 | C++20 | `std::span`、concepts、指定初始化、`constexpr` 工具 |
| 框架 | JUCE 8（或 7.x） | 跨平台插件封装、UI、音频设备、文件格式、参数自动化 |
| 构建 | CMake ≥ 3.22 + JUCE `juce_add_plugin` | 一套工程输出 VST3/AU/Standalone |
| 测试 | Catch2 v3（或 JUCE UnitTest） | DSP 单元测试与基准音频回归 |
| 序列化 | JUCE `ValueTree` ↔ JSON（`juce::JSON`） | 补丁文件与宿主状态 |
| 编译器 | MSVC 2022 / Clang 16+ / GCC 12+ | C++20 完整支持 |

### 2.2 依赖库

| 依赖 | 用途 | 说明 |
| --- | --- | --- |
| JUCE | 插件/UI/IO | 注意授权：GPLv3 或商业授权 |
| Catch2 | 单元测试 | 仅测试目标链接 |
| （可选）libsamplerate / r8brain | 高质量离线重采样 | 采样导入时预重采样 |
| （可选）dr_libs | 额外音频格式 | JUCE 已内置 WAV/AIFF/FLAC，一般不需要 |

### 2.3 输出目标

- **VST3**（Windows/macOS/Linux）
- **AU**（macOS，可选）
- **Standalone**（开发调试）
- 采样率 44.1–192 kHz；块大小 32–2048 帧
- 输入：MIDI In；输出：Main L/R + Out1 L/R + Out2 L/R（固定布局）
- 音频线程只处理浮点；输出不做采样率转换（由宿主处理）；采样播放包含采样率比换算（见 5.4）

---

## 3. 总体架构

### 3.1 分层架构

```
┌─────────────────────────────────────────────────────────────┐
│ 应用层（JUCE 插件外壳）                                      │
│  PluginProcessor / PluginEditor / Standalone / PresetManager │
├─────────────────────────────────────────────────────────────┤
│ UI 层                                                       │
│  Patch 页面 / Tone 页面 / 采样器页面 / 控件库 / LookAndFeel  │
├─────────────────────────────────────────────────────────────┤
│ 参数层                                                      │
│  ParameterRegistry（APVTS）/ ParamSnapshot / Mapping         │
├─────────────────────────────────────────────────────────────┤
│ 模型层                                                      │
│  Patch / Tone / ZoneSet / Sample / SampleLibrary             │
├─────────────────────────────────────────────────────────────┤
│ 引擎层（DSP）                                               │
│  SynthEngine / VoiceManager / Note / ToneVoice /             │
│  TonePairProcessor（Structure）/ AssetReclaimer              │
├─────────────────────────────────────────────────────────────┤
│ DSP 基础层                                                  │
│  SamplePlayer / Envelope / LFO / SVF / Biquad / Booster /    │
│  FXM / RingMod / Smoother / DeterministicRandom              │
├─────────────────────────────────────────────────────────────┤
│ 平台层                                                      │
│  JUCE AudioProcessor / AudioFormatManager / MidiBuffer       │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 模块清单与职责

| 模块 | 主要类 | 职责 |
| --- | --- | --- |
| Plugin | `MyJVProcessor`、`MyJVEditor` | 插件生命周期、状态存取、MIDI 收集、UI 宿主 |
| Params | `ParameterIDs`、`ParamSnapshot`、`Calibration` | 参数定义、快照、映射常量 |
| Model | `Patch`、`Tone`、`ZoneSet`、`Sample`、`SampleLibrary` | 声音数据模型（不依赖 DSP） |
| Engine | `SynthEngine`、`VoiceManager`、`Note`、`ToneVoice`、`TonePairProcessor`、`AssetReclaimer` | 复音管理、逐采样渲染、结构路由、资产回收 |
| DSP | `SamplePlayer`、`Envelope`、`LFO`、`SVF`、`Biquad`、`Booster`、`FXM`、`RingMod`、`Smoother`、`DeterministicRandom` | 无状态/轻状态 DSP 单元 |
| IO | `SampleImporter`、`PatchSerializer`、`LibraryIndex`、`SysExImporter`（可选） | 文件读写、后台解码、序列化 |
| UI | `PatchPage`、`TonePage`、`SamplerPage`、`Knob`、`EnvelopeView`、`ZoneMapView`、`FilterCurveView` | 参数编辑与可视化 |
| Tests | `dsp_tests`、`mapping_tests`、`render_tests` | 单元/基准音频回归 |

### 3.3 运行时对象与生命周期（v1.1 核心修正）

**不可变资产**（发布后只读，音频线程只持裸指针）：

| 资产 | 内容 | 说明 |
| --- | --- | --- |
| `PatchRuntime` | `id` + `PatchSnapshot` + `ZoneSet[4]` | 一次"补丁实例"的完整运行数据 |
| `ZoneSet` | 某 Tone 的采样映射表 | 由 Runtime 持有 |
| `Sample` | PCM 数据与元数据 | 由 `SampleLibrary` 持有 |

**生命周期规则**：

1. 单调递增的 `AssetId` **仅分配给 `PatchRuntime`**；`ZoneSet`/`Sample` 不参与 id 比较，其存活由 `shared_ptr` 引用链保证（见规则 6）。
2. `PatchRuntime` 的创建与发布在消息/后台线程完成；音频线程通过原子指针读取当前活动 Runtime。
3. **活动 Runtime 的快照由音频线程每块刷新**（从 APVTS 读取）；**退休 Runtime 的快照冻结**（Patch Remain 的旧音继续使用冻结值）。
4. `AssetReclaimer` 维护退休队列：音频线程每块结束扫描活动 Note 的 Runtime id，记录 `oldestAssetInUse`（原子变量，仅比较 Runtime id；无活动 Note 时记录当前活动 Runtime 的 id；引擎首次渲染前保持"不回收"初值）。消息线程定时器回收 `id < oldestAssetInUse` 且退休超过宽限期（默认 500 ms）的 Runtime；**当前活动 Runtime 永不被回收**。
5. **音频线程绝不执行任何释放/引用计数操作**，只读取裸指针。
6. `ZoneSet`/`Sample` 的存活：`PatchRuntime` 通过 `shared_ptr` 持有其 `ZoneSet`，`ZoneSet` 通过 `shared_ptr` 持有 `Sample`；只要任一 Runtime 尚未被回收，其引用的 ZoneSet/Sample 就不会被释放；最后一个引用消失时，物理释放在消息线程完成。

**访问规则表**：

| 对象 | 创建/发布 | 音频线程 | 回收 |
| --- | --- | --- | --- |
| `PatchRuntime` | 消息/后台线程 | 裸指针只读；仅刷新活动实例的快照 | 消息线程退休队列 |
| `ZoneSet` | 后台线程（Zone 编辑） | 经快照裸指针只读 | 由 `PatchRuntime` 持有（`shared_ptr`）；最后一个引用消失时释放 |
| `Sample` | 后台线程（导入） | 经 `ZoneSet` 只读 | 由 `ZoneSet` 的 `shared_ptr` 链保活；最后一个引用（消息线程）释放后回收 |
| APVTS | 消息线程 | 原子读 | — |
| `Note` / `ToneVoice` 池 | 引擎构造期 | 读写 | — |

### 3.4 线程模型

```
┌──────────────┐    APVTS 原子参数          ┌──────────────────┐
│  Message     │ ──────────────────────────▶ │  Audio Thread    │
│  Thread (UI) │                             │  processBlock()  │
│              │ ◀────────────────────────── │                  │
└──────┬───────┘    统计值（原子）           └────────┬─────────┘
       │                                              │
       │ 加载补丁/采样/Zone 编辑                       │ 裸指针读取
       ▼                                              ▼
┌──────────────┐   解码/重采样/构建资产      ┌──────────────────┐
│  Background  │ ──────────────────────────▶ │ PatchRuntime /   │
│  Thread(s)   │   原子发布（AssetId）        │ ZoneSet / Sample │
└──────────────┘                             └──────────────────┘
       │                                              ▲
       │ 退休资产（消息线程定时回收）                    │
       └──────────────────────────────────────────────┘
                    AssetReclaimer
```

**补丁加载流程**（消息/后台线程）：

1. 解析补丁 JSON → 解析采样引用 → 必要时从采样库加载。
2. 构建 `ZoneSet[4]` → 构建 `PatchRuntime`（快照初值从 APVTS 读取）。
3. 原子发布新 Runtime；旧 Runtime 移交 `AssetReclaimer`。
4. 旧 Note 继续使用旧 Runtime（Patch Remain），直到自然释放。
5. Program Change 触发的补丁加载在后台线程执行；加载期间旧补丁继续正常发声，完成后发布新 Runtime。

**实时编辑流程**：APVTS 写入 → 音频线程下一块刷新活动 Runtime 快照 → 所有使用该 Runtime 的发声音符立即跟随。

**Zone 结构编辑流程**：构建新 `ZoneSet` 与新 `PatchRuntime` → 原子发布（旧音符保留旧采样，避免爆音与悬空指针）。

### 3.5 实时性约束

| 约束 | 措施 |
| --- | --- |
| 无锁 | APVTS 原子读、`std::atomic` 指针交换 |
| 无分配 | 所有 buffer 预分配；`Note`/`ToneVoice` 池固定大小；无 `std::vector` 扩容 |
| 无引用计数 | 资产用裸指针 + AssetId + 退休队列，禁止 `shared_ptr` 拷贝/析构 |
| 无文件 IO | 采样全预载（v1.0）或后台流式 + 无锁环形缓冲（v1.1+） |
| 无异常 | 音频路径 `noexcept`；错误在后台处理并上报 UI |
| 无 denormal | `juce::ScopedNoDenormals` |
| 分支可预测 | 参数快照 + 每块一次性分支决策（结构、滤波类型等） |

---

## 4. 参数系统

### 4.1 参数标识与注册

参数 ID 采用点分路径，按 `Patch → Tone → 功能组` 层级组织：

| 示例 ID | 含义 | 类型 | 范围 |
| --- | --- | --- | --- |
| `patch.common.level` | Patch 电平 | Float | 0–127 |
| `patch.common.pan` | Patch 声像 | Float | 0–127（64 居中） |
| `patch.common.bendUp` | 弯音范围上 | Int | 0–24 |
| `patch.common.structure12` | Structure 1&2 | Choice | 1–4 |
| `tone1.wg.waveGain` | Wave Gain | Choice | -6/0/+6/+12 dB |
| `tone1.wg.coarseTune` | 粗调 | Int | -24–+24 |
| `tone1.tvf.type` | 滤波类型 | Choice | Off/LPF/BPF/HPF/PKG |
| `tone1.tvf.cutoff` | 截止频率 | Float | 0–127 |
| `tone1.tvf.resonance` | 共振 | Float | 0–127 |
| `tone1.tva.level` | Tone 电平 | Float | 0–127 |
| `tone1.env.a.t1` … `t4` | A-ENV 时间 | Float | 0–127 |
| `tone1.lfo1.wave` | LFO1 波形 | Choice | 8 种 |
| `tone1.ctrl1.dest1` / `depth1` | 调制矩阵 | Choice/Float | — |

- 4 个 Tone 的参数通过循环批量生成（`createParameterLayout()` 中 `for tone in 1..4`）。
- 总自动化参数约 **500–550 个**；APVTS 可承受，但 UI 需按页懒加载。
- 非自动化数据（Zone Set、采样引用、UI 状态）不走 APVTS，由模型层持有。

### 4.2 参数快照（ParamSnapshot）

音频线程**不得**逐采样读取 APVTS，也**不得**重建退休 Runtime 的快照。规则：

| Runtime 状态 | 快照来源 | 行为 |
| --- | --- | --- |
| 活动 | 音频线程每块从 APVTS 刷新 | 实时编辑立即生效 |
| 退休 | Runtime 内冻结的快照 | Patch Remain 的旧音保持原参数 |

```cpp
struct ToneSnapshot {          // 纯 POD，可复制，无指针
    float coarseTune, fineTune, randomPitch, pitchKeyfollow;
    int   tvfType; float cutoff, resonance, cutoffKeyfollow, resVelSens;
    float pEnvDepth, fEnvDepth, fEnvVelSens, aEnvVelSens;
    EnvShape pEnv, fEnv, aEnv;            // 各 4T/4L + 灵敏度
    LfoShape lfo[2];                      // 波形/速率/延迟/淡入/同步
    float pitchLfoDepth[2], filterLfoDepth[2], ampLfoDepth[2], panLfoDepth[2];
    int   ctrlDest[3][4]; float ctrlDepth[3][4];
    float toneLevel, tonePan, panKeyfollow, randomPan, altPan;
    int   outputAssign; float outputLevel;
    float waveGain; int fxmColor; float fxmDepth; bool fxmOn;
    int   toneDelayMode; float toneDelayTime;
    float velXfade; int velLow, velHigh, keyLow, keyHigh;
    bool  redamper, volCtrl, holdCtrl, bendCtrl, panCtrl;
    // ...
};

struct PatchSnapshot {
    PatchCommonSnapshot common;
    std::array<ToneSnapshot, 4> tones;
    const ZoneSet* zoneSets[4];    // 裸指针，生命周期由 AssetReclaimer 保证
    uint64_t assetId;              // 所属 Runtime
};
```

### 4.3 参数平滑

需要连续平滑的参数（避免 zipper noise）：

| 参数 | 平滑方式 |
| --- | --- |
| Patch/Tone Level、Pan | 一阶低通（5–20 ms） |
| Cutoff、Resonance | 逐采样一阶平滑（5 ms）；SVF 系数按控制率（默认每 16 采样）或逐采样更新，见 5.5 |
| Pitch（tune/bend） | 每采样插值 |
| LFO 深度 | 每块更新，内部按相位连续 |
| 离散选择（类型/波形/开关） | 立即生效（必要时交叉淡化 1 块） |

### 4.4 参数映射常量（Calibration.h）

所有"数值 → 物理量"的映射集中在 `Calibration.h`，便于统一调试：

| 映射 | 默认公式 | 范围 |
| --- | --- | --- |
| 包络时间 | `ms = 1.0 * pow(20000.0, t/127)` | 1 ms – 20 s |
| 包络电平 | `gain = l/127` | 0–1 |
| Cutoff | `Hz = 20 * pow(1000.0, c/127)` | 20 Hz – 20 kHz |
| Resonance | `Q = 0.5 * pow(40.0, r/127)` | 0.5 – 20 |
| LFO 速率 | `Hz = 0.05 * pow(400.0, r/127)` | 0.05 – 20 Hz |
| Tone Delay | `ms = 1.0 * pow(2000.0, t/127)` | 1 ms – 2 s |
| Portamento | `ms = 1.0 * pow(5000.0, t/127)` | 1 ms – 5 s |
| Keyfollow | `factor = pow(2, kf * (note-60)/12)` | ±100% |
| Bend | `semitones = range * bend/8192` | ±0–24 |
| Master Tune | A4 = 440 Hz 为中心 | — |

> 具体常量的调试方式见附录 C；修改常量后必须更新基准音频回归测试的容差。

### 4.5 序列化

- 参数 → `ValueTree` → JSON（补丁文件）
- 宿主状态：`getStateInformation()` 写入当前 Patch JSON + 采样库路径 + UI 状态
- 版本迁移：JSON 中保留 `schemaVersion`，提供迁移函数表

---

## 5. 声音引擎

### 5.1 数据模型

```
Patch
 ├─ PatchCommon
 │   ├─ name / level / pan
 │   ├─ analogFeel / bendUp / bendDown / octaveShift / stretchTune
 │   ├─ keyAssign(Poly/Mono) / legato / portamento{sw,mode,type,start,time}
 │   ├─ voicePriority(Last/Loudest)
 │   ├─ structure{type12, booster12, type34, booster34}
 │   ├─ ctrlSource2 / ctrlSource3 / holdPeak[4]
 │   └─ 预留区（效果器相关，v1.0 不接线）
 └─ Tone[4]
     ├─ common: toneSwitch / output / level / pan / control switches
     ├─ WG: zoneSetRef / waveGain / fxm / toneDelay / velXfade / keyRange / velRange
     ├─ TVF: type / cutoff / keyfollow / resonance / resVelSens / fEnv
     ├─ TVA: level / bias / aEnv / pan 相关
     ├─ LFO[2]: wave / rate / delay / fade / keyTrig / sync / levelOffset
     ├─ ENV[3]: P/F/A（4T + L1–4 / L1–3）
     └─ CTRL[3]: dest[4] + depth[4]
```

### 5.2 声部管理（VoiceManager）

**声部计数**：**以 Tone 为单位**（每个正在发声的 Tone 占用 1 个声部），上限 64。

**对象模型**（v1.1 修正）：

```
ToneVoice（Tone 级声部，固定池 64 个 = kMaxVoices）
 ├─ 绑定 Note 编号、toneIndex
 ├─ 捕获的 Runtime 指针（Patch Remain 依据）
 ├─ SamplePlayer / 3×Envelope / 2×LFO / SVF / FXM / 声像状态
 ├─ 状态：Free / Active / Releasing / KillFading
 └─ 当前电平（用于 Loudest 抢占）

Note（一个 MIDI 音符，固定池 64 个 = kMaxNotes）
 ├─ 持有 1–4 个 ToneVoice 索引（由 toneSwitch 在 Note-On 时确定）
 ├─ TonePairProcessor[2]（Structure 编排 + Booster/RingMod 状态）
 └─ 触发顺序、力度、踏板状态
```

**分配算法**：

1. 计算新 Note 需要的声部数 N（Note-On 时启用的 Tone 数，之后不再改变）。
2. 可用声部 = Free；KillFading 中的声部在淡出完成前不可用。
3. 若可用声部 < N，按抢占策略选择牺牲 Note：优先将其声部置为 KillFading（`kVoiceFadeMs` 快速淡出）；若仍不足 N，对最旧的 KillFading 声部执行硬接管（立即复用，可能产生轻微跳变，属极端情况）。
4. 从 Free 表取 N 个声部，绑定到新 Note 并捕获当前 Runtime。

**抢占策略**：

| 策略 | 行为 |
| --- | --- |
| Last | 牺牲触发顺序最早的 Note |
| Loudest | 牺牲当前电平最小的 Note（电平每块更新） |

**其他演奏行为**：

- **Mono**：仅一个 Note 槽；`Legato=On` 时复用同一组 ToneVoice，不重触发包络，仅更新目标音高（滑音）。
- **Portamento**：引擎提供滑音状态机（时间模式 / 速率模式、起始方式），具体语义由参数规格定义。
- **Hold-1（延音踏板）**：按 Note 记录踏板状态；踏板按下期间 Note-Off 进入待释放，踏板抬起时释放。
- **All Notes Off / All Sound Off**：立即释放或快速淡出全部 Note。
- **Patch Remain**：切换 Runtime 时旧 Note 继续使用旧 Runtime，直到自然释放。
- **Tone Switch 变更**：声部构成仅在 Note-On 时确定；已发声音符不受开关变化影响，避免中途断音。
- **试听声部**：独立于 64 声部池的 1 个 Audition 声部（独立 ToneVoice），供 UI 采样试听。

### 5.3 渲染流程

```
SynthEngine::processBlock(buffer, midi)
  1. juce::ScopedNoDenormals
  2. 刷新活动 Runtime 快照（从 APVTS 原子读取）
  3. 按 MIDI 事件采样位置把块切分为子块（采样精度）
  4. 对每个子块：
     a. 处理事件（Note-On/Off、CC、Bend、Aftertouch、Patch Change）
     b. 对每个活动 Note：Note::render(buses, 子块)（读取其所属 Runtime 的快照，活动/退休一致）
  5. Patch Level/Pan（平滑后）应用于全部总线 → 输出 Main / Out1 / Out2
  6. 更新统计（声部数、电平、oldestAssetInUse；无活动 Note 时取活动 Runtime id）
```

**Note::render 内每采样顺序**（保证调制顺序一致）：

```
1. LFO1 / LFO2 推进（两 Tone）
2. P-ENV 推进 → 计算音高
3. F-ENV 推进 → 计算截止频率
4. A-ENV 推进 → 计算音量
5. 按 Structure 编排各阶段（WG → TVF → TVA）
6. 声像处理 → 累加到目标总线
```

**初始化与采样率变化**：`prepare(sampleRate, maxBlockSize)` 在非实时线程完成全部缓冲分配与状态复位；采样率变化时仅更新引擎采样率（采样播放比率中的 `sample.sr / engine.sr` 自动生效），采样数据无需重新加载。

### 5.4 WG 与采样器（分阶段接口，v1.1 修正）

`ToneVoice` 不再直接输出到总线，而是暴露分阶段接口，供 `TonePairProcessor` 按结构自由编排：

```cpp
class ToneVoice {
public:
    void  startNote(const ToneSnapshot&, const ZoneSet*, int note, float vel,
                    double engineSampleRate, uint64_t rngSeed) noexcept;
    void  release() noexcept;
    void  kill() noexcept;                     // 抢占：快速淡出（KillFading）

    void  updateModulators() noexcept;         // LFO/ENV/音高计算，每采样
    float processWG() noexcept;                // 采样播放 + WaveGain + FXM
    float processTVF(float in) noexcept;       // 含 F-ENV/LFO/矩阵调制
    float processTVA(float in) noexcept;       // 含 A-ENV/Bias/矩阵调制
    void  addToBus(float sample, BusBuffers& buses) noexcept; // 声像 + 输出电平 + 输出分配
    bool  finished() const noexcept;
};
```

**状态归属**：`ToneVoice` 持有 SamplePlayer、包络、LFO、SVF、FXM、声像与平滑状态；`TonePairProcessor` 持有 Booster/RingMod 的过采样与 DC 阻断状态（每个 Note 的两对 Tone 各一份）。

**采样器播放要点**：

| 项 | 实现 |
| --- | --- |
| 音高比 | `ratio = (sample.sr / engine.sr) * 2^((note - rootKey + tune + bend + mod)/12)` |
| 插值 | 默认 4 点 Hermite；高质量模式 8 点加窗 sinc（可配置） |
| 循环 | `Off / Forward / Sustain`；`Sustain` = 按键期间循环，释放后继续循环直到 A-ENV 结束 |
| 循环交叉淡化 | 线性/等功率交叉淡化 |
| 反向 | 读取方向取反 |
| 起始/结束去点击 | Note-On 淡入 1–2 ms；ToneVoice 结束时淡出 |
| Zone 选择 | **Note-On 时一次性选定**（力度在音符生命周期内固定） |
| 单声道/立体声 | 统一输出 L/R；单声道采样复制到两声道 |

**Zone 选择策略**（Note-On 时）：

1. 过滤出 `keyLow ≤ note ≤ keyHigh` 且 `velLow ≤ velocity ≤ velHigh` 的 Zone。
2. 多个命中时：优先键区最窄者（`keyHigh − keyLow` 最小）→ `velLow` 最大者 → 列表靠前者。
3. 无命中：该 Tone 静音。
4. Tone 自身的 Key Range / Velocity Range 作为二次裁剪（与 Zone 区间取交集）。

### 5.5 TVF 滤波器

| 类型 | 实现 |
| --- | --- |
| Off | 直通 |
| LPF | TPT/ZDF 状态变量滤波器（SVF）低通输出，12 dB/oct |
| BPF | 同 SVF 带通输出 |
| HPF | 同 SVF 高通输出 |
| PKG（Peaking） | RBJ 双二阶 peaking EQ（固定 Q/增益映射，作为静态 EQ） |

- Cutoff 调制源：Keyfollow、F-ENV、LFO1/2、力度、Control 矩阵；合成后限制在 20 Hz–20 kHz。
- Resonance 映射到 SVF 的 Q；接近自激时做软限幅保护。
- 稳定性：TPT 拓扑支持参数快速变化而无需重建滤波器结构；系数按控制率（默认每 16 采样）或逐采样更新，以跟随逐采样的 F-ENV/LFO 调制。
- 滤波类型在每块开头读取快照一次，避免逐采样分支抖动。

### 5.6 TVA 与声像

| 参数 | 实现 |
| --- | --- |
| Tone Level | A-ENV 输出乘系数，块内平滑 |
| Bias Direction/Point/Level | 波形非对称偏置：`y = shape(x, direction, point, level)`，方向决定削顶侧，point 为作用点，level 为强度 |
| A-ENV | 4 段包络（L1–L3，L4=0），逐采样 |
| Velocity Curve / Sens | 7 种曲线（线性、指数、对数等）+ 灵敏度缩放 |
| Amplitude LFO 1/2 | 乘法调制（深度 0–127） |
| Tone Pan / Pan Keyfollow / Random Pan / Alt Pan / Pan LFO | 声像合成后使用等功率 pan 法则（-3 dB 中心） |
| Output | Main / Out1 / Out2（编译期固定总线） |
| Output Level | Tone 输出电平，平滑处理 |

### 5.7 包络（P-ENV / F-ENV / A-ENV）

四段折线包络状态机：

```
状态：Idle → Attack(T1→L1) → Decay(T2→L2) → Sustain(T3→L3, 保持)
        Note-Off → Release(T4→L4)
```

| 参数 | P-ENV | F-ENV | A-ENV |
| --- | --- | --- | --- |
| Time 1–4 | ✓ | ✓ | ✓ |
| Level 1–4 | ✓（4 个） | ✓（4 个） | L1–L3（L4 固定 0） |
| Depth | ±（半音） | ±（滤波调制量） | — |
| Velocity Sens | ✓ | ✓ | ✓ |
| Velocity Curve | — | ✓ | ✓ |
| Time Keyfollow | ✓ | ✓ | ✓ |
| Vel Time 1/4 Sens | ✓ | ✓ | ✓ |

- **A-ENV 逐采样推进**（音量路径，避免 zipper noise）。
- P-ENV/F-ENV v1.0 同样逐采样；后续优化可降为控制率 + 线性插值。
- **Release 从当前电平出发**（不是从 L3），保证快速重触发时无跳变。
- 时间映射见 4.4；常量集中在 `Calibration.h`。

### 5.8 LFO

| 参数 | 实现 |
| --- | --- |
| 波形（8 种） | Sine / Sawtooth / Square / Triangle / Trapezoid / Sample&Hold / Random / Chaotic |
| Key Trigger | Note-On 时相位归零 |
| Rate | 0–127 → 0.05–20 Hz（见 4.4） |
| Level Offset | 输出加 DC 偏移（改变调制中心） |
| Delay Time | Note-On 后延迟启动（计时器） |
| Fade Mode / Time | 延迟后淡入（线性/指数） |
| Ext Sync | 同步到宿主 BPM：Rate 映射为音符时值（1/1 … 1/32、附点/三连）；Standalone 或宿主无速度信息时回退到 Patch 的 Default Tempo |
| 调制目标 | Pitch、Cutoff、Amp、Pan（各 Tone 独立深度） |

- 波形实现：Sine（多项式/查表）、Saw/Square/Triangle/Trapezoid（相位分段函数）、S&H（每周期采样随机）、Random（随机值间平滑插值）、Chaotic（Logistic 映射 `x' = r·x(1-x)`，r≈3.9，按 Rate 推进）。
- 所有随机源支持**确定性种子**（测试与基准音频回归用），并可切换为真随机。

### 5.9 调制矩阵（Control 1/2/3）

- 每组：1 个来源 + 4 个目标（Dest）+ 4 个深度（Depth）。
- 来源解析：
  - Control 1：固定来源（默认 Modulation CC1，可在设置中更改）
  - Control 2/3：Patch Common 的 `Control Source 2/3`（CC 号、Pitch Bend、Aftertouch 等）
- 目标列表（v1.0 子集，可按参数规格扩充）：
  `OFF / PITCH / CUTOFF / RESONANCE / LEVEL / PAN / LFO1_RATE / LFO1_PITCH / LFO1_CUTOFF / LFO1_AMP / LFO1_PAN / LFO2_* / P_ENV_TIME / P_ENV_LEVEL / F_ENV_TIME / F_ENV_LEVEL / A_ENV_TIME / A_ENV_LEVEL / TONE_DELAY_TIME`
- **作用点**：音高在 `updateModulators` 中应用；Cutoff 在 `processTVF` 中应用；Level/Pan 在 `processTVA`/`addToBus` 中应用；LFO 深度/包络时间在每块更新时应用。
- **Hold/Peak**：控制器值跟踪模式——Hold 在保持期间冻结当前值；Peak 更新到新的峰值（按 Patch Common 的 4 组设置）。
- 来源值经平滑后与深度相乘，逐采样叠加到目标。

### 5.10 Structure 1–4

每个 Tone 对（1/2、3/4）由 `TonePairProcessor` 按结构编排（`v1` = Tone 1 的 ToneVoice，`v2` = Tone 2 的 ToneVoice）。每采样先对 `v1`、`v2` 各调用一次 `updateModulators()`（推进 LFO/包络并计算音高），再执行下述阶段：

```
Type 1:  a = v1.processTVA(v1.processTVF(v1.processWG()))
         b = v2.processTVA(v2.processTVF(v2.processWG()))
         v1.addToBus(a, buses); v2.addToBus(b, buses)

Type 2:  a = v1.processTVA(booster(v1.processTVF(v1.processWG())))
         b = v2.processTVA(v2.processTVF(v2.processWG()))
         v1.addToBus(a, buses); v2.addToBus(b, buses)

Type 3:  a = v1.processTVA(v1.processTVF(v1.processWG()))
         b = v2.processTVA(booster(v2.processTVF(v2.processWG())))
         v1.addToBus(a, buses); v2.addToBus(b, buses)

Type 4:  a  = v1.processTVA(v1.processWG())        // Tone 1 不经 TVF
         m  = ringMod(a, v2.processWG())           // 环形调制（乘法）
         m  = v1.processTVF(m)                     // Tone 1 的滤波器
         m  = booster(m)
         m  = v2.processTVF(m)                     // Tone 2 的滤波器
         v2.addToBus(v2.processTVA(m), buses)      // Tone 2 的 TVA/声像为末级
```

- **Booster**：非线性饱和（`tanh`/软削波），2× 过采样，Booster Level 0–127 映射驱动量。
- **RingMod**：两信号相乘；加入 DC 阻断与软限幅。
- `addToBus` 内部完成声像、Tone 输出电平与输出分配（Main/Out1/Out2）。
- Type 4 中 Tone 1 的声像与输出分配不参与（其电平由 TVA1 控制），与信号流定义一致。
- Tone 3/4 对处理完成后与 Tone 1/2 对输出相加，进入 Patch 输出级。

### 5.11 FXM、Tone Delay、Bias

| 功能 | 实现要点 |
| --- | --- |
| FXM | 自相位调制：`phaseMod = fxmDepth * lastOut`；Color 1–7 控制反馈信号的低通/非线性特性；2× 过采样；逐采样计算 |
| Tone Delay | 每个 ToneVoice 的启动延迟计数器；**延迟期间调制器与包络不推进且输出为 0**；引擎状态：`delaySamples`、`startOnNoteOff`（Hold 模式）、`intervalScale`（Key Interval 模式，按上一 Note-On 的时间间隔缩放延迟）；延迟期间收到 Note-Off 时按模式取消或转为启动 |
| Bias | TVA 前置波形整形；Direction（偏置方向）、Point（作用位置）、Level（强度）；逐采样计算 |

### 5.12 输出与总线（无效果器）

```
TonePair(1,2) ─┐
TonePair(3,4) ─┴─▶ Patch Mix ─▶ Patch Level/Pan（平滑）─▶ Main L/R
                              ├▶ Out1 L/R（Tone 输出分配）
                              └▶ Out2 L/R（Tone 输出分配）
```

- 移除效果器后，信号直接进入 Patch 输出级；Patch Level/Pan 应用于全部输出总线（Main/Out1/Out2）。
- 总线缓冲以 `BusBuffers` 结构在引擎内传递（Main/Out1/Out2 三组 L/R，定义见附录 B）。
- **总线布局编译期固定**：Main + Out1 + Out2（各立体声）；宿主不支持多输出时仅 Main。
- 输出保护：可选软限幅开关，避免多 Tone 叠加削波。
- 电平表与声部计数以原子变量暴露给 UI。

---

## 6. 采样器子系统

### 6.1 采样库管理

- 采样库目录结构：

```
MyJV Library/
├─ library.json          # 采样索引（路径、哈希、rootKey、循环点、缩略图缓存）
├─ Piano/
│  ├─ Piano_C2.wav
│  └─ ...
└─ Strings/
```

- `SampleLibrary` 负责：扫描、导入、去重（哈希）、缓存、引用管理。
- 采样移除时 `SampleLibrary` 释放其引用；若仍被 ZoneSet/Runtime 引用则继续存活，最后一个引用（消息线程）消失时释放（见 3.3 规则 6）。
- UI 提供：采样浏览器、试听（Audition Voice）、标签/收藏（可选）、缺失采样提示与重新定位。

### 6.2 Zone Set 编辑器

- 键区/力度层网格视图（横轴键位、纵轴力度），支持拖拽创建/调整 Zone。
- 波形显示：波形、循环点、交叉淡化区；支持缩放与试听（点击键位即发声）。
- 属性面板：根音、微调、增益、声像、循环模式、反向。
- 自动映射：按文件名解析音名（如 `Piano_C#3.wav`）自动设定 rootKey 与键区；力度层按文件名或导入顺序分配。

### 6.3 导入与发布流程（v1.1 修正）

```
1. 后台线程：解码 → 读取循环点 → 可选重采样 → 峰值缩略图 → 生成 Sample（不可变）
2. SampleLibrary 发布 Sample（`shared_ptr` 入索引）；是否释放由引用它的 ZoneSet/Runtime 决定，物理释放在消息线程
3. 用户编辑 Zone → 构建新 ZoneSet（不可变）
4. 构建新 PatchRuntime（含新 ZoneSet；快照初值从 APVTS 读取）
5. 原子发布新 Runtime；旧 Runtime 移交退休队列
   （旧 Note 继续使用旧 Runtime/旧采样，避免爆音与悬空指针）
```

### 6.4 大文件与流式播放（可选，v1.1+）

- 预载阈值（如 10 秒）以上采样改为磁盘流式：
  - 后台线程按 1 秒块预读，写入无锁环形缓冲（SPSC）。
  - 音频线程从环形缓冲读取，欠载时淡出并上报。
- v1.0 采用全预载，简化实现；接口预留 `SampleSource` 抽象。

---

## 7. 用户界面

### 7.1 页面结构

```
主窗口
├─ 顶部栏：Patch 名 / Level / Pan / 声部数 / CPU / 采样库状态
├─ 标签页：Patch Common │ Tone 1 │ Tone 2 │ Tone 3 │ Tone 4 │ Sampler │ Keyboard
│   ├─ Tone 页子标签：WG │ TVF │ TVA │ LFO │ ENV │ CTRL
│   └─ Sampler 页：Zone 映射编辑器 + 波形视图 + 采样浏览器
└─ 底部：虚拟键盘（试听/Zone 编辑联动）
```

### 7.2 自定义控件

| 控件 | 用途 |
| --- | --- |
| `JvKnob` / `JvSlider` | 旋钮/滑杆，支持拖动、双击复位、数值气泡 |
| `JvLcd` | 参数名 + 数值显示 |
| `EnvelopeView` | 4 段包络折线可视化与拖拽编辑 |
| `FilterCurveView` | 滤波器频响曲线（随 cutoff/resonance 实时更新） |
| `LfoScopeView` | LFO 波形预览 |
| `ZoneMapView` | 键区/力度层网格编辑 |
| `WaveformView` | 波形、循环点、交叉淡化显示与拖拽 |
| `ToneLevelMeter` | 每 Tone 电平表 |

### 7.3 绑定与刷新

- 自动化参数：`SliderAttachment` / `ComboBoxAttachment` / `ButtonAttachment`。
- 非自动化数据（ZoneSet 等）：模型 → `ChangeBroadcaster` → UI 刷新。
- UI 刷新率 30–60 Hz；音频相关统计走定时器（10–20 Hz）。
- 参数值显示需与映射函数一致（如 cutoff 显示 Hz、LFO 显示 Hz、包络显示 ms）。

### 7.4 状态与撤销

- 撤销/重做：`juce::UndoManager` 挂接 ValueTree（参数级）。
- 采样编辑操作（增删 Zone、移动循环点）纳入撤销栈（可选）。

---

## 8. 文件与数据格式

### 8.1 补丁文件（`.myjvp`，JSON）

```json
{
  "schemaVersion": 1,
  "name": "My Patch",
  "common": { "level": 100, "pan": 64, "structure12": 4, "...": "..." },
  "tones": [
    {
      "enabled": true,
      "wg": { "waveGainDb": 0, "fxm": {"on": false}, "keyLow": 0, "keyHigh": 127 },
      "tvf": { "type": "LPF", "cutoff": 90, "resonance": 40 },
      "env": { "a": {"t": [10,40,60,50], "l": [127,90,80]} },
      "zoneSet": {
        "zones": [
          { "sample": "Piano/Piano_C2.wav", "hash": "…",
            "keyLow": 0, "keyHigh": 47, "velLow": 1, "velHigh": 127,
            "rootKey": 36, "loop": {"mode": "sustain", "start": 12345, "end": 67890} }
        ]
      }
    }
  ]
}
```

- 采样默认以**库相对路径 + 哈希**引用；可选项：嵌入采样（单文件补丁，体积大）。
- 载入时校验哈希，缺失时提示定位。

### 8.2 采样库索引（`library.json`）

记录采样路径、哈希、rootKey、循环点、采样率、时长、缩略图缓存路径。

### 8.3 宿主状态

`getStateInformation()`：当前 Patch JSON + 库路径 + UI 状态（当前页、缩放）。
`setStateInformation()`：异步解析并原子发布新 Runtime（不阻塞消息线程）；加载完成前使用静音占位 Zone，避免阻塞宿主。

### 8.4 参数导入（可选，v1.2+）

- 解析外部 SysEx 参数数据（DT1 格式）到当前补丁。
- 采样/波形映射无法自动对应，导入后 Tone 指向空 Zone Set，由用户手动指定采样。

---

## 9. 开发环境与目录结构

### 9.1 环境

| 项 | 要求 |
| --- | --- |
| 平台 | Windows 10+（MSVC 2022）、macOS 12+（Xcode 14+）、Linux（GCC 12+） |
| 工具 | CMake ≥ 3.22、Git、JUCE（submodule 或 FetchContent） |
| IDE | Visual Studio 2022 / CLion / Xcode |
| 调试 | JUCE `AudioPluginHost`、RealtimeSanitizer（Clang） |

### 9.2 目录结构

```
myJV/
├─ CMakeLists.txt
├─ cmake/                      # JUCE 集成与工具链
├─ Source/
│  ├─ Plugin/
│  │   ├─ MyJVProcessor.h/.cpp
│  │   ├─ MyJVEditor.h/.cpp
│  │   └─ PresetManager.h/.cpp
│  ├─ Params/
│  │   ├─ ParameterIDs.h
│  │   ├─ ParameterLayout.cpp
│  │   ├─ ParamSnapshot.h
│  │   └─ Calibration.h
│  ├─ Model/
│  │   ├─ Patch.h  Tone.h  ZoneSet.h  Sample.h  SampleLibrary.h/.cpp
│  ├─ Engine/
│  │   ├─ SynthEngine.h/.cpp
│  │   ├─ VoiceManager.h/.cpp
│  │   ├─ Note.h/.cpp
│  │   ├─ ToneVoice.h/.cpp
│  │   ├─ TonePairProcessor.h/.cpp
│  │   ├─ AssetReclaimer.h/.cpp
│  │   └─ ModulationMatrix.h/.cpp
│  ├─ DSP/
│  │   ├─ SamplePlayer.h/.cpp
│  │   ├─ Envelope.h/.cpp
│  │   ├─ LFO.h/.cpp
│  │   ├─ SVF.h  Biquad.h
│  │   ├─ Booster.h  FXM.h  RingMod.h
│  │   └─ Smoother.h  DeterministicRandom.h
│  ├─ IO/
│  │   ├─ SampleImporter.h/.cpp
│  │   ├─ PatchSerializer.h/.cpp
│  │   ├─ LibraryIndex.h/.cpp
│  │   └─ SysExImporter.h/.cpp        # 可选
│  └─ UI/
│      ├─ Theme.h  LookAndFeel.h/.cpp
│      ├─ Components/ (JvKnob, JvLcd, EnvelopeView, FilterCurveView, …)
│      └─ Pages/ (PatchPage, TonePage, SamplerPage, KeyboardPage)
├─ Tests/
│  ├─ dsp_tests.cpp
│  ├─ mapping_tests.cpp
│  └─ render_tests.cpp
├─ Resources/
│  ├─ fonts/  images/
│  └─ default_patches/
└─ Docs/
    ├─ myJV_开发架构文档.md
    └─ Roland_JV-1080_参数与工作方式.md   # 内部参数语义资料
```

### 9.3 编码规范

- 音频线程代码标注 `// RT-safe`，禁止分配/加锁/引用计数/异常。
- DSP 单元为独立类，输入输出显式，便于单元测试。
- 所有映射常量集中在 `Calibration.h`。
- 参数命名与分组保持稳定，变更需提供迁移函数。

---

## 10. 测试与验证

### 10.1 单元测试

| 对象 | 用例 |
| --- | --- |
| Envelope | 各段时间/电平、Release 从当前值出发、力度/键跟随 |
| LFO | 8 种波形形状、延迟/淡入、Key Trigger、同步 |
| SVF/Biquad | 频响与理论曲线对比（±0.5 dB）、稳定性扫描 |
| SamplePlayer | 音高比、循环、交叉淡化、反向、边界条件 |
| Mapping | 每个映射函数端点/单调性 |
| ParamSnapshot | 参数一致性、活动/退休快照隔离 |
| VoiceManager | 抢占策略（含 KillFading 状态）、Mono/Legato、延音踏板、Note 与 ToneVoice 索引一致性 |
| AssetReclaimer | 退休顺序、宽限期、无悬空指针（ASan） |

### 10.2 基准音频回归

- 固定 MIDI 序列 + 固定参数 + **固定随机种子** → 渲染 WAV → 与基准比对（RMS、频谱、哈希）。
- 基准音频由本软件生成并纳入版本库；修改映射常量或 DSP 后需重新生成基准并人工确认差异。
- 覆盖：Structure 1–4、FXM、Booster、随机/混沌波形、极端包络、64 声部压力。
- 容差：浮点差异 ±0.1 dB / 频谱差异 ±0.5 dB（可按用例调整）。

### 10.3 实时安全与性能

| 检查 | 方法 |
| --- | --- |
| 无分配/加锁 | RealtimeSanitizer（Clang）、`JUCE_DEBUG` 断言、自定义分配钩子 |
| 无引用计数 | 音频线程代码审查 + 静态分析（禁止 `shared_ptr` 出现在 RT 路径） |
| 无悬空指针 | ASan + AssetReclaimer 压力测试（频繁切换补丁/Zone） |
| 无爆音 | 参数突变扫描测试 + 听感测试 |
| CPU | 64 个 Tone 声部满负荷（全部调制开启）在 48 kHz/256 块下实时占用 < 30%（i5 级别） |
| 内存 | 采样库用量监控；插件状态大小检查 |
| 插件验证 | `pluginval`（level 5+）通过 VST3/AU 验证 |

---

## 11. 里程碑计划

| 阶段 | 目标 | 交付物 | 预估 |
| --- | --- | --- | --- |
| M0 骨架 | JUCE 工程、APVTS、MIDI 输入、空引擎、Standalone/VST3 出声 | 可加载的最小插件 | 1–2 周 |
| M1 采样器 | Sample/Zone/ZoneSet、导入、播放、循环、键区/力度层、Zone 编辑器（基础版） | 可用采样器 | 3–4 周 |
| M2 单 Tone 引擎 | WG 参数、TVF、TVA、3 包络、2 LFO、Control 矩阵 | 单 Tone 完整发声 | 4–6 周 |
| M3 完整 Patch | 4 Tone、Structure 1–4、Booster/RingMod、Patch Common、声部管理 | Patch 级功能齐全 | 3–4 周 |
| M4 表现力 | Mono/Legato/Portamento、Patch Remain、Analog Feel、Stretch Tune、Tone Delay、FXM、Bias | 演奏功能完整 | 2–3 周 |
| M5 UI 完善 | 全部页面、可视化控件、采样编辑器完善、预设管理、撤销 | 可用 UI | 3–4 周 |
| M6 调音与发布 | 映射常量调音（听感/频谱）、性能优化、pluginval、打包 | v1.0 RC | 3–4 周 |

> 总计约 19–27 周（单人全职估算，视调音迭代次数浮动）。

---

## 12. 性能预算

以 48 kHz、**64 个 Tone 级声部**（复音上限，即最坏情况下每个声部都开启全部调制）为基准：

| 单元 | 每采样估计 | 备注 |
| --- | --- | --- |
| SamplePlayer（Hermite） | ~30 flops | 含循环/交叉淡化 |
| SVF | ~20 flops | TPT，系数按控制率更新 |
| 包络 ×3 | ~30 flops | A-ENV 逐采样；P/F 后续可降为控制率 |
| LFO ×2 | ~20 flops | 可块率 + 插值（优化项） |
| 调制矩阵 | ~20 flops | 12 目标 |
| 其他（FXM/Booster/Pan/总线） | ~30 flops | 仅启用时 |
| 合计/声部 | ~150 flops | 64 声部 × 150 × 48 kHz ≈ **0.46 GFlops/s**（单核可承受） |

> 注意：上限是 64 个 Tone 级声部，而不是"64 Note × 4 Tone"（后者为 256 个 Tone，已超过复音上限）。

优化顺序：LFO/P-ENV/F-ENV 控制率降采样 → 块处理与 SIMD（SSE/NEON）→ 关闭未使用 Tone 的快速路径 → 多线程声部分组（谨慎评估延迟）。

---

## 13. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| 映射常量偏主观（包络时间、cutoff 曲线、LFO、FXM、Booster、Chaos） | 音色不达预期 | 全部集中在 `Calibration.h`；听感 + 频谱分析迭代；基准音频回归锁定 |
| Structure 4 拓扑为设计决策 | 结构音色不符合预期 | 以基准音频回归锁定；必要时提供结构拓扑开关 |
| 资产回收缺陷（悬空指针） | 随机崩溃 | AssetId + 退休队列 + ASan 压力测试；禁止 RT 路径引用计数 |
| 采样加载在音频线程造成卡顿 | 爆音/崩溃 | 后台加载 + 原子发布；v1.0 全预载 |
| 参数数量大导致 UI 卡顿 | 体验差 | 分页懒加载、控件复用、非自动化数据分离 |
| 大采样内存占用 | 内存告警 | 用量监控、可选流式（v1.1）、格式建议（单声道/短循环） |
| JUCE 授权 | 法律风险 | 明确 GPLv3 或购买商业授权；采样内容版权自查 |
| C++ 实时安全缺陷 | 随机崩溃 | RealtimeSanitizer、CI 压测、pluginval |
| 调音工作量超预期 | 延期 | M6 预留缓冲；优先调音最常用参数（cutoff、包络、LFO） |

### 13.1 遗留待定事项（Open Questions）

| 事项 | 当前状态 | 关闭方式 |
| --- | --- | --- |
| Tone Delay 的 Hold / Key Interval 用户可见语义 | 引擎机制已定义（5.11），模式语义未定稿 | 参数规格文档确认 |
| Structure 4 环形调制拓扑 | 按 5.10 定义为乘法环形调制 | 基准音频锁定；必要时提供拓扑开关 |
| Control 矩阵目标完整列表 | v1.0 为子集（5.9） | 参数规格文档补充并扩充枚举 |
| Audition Voice 的参数来源 | 未指定（当前补丁 Tone 1 或独立预览参数） | 设计确认后写入 5.2 |
| 映射常量的具体数值 | 附录 C 为默认值 | M6 调音阶段确定并冻结 |
| 流式播放的缓冲尺寸与预读策略 | v1.1+ 设计 | 独立设计文档 |
| 多输出总线在 AU 下的宿主兼容性 | 固定 3 组立体声总线 | M6 兼容性矩阵阶段验证（计划 M6-05） |

---

## 附录 A：参数组总览

| 分组 | 参数数量（约） | 备注 |
| --- | --- | --- |
| Patch Common | 30 | 名称、电平/声像、演奏行为、结构、控制源、Hold/Peak |
| Tone WG | 21 ×4 | Wave Gain、FXM、Tone Delay、键区/力度区、控制器开关（采样 Zone 引用非自动化） |
| Tone TVF（含 F-ENV） | 21 ×4 | 5 种滤波类型、截止/共振/键跟随、F-ENV、LFO 深度 |
| Tone TVA（含 A-ENV） | 18 ×4 | 电平、Bias、A-ENV、Amp LFO |
| Tone P-ENV | 13 ×4 | 4T/4L + 灵敏度/时间键跟随 |
| Tone Pan/Output | 8 ×4 | 声像组、输出分配、输出电平 |
| Tone LFO ×2 | 16 ×4 | 8 波形、速率、延迟、淡入、同步 |
| Tone Control ×3 | 24 ×4 | 每控制 4 目标 + 4 深度 |
| 合计（自动化） | **约 515** | 30 + 4×121；与 4.1 的"约 500–550"一致，不含采样区域数据与 UI 状态 |

---

## 附录 B：关键数据结构草案

```cpp
// ---------- 模型层（消息/后台线程） ----------
struct Patch {
    PatchCommon common;
    std::array<Tone, 4> tones;
};

struct Tone {
    bool enabled = true;
    WgParams wg;
    TvfParams tvf;
    TvaParams tva;
    EnvParams pEnv, fEnv, aEnv;
    LfoParams lfo[2];
    CtrlSlot  ctrl[3];
    ToneOutput output;
    std::shared_ptr<const ZoneSet> zoneSet;   // 构建期持有；发布后只读
};

// ---------- 采样器资产（不可变，发布后只读） ----------
struct Sample {
    juce::AudioBuffer<float> data;            // 单声道或立体声
    double sourceSampleRate = 48000.0;
    int    rootKey = 60;
    LoopInfo embeddedLoop;
    std::string name, fileHash;
};

struct Zone {
    std::shared_ptr<const Sample> sample;     // 引用链保活
    int   keyLow = 0, keyHigh = 127;
    int   velLow = 1, velHigh = 127;
    int   rootKeyOverride = -1;
    float coarseTune = 0, fineTune = 0, gainDb = 0;
    int   pan = 64;
    bool  reverse = false;
    LoopMode loopMode = LoopMode::Sustain;
    LoopInfo loop;
};

struct ZoneSet {
    std::string name;
    std::vector<Zone> zones;                  // 发布后只读
};

// ---------- 运行资产（不可变，发布后只读） ----------
struct PatchRuntime {
    uint64_t id = 0;                          // AssetId（仅 Runtime 参与回收比较）
    PatchSnapshot snapshot;                   // 活动时由音频线程刷新；退休后冻结
    std::shared_ptr<const ZoneSet> zoneSets[4]; // 拥有权（消息线程）
    const ZoneSet* rawZoneSets[4] = {};       // 供快照填充；音频线程只读
};

class AssetReclaimer {
public:
    void publish(std::shared_ptr<PatchRuntime> next);      // 消息线程
    void collect(uint64_t oldestAssetInUse);               // 消息线程定时器；不回收活动 Runtime
    const PatchRuntime* activeForAudio() const noexcept;   // 原子读
private:
    std::atomic<PatchRuntime*> active { nullptr };
    std::vector<std::shared_ptr<PatchRuntime>> pending;    // 含活动与退休
};

// ---------- 引擎层（音频线程） ----------
static constexpr int kMaxVoices = 64;   // Tone 级声部上限
static constexpr int kMaxNotes  = 64;   // Note 池上限（最坏情况 1 Tone/音符）

struct BusBuffers { float* l[3]; float* r[3]; };   // Main / Out1 / Out2

class ToneVoice {                            // 一个 Tone 实例 = 一个声部
public:
    enum class State { Free, Active, Releasing, KillFading };

    void  startNote(const ToneSnapshot&, const ZoneSet*, int note, float vel,
                    double engineSampleRate, uint64_t rngSeed) noexcept;
    void  release() noexcept;                 // 正常包络释放
    void  kill() noexcept;                    // 抢占：快速淡出
    void  updateModulators() noexcept;        // LFO/ENV/音高，每采样
    float processWG() noexcept;               // 采样播放 + WaveGain + FXM
    float processTVF(float in) noexcept;      // 含 F-ENV/LFO/矩阵调制
    float processTVA(float in) noexcept;      // 含 A-ENV/Bias/矩阵调制
    void  addToBus(float sample, BusBuffers&) noexcept; // 声像 + 输出电平 + 分配
    bool  finished() const noexcept;

private:
    State state = State::Free;
    int  note = 60, toneIndex = 0;
    float velocity = 1.0f;
    uint64_t startOrder = 0;
    float currentLevel = 0.0f;                // 用于 Loudest 抢占
    const PatchRuntime* runtime = nullptr;    // Patch Remain 依据

    SamplePlayer player;
    Envelope pEnv, fEnv, aEnv;
    LFO lfo[2];
    SVF filter;
    FXM  fxm;
    float panL = 0.707f, panR = 0.707f;
    int   delaySamplesRemaining = 0;
    // 平滑/调制状态 ...
};

struct Note {                                 // 一个 MIDI 音符
    bool active = false;
    int  note = 60;
    float velocity = 1.0f;
    uint64_t startOrder = 0;
    int  voiceIndex[4] = {-1, -1, -1, -1};
    int  numVoices = 0;
    bool sustained = false;                   // Hold-1
    const PatchRuntime* runtime = nullptr;
    TonePairProcessor pairs[2];               // 含 Booster/RingMod 状态

    void render(BusBuffers&, int numSamples) noexcept;  // 读取 runtime->snapshot
};
```

---

## 附录 C：映射常量

`Calibration.h` 中的常量及其调试方式（应用内听感/频谱分析，不依赖外部设备）：

| 常量 | 默认值 | 作用 | 调试方式 |
| --- | --- | --- | --- |
| `kEnvTimeMinMs` / `kEnvTimeMaxMs` | 1 / 20000 | 包络时间范围 | 包络图与实际听感对比 |
| `kEnvCurve` | 指数逼近系数 | 包络段形状 | 包络图对比 |
| `kCutoffMinHz` / `kCutoffMaxHz` | 20 / 20000 | 截止频率范围 | 白噪声扫频 + 频谱 |
| `kResonanceMaxQ` | 20 | 共振上限 | 频谱共振峰高度 |
| `kPkgGainDb` | 12 | PKG 最大增益 | 频谱对比 |
| `kLfoRateMinHz` / `kLfoRateMaxHz` | 0.05 / 20 | LFO 速率范围 | 示波器/频谱 |
| `kChaosR` | 3.9 | 混沌映射参数 | 波形统计对比 |
| `kBoosterDrive` | 6 | Booster 最大驱动 | 谐波失真分析 |
| `kFxmDepthScale` | 1.0 | FXM 深度缩放 | 频谱对比 |
| `kToneDelayMinMs` / `kToneDelayMaxMs` | 1 / 2000 | Tone Delay 范围 | 听感 |
| `kPortamentoMinMs` / `kPortamentoMaxMs` | 1 / 5000 | 滑音时间范围 | 听感 |
| `kStretchTuneDepth` | 0.5 | 拉伸调律系数 | 逐键测频 |
| `kAnalogFeelRate` | 0.5 Hz | 模拟感漂移速率 | 录音统计 |
| `kVoiceFadeMs` | 5 | 抢占/停止淡出时间 | 听感 |
| `kOutputHeadroomDb` | -6 | 输出余量 | 满复音录音对比 |

> 修改常量后：更新单元测试阈值 → 重新生成基准音频 → 人工确认差异 → 提交。

---

## 附录 D：参考资料

| 资料 | 地址 |
| --- | --- |
| 内部参数语义资料 | `Docs/Roland_JV-1080_参数与工作方式.md` |
| JUCE 官方文档 | https://juce.com/learn/documentation |
| JUCE 论坛与示例 | https://forum.juce.com/ |
| Catch2 | https://github.com/catchorg/Catch2 |
| pluginval（插件验证） | https://github.com/Tracktion/pluginval |
| RealtimeSanitizer | https://clang.llvm.org/docs/RealtimeSanitizer.html |
| libsamplerate | https://github.com/libsndfile/libsamplerate |
