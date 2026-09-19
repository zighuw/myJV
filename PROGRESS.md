# myJV 进度看板

> 唯一事实来源（开发计划 8.1）。状态：Backlog / Ready / In Progress / In Review / Changes Requested / Done / Blocked。
> 审查级别：R1 常规 / R2 深度 / R3 交叉 / R4 里程碑。

## 里程碑 M0：工程骨架（W1–W2）

| 任务 ID | 状态 | 会话记录 | 证据 | 审查 | 遗留问题 |
| --- | --- | --- | --- | --- | --- |
| M0-01 CMake + JUCE 工程 | Done | 2026-09-20 会话 1 | [evidence](REVIEWS/M0-01/evidence.md) | R2 | 三平台日志待 M0-06（ADR-003） |
| M0-02 目录与模块骨架 | In Review | 2026-09-20 会话 2 | [evidence](REVIEWS/M0-02/evidence.md) | R2 | JUCE 链接策略待 M1-01（ADR-007）；头文件加入 `target_sources` 转 M0-06 |
| M0-03 APVTS 参数框架 | Backlog | — | — | R1 | — |
| M0-04 引擎空壳 + BusBuffers + 测试音 | Backlog | — | — | R2 | — |
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
