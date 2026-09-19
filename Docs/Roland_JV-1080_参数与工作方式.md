# Roland JV-1080 合成器参数与工作方式详解

> 本文根据 Roland 官方产品资料、Sound On Sound（1994 年 12 月）评测、Vintage Synth Explorer 规格页、
> Roland 官方 Owner's Manual（JV-1080_OM.pdf）、开源项目 JV1080lib 的结构定义与 EFX 实现、
> sonic-codex/jv1080-sysex-manager 的 SysEx 参数表等网络资料整理而成，面向"想深入了解 JV-1080
> 音色引擎参数与工作原理"的读者（含编辑器/仿真器开发者）。
>
> 说明：JV-1080 面板与手册中大部分连续参数为 0–127；本文标注的范围以官方手册与 SysEx 实现为准，
> 个别未在资料中明确的范围用"±"或文字描述。

---

## 目录

1. [产品概述与规格](#1-产品概述与规格)
2. [总体架构与工作方式](#2-总体架构与工作方式)
3. [合成引擎参数详解](#3-合成引擎参数详解)
   - [3.1 Patch / Tone 结构总览](#31-patch--tone-结构总览)
   - [3.2 WG 波形发生器](#32-wg-波形发生器)
   - [3.3 TVF 滤波器](#33-tvf-滤波器)
   - [3.4 TVA 放大器与声像](#34-tva-放大器与声像)
   - [3.5 包络与 LFO](#35-包络与-lfo)
   - [3.6 Patch Common 音色公共参数](#36-patch-common-音色公共参数)
   - [3.7 Rhythm Set 鼓组](#37-rhythm-set-鼓组)
   - [3.8 Performance 演奏组合](#38-performance-演奏组合)
   - [3.9 System 系统参数](#39-system-系统参数)
4. [效果系统](#4-效果系统)
5. [MIDI 与 SysEx 数据通信](#5-midi-与-sysex-数据通信)
6. [存储与扩展](#6-存储与扩展)
7. [特色功能与设计要点](#7-特色功能与设计要点)
8. [参数速查表（Tone 数据块）](#8-参数速查表tone-数据块)
9. [参考资料](#9-参考资料)

---

## 1. 产品概述与规格

**Roland JV-1080** 是 Roland 于 1994 年推出的 2U 机架式合成器音源模块，绰号 "Super JV"，
生产至 2001 年前后。它采用 PCM 采样回放（S+S：Sample + Synthesis）合成方式，
是 90 年代使用最广泛的音源模块之一，也被 Roland 官方称为"历史上出现在最多唱片中的音源模块"。

### 核心规格

| 项目 | 规格 |
| --- | --- |
| 合成方式 | PCM 采样回放 + 减法合成（S+S） |
| 处理器 | 32 位 RISC CPU，66 MHz |
| 最大复音数 | 64 声（动态分配） |
| 多音色 | 16 声部（15 个常规声部 + 1 个鼓/节奏声部） |
| 内部波形 | 8 MB 波形 ROM，448 个内置波形 |
| 音色（Patch） | 640 个：Preset A/B/C 各 128（共 384）+ User 128 + GM 128（Preset D） |
| 演奏组合（Performance） | 96 个：Preset A/B 各 32（共 64）+ User 32 |
| 节奏组（Rhythm Set） | 10 个：Preset 6 + GM 2 + User 2 |
| 效果 | EFX 插入效果 40 种 + Chorus + Reverb（8 种算法） |
| 输出 | MIX L/R、OUTPUT 1 L/R、OUTPUT 2 L/R（共 6 路）、耳机 |
| 扩展 | 4 个 SR-JV80 扩展卡槽、1 个 PCM 波形卡槽（SO-PCM1 系列）、1 个数据卡槽（PN-JV80 系列） |
| MIDI | IN / OUT / THRU，支持 GM（General MIDI） |
| 显示 | 40 字符 × 2 行背光 LCD，α 旋钮（可按下/双击复位参数） |
| 工作模式 | Patch / Performance / Rhythm Set / GM / System / Utility |

---

## 2. 总体架构与工作方式

### 2.1 声音层级

JV-1080 的声音数据是严格的树状层级结构：

```
Performance（演奏组合，最多 16 个 Part）
 └─ Part（声部，1–16，可分配 MIDI 通道、键区、电平、声像、效果发送等）
     └─ Patch（音色，由 Common + 最多 4 个 Tone 组成）
         ├─ Patch Common（公共参数：电平、声像、滑音、结构、EFX/Chorus/Reverb 等）
         └─ Tone 1–4（每个 Tone 是一台"小型合成器"）
             ├─ WG（Wave Generator，波形发生器/PCM 采样）
             ├─ TVF（Time Variant Filter，时变滤波器）
             ├─ TVA（Time Variant Amplifier，时变放大器）
             ├─ LFO 1 / LFO 2
             ├─ P-ENV（音高包络）
             ├─ F-ENV（滤波包络）
             ├─ A-ENV（音量包络）
             └─ Control 1/2/3 调制矩阵（各 4 个目标）
```

- **Patch 模式**：一次只演奏一个 Patch（最多 4 个 Tone 叠加/分层/分键区），使用 Patch 自带的效果设置。
- **Performance 模式**：16 个 Part 各指定一个 Patch（或节奏组），用于多音色编曲或大型叠加音色。
- **Rhythm Set**：一套鼓组，64 个琴键位置各自拥有独立的波形与完整 Tone 参数。

### 2.2 信号流

单个 Tone 的基本信号流（结构类型 1）：

```
WG（PCM 波形，可加 FXM 频率交叉调制）
   │
   ▼
TVF（低通/带通/高通/Peaking 滤波器 + 包络）
   │
   ▼
TVA（音量包络 + LFO 调制 + 声像）
   │
   ▼
Tone 输出（Output 分配、电平、Chorus/Reverb 发送）
```

Patch 级信号流：

```
Tone 1 ┐
Tone 2 ├─（按 Structure 1–4 组合，可含 Booster、环形调制）─┐
Tone 3 │                                                     │
Tone 4 ┘                                                     ▼
                                        Patch Common 的 EFX（40 种插入效果）
                                                             │
                                                             ▼
                                                        Chorus → Reverb
                                                             │
                                                             ▼
                                     MIX L/R 或 OUTPUT 1 / OUTPUT 2
```

### 2.3 Structure（声音结构）1–4

4 个 Tone 可按两对组合（Tone 1/2、Tone 3/4），每对可选 4 种结构，从而改变信号路径：

| 结构 | 工作方式 |
| --- | --- |
| Type 1 | 两个 Tone 各自独立：`WG→TVF→TVA`，无 Booster、无环形调制 |
| Type 2 | 在 Tone 1 的路径中插入 **Booster**（对滤波后的信号进行过载/增益） |
| Type 3 | 在 Tone 2 的路径中插入 **Booster** |
| Type 4 | 两个 Tone 串接：Tone 1 的 WG 直接进入 TVA1，与 Tone 2 的 WG 混合后进入 TVF1 → Booster → TVF2 → TVA2；此结构同时提供**环形调制（Ring Mod）**通路 |

> Sound On Sound 1994 年评测中对 Structure 4 的描述：
> "Tone 1 的波形发生器直接进入其放大器，然后与 Tone 2 的波形发生器合并后送入第一个滤波器；
> 结果再经过 Booster 效果，进入 Tone 2 的滤波器和放大器。"
> Booster 是 JV 系列新增的参数，用于使信号产生过载失真；环形调制则用于制造金属感、无调性音色。

### 2.4 复音与声部分配

- 64 个声部由全部 16 个 Part 共享，采用**动态声部分配**：每个正在发声的 Tone 占用 1 个声部。
  因此一个 4-Tone 的音色每按一个键最多消耗 4 个声部。
- **Voice Priority（声部优先）**：当超过 64 声时决定抢占策略（Last / Loudest 两种）。
- **Voice Reserve（声部保留）**：Performance 中可为每个 Part 保留一定数量的声部，
  保证重要声部（如贝斯、鼓）不会被抢占。
- **Patch Remain**：切换音色时旧音色继续自然衰减，避免断音。

### 2.5 六种工作模式（SOS 评测）

| 模式 | 用途 |
| --- | --- |
| Patch（Play/Edit） | 演奏/编辑单个音色（一般演奏用） |
| Performance（Play/Edit） | 叠加、分层、分区，或设置多音色响应音序器 |
| Rhythm Set（Play/Edit） | 演奏/编辑打击乐组 |
| GM（Play/Edit） | 回放 General MIDI 乐曲（通过 Shift+Performance 或 GM System On 进入） |
| System | 全局参数（调音、MIDI 接收、控制器分配、试听设置等） |
| Utility | 数据存储等事务（批量转储、卡操作等） |

---

## 3. 合成引擎参数详解

### 3.1 Patch / Tone 结构总览

一个 Patch = **Patch Common + 4 个 Tone**。每个 Tone 包含：

| 区块 | 内容 |
| --- | --- |
| WG | 波形选择、音高、键区/力度区、FXM、Tone Delay、控制器开关与调制矩阵 |
| TVF | 滤波类型、截止频率、共振、键跟随、力度灵敏度、滤波包络（F-ENV） |
| TVA | 音量、偏置（Bias）、音量包络（A-ENV）、声像、输出分配 |
| LFO 1/2 | 波形、速率、延迟、淡入、同步、对音高/滤波/音量/声像的调制深度 |
| ENV | P-ENV（音高）、F-ENV（滤波）、A-ENV（音量）三段 4 段式包络 |

### 3.2 WG 波形发生器

| 参数 | 说明 | 范围/取值 |
| --- | --- | --- |
| Tone Switch | 该 Tone 是否发声 | On / Off |
| Wave Group / Group ID | 波形来源组：内部 A/B、PCM 卡、扩展卡 A/B/C | — |
| Wave Number | 波形编号（内部 448 个波形；扩展卡各有数百个波形） | 1–448+ |
| Wave Gain | 波形增益，可在进入滤波器前提升电平 | -6 / 0 / +6 / +12 dB |
| FXM Switch / Color / Depth | 频率交叉调制：用波形自身调制频率，产生金属、钟声、噪声类泛音 | On/Off；Color、Depth 0–127 |
| Tone Delay Mode / Time | 音符延迟发声；模式：Normal、Hold、Key Interval | 0–127 |
| Velocity Crossfade | 力度交叉淡化深度（多 Tone 间按力度过渡） | 0–127 |
| Velocity Range Lower/Upper | 力度响应范围（范围外不发声） | 1–127 |
| Key Range Lower/Upper | 键区范围（分键、分区） | C-1 – G9（0–127） |
| Redamper Control | 是否响应延音踏板"半踩重新制音"（钢琴音色用） | On / Off |
| Volume / Hold-1 / Bender / Pan Control | 是否响应音量 CC、延音踏板、弯音轮、声像 CC | On / Off |
| Control 1/2/3 Dest ×4 + Depth ×4 | 调制矩阵：每组 4 个调制目标及深度（Control 2/3 的来源在 Patch Common 指定） | Dest 列表；Depth 0–127 |
| Coarse Tune | 粗调音（半音） | -24 – +24 |
| Fine Tune | 微调音（音分） | -50 – +50 cents |
| Random Pitch Depth | 每个音符随机音高偏移（模拟自然感/老式合成器） | 0–127 |
| Pitch Keyfollow | 音高键跟随（100% 时按平均律跟随） | ±100% |
| P-ENV Depth / Sens / Time | 音高包络深度、力度灵敏度、时间键跟随等 | — |
| Pitch LFO 1/2 Depth | LFO 对音高的调制深度 | 0–127 |

### 3.3 TVF 滤波器

JV-1080 每个 Tone 有一个 TVF（-12 dB/oct 数字滤波器），可选用 5 种类型：

| 类型 | 含义 |
| --- | --- |
| Off | 关闭滤波器（信号直通） |
| LowPass（LPF） | 低通：只保留截止频率以下成分，最常用 |
| BandPass（BPF） | 带通：只保留截止频率附近成分 |
| HighPass（HPF） | 高通：只保留截止频率以上成分 |
| Peaking（PKG） | 峰值（加重）滤波：在截止频率处提升/衰减，配合共振可作为固定 EQ 使用 |

| 参数 | 说明 | 范围 |
| --- | --- | --- |
| Filter Type | 滤波类型（上表） | Off/LPF/BPF/HPF/PKG |
| Cutoff Frequency | 截止频率（截止点越高声音越亮） | 0–127 |
| Cutoff Keyfollow | 截止频率随键位变化（高音更亮/更暗） | ±100% |
| Resonance | 共振：提升截止点附近的成分，过大时产生自激/啸叫 | 0–127 |
| Resonance Velocity Sens | 共振的力度灵敏度 | — |
| F-ENV Depth | 滤波包络对截止频率的调制深度（可为负） | — |
| F-ENV Velocity Curve / Sens | 滤波包络的力度曲线/灵敏度 | — |
| F-ENV Time 1–4 / Level 1–4 | 滤波包络四段折线（时间/电平） | 0–127 |
| F-ENV Time Keyfollow | 包络时间随键位变化（高音包络更快/更慢） | — |
| F-ENV Vel Time 1/4 Sens | 力度对包络起音/释放时间的灵敏度 | — |
| Filter LFO 1/2 Depth | LFO 对截止频率的调制深度（自动哇音、律动感） | 0–127 |

> Sound On Sound 特别指出：JV-1080 新增的 **PKG（Peaking）滤波模式**配合共振，
> 可当作固定的加性 EQ 使用，用于单个 Tone 或结构内部。

### 3.4 TVA 放大器与声像

| 参数 | 说明 | 范围 |
| --- | --- | --- |
| Tone Level | 该 Tone 的音量 | 0–127 |
| Bias Direction / Point / Level | 波形偏置（非对称削波）：Direction 选择偏置方向，Point 指定作用点，Level 指定偏置量，用于制造失真/音色变化 | — |
| A-ENV Velocity Curve / Sens | 音量包络的力度曲线/灵敏度 | — |
| A-ENV Time 1–4 | 音量包络四段时间（Attack/Decay/Sustain/Release 等） | 0–127 |
| A-ENV Level 1–3 | 音量包络电平（第 4 段固定归零，因此只有 3 个电平） | 0–127 |
| A-ENV Time Keyfollow | 包络时间随键位变化 | — |
| A-ENV Vel Time 1/4 Sens | 力度对起音/释放时间的灵敏度 | — |
| Amplitude LFO 1/2 Depth | LFO 对音量的调制（颤音/律动） | 0–127 |
| Tone Pan | Tone 声像（L64–C–R63） | 0–127（64=中） |
| Pan Keyfollow | 声像随键位变化（低音偏左/高音偏右） | — |
| Random Pan Depth | 每个音符随机声像 | 0–127 |
| Alt Pan Depth | 交替声像（相邻音符左右交替） | 0–127 |
| Pan LFO 1/2 Depth | LFO 对声像的调制（自动摇摆） | 0–127 |
| Output | 输出分配：MIX / OUTPUT 1 / OUTPUT 2 | — |
| Output Level | Tone 输出电平 | 0–127 |
| Chorus Send / Reverb Send | 该 Tone 送往 Chorus / Reverb 的量 | 0–127 |

### 3.5 包络与 LFO

#### 三个包络

每个 Tone 有 3 个 4 段折线包络，均为 **Time 1–4 + Level 1–4**（TVA 的 Level 4 固定为 0）：

| 包络 | 作用 | 专属参数 |
| --- | --- | --- |
| P-ENV | 调制音高（起音弯音、打击感） | Depth（±）、Velocity Sens、Time Keyfollow、Vel Time 1/4 Sens |
| F-ENV | 调制滤波截止频率（音色随时间变化） | Depth（±）、Velocity Curve/Sens、Time Keyfollow、Vel Time 1/4 Sens |
| A-ENV | 调制音量（ADSR 主体） | Velocity Curve/Sens、Time Keyfollow、Vel Time 1/4 Sens |

#### LFO 1 / LFO 2

每个 Tone 有 2 个 LFO，波形共 8 种（与编辑器 UI/手册一致）：

| 波形 | 说明 |
| --- | --- |
| Sine | 正弦波 |
| Sawtooth | 锯齿波 |
| Square | 方波 |
| Triangle | 三角波 |
| Trapezoid | 梯形波 |
| Sample/Hold | 采样保持（随机阶梯） |
| Random | 随机（平滑随机） |
| Chaotic（CHS） | Roland 特色的混沌波形，适合制造不规则的有机变化 |

每个 LFO 的公共参数：

| 参数 | 说明 |
| --- | --- |
| Waveform | 上述 8 种波形 |
| Key Trigger | 每次 Note On 时是否重新启动 LFO 周期 |
| Rate | LFO 速率（0–127） |
| Level Offset | LFO 输出电平偏移（改变调制中心） |
| Delay Time | Note On/Off 后延迟一段时间才开始振荡 |
| Fade Mode / Fade Time | LFO 淡入方式与时间（逐渐进入调制） |
| Ext Sync | 外部同步：可同步到 MIDI Clock / MIDI 音符（Tap），使律动与音序器一致 |
| Pitch / Filter / Amp / Pan Depth | 对各调制目标的深度（在对应区块中设置） |

> Sound On Sound 指出：JV-1080 的 LFO 与部分效果可以跟随 MIDI Clock 同步，
> "对滤波器、放大器、音高调制、声像、音频延迟、Tone Delay 和镶边所做的事，
> 就像 Wavestation 对波序所做的一样"。

### 3.6 Patch Common 音色公共参数

| 参数 | 说明 |
| --- | --- |
| Patch Name | 12 字符音色名 |
| Patch Level / Pan | 音色总电平/声像 |
| Analog Feel Depth | 模拟感：加入细微的随机音高漂移，模仿老式模拟合成器 |
| Bend Range Up / Down | 弯音轮上/下弯音范围（0–24 半音） |
| Key Assign Mode | Poly（复音）/ Mono（单音） |
| Legato | 单音模式下是否连奏（滑音触发方式） |
| Portamento Switch / Mode / Type / Start / Time | 滑音开关、模式、类型、起始方式与时间 |
| Octave Shift | 八度移调（±3 八度） |
| Stretch Tune Depth | 拉伸调律（钢琴式"高音偏高、低音偏低"的调音），可调深度 |
| Voice Priority | 声部抢占优先：Last / Loudest |
| Structure Type 1&2 / Booster Level 1&2 | Tone 1/2 的结构类型与 Booster 电平 |
| Structure Type 3&4 / Booster Level 3&4 | Tone 3/4 的结构类型与 Booster 电平 |
| Control Source 2 / 3 | 调制矩阵 Control 2/3 的调制来源（控制器编号等） |
| Control Hold/Peak、Control 1/2/3 Hold/Peak | 控制器到达后的保持/峰值行为（踏板保持时是否保留当前值或更新到新峰值） |
| EFX Type / Param 1–12 / Output / Level / Sends / Control 1/2 | 插入效果的类型、最多 12 个参数、输出路由、电平、送往 Chorus/Reverb 的量、两个可分配效果控制器及深度 |
| Chorus Level / Rate / Depth / Pre-Delay / Feedback / Output | 合唱效果参数（电平、速率、深度、预延迟、反馈、输出分配） |
| Reverb Type / Level / Time / HF Damp / Feedback | 混响类型（8 种）、电平、时间、高频阻尼、反馈 |
| Default Tempo | 默认速度（20–250 BPM），供同步延迟等使用 |

### 3.7 Rhythm Set 鼓组

Rhythm Set 由 **Common（名称）** 和 **64 个键位（Note）** 组成。每个键位本质上是一个完整 Tone：

| 参数 | 说明 |
| --- | --- |
| Tone Switch | 该键位是否发声 |
| Wave Group / Number、Wave Gain | 波形来源与增益 |
| Bender Range | 该打击乐的弯音范围（0–24） |
| Mute Group | 静音组（同一组的音会互相切断，如开/闭镲） |
| Envelope Mode | 包络模式（是否受延音踏板/Note Off 影响） |
| Volume / Hold-1 / Pan Control | 控制器响应开关 |
| Source Key | 音高参考键（改变采样回放音高） |
| Fine Tune、Random Pitch | 微调与随机音高 |
| P-ENV / F-ENV / A-ENV | 与常规 Tone 相同的包络参数 |
| Filter Type / Cutoff / Resonance | 每键独立滤波 |
| Tone Level / Tone Pan / Random Pan / Alt Pan | 音量与声像 |
| Output / Output Level / Chorus / Reverb Send | 输出与效果发送 |

> 打击乐波形与常规音色取自同一套波形库，因此二者可互换使用。
> Performance 模式下 16 个 Part 中有一个被指定为节奏声部。

### 3.8 Performance 演奏组合

#### Performance Common

| 参数 | 说明 |
| --- | --- |
| Performance Name | 12 字符名称 |
| EFX（Type / Param 1–12 / Output / Level / Sends / Control 1/2） | 整个 Performance 共用的插入效果 |
| Chorus / Reverb | 整个 Performance 共用的合唱与混响（类型、电平、时间、HF 阻尼、反馈等） |
| Default Tempo | 默认速度 |
| Key Range Switch | 是否启用各 Part 的键区设置 |
| Voice Reserve 1–16 | 为每个 Part 保留的声部数 |

#### Performance Part 1–16

| 参数 | 说明 |
| --- | --- |
| MIDI Receive | 是否接收 MIDI |
| MIDI Channel | 接收通道（1–16） |
| Patch Group / Group ID / Number | 选择的音色（内部/用户/扩展卡/GM 等） |
| Level / Pan | 声部电平与声像 |
| Coarse Tune / Fine Tune | 声部移调（粗调/微调） |
| Output / Output Level | 输出路由（MIX / OUTPUT 1 / OUTPUT 2）与电平 |
| Chorus Send / Reverb Send | 送往 Performance 效果的量 |
| Receive Program Change / Volume / Hold-1 | 是否响应相应 MIDI 消息 |
| Key Lower / Upper | 该声部的键区（分键/分区） |

> 通过 16 个 Part 的不同设置，Performance 既可做成"巨型叠加音色"，
> 也可把 16 个 Patch 铺满 MIDI 通道，作为多音色音源使用。

### 3.9 System 系统参数

| 分组 | 参数 |
| --- | --- |
| 全局 | Panel Mode（Performance/Patch/GM）、当前 Performance/Patch 选择、Master Tune（A4=440 Hz 为中心）、Scale Tune Switch、EFX/Chorus/Reverb 开关、Patch Remain、Clock Source |
| Scale Tune | 16 个声部各自的 12 音音阶微调 + Patch Scale Tune（每音 -64 – +63 音分） |
| 控制源分配 | Tap Control Source、Hold Control Source、Peak Control Source、Volume Control Source、Aftertouch Control Source、System Control Source 1/2（可分配控制器编号、弯音、触后等） |
| MIDI 接收 | Receive Program Change / Bank Select / Control Change / Modulation / Volume / Hold-1 / Bender / Aftertouch 开关；Control Channel、Patch Receive Channel |
| 其他 | Rhythm Edit Source、Preview Mode、Preview Key/Velocity Set 1–4 |

> **Preview（试听）**：面板音量旋钮可按下作为 Preview 键，按 System 中设置的
> Key/Velocity Set 1–4 依次试听音符或和弦，方便没有连接键盘时检查音色。

---

## 4. 效果系统

JV-1080 的效果分为三级，可同时使用：

```
Tone → EFX（40 种插入效果，可分配到 OUTPUT 1/2/MIX）
     → Chorus（合唱，可分配到独立输出）
     → Reverb（8 种混响）
     → MIX L/R 或 OUTPUT 1/2
```

- 每个 Tone 有独立的 **Chorus Send / Reverb Send**；
- Performance 中每个 Part 也有独立的 Chorus/Reverb 发送量；
- EFX 输出可指定为 **MIX、OUTPUT 1、OUTPUT 2**，实现"六路输出全部可用而不牺牲效果"；
- EFX 有 2 个可分配的**效果控制器（EFX Control Source 1/2 + Depth）**，
  可用 MIDI CC、弯音、触后等实时改变效果参数；
- 部分延迟类效果（如 Tempo Delay、Step Flanger）以及 LFO 可跟随 **MIDI Clock** 同步；
- Default Tempo 用于未收到外部时钟时的时间基准。

### EFX 40 种插入效果一览

| # | 名称 | 中文 | # | 名称 | 中文 |
| --- | --- | --- | --- | --- | --- |
| 01 | Stereo EQ | 立体声均衡 | 21 | Time Control Delay | 时间控制延迟 |
| 02 | Overdrive | 过载 | 22 | Voice Pitch Shifter | 人声变调器 |
| 03 | Distortion | 失真 | 23 | Feedback Pitch Shifter | 反馈变调器 |
| 04 | Phaser | 相位器 | 24 | Reverb | 混响 |
| 05 | Spectrum | 频谱均衡 | 25 | Gate Reverb | 门混响 |
| 06 | Enhancer | 增强器 | 26 | Overdrive→Series Chorus | 过载→串联合唱 |
| 07 | Auto Wah | 自动哇音 | 27 | Overdrive→Series Flanger | 过载→串联镶边 |
| 08 | Rotary | 旋转扬声器 | 28 | Overdrive→Series Delay | 过载→串联延迟 |
| 09 | Compressor | 压缩器 | 29 | Distortion→Series Chorus | 失真→串联合唱 |
| 10 | Limiter | 限制器 | 30 | Distortion→Series Flanger | 失真→串联镶边 |
| 11 | Hexa Chorus | 六重合唱 | 31 | Distortion→Series Delay | 失真→串联延迟 |
| 12 | Tremolo Chorus | 颤音合唱 | 32 | Enhancer→Series Chorus | 增强→串联合唱 |
| 13 | Space-D | 空间合唱 | 33 | Enhancer→Series Flanger | 增强→串联镶边 |
| 14 | Stereo Chorus | 立体声合唱 | 34 | Enhancer→Series Delay | 增强→串联延迟 |
| 15 | Stereo Flanger | 立体声镶边 | 35 | Chorus→Series Delay | 合唱→串联延迟 |
| 16 | Step Flanger | 步进镶边 | 36 | Flanger→Series Delay | 镶边→串联延迟 |
| 17 | Stereo Delay | 立体声延迟 | 37 | Chorus→Series Flanger | 合唱→串联镶边 |
| 18 | Modulation Delay | 调制延迟 | 38 | Chorus→Parallel Delay | 合唱→并联延迟 |
| 19 | Triple Tap Delay | 三抽头延迟 | 39 | Flanger→Parallel Delay | 镶边→并联延迟 |
| 20 | Quadruple Tap Delay | 四抽头延迟 | 40 | Chorus→Parallel Flanger | 合唱→并联镶边 |

### Reverb 8 种类型

Room 1、Room 2、Stage 1、Stage 2、Hall 1、Hall 2、Delay、Pan-Delay。

每种混响可调：Level（电平）、Time（时间）、HF Damp（高频阻尼）、Feedback（反馈）。

### Chorus 参数

Level、Rate、Depth、Pre-Delay、Feedback、Output（输出分配）。
与 EFX 中的合唱/镶边不同，Chorus 是全局并联效果，可为每个 Tone/Part 单独设置发送量。

---

## 5. MIDI 与 SysEx 数据通信

### 5.1 基本 MIDI

- 3 个 5-pin DIN：MIDI IN / OUT / THRU。
- 支持 GM System On（自动进入 GM 模式）、Bank Select + Program Change 选音色。
- 支持 Control Change（音量、声像、表情、延音、调制等）、弯音、触后、RPN 等常规消息。
- 每个 Part 可独立设置接收通道、是否响应 Program Change / Volume / Hold-1。

### 5.2 系统专用消息（SysEx）

JV-1080 的 Model ID 为 **6AH**，默认 Device ID 为 **10H**（17 号机）。消息格式：

```
F0  41  <Dev>  6A  <Cmd>  <A1 A2 A3 A4>  <Data...>  <Checksum>  F7
│   │    │     │    │      │              │          │           │
│   │    │     │    │      │              │          │           └ 结束
│   │    │     │    │      │              │          └ Roland 校验和
│   │    │     │    │      │              └ 数据字节（DT1）或请求长度（RQ1）
│   │    │     │    │      └ 4 字节参数地址
│   │    │     │    └ 12H=DT1（写数据）/ 11H=RQ1（请求数据）
│   │    │     └ 型号 ID（JV-1080 = 6AH）
│   │    └ 设备 ID（默认 10H）
│   └ 厂商 ID（Roland = 41H）
└ 开始
```

**Roland 校验和算法**（对地址+数据求和）：

1. 将地址与数据各字节按十进制相加；
2. 每步相加后若超过 127 则减去 128；
3. 用 128 减去最终结果；
4. 结果即校验和（若结果为 128 则校验和为 0）。

示例（Eddie Lotter 教程）：地址 `40 11 00`、数据 `41 63`
→ 64+17+0+65+99 = 245；245 mod 128 = 117；128-117 = 11 = `0BH`。

### 5.3 主要参数地址区（示例）

| 地址（4 字节） | 内容 |
| --- | --- |
| `01 00 00 xx` | 临时 Performance Common（名称、电平、速度、EFX、Reverb 等） |
| `01 00 10 xx` | 临时 Performance Part 1（依此类推至 Part 16） |
| `01 00 20 xx` | 临时 Patch Common |
| `01 00 4x xx` | 临时 Patch Tone 1–4（每个 Tone 一个数据块） |
| `11 00 00 xx` | 扩展卡 Performance 区 |
| `11 00 20 xx` | 扩展卡 Patch 区 |
| `11 00 60 xx` | 扩展卡 Rhythm 区 |

> 完整地址映射见 JV-1080 Owner's Manual 的 MIDI Implementation 章节。
> 各数据块内部偏移详见第 8 节速查表。

### 5.4 数据转储

- 用户音色/组合数据保存在内置 RAM（电池后备），可整体通过 SysEx **Bulk Dump** 转储；
- 支持 **Data Card（PN-JV80 系列）** 保存/读取音色数据；
- 支持 **PCM Card（SO-PCM1 系列）** 增加波形（占用独立卡槽，与数据卡槽分开）。

---

## 6. 存储与扩展

### SR-JV80 扩展卡

机内 4 个扩展槽可同时安装 4 张 SR-JV80 波形扩展卡，每张卡提供新的波形与音色
（部分卡 255 个波形 / 255 个音色）。8 MB 内部波形 + 扩展后可扩展到约 42 MB 规模。

1994 年评测时在售的五张卡：

| 卡 | 内容 |
| --- | --- |
| SR-JV80-01 Pop | 224 波形 / 145 音色，流行乐器全套 |
| SR-JV80-02 Orchestral | 174 波形 / 255 音色，管弦乐 |
| SR-JV80-03 Piano | 73 波形 / 111 音色，原声/电钢琴 |
| SR-JV80-04 Vintage Synth | 255 波形 / 255 音色，老式模拟合成器 |
| SR-JV80-05 World | 255 波形 / 255 音色，世界民族乐器 |

后续还推出过 Latin、Keyboards of the 60's & 70's、Session、Orchestral II、SFX、
Country、Vocal、World II、Brass 等数十张卡。

### 卡槽与数据

- **4 × SR-JV80 扩展槽**（内部安装，机背检修板下）；
- **1 × PCM 波形卡槽**（SO-PCM1 系列）；
- **1 × 数据卡槽**（PN-JV80 系列，存储音色/组合数据）；
- 用户数据由内置电池保持，可整体 SysEx 备份。

---

## 7. 特色功能与设计要点

1. **包络异常平滑**：66 MHz 32 位 RISC 处理器使数字包络没有量化/毛刺感，
   无论多快多极端都保持自然（SOS 与 Nick Magnus 均特别称赞）。
2. **Wave Gain（+12 dB）**：可在进入滤波器前提升单个波形电平，
   在鼓组/多声部混音中"救回"容易被淹没的音色。
3. **Booster 结构**：把滤波后的信号过载，得到类似 Odyssey 的"粗野"音色。
4. **PKG（Peaking）滤波**：可作为固定 EQ 使用，是当时 Roland 新加入的模式。
5. **Analog Feel / Stretch Tune**：模拟感随机音高漂移与钢琴式拉伸调律，增加真实感。
6. **Tone Delay**：每个 Tone 可延迟发声（Normal/Hold/Key Interval），用于琶音/节奏型音色。
7. **FXM（频率交叉调制）**：制造金属、钟、噪声等复杂泛音。
8. **环形调制**：通过 Structure 4 提供，用于无调性/工业音色。
9. **六路输出**：MIX、OUTPUT 1、OUTPUT 2 可同时使用且不影响效果；
   EFX/Chorus 可路由到独立输出，方便分轨录音。
10. **MIDI Clock 同步**：LFO 与部分效果可跟随外部时钟，产生"音乐性"的延迟与律动。
11. **操作细节**：α 旋钮可按下（试听/确认）、双击恢复参数原值；音量旋钮可按下试听。
12. **Patch Remain**：切换音色不断音；**Voice Reserve**：为关键声部保留复音。

---

## 8. 参数速查表（Tone 数据块）

以下偏移量来自 JV1080lib 的 `JV-struct-defs.h`（依据 JV-1080 手册 MIDI 实现整理），
单位为字节，十六进制。可用于编写编辑器/解析 SysEx 数据。

### 8.1 Patch Common（块大小 0x48）

| 偏移 | 参数 | 备注 |
| --- | --- | --- |
| 00–0B | Patch Name | 12 字节 ASCII |
| 0C | EFX Type | 0–39（40 种） |
| 0D–18 | EFX Parameter 1–12 | 依效果类型而定 |
| 19 | EFX Output | MIX / OUTPUT 1 / OUTPUT 2 |
| 1A | EFX Output Level | 0–127 |
| 1B | EFX Chorus Send Level | 0–127 |
| 1C | EFX Reverb Send Level | 0–127 |
| 1D | EFX Control Source 1 | 可分配 |
| 1E | EFX Control Depth 1 | — |
| 1F | EFX Control Source 2 | 可分配 |
| 20 | EFX Control Depth 2 | — |
| 21 | Chorus Level | 0–127 |
| 22 | Chorus Rate | 0–127 |
| 23 | Chorus Depth | 0–127 |
| 24 | Chorus Pre-Delay | 0–127 |
| 25 | Chorus Feedback | 0–127 |
| 26 | Chorus Output | 输出分配 |
| 27 | Reverb Type | 0–7（8 种） |
| 28 | Reverb Level | 0–127 |
| 29 | Reverb Time | 0–127 |
| 2A | Reverb HF Damp | 0–127 |
| 2B | Reverb Feedback | 0–127 |
| 2C–2D | Default Tempo | 高字节×16 + 低字节（20–250 BPM） |
| 2E | Patch Level | 0–127 |
| 2F | Patch Pan | 0–127（64=中） |
| 30 | Analog Feel Depth | 0–127 |
| 31 | Bend Range Up | 0–24 |
| 32 | Bend Range Down | 0–24 |
| 33 | Key Assign Mode | Poly / Mono |
| 34 | Legato | On / Off |
| 35 | Portamento | On / Off |
| 36 | Portamento Mode | — |
| 37 | Portamento Type | — |
| 38 | Portamento Start | — |
| 39 | Portamento Time | 0–127 |
| 3A | Patch Control Source 2 | 调制矩阵来源 |
| 3B | Patch Control Source 3 | 调制矩阵来源 |
| 3C | Control Hold/Peak | — |
| 3D–3F | Control 1/2/3 Hold/Peak | — |
| 40 | Velocity Range | — |
| 41 | Octave Shift | ±3 八度 |
| 42 | Stretch Tune Depth | 0–3（Off/1/2/3） |
| 43 | Voice Priority | Last / Loudest |
| 44 | Structure Type 1&2 | 1–4 |
| 45 | Booster Level 1&2 | 0–127 |
| 46 | Structure Type 3&4 | 1–4 |
| 47 | Booster Level 3&4 | 0–127 |

### 8.2 Patch Tone（每 Tone 0x81 字节）

| 偏移 | 参数 | 偏移 | 参数 |
| --- | --- | --- | --- |
| 00 | Tone Switch | 50 | Filter Type |
| 01–04 | Wave Group / ID / Number | 51 | Cutoff Frequency |
| 05 | Wave Gain | 52 | Cutoff Keyfollow |
| 06–08 | FXM Switch / Color / Depth | 53 | Resonance |
| 09–0A | Tone Delay Mode / Time | 54 | Resonance Velocity Sens |
| 0B–0F | Vel Xfade / Vel Range / Key Range | 55–5A | F-ENV Depth / Curve / Sens / Time Keyfollow |
| 10–14 | Redamper / Volume / Hold-1 / Bender / Pan Control | 5B–62 | F-ENV Time 1–4、Level 1–4 |
| 15–2C | Control 1/2/3（各 Dest×4 + Depth×4） | 63–64 | Filter LFO 1/2 Depth |
| 2D–34 | LFO 1（Wave/KeyTrig/Rate/Offset/Delay/Fade/Sync） | 65 | Tone Level |
| 35–3C | LFO 2（同上） | 66–68 | Bias Direction / Point / Level |
| 3D–3F | Coarse / Fine Tune、Random Pitch | 69–71 | A-ENV Curve / Sens / Time Keyfollow |
| 40 | Pitch Keyfollow | 72–74 | A-ENV Vel Time 1/4 Sens |
| 41–4D | P-ENV（Depth/Sens/Time Keyfollow、Time 1–4、Level 1–4） | 75–76 | Amplitude LFO 1/2 Depth |
| 4E–4F | Pitch LFO 1/2 Depth | 77–7C | Tone Pan / Pan Keyfollow / Random / Alt / Pan LFO 1/2 |
|  |  | 7D–80 | Output / Output Level / Chorus Send / Reverb Send |

### 8.3 其他数据块

| 数据块 | 大小 | 主要内容 |
| --- | --- | --- |
| System Common | 0x28 | 面板模式、主调音、效果开关、控制源、MIDI 接收开关、试听设置 |
| Scale Tune | 16×0x0C + 0x0C | 16 个声部 + Patch 的 12 音音阶微调 |
| Performance Common | 0x40 | 名称、EFX、Chorus、Reverb、默认速度、键区开关、Voice Reserve×16 |
| Performance Part | 0x13 ×16 | 接收通道、音色选择、电平、声像、移调、输出、发送、键区 |
| Rhythm Common | 0x0C | 鼓组名称 |
| Rhythm Note | 0x3A ×64 | 每个键位的波形、包络、滤波、声像、输出等 |

---

## 9. 参考资料

| 资料 | 地址 |
| --- | --- |
| Roland 官方 JV-1080 产品页（功能、扩展槽、效果说明） | https://www.roland.com/us/products/jv-1080/ |
| Roland 官方 Owner's Manual（PDF，扫描版） | https://static.roland.com/assets/media/pdf/JV-1080_OM.pdf |
| Sound On Sound 评测 "Roland Super JV1080"（1994 年 12 月，Dave Crombie / Nick Magnus） | https://www.soundonsound.com/reviews/roland-super-jv1080 |
| Vintage Synth Explorer：Roland JV-1080 规格 | https://www.vintagesynth.com/roland/jv1080.php |
| JV1080lib（C++ 编辑器/管理库，含 JV-1080 数据结构与 40 种 EFX 实现） | https://github.com/RoberNoTson/JV1080lib |
| sonic-codex/jv1080-sysex-manager（SysEx 参数表 YAML 与解析器） | https://github.com/sonic-codex/jv1080-sysex-manager |
| Eddie Lotter：Roland MIDI SysEx 教程与校验和算法 | http://www.2writers.com/eddie/TutSysEx.htm |
| 扩展卡克隆项目（SR-JV80 结构参考） | https://github.com/tadas-s/roland-expansion-clone |

> 注：不同资料对部分细节（如 Performance 数量、扩展卡编号）存在个别出入，
> 本文以 Roland 官方资料、Sound On Sound 规格表与 JV1080lib 结构定义为准。
