# myJV 项目规则（AI 代理必读）

1. 技术基线：`Docs/myJV_开发架构文档.md`（v1.4）。实现必须与之一致；歧义时停止并提问。
2. 编码规范：C++20；RT 路径无分配/锁/引用计数/IO/异常；所有映射常量进 `Source/Params/Calibration.h`。
3. 禁止模式：见架构文档 3.5 与开发计划附录 C；RT 路径代码标注 `// RT-safe`。
4. 受保护区域：`AssetReclaimer` / `ToneVoice` RT 路径 / `VoiceManager` / `Calibration.h` / 序列化 / `CMakeLists.txt` / CI / 依赖清单；修改必须在人类批准的任务卡中进行（审查级别 R2）。
5. 构建（Windows 本机）：配置 `D:\Program\CMake\bin\cmake.exe -B build -G "Visual Studio 18 2026" -A x64`；构建 `D:\Program\CMake\bin\cmake.exe --build build --config Release`。工具链基线 MSVC 2022+（本机为 VS 18 2026 BuildTools 18.10 / MSVC 14.51，见 ADR-004）。
6. 提交：`feat/fix/test/docs(<任务ID>): <摘要>`；不 amend / 不强推 / 不合并；不修改测试以通过。
7. 每个会话只做一个任务卡；结束前更新 `PROGRESS.md`。
8. 工作流：任务卡 → 实现 + 测试 + 证据 → R0 门禁 → 人类审查（R1–R4）；无证据不算完成。
