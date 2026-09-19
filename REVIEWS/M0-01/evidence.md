# M0-01 证据包：CMake + JUCE 工程（VST3/AU/Standalone）

- 日期：2026-09-20
- 分支：`feature/M0-01-cmake-juce`
- 审查级别：R2
- 状态：待人类审查（宿主加载截图待人类补入；macOS/Linux 构建日志待 M0-06 CI 补全，见 ADR-003）

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

结果（`configure.log` 尾部）：

```
-- Selecting Windows SDK version 10.0.26100.0 to target Windows 10.0.22631.
-- The CXX compiler identification is MSVC 19.51.36257.0
-- Configuring juceaide / Building juceaide / Testing juceaide: OK
-- Configuring done (76.3s)
-- Build files have been written to: D:/Workspace/myJV/build
EXIT_CODE=0
```

### 3.2 构建

命令：`D:\Program\CMake\bin\cmake.exe --build build --config Release`

结果（`build.log`）：`EXIT_CODE=0`

| 指标 | 数量 |
| --- | --- |
| error（C/LNK/MSB） | **0** |
| warning C4819（JUCE harfbuzz 等源文件，CP936 环境编码提示） | 33 |
| CUSTOMBUILD warning（CMAKE_GENERATOR_PLATFORM 环境变量，CMake 自身） | 7 |
| **本工程源码（MyJVProcessor.cpp）警告** | **0** |

> 说明：C4819 与 CUSTOMBUILD 警告均来自 JUCE/CMake，不是本任务代码；记录备查。

### 3.3 产物

| 产物 | 路径 | 大小（字节） | 时间 | SHA-256 |
| --- | --- | --- | --- | --- |
| Standalone | `build/myJV_artefacts/Release/Standalone/myJV.exe` | 7,909,376 | 2026-09-20 00:18:28 | `10AE7467348B3C7AECA30EB807AFE5B2D10B98B6D4769A993707B713E39856EF` |
| VST3（二进制） | `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` | 6,841,856 | 2026-09-20 00:18:22 | `D5881537D48EED96B4405F53F8950531AB1FF07F4D33E8889DB9F74E4A03D066` |

VST3 bundle 结构：`Contents/x86_64-win/myJV.vst3` + `Contents/Resources/moduleinfo.json`（JUCE 自动清单）。

AU 在 Windows 平台由 `juce_add_plugin` 自动跳过（符合预期）；macOS 构建留待 M0-06。

### 3.4 Standalone 启动冒烟

方法：启动进程 → 等待 5 秒 → 检查存活 → 强制结束。

结果：

```
SMOKE: process alive after 5s (pid=23920)
SMOKE: stopped
```

> 局限：仅验证进程不即时崩溃，不验证音频设备与出声（测试音在 M0-04）。

## 4. 过程中发现并修复的问题（供审查）

| # | 问题 | 根因 | 修复 | 证据日志 |
| --- | --- | --- | --- | --- |
| P1 | `C2065: JucePlugin_AcceptsMidi/ProducesMidi 未声明` | 实现时使用了不存在的宏名（幻觉 API） | 改为 `JucePlugin_WantsMidiInput` / `JucePlugin_ProducesMidiOutput`（经 JUCE 源码与官方示例核对） | `build-attempt1-macro-error.log` |
| P2 | `C1189: VST2/VST3 参数自动化冲突` | JUCE 对"VST3 可替代 VST2"默认开启的编译期保护 | `JUCE_VST3_CAN_REPLACE_VST2=0`（本项目从未发布 VST2；与官方 CMake 示例一致） | `build-attempt2-vst3-replace-vst2.log` |
| E1 | 配置失败：无 VS 生成器 | PATH 上 cmake 为 MSYS2 版 | 便携版 CMake 4.4.3（ADR-004） | `configure-attempt1-msys2-failed.log` |
| E2 | 配置失败：找不到 VS 实例 | 本机为 VS 18 2026 BuildTools，无 VS 2022 | 生成器改用 `Visual Studio 18 2026`（ADR-004） | `configure-attempt2-vs17-not-found.log` |
| E3 | 链接失败：`LNK1181 kernel32.lib` | Windows SDK 文件缺失（注册表有登记） | 人类安装 SDK 10.0.26100（ADR-005） | `configure-attempt4-sdk-missing.log`、`CMakeConfigureLog-sdk-missing.yaml` |

> 注：另有一次配置失败（VS18 生成器 + 残留 VS17 CMakeCache）未保留日志，属操作序列问题，清理 `build/` 后解决。

## 5. 待补证据

- [ ] 宿主加载截图（人类：VST3 装入宿主 / Standalone 启动截图）
- [ ] macOS/Linux 构建日志（M0-06 CI）
- [x] 参数核对表：不适用（M0-01 无自动化参数）
- [x] 变异抽查：不适用（无单元测试；M0-01 为构建配置 + 样板代码，属 TDD 明示例外，已随设计批准）

## 6. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `configure.log` | 成功配置（UTF-8） |
| `configure-attempt1-msys2-failed.log` | MSYS2 CMake 无 VS 生成器 |
| `configure-attempt2-vs17-not-found.log` | VS17 生成器找不到实例 |
| `configure-attempt4-sdk-missing.log` | VS18 生成器下编译器探测失败（SDK 缺失） |
| `CMakeConfigureLog-sdk-missing.yaml` | 编译器探测明细（LNK1181 kernel32.lib） |
| `build.log` | 成功构建（UTF-8） |
| `build-attempt1-macro-error.log` | P1 宏名错误 |
| `build-attempt2-vst3-replace-vst2.log` | P2 VST3/VST2 保护错误 |
