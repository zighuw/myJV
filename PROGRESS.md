# myJV 进度看板

> 唯一事实来源（开发计划 8.1）。状态：Backlog / Ready / In Progress / In Review / Changes Requested / Done / Blocked。
> 审查级别：R1 常规 / R2 深度 / R3 交叉 / R4 里程碑。

## 里程碑 M0：工程骨架（W1–W2）

| 任务 ID | 状态 | 会话记录 | 证据 | 审查 | 遗留问题 |
| --- | --- | --- | --- | --- | --- |
| M0-01 CMake + JUCE 工程 | Done | 2026-09-20 会话 1 | [evidence](REVIEWS/M0-01/evidence.md) | R2 | 三平台日志待 M0-06（ADR-003） |
| M0-02 目录与模块骨架 | Done | 2026-09-20 会话 2 | [evidence](REVIEWS/M0-02/evidence.md) | R2 | 构建结构转 M0-03（ADR-008）；头文件加入 `target_sources` 转 M0-06 |
| M0-03 APVTS 参数框架 | Done | 2026-09-20 会话 3 | [evidence](REVIEWS/M0-03/evidence.md) | R2 | 参数假设待裁决（M4-08：默认值/选项命名/ID 命名/控制深度范围） |
| M0-04 引擎空壳 + BusBuffers + 测试音 | In Review | 2026-09-20 会话 4 | [evidence](REVIEWS/M0-04/evidence.md) | R2 | 临时测试音待 M1-04 移除（ADR-010）；宿主多输出截图待补 |
| M0-05 MIDI 子块切分框架 | Backlog | — | — | R2 | — |
| M0-06 CI 流水线 | Backlog | — | — | R2 | — |
| M0-07 日志/断言/崩溃报告基础设施 | Backlog | — | — | R1 | — |

## 会话记录

### 2026-09-20 会话 1（M0-01）

- 范围：CMake + JUCE 工程（VST3/AU/Standalone）
- 决策：ADR-001（JUCE 8.0.9 + FetchContent）、ADR-002（本地仓库、CI 延后）、ADR-003（Windows 本地证据口径）、ADR-004（便携 CMake + VS 18 2026 生成器）、ADR-005（SDK 安装，已完成）、ADR-006（`JUCE_VST3_CAN_REPLACE_VST2=0`）
- 环境发现：PATH 上 cmake 为 MSYS2 版无 VS 生成器；本机为 VS 18 2026 BuildTools；Windows SDK 文件缺失（LNK1181）
- 结果：配置 + 清空全量 Release 构建通过（0 error、本工程 0 warning）；VST3/Standalone 产物生成；Standalone 冒烟存活 5s；证据包已归档
- 终审（AI 交叉审查）：0 Critical / 2 Important / 6 Minor；Important 已修复（ADR-005 状态、ADR-006）；证据补强（原始日志 + 退出码 + smoke.log + 全量重建哈希）；`// RT-safe` 注释按项目规范补齐
- 遗留（Minor，待人类裁决）：`JUCE_WEB_BROWSER=0`/`JUCE_USE_CURL=0`/LTO、CMakePresets、`CMAKE_CXX_EXTENSIONS OFF`、头文件加入 `target_sources`、宿主加载拷贝路径文档 → 随 M0-06/M0-02 处理
- 状态：In Review（待人类 R2 审查；宿主加载截图待人类补入）
- 收口（2026-09-20）：人类合并至 main（`43535c0`）并补入宿主加载截图（`REVIEWS/M0-01/2026-09-20 004115.png`）→ Done

### 2026-09-20 会话 2（M0-02）

- 范围：目录与模块骨架（架构 9.2）+ 文档归位 `Docs/` + `myJV_core` 静态库
- 决策：ADR-007（核心库暂不链接 JUCE；链接策略决策点延后至 M1-01）
- 结果：5 个模块编译锚编译通过；`myJV_core.lib` 3,166 字节；插件 0 error / 0 warning；冒烟通过；接口头文件清单归档
- 发现：核心库链接 JUCE 会导致模块重复编译与 100 MB 静态库（已修复，证据见 M0-02 日志）
- 终审（AI 交叉审查）：0 Critical / 1 Important / 8 Minor；Important（核心库 PIC）已修复；证据补强（清空全量重建 + UTF-8 日志）；ADR-007 状态改为"提议（待 R2）"
- 状态：In Review（R2，待人类审查）
- 收口（2026-09-20）：人类合并至 main（`7f5c7de`）并补入宿主加载截图（`REVIEWS/M0-02/2026-09-20 133140.png`）→ Done

### 2026-09-20 会话 3（M0-03）

- 范围：APVTS 参数框架 + 全量参数注册（510）+ Catch2 测试目标 + 构建结构改源列表
- 决策：ADR-008（`MYJV_CORE_SOURCES` 源列表，取代 ADR-007 结构选择）；Catch2 v3.16.0 经 tarball URL + SHA256 固定（git 协议不可达）
- 结果：510 参数注册（Patch Common 26 + Tone×4×121；Float 198 / Int 160 / Choice 106 / Bool 46）；测试 8 用例 / 3087 断言全绿；ctest 通过；变异抽查有效（pan 64→63 变红）；构建 0 error / 0 warning（`/utf-8` 消除 C4819）；冒烟通过；注册表归档
- 终审（AI 交叉审查）：0 Critical / 4 Important / 11 Minor；Important 已修复（补每 Tone Pitch LFO 深度 → 每 Tone 123、总数 **518**；自动化断言强化并做敏感性抽查；VST3 哈希修正）；Minor 修复（Catch2 条件拉取、Choice 良构测试、空指针检查、ADR-009 依赖记录等）
- 最终验证：9 用例 / 9436 断言全绿；ctest 1/1；清空全量重建 0 error / 0 warning
- 状态：In Review（R2，待人类审查）
- 收口（2026-09-20）：人类合并至 main（`fd4f611`）并补入宿主自动化截图（`REVIEWS/M0-03/2026-09-20 213614.png`）→ Done

### 2026-09-20 会话 4（M0-04）

- 范围：引擎空壳（SynthEngine/BusBuffers）+ 插件总线接线 + 临时测试音 + 引擎测试
- 决策：ADR-010（临时测试音，M1-04 移除）
- 结果：3 路立体声输出（Main/Out1/Out2）；测试音 440/660/880 Hz @ -20 dBFS；13 用例 / 57458 断言全绿；ctest 1/1；清空全量重建 0 error / 0 warning；WAV ×3 归档；冒烟通过
- 变异抽查：Out1 660→700 Hz → 测试变红（已恢复）
- 状态：In Review（R2，待人类审查）
