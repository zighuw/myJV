# M0-07 证据包：日志/断言/崩溃报告基础设施

- 日期：2026-09-23
- 分支：`feature/M0-07-diagnostics`
- 审查级别：R2（受保护区域：`CMakeLists.txt` 新增两个源文件；人类裁决）
- 状态：待人类审查（终审修复轮已执行，见 5.2）

## 1. 环境

同 M0-01（`REVIEWS/M0-01/evidence.md`）。

## 2. 交付物

| 文件 | 内容 |
| --- | --- |
| `Source/Plugin/MyJVLog.h/.cpp` | 引用计数 `FileLogger`；保存/恢复宿主 logger；未配对 `shutdown()` 静默忽略 |
| `Source/Plugin/MyJVCrashHandler.h/.cpp` | `std::atomic` 幂等 `install()`；Windows `SetUnhandledExceptionFilter`（链式 + 卸载恢复 + 重入保护）；`writeCrashReport()`（可测）；minidump（DbgHelp，失败在报告中注明） |
| `Source/Plugin/MyJVProcessor.h/.cpp` | 构造：初始化日志 + 安装崩溃处理器 + 记录；析构：记录 + 关闭 |
| `CMakeLists.txt` | `MyJVLog.cpp`、`MyJVCrashHandler.cpp` 入 `MYJV_CORE_SOURCES`；测试目标增加 `diagnostics_tests.cpp` |
| `Tests/diagnostics_tests.cpp` | 4 个用例（日志内容、引用计数、未配对关闭、崩溃报告字段） |

## 3. 配置说明

| 项 | 值 |
| --- | --- |
| 日志目录 | `%APPDATA%\myJV\logs\`（`userApplicationDataDirectory/myJV/logs`） |
| 日志文件 | `myJV.log`；**每次 `initialise()` 时裁剪至 ≤1 MB（JUCE `FileLogger` 行为）；会话期间不轮转、可持续增长** |
| 初始化 | `MyJVProcessor` 构造：`MyJVLog::initialise()`；引用计数（多插件实例安全；仅消息线程） |
| 宿主兼容 | 初始化时保存宿主 `Logger`，关闭时恢复（尽力而为；宿主若在期间更换 logger 则以最近者为准） |
| 关闭 | 引用计数归零移除 logger；未配对 `shutdown()` 静默忽略（Release 安全） |
| 崩溃文件 | `crash-<时间戳>.txt` + `crash-<时间戳>.dmp`（同目录；文件名仅保留字母数字与 `-`/`_`） |
| 崩溃处理器 | 进程级 `SetUnhandledExceptionFilter`；`std::atomic` 幂等（无自链）；链式调用原处理器；**模块卸载时尽力恢复原处理器**；重入保护；非 Windows no-op |
| 链接 | `#pragma comment(lib, "DbgHelp.lib")`（**MSVC 专属**；其他 Windows 工具链需自行链接 dbghelp） |
| 断言策略 | RT 路径仅允许 `jassert`（debug、无副作用）；**禁止日志/IO/异常**（计划 7.4）；日志 API 仅限非 RT 线程 |
| 残留限制 | 无法检测/枚举其后第三方安装的异常处理器；进程内捕获对栈溢出/堆损坏等不保证（必要时 M6 引入外部捕获） |
| RT 无日志证据 | `REVIEWS/M0-07/rt-logging-scan.txt`（加宽模式集；RT 模块与 `MyJVProcessor.h` 均 0 匹配） |

## 4. 验证

### 4.1 配置与构建

- `build.log`（含命令/日期头注，清空全量重建）末行：`BUILD_EXIT=0`；error **0**；warning **0**

### 4.2 测试

- `myJV_tests`：**All tests passed（30 test cases / 60604 assertions）**（`tests.log`）
- `ctest -C Release`：1/1 Passed（`ctest.log`）
- 新增覆盖：
  1. 日志初始化创建文件并写入 `[INFO]/[WARN]/[ERROR]` 内容（含前缀断言）
  2. 引用计数：两次初始化 + 一次关闭后 logger 仍在；再关闭后为空
  3. 未配对 `shutdown()` 被忽略且后续初始化正常
  4. 崩溃报告：合成 `CrashInfo` 写出，校验时间戳/版本/异常码/地址/minidump 文件名与文件存在性
- **变异抽查**：报告省略版本字段 → 1 断言失败（`mutation-check.log`）；恢复后全绿

### 4.3 真实运行证据

Standalone 冒烟运行后，`%APPDATA%\myJV\logs\myJV.log` 实际生成：

```
myJV log
Log started: 23 Sep 2026 10:07:14am

[INFO] processor created: 0.1.0
```

> 冒烟为强制结束进程，故无 `processor destroyed` 记录（析构未执行），属预期。

### 4.4 冒烟与产物

- `smoke.log`：Standalone 存活 5s（pid=24276）
- `build/myJV_artefacts/Release/Standalone/myJV.exe` SHA-256：`D83B5431328D79566F146FE5AFA3A6942440383FF0ECAF5091251D070A90712C`
- `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` SHA-256：`9892513C76E5EDF3102EE1E75EE27685536EB0076A74BE1C07B0F60F0C633F8F`

## 5. 修复记录

### 5.1 实现过程中

| # | 问题 | 根因 | 修复 |
| --- | --- | --- | --- |
| F1 | 崩溃报告用例失败：期望文件名含 `-123456` | 文件名清洗规则仅保留字母数字与 `-`/`_`（空格/冒号被移除） | 修正测试期望与规范一致 |

### 5.2 终审修复轮（AI 交叉审查，2026-09-23）

终审结论：0 Critical / 4 Important / 7 Minor。

| # | 发现 | 级别 | 处置 |
| --- | --- | --- | --- |
| R1 | "1 MB 轮转"表述不实（`FileLogger` 仅构造时裁剪） | Important | ADR-012 与本文档改为"每次 initialise 裁剪至 ≤1 MB；会话内不轮转"，并记录该语义 |
| R2 | 异常过滤器随模块卸载悬空、宿主原处理器不恢复 | Important | 增加 TU 静态作用域：模块卸载时尽力恢复原处理器；残限写入 ADR-012 |
| R3 | 宿主 `Logger` 被覆盖且不恢复 | Important | 首次初始化保存宿主 logger，最终关闭时恢复（尽力而为） |
| R4 | `install()` 非线程安全可自链（递归 → 栈溢出） | Important | `std::atomic` + `compare_exchange` 幂等；`initialisationCount` 同步改 `std::atomic` |
| R5 | 处理器缺防御/重入保护 | Minor | 空指针检查 + `InterlockedCompareExchange` 重入保护 |
| R6 | 转储失败被当作成功 | Minor | `MiniDumpWriteDump` 返回值检查；失败时在报告追加说明 |
| R7 | 非 MSVC 工具链链接缺口 | Minor | ADR/evidence 注明 MSVC 专属，其他工具链需自行链接 dbghelp |
| R8 | 测试缺口（未配对关闭、INFO 前缀、并行目录） | Minor | 补未配对关闭与 INFO 前缀断言；并行目录限制记录为已知（单目标串行执行） |
| R9 | 未配对 `shutdown()` 在 Release 破坏计数 | Minor | 增加 `count <= 0` 运行时守卫（静默忽略） |
| R10 | 契约仅存在于文档 | Minor | 契约注释写入两个头文件（消息线程/非 RT/幂等/链式） |
| R11 | RT 扫描模式集偏窄、未含 `MyJVProcessor.h` | Minor | 加宽模式（`std::cerr`/`fprintf`/`juce::File` 等）并纳入 `MyJVProcessor.h`；建议 M0-06 固化为 CI 门禁 |

## 6. 待补证据

- [ ] macOS/Linux 构建与测试（M0-06 CI；崩溃处理器为 Windows 专属，其余平台 no-op）
- [ ] 崩溃处理器真实触发验证（建议子进程触发；M0-06 或 M6 稳定性阶段）
- [x] 变异抽查（`mutation-check.log`）
- [x] RT 路径无日志扫描（`rt-logging-scan.txt`）

## 7. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `build.log` | 最终清空全量构建（含命令/日期头注，0 error / 0 warning） |
| `tests.log` | 测试全绿（30 用例 / 60604 断言） |
| `ctest.log` | ctest 1/1 通过 |
| `mutation-check.log` | 变异抽查（报告省略版本字段，1 断言失败） |
| `rt-logging-scan.txt` | RT 路径无日志扫描证据（加宽模式集） |
| `smoke.log` | Standalone 冒烟 |
