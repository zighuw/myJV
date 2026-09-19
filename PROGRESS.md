# myJV 进度看板

> 唯一事实来源（开发计划 8.1）。状态：Backlog / Ready / In Progress / In Review / Changes Requested / Done / Blocked。
> 审查级别：R1 常规 / R2 深度 / R3 交叉 / R4 里程碑。

## 里程碑 M0：工程骨架（W1–W2）

| 任务 ID | 状态 | 会话记录 | 证据 | 审查 | 遗留问题 |
| --- | --- | --- | --- | --- | --- |
| M0-01 CMake + JUCE 工程 | In Progress | 2026-09-20 会话 1 | — | R2 | — |
| M0-02 目录与模块骨架 | Backlog | — | — | R1 | — |
| M0-03 APVTS 参数框架 | Backlog | — | — | R1 | — |
| M0-04 引擎空壳 + BusBuffers + 测试音 | Backlog | — | — | R2 | — |
| M0-05 MIDI 子块切分框架 | Backlog | — | — | R2 | — |
| M0-06 CI 流水线 | Backlog | — | — | R2 | — |
| M0-07 日志/断言/崩溃报告基础设施 | Backlog | — | — | R1 | — |

## 会话记录

### 2026-09-20 会话 1（M0-01）

- 范围：CMake + JUCE 工程（VST3/AU/Standalone）
- 决策：ADR-001（JUCE 8.0.9 + FetchContent）、ADR-002（本地仓库、CI 延后）、ADR-003（Windows 本地证据口径）
- 状态：进行中
