# M0-03 证据包：APVTS 参数框架 + 全量参数注册

- 日期：2026-09-20
- 分支：`feature/M0-03-apvts-parameters`
- 审查级别：R2（受保护区域：`CMakeLists.txt`、`ParameterIDs.h`；人类裁决）
- 状态：待人类审查

## 1. 环境

同 M0-01（`REVIEWS/M0-01/evidence.md`）。新增依赖：Catch2 v3.16.0（FetchContent tarball URL + SHA256 固定，原因见 4.1 F1）。

## 2. 交付物

| 文件 | 内容 |
| --- | --- |
| `Source/Params/ParameterIDs.h` | ID 生成辅助（`patch()` / `tone()`）+ `createParameterLayout()` 声明 |
| `Source/Params/ParameterLayout.cpp` | 全量参数表与生成（Patch Common 26 + Tone×4×121 = **510**） |
| `Source/Plugin/MyJVProcessor.*` | APVTS 成员 + `createParameterLayout()` 接入 |
| `Tests/test_main.cpp` | Catch2 自定义 main（JUCE GUI 初始化，见 4.1 F4） |
| `Tests/param_tests.cpp` | 8 个用例 / 3087 断言 |
| `CMakeLists.txt` | ADR-008 源列表重构 + `myJV_tests` 目标 + MSVC `/utf-8` |
| `DECISIONS.md` | ADR-007 标注已取代；新增 ADR-008 |

参数统计（由 `parameter-registry.md` 逐项列出，510 行）：

| 分组 | 数量 |
| --- | --- |
| Patch Common | 26 |
| Tone WG / TVF（含 F-ENV）/ TVA（含 A-ENV）/ P-ENV / Pan·Output / LFO×2 / Control×3 | 21 / 21 / 18 / 13 / 8 / 16 / 24 = 121 × 4 |
| 合计 | **510** |

类型分布：Float 198 / Int 160 / Choice 106 / Bool 46。

> 与架构附录 A（约 515）的差异：附录 A 的 Patch Common ~30 含名称（非自动化）等未定项；本任务实现 26 项，差额与假设见 5。

## 3. 验证

### 3.1 配置与构建

- `configure.log` 末行：`CONFIGURE_EXIT=0`
- `build.log` 末行：`BUILD_EXIT=0`；error **0**；warning **0**（`/utf-8` 后 JUCE 源 C4819 亦消失）

### 3.2 测试

- `myJV_tests`：**All tests passed（8 test cases / 3087 assertions）**（`tests.log`）
- `ctest -C Release`：1/1 Passed（`ctest.log`）
- 覆盖：计数=510、ID 唯一性、点分路径规则、分组计数（26/121×4）、四 Tone 参数对称、关键默认值、选项数（LFO=8、TVF=5、CTRL=23、Control Source=97）、**全参数 set/get 自动化往返**、注册表导出
- **变异抽查**：`patch.common.pan` 默认值 64→63 → 1 断言失败（exit 42，`mutation-check.log`）；恢复后全绿

### 3.3 冒烟

`smoke.log`：Standalone 存活 5s（pid=20964）。

### 3.4 产物

| 产物 | SHA-256 |
| --- | --- |
| `build/myJV_artefacts/Release/Standalone/myJV.exe` | `17EE006CA67036F3E2835D738136E57FCEAA0BD537D5B966F7DA7CCAFAF0E2C8` |
| `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` | `84F3A45F4F89ECA9CE82F94BD48159B3540140696D7C1CA7178FEE98021A0F8` |

## 4. 发现与修复

| # | 问题 | 根因 | 修复 | 证据 |
| --- | --- | --- | --- | --- |
| F1 | Catch2 `git clone` 失败（重试 3 次） | github.com git 端口连接被重置；`codeload.github.com` HTTPS 可用 | FetchContent 改 URL + `URL_HASH SHA256=0957CAE5…AF34` | 首次失败日志被后续成功日志覆盖；现象见本表 |
| F2 | 测试编译失败：`ParameterLayout` 无 `begin/end` | JUCE 8 的 `ParameterLayout` 已改为不可迭代的 class | 测试改用真实 `AudioProcessorValueTreeState` + `TestProcessor`，经 `getParameters()` 读取 | `build-attempt1-layout-api.log` |
| F3 | 编译失败：`setValueNotifyingHost` 不能用于 const 指针 | 该 API 非 const | 测试收集非 const 参数指针 | `build-attempt2-const-pointer.log` |
| F4 | 测试进程崩溃（0xC0000005，无输出） | `ScopedJuceInitialiser_GUI` 作为命名空间静态对象触发 JUCE 静态初始化顺序问题 | 新增 `Tests/test_main.cpp` 自定义 main，在 main 内初始化 JUCE | 崩溃时 `tests.log` 为空；修复后全绿 |
| F5 | 注册表中文乱码 | MSVC 默认按 CP936 解释 UTF-8 源码 | `myJV` / `myJV_tests` 目标加 `/utf-8`（顺带消除 JUCE 源 C4819） | 乱码版注册表已被重新生成覆盖 |

## 5. 参数假设清单（待人类裁决，M4-08 参数语义规格定稿时确认）

| 类别 | 假设内容 |
| --- | --- |
| Patch Common 计数 | 实现 26 项；附录 A 标"约 30"（名称非自动化；Velocity Range 等未定项未注册） |
| 默认值 | 架构明确者（pan=64）优先；其余取中性初值：level/outputLevel=127、bendUp/Down=2、portamento.time=64、booster=64、cutoff=127、包络 level=127、包络 time=0、LFO rate=64、FXM color=1 |
| 选项命名 | 力度曲线 7 种命名（LINEAR/EXP1-3/LOG1-3）、Portamento Mode/Type/Start、Bias Direction、LFO Fade Mode 为占位命名 |
| Control Source | 列表 = CC01–CC95 + PITCH BEND + CH AFTERTOUCH（97 项）；默认 ctrlSource2=CC11、ctrlSource3=CC13 |
| 音域/力度范围 | velLow=1、velHigh=127、keyLow=0、keyHigh=127（资料值） |

## 6. 待补证据

- [ ] 宿主参数自动化演示截图（人类：在宿主中自动化 `patch.common.level` 等）
- [ ] macOS/Linux 构建与测试（M0-06 CI）
- [x] 变异抽查（`mutation-check.log`）

## 7. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `configure.log` | 配置（`CONFIGURE_EXIT=0`） |
| `build.log` | 最终构建（`BUILD_EXIT=0`，0 error / 0 warning） |
| `build-attempt1-layout-api.log` | F2（ParameterLayout API） |
| `build-attempt2-const-pointer.log` | F3（const 指针） |
| `tests.log` | 测试全绿（8 用例 / 3087 断言） |
| `ctest.log` | ctest 1/1 通过 |
| `mutation-check.log` | 变异抽查（1 断言失败） |
| `registry-run.log` | `[.registry]` 运行输出 |
| `parameter-registry.md` | 参数注册表（510 行，自动生成） |
| `smoke.log` | Standalone 冒烟 |
