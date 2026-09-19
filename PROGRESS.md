# myJV 进度看板

> 唯一事实来源（开发计划 8.1）。状态：Backlog / Ready / In Progress / In Review / Changes Requested / Done / Blocked。
> 审查级别：R1 常规 / R2 深度 / R3 交叉 / R4 里程碑。

## 里程碑 M0：工程骨架（W1–W2）

| 任务 ID | 状态 | 会话记录 | 证据 | 审查 | 遗留问题 |
| --- | --- | --- | --- | --- | --- |
| M0-01 CMake + JUCE 工程 | In Review | 2026-09-20 会话 1 | [evidence](REVIEWS/M0-01/evidence.md) | R2 | 宿主加载截图待人类补；三平台日志待 M0-06 |
| M0-02 目录与模块骨架 | Backlog | — | — | R1 | — |
| M0-03 APVTS 参数框架 | Backlog | — | — | R1 | — |
| M0-04 引擎空壳 + BusBuffers + 测试音 | Backlog | — | — | R2 | — |
| M0-05 MIDI 子块切分框架 | Backlog | — | — | R2 | — |
| M0-06 CI 流水线 | Backlog | — | — | R2 | — |
| M0-07 日志/断言/崩溃报告基础设施 | Backlog | — | — | R1 | — |

## 会话记录

### 2026-09-20 会话 1（M0-01）

- 范围：CMake + JUCE 工程（VST3/AU/Standalone）
- 决策：ADR-001（JUCE 8.0.9 + FetchContent）、ADR-002（本地仓库、CI 延后）、ADR-003（Windows 本地证据口径）、ADR-004（便携 CMake + VS 18 2026 生成器）、ADR-005（SDK 缺失待安装）
- 环境发现：PATH 上 cmake 为 MSYS2 版无 VS 生成器；本机为 VS 18 2026 BuildTools；Windows SDK 文件缺失（LNK1181）
- 结果：配置 + Release 构建通过（0 error、本工程 0 warning）；VST3/Standalone 产物生成；Standalone 冒烟存活 5s；证据包已归档
- 状态：In Review（待人类 R2 审查；宿主加载截图待人类补入）
