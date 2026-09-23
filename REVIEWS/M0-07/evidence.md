# M0-07 证据包：日志/断言/崩溃报告基础设施

- 日期：2026-09-23
- 分支：`feature/M0-07-diagnostics`
- 审查级别：R2（受保护区域：`CMakeLists.txt` 新增两个源文件；人类裁决）
- 状态：待人类审查

## 1. 环境

同 M0-01（`REVIEWS/M0-01/evidence.md`）。

## 2. 交付物

| 文件 | 内容 |
| --- | --- |
| `Source/Plugin/MyJVLog.h/.cpp` | 引用计数 `FileLogger`（1 MB 轮转）；`initialise/shutdown`；`logInfo/logWarning/logError` |
| `Source/Plugin/MyJVCrashHandler.h/.cpp` | 幂等 `install()`；Windows `SetUnhandledExceptionFilter`（链式）；`writeCrashReport()`（可测）；minidump（DbgHelp） |
| `Source/Plugin/MyJVProcessor.h/.cpp` | 构造：初始化日志 + 安装崩溃处理器 + 记录；析构：记录 + 关闭 |
| `CMakeLists.txt` | `MyJVLog.cpp`、`MyJVCrashHandler.cpp` 入 `MYJV_CORE_SOURCES`；测试目标增加 `diagnostics_tests.cpp` |
| `Tests/diagnostics_tests.cpp` | 3 个用例（日志文件/引用计数/崩溃报告字段） |

## 3. 配置说明

| 项 | 值 |
| --- | --- |
| 日志目录 | `%APPDATA%\myJV\logs\`（`userApplicationDataDirectory/myJV/logs`） |
| 日志文件 | `myJV.log`；最大 1 MB 后轮转（`FileLogger` 行为） |
| 初始化 | `MyJVProcessor` 构造：`MyJVLog::initialise()`；引用计数（多插件实例安全；仅消息线程） |
| 关闭 | `MyJVProcessor` 析构：`MyJVLog::shutdown()`；计数归零移除 logger |
| 崩溃文件 | `crash-<时间戳>.txt` + `crash-<时间戳>.dmp`（同一目录；文件名仅保留字母数字与 `-`/`_`） |
| 崩溃处理器 | 进程级 `SetUnhandledExceptionFilter`，幂等安装；处理后**链式调用原处理器**；非 Windows 为 no-op |
| 断言策略 | RT 路径仅允许 `jassert`（debug、无副作用）；**禁止日志/IO/异常**（计划 7.4）；日志 API 仅限非 RT 线程 |
| RT 无日志证据 | `REVIEWS/M0-07/rt-logging-scan.txt`：RT 模块 0 匹配；`processBlock` 内无日志 |

## 4. 验证

### 4.1 配置与构建

- `build.log`（含命令/日期头注，清空全量重建）末行：`BUILD_EXIT=0`；error **0**；warning **0**

### 4.2 测试

- `myJV_tests`：**All tests passed（29 test cases / 60603 assertions）**（`tests.log`）
- `ctest -C Release`：1/1 Passed（`ctest.log`）
- 新增覆盖：
  1. 日志初始化创建文件并写入 `[INFO]/[WARN]/[ERROR]` 内容
  2. 引用计数：两次初始化 + 一次关闭后 logger 仍在；再关闭后为空
  3. 崩溃报告：合成 `CrashInfo` 写出，校验时间戳/版本/异常码/地址/minidump 文件名与文件存在性
- **变异抽查**：报告省略版本字段 → 1 断言失败（`mutation-check.log`）；恢复后全绿

### 4.3 真实运行证据

Standalone 冒烟运行后，`%APPDATA%\myJV\logs\myJV.log` 实际生成（144 字节）：

```
myJV log
Log started: 23 Sep 2026 10:07:14am

[INFO] processor created: 0.1.0
```

> 冒烟为强制结束进程，故无 `processor destroyed` 记录（析构未执行），属预期。

### 4.4 冒烟与产物

- `smoke.log`：Standalone 存活 5s（pid=22240）
- `build/myJV_artefacts/Release/Standalone/myJV.exe` SHA-256：`3D8184A519136FAD7C273DBEBD53859DFB23ADD77B1C3E788F091940474459F1`
- `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` SHA-256：`4CB2A2389659A3E5A3FB3B1C5D1233C4BCB2A2E29691CF8462FAF69EA163E666`

## 5. 发现与修复

| # | 问题 | 根因 | 修复 |
| --- | --- | --- | --- |
| F1 | 崩溃报告用例失败：期望文件名含 `-123456` | 文件名清洗规则仅保留字母数字与 `-`/`_`，空格/冒号被移除（`2026-09-22123456`） | 修正测试期望与规范一致（保留清洗规则） |

## 6. 待补证据

- [ ] macOS/Linux 构建与测试（M0-06 CI；`SetUnhandledExceptionFilter` 为 Windows 专属，其余平台 no-op）
- [ ] 崩溃处理器真实触发验证（建议 M0-06 或 M6 稳定性阶段；本任务仅验证报告写出函数）
- [x] 变异抽查（`mutation-check.log`）
- [x] RT 路径无日志扫描（`rt-logging-scan.txt`）

## 7. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `build.log` | 最终清空全量构建（含命令/日期头注，0 error / 0 warning） |
| `tests.log` | 测试全绿（29 用例 / 60603 断言） |
| `ctest.log` | ctest 1/1 通过 |
| `mutation-check.log` | 变异抽查（报告省略版本字段，1 断言失败） |
| `rt-logging-scan.txt` | RT 路径无日志扫描证据 |
| `smoke.log` | Standalone 冒烟 |
