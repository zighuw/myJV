# M0-03 证据包：APVTS 参数框架 + 全量参数注册

- 日期：2026-09-20
- 分支：`feature/M0-03-apvts-parameters`
- 审查级别：R2（受保护区域：`CMakeLists.txt`、`ParameterIDs.h`；人类裁决）
- 状态：已完成（人类已合并至 main `fd4f611` 并补入宿主自动化截图 `REVIEWS/M0-03/2026-09-20 213614.png`）

## 1. 环境

同 M0-01（`REVIEWS/M0-01/evidence.md`）。新增依赖：Catch2 v3.16.0（FetchContent tarball URL + SHA256 固定，原因见 4.1 F1；依赖记录见 ADR-009）。

## 2. 交付物

| 文件 | 内容 |
| --- | --- |
| `Source/Params/ParameterIDs.h` | `createParameterLayout()` 声明 |
| `Source/Params/ParameterLayout.cpp` | 全量参数表与生成（Patch Common 26 + Tone×4×123 = **518**） |
| `Source/Plugin/MyJVProcessor.*` | APVTS 成员 + `createParameterLayout()` 接入 |
| `Tests/test_main.cpp` | Catch2 自定义 main（JUCE GUI 初始化，见 4.1 F4） |
| `Tests/param_tests.cpp` | 9 个用例 / 9436 断言 |
| `CMakeLists.txt` | ADR-008 源列表重构 + `myJV_tests` 目标 + MSVC `/utf-8` |
| `DECISIONS.md` | ADR-007 标注已取代；新增 ADR-008、ADR-009 |

参数统计（`parameter-registry.md` 逐项列出，518 行）：

| 分组 | 数量 |
| --- | --- |
| Patch Common | 26 |
| Tone WG / TVF（含 F-ENV）/ TVA（含 A-ENV）/ P-ENV / Pan·Output / LFO×2 / Control×3 | 23 / 21 / 18 / 13 / 8 / 16 / 24 = 123 × 4 |
| 合计 | **518** |

类型分布：Float 206 / Int 160 / Choice 106 / Bool 46。

> 与架构附录 A（约 515）的差异：WG 含 `pitchLfo1Depth`/`pitchLfo2Depth`（架构 4.2 `pitchLfoDepth[2]` 与 5.8 要求、JV-1080 资料 8.2 偏移 4E–4F；附录 A 的 WG=21 未计入）；Patch Common 实现 26（附录 A 约 30 含名称等非自动化项）。详见 5。

## 3. 验证

### 3.1 配置与构建

- `configure.log` 末行：`CONFIGURE_EXIT=0`
- `build.log`（清空全量重建，198 行）末行：`BUILD_EXIT=0`；error **0**；warning **0**（含 C4819=0，`/utf-8` 生效）

### 3.2 测试

- `myJV_tests`：**All tests passed（9 test cases / 9436 assertions）**（`tests.log`）
- `ctest -C Release`：1/1 Passed（`ctest.log`）
- 覆盖：计数=518、ID 唯一性、点分路径规则、分组计数（26/123×4）、四 Tone 参数对称、关键默认值、选项数（LFO=8、TVF=5、CTRL=23、Control Source=97）、Choice 良构（≥2 项、非空）、**全参数 set/get 自动化往返**、注册表导出
- **变异抽查（生产代码）**：`patch.common.pan` 默认值 64→63 → 1 断言失败（exit 42，`mutation-check.log`）；恢复后全绿
- **变异抽查（测试敏感性）**：临时移除 `setValueNotifyingHost` 调用 → 2 断言失败（exit 42，`mutation-check2-test-sensitivity.log`）；证明自动化断言非恒真

### 3.3 冒烟

`smoke.log`：Standalone 存活 5s（pid=26356）。

### 3.4 产物

| 产物 | SHA-256 |
| --- | --- |
| `build/myJV_artefacts/Release/Standalone/myJV.exe` | `BCB860516D341965E9FD7F853895A1A75D6D48956A1A2E3001F96C4D593FEEC6` |
| `build/myJV_artefacts/Release/VST3/myJV.vst3/Contents/x86_64-win/myJV.vst3` | `72D70227E600243F1497FB1DA6908E10FF492A7559480E9628780438D8F7DDF0` |

## 4. 修复记录

### 4.1 实现过程中

| # | 问题 | 根因 | 修复 | 证据 |
| --- | --- | --- | --- | --- |
| F1 | Catch2 `git clone` 失败（重试 3 次） | github.com git 端口连接被重置；`codeload.github.com` HTTPS 可用 | FetchContent 改 URL + `URL_HASH SHA256=0957CAE5…AF34` | 首次失败日志被后续成功日志覆盖；现象见本表 |
| F2 | 测试编译失败：`ParameterLayout` 无 `begin/end` | JUCE 8 的 `ParameterLayout` 已改为不可迭代的 class | 测试改用真实 `AudioProcessorValueTreeState` + `TestProcessor`，经 `getParameters()` 读取 | `build-attempt1-layout-api.log` |
| F3 | 编译失败：`setValueNotifyingHost` 不能用于 const 指针 | 该 API 非 const | 测试收集非 const 参数指针 | `build-attempt2-const-pointer.log` |
| F4 | 测试进程崩溃（0xC0000005，无输出） | `ScopedJuceInitialiser_GUI` 作为命名空间静态对象触发 JUCE 静态初始化顺序问题 | 新增 `Tests/test_main.cpp` 自定义 main，在 main 内初始化 JUCE | 崩溃时 `tests.log` 为空；修复后全绿 |
| F5 | 注册表中文乱码 | MSVC 默认按 CP936 解释 UTF-8 源码 | `myJV` / `myJV_tests` 目标加 `/utf-8`（顺带消除 JUCE 源 C4819） | 乱码版注册表已被重新生成覆盖 |

### 4.2 终审修复轮（AI 交叉审查，2026-09-20）

终审结论：0 Critical / 4 Important / 11 Minor。

| # | 发现 | 级别 | 处置 |
| --- | --- | --- | --- |
| R1 | 缺每 Tone Pitch LFO 1/2 Depth（架构 4.2/5.8、资料 8.2） | Important | 已补 8 个参数；每 Tone 121→123、总数 510→**518**；注册表重生成 |
| R2 | 自动化往返断言对 2 步参数恒真 | Important | 改为对照 `convertTo0to1(convertFrom0to1(target))`；Bool 因 JUCE 原样存储归一化值单独处理；并以敏感性抽查证明非恒真 |
| R3 | 证据中 VST3 哈希少 1 位（转录错误） | Important | 修正为最终产物哈希（见 3.4） |
| R4 | 宿主参数自动化演示截图缺失 | Important | 人类待补（本文件 6；任务保持 In Review） |
| R5 | `ParameterIDs` 辅助函数为死代码 | Minor | 删除（单一来源 = `ParameterLayout.cpp`；ID 规则由测试约束） |
| R6 | `BUILD_TESTING=OFF` 时仍拉取 Catch2 | Minor | Catch2 声明与拉取移入 `if(BUILD_TESTING)` |
| R7 | "0 warning" 口径依赖增量构建 | Minor | 改为清空全量重建日志（198 行，0 warning） |
| R8 | Choice 覆盖不足 | Minor | 新增 "all choice parameters are well-formed" 用例（106 项） |
| R9 | `dynamic_cast` 无空检查 | Minor | `collectParameters` 中加 `REQUIRE` |
| R10 | `kControlDestCount` 手写 23 | Minor | 改用 `std::size` |
| R11 | 锚点移除后骨架头文件不再编译；控制深度范围；ID 命名与 4.1 示例差异 | Minor | 已记入 ADR-008 与本文档 5 |
| R12 | 依赖记录缺失 | Minor（建议） | 新增 ADR-009（Catch2 用途/许可证/体积/替代） |

## 5. 参数假设与规格缺口（待人类裁决，M4-08 参数语义规格定稿时确认）

| 类别 | 内容 |
| --- | --- |
| 默认值 | 架构明确者（pan=64）优先；其余取中性初值：level/outputLevel=127、bendUp/Down=2、portamento.time=64、booster=64、cutoff=127、包络 level=127、包络 time=0、LFO rate=64、FXM color=1 |
| 选项命名 | 力度曲线 7 种命名（LINEAR/EXP1-3/LOG1-3）、Portamento Mode/Type/Start、Bias Direction、LFO Fade Mode 为占位命名 |
| Control Source | 列表 = CC01–CC95 + PITCH BEND + CH AFTERTOUCH（97 项）；默认 ctrlSource2=CC11、ctrlSource3=CC13 |
| Control Depth | 实现为 Int −63..+63（资料 3.2 标 0–127，架构 4.1 标 Choice/Float）；待规格确认 |
| ID 命名 | 包络采用 `tone1.tva.aEnv.time1` 形式（架构 4.2 支持），与 4.1 示例 `tone1.env.a.t1` 不同；ID 冻结前需确认 |
| 音域/力度范围 | velLow=1、velHigh=127、keyLow=0、keyHigh=127（资料值） |

## 6. 待补证据

- [x] 宿主参数自动化演示截图（人类已补：`REVIEWS/M0-03/2026-09-20 213614.png`，`level/pan/analogFeel` 自动化）
- [ ] macOS/Linux 构建与测试（M0-06 CI）
- [x] 变异抽查 ×2（`mutation-check.log`、`mutation-check2-test-sensitivity.log`）
- [x] 注册表由 `[.registry]` 隐藏用例生成（写入检出目录属证据生成例外；注册表只含选项数，不含完整选项名）

## 7. 日志清单（本目录）

| 文件 | 内容 |
| --- | --- |
| `configure.log` | 配置（`CONFIGURE_EXIT=0`） |
| `build.log` | 最终清空全量构建（198 行，`BUILD_EXIT=0`，0 error / 0 warning） |
| `build-attempt1-layout-api.log` | F2（ParameterLayout API） |
| `build-attempt2-const-pointer.log` | F3（const 指针） |
| `tests.log` | 测试全绿（9 用例 / 9436 断言） |
| `ctest.log` | ctest 1/1 通过 |
| `mutation-check.log` | 生产代码变异（pan 64→63，1 断言失败） |
| `mutation-check2-test-sensitivity.log` | 测试敏感性（移除 set 调用，2 断言失败） |
| `registry-run.log` | `[.registry]` 运行输出 |
| `parameter-registry.md` | 参数注册表（518 行，自动生成） |
| `smoke.log` | Standalone 冒烟 |
