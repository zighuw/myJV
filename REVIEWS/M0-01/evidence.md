# M0-01 证据包：CMake + JUCE 工程（VST3/AU/Standalone）

- 日期：2026-09-20
- 分支：`feature/M0-01-cmake-juce`
- 审查级别：R2
- 状态：待人类审查（宿主加载截图待人类补入；macOS/Linux 构建日志待 M0-06 CI 补全，见 ADR-003）
- 修订：2026-09-20 终审修复轮（ADR-006、RT-safe 注释、原始日志重新采集）

## 1. 环境

| 项 | 值 |
| --- | --- |
| OS | Windows（10.0.22631 目标） |
| CMake | 4.4.3 便携版 `D:\Program\CMake`（ADR-004，PATH 上的 MSYS2 CMake 不可用） |
| 生成器 | `Visual Studio 18 2026`，x64 |
| 编译器 | MSVC 19.51.36257.0（VS 18 2026 BuildTools 18.10.1，工具集 14.51.36231） |
| Windows SDK | 10.0.26100.0（人类安装，ADR-005） |
| JUCE | 8.0.9（FetchContent，浅克隆，ADR-001） |

## 2. 交付物

- `CMakeLists.txt`
- `Source/Plugin/MyJVProcessor.h`
- `Source/Plugin/MyJVProcessor.cpp`

## 3. 构建证据

### 3.1 配置

命令：`D:\Program\CMake\bin\cmake.exe -B build -G "Visual Studio 18 2026" -A x64`

首次完整配置（`configure-initial-full.log`，含工具链探测）：

```
-- Selecting Windows SDK version 10.0.26100.0 to target Windows 10.0.22631.
-- The CXX compiler identification is MSVC 19.51.36257.0
-- Check for working CXX compiler: D:/Program/MSVC/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe - skipped
-- Configuring done (76.3s)
-- Build files have been written to: D:/Workspace/myJV/build
```

最终树重跑（`configure.log` 原始输出尾部，含退出码）：

```
-- Configuring done (2.8s)
-- Generating done (0.1s)
-- Build files have been written to: D:/Workspace/myJV/build
CONFIGURE_EXIT=0
```

### 3.2 构建

命令：`D:\Program\CMake\bin\cmake.exe --build build --config Release --clean-first`（清空后全量重建）

结果（`build.log` 原始输出，末行 `BUILD_EXIT=0`）：

| 指标 | 数量 |
| --- | --- |
| error（C/LNK/MSB） | **0** |
| warning C4819（JUCE harfbuzz 等源文件，CP936 环境编码提示） | 33 |
| CUSTOMBUILD warning | 0 |
| **本工程源码（MyJVProcessor.cpp）警告** | **0** |

> 说明：C4819 来自 JUCE 自身源文件，非本任务代码。日志编码：cmd 重定向时 MSVC 使用 ANSI（CP936），已转码为 UTF-8；个别字符因捕获丢失显示为替换符。早期增量构建日志（含 7 条 CMAKE_GENERATOR_PLATFORM 环境警告）见提交 474fc2b。

### 3.3 产物

| 产物 | 路径 | 大小（字节） | 时间 | SHA-256 |
| --- | --- | --- | --- | --- |
| Standalone | `build/myJV_artefacts/Release/Standalone/myJV.exe` | 7,909,376 | 2026-09-20 00:25:10 | `02BC2F843A1723F54FF4412D6C9A3A57C66DF8AFA35E7AA69F9B9492587CA601` |
| VST3（二进制） | `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` | 6,841,856 | 2026-09-20 00:25:04 | `8EC2EE95A2AA5666A8FDC6A81F1EC19AB58DB27C60A0A4B691BAA7E473CFF24D` |

VST3 bundle 结构：`Contents/x86_64-win/myJV.vst3` + `Contents/Resources/moduleinfo.json`（JUCE 自动清单）。

AU 在 Windows 平台由 `juce_add_plugin` 自动跳过（符合预期）；macOS 构建留待 M0-06。

### 3.4 Standalone 启动冒烟

`smoke.log` 原始记录：

```
SMOKE TEST 2026-09-20 00:25:20
command: Start-Process build\myJV_artefacts\Release\Standalone\myJV.exe; wait 5s; check HasExited; Stop-Process -Force
result: ALIVE after 5s (pid=6076)
```

> 局限：仅验证进程不即时崩溃，不验证音频设备与出声（测试音在 M0-04）。

## 4. 修复记录

### 4.1 实现过程中（构建门禁捕获）

| # | 问题 | 根因 | 修复 | 证据日志 |
| --- | --- | --- | --- | --- |
| P1 | `C2065: JucePlugin_AcceptsMidi/ProducesMidi 未声明` | 实现时使用了不存在的宏名（幻觉 API） | 改为 `JucePlugin_WantsMidiInput` / `JucePlugin_ProducesMidiOutput`（经 JUCE 源码与官方示例核对） | `build-attempt1-macro-error.log` |
| P2 | `C1189: VST2/VST3 参数自动化冲突` | JUCE 对"VST3 可替代 VST2"默认开启的编译期保护 | `JUCE_VST3_CAN_REPLACE_VST2=0`（本项目从未发布 VST2；与官方 CMake 示例一致，见 ADR-006） | `build-attempt2-vst3-replace-vst2.log` |
| E1 | 配置失败：无 VS 生成器 | PATH 上 cmake 为 MSYS2 版 | 便携版 CMake 4.4.3（ADR-004） | `configure-attempt1-msys2-failed.log` |
| E2 | 配置失败：找不到 VS 实例 | 本机为 VS 18 2026 BuildTools，无 VS 2022 | 生成器改用 `Visual Studio 18 2026`（ADR-004） | `configure-attempt2-vs17-not-found.log` |
| E3 | 链接失败：`LNK1181 kernel32.lib` | Windows SDK 文件缺失（注册表有登记） | 人类安装 SDK 10.0.26100（ADR-005） | `configure-attempt4-sdk-missing.log`、`CMakeConfigureLog-sdk-missing.yaml` |

> 注：另有一次配置失败（VS18 生成器 + 残留 VS17 CMakeCache）未保留日志，属操作序列问题，清理 `build/` 后解决。

### 4.2 终审修复轮（AI 交叉审查，2026-09-20）

终审结论：0 Critical / 2 Important / 6 Minor。

| # | 发现 | 级别 | 处置 |
| --- | --- | --- | --- |
| R1 | ADR-005 状态仍为"待安装"，与证据矛盾 | Important | `DECISIONS.md` 更新为"已完成" |
| R2 | `JUCE_VST3_CAN_REPLACE_VST2=0` 未记录为 ADR（影响发布后兼容性） | Important | 新增 ADR-006（发布后不得更改） |
| R3 | `processBlock` 缺 `// RT-safe` 注释（AGENTS.md 规范） | 规范项（升为必修） | 已添加 |
| R4 | 证据日志缺退出码/原始输出；无 smoke 日志；中文乱码 | Minor（升为必修：证据门禁 DoD） | 重跑配置/全量构建/冒烟并归档原始日志（UTF-8 + 退出码） |
| R5 | 建议关闭 `JUCE_WEB_BROWSER`/`JUCE_USE_CURL`、启用 LTO、CMakePresets 等 | Minor | 推迟（记入 PROGRESS 遗留，随 M0-06/M0-02 处理） |

## 5. 待补证据

- [ ] 宿主加载截图（人类：VST3 装入宿主 / Standalone 启动截图）
- [ ] macOS/Linux 构建日志（M0-06 CI）
- [x] 参数核对表：不适用（M0-01 无自动化参数）
- [x] 变异抽查：不适用（无单元测试；M0-01 为构建配置 + 样板代码，属 TDD 明示例外，已随设计批准）

## 6. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `configure-initial-full.log` | 首次完整配置（工具链探测，76.3s） |
| `configure.log` | 最终树配置重跑（原始输出 + `CONFIGURE_EXIT=0`） |
| `build.log` | 清空全量重建（原始输出 + `BUILD_EXIT=0`） |
| `smoke.log` | Standalone 冒烟原始记录 |
| `configure-attempt1-msys2-failed.log` | MSYS2 CMake 无 VS 生成器 |
| `configure-attempt2-vs17-not-found.log` | VS17 生成器找不到实例 |
| `configure-attempt4-sdk-missing.log` | VS18 生成器下编译器探测失败（SDK 缺失） |
| `CMakeConfigureLog-sdk-missing.yaml` | 编译器探测明细（LNK1181 kernel32.lib） |
| `build-attempt1-macro-error.log` | P1 宏名错误 |
| `build-attempt2-vst3-replace-vst2.log` | P2 VST3/VST2 保护错误 |
