# 渲染自省工作交接

本文是截至 2026-11-22 的交接记录。功能契约、调用方式和限制以
[渲染自省设计文档](debug-analysis-and-view-modes.md) 和
[诊断 Skill](../.ai/skills/SKILL_RENDER_DIAGNOSTICS.md) 为准；本文件中的未来方案不代表已经实现。

## 分支与迁移

- 工作分支为 `feature/render-introspection`，主仓库目标远端为
  `https://git.hyzgame.com/hyzboy/ULRE.git`。
- 在另一台机器上使用能够访问 `hyzgame` 的账号取得该分支并初始化子模块，例如：

  ```powershell
  git clone --branch feature/render-introspection --recurse-submodules https://git.hyzgame.com/hyzboy/ULRE.git
  ```

  若已有检出，先确认没有会被覆盖的本地修改，再获取该分支，并执行
  `git submodule update --init --recursive`。子模块使用 `.gitmodules` 中各自的地址，
  有的依赖 GitHub；需要分别具备网络访问权限。不要把当前机器的构建目录或 CMake 缓存搬到新机器使用。
- 用户要求一并保留 `example/Benchmark/01_ChessboardStress.cpp` 的修改：
  默认骰子数从 3500 改为 350。该文件的容量说明、`CSTRESS_SELFCHECK` 的旧像素摘要及
  `example/Benchmark/ChessboardStressValidate.cmake` 的 7145 行预期仍按 3500 骰子编写，
  因而相关自检不能视为已验证，可能失败。后续需确认期望场景后同步判据；
  不要未经验证直接更新摘要或把失败视作通过。
- 以下原有未跟踪内容**未包含在主仓库提交**，不会随主仓库推送迁移；如仍需要，
  须由所有者分别检查并安全迁移，勿直接把第三方源码、归档或编译产物加入主仓库：

  | 相对路径 | 当前性质 |
  | --- | --- |
  | `CMScript/src/DevilVM/MicrosoftCallX64.obj` | 未跟踪的对象文件 |
  | `CMScript/src/WasmVM/wasm-micro-runtime/` | 未跟踪的独立第三方 Git 检出，当前 HEAD `b70d708d46be750bfcf008218b42c7b98c49368a` |
  | `src/Tools/GLTFConvert/meshoptimizer/tools/objloader.cpp` | 嵌套子模块中的未跟踪源码 |
  | `src/Tools/TexConv/ISPCTextureCompressor/ISPC/ispc-trunk-windows.zip` | 嵌套子模块中的未跟踪归档 |

  这些路径和 HEAD 仅记录本机状态，不代表内容已备份、已审阅或可直接重新分发。
  临时抓帧、设备基线和会话目录下的探针/日志也不包含在本次提交中。

## 已完成工作

最初四阶段为 GPU 查询采样器（`e91ce2d7f`）、健康账本和帧调试器（`2c9742412`）、
Windows 命名管道桥（`13aaf5d3b`）、MCP 与诊断 Skill（`09a0ab6ce`）。
其后逐步完成：

1. 真实抓帧请求和状态、同帧 color/depth、版本化清单、设备和相机快照、
   失败/取消/释放及严格格式检查；旧 `dump_*` 只读取最近结果，不发起请求。
2. 受所有权、提交与同步约束的 opt-in 托管离屏抓取；有界 GPU buffer 读回。
   目前实际生产 SSBO 仅登记 `GPUCullSystem.DrawCount`，其序号是派发序号而非主帧 ID。
3. 不改作者材质的独立 Debug View，支持 `world_normal`、`shading_normal`、
   `base_color`、`surface`、`emissive`、`uv0` 和按需 RGBA32F `world_position`。
   有效几何由 alpha mask 标记；成对深度可与位置 RT 交叉验证。
4. metadata-first 亮度、法线、表面、深度与位置分析；逐通道帧差、SSIM、
   变化区域和独立 mask 覆盖差异。不可确认的上下文需要显式 opt-in，不等同基线判定。
5. `RenderDiagnosticsFixture` 固定场景及显式创建、更新、比较基线工具；
   不匹配或未覆盖返回 `NOT_COVERED`。增加法线、材质、相机、几何及 shadow caster
   负向测试；投射物覆盖不等同屏幕受影比例。
6. 支持范围内的 alpha-test、法线贴图/TBN、非白顶点/实例 alpha 和同桶多实例验证；
   dither 在无法保证与主帧覆盖一致时明确拒绝。

主要入口：`inc/hgl/vk/DebugCaptureQueue.h`、`src/Vulkan/DebugCaptureQueue.cpp`、
`src/Vulkan/DebugBridgeServer.cpp`、`src/ecs/support/debug/DebugViewPass.cpp`、
`tools/ulre-mcp-server/`、`example/Debug/RenderDiagnosticsFixture.cpp`。

## 现有边界和已确认决策

- DebugBridge 默认关闭，仅支持 Windows 本机命名管道；Debug View 需显式开启，
  进程级状态仅表示最近处理的世界，并非世界选择器。
- Debug View 当前在每帧复用离屏附件前等待设备空闲；适合查 BUG，不适合性能基准。
  支持 GPU-driven StandardOpaque 三角形及文档列出的 PBRSurface、相机、纹理和顶点格式；
  不支持的场景批次整体失败，不把部分图像当成完整结果。
- **保持主帧不变，暂不支持 dither**：同帧实测主/调试阈值不一致；仅使用深度 EQUAL
  不能区分同深度获胜片元；修改主 shader 的编译上下文也不能保证主帧不变。
  透明混合、自定义材质路径和 MSAA 不应视为已支持。具体反例与同步限制见设计文档。
- `world_normal` 是插值顶点世界法线，`shading_normal` 包含受支持的法线贴图/TBN。
  常规属性 RT 是量化并截断到 [0,1] 的 RGBA8，不是 HDR。
- shadow-map 非清屏比例只表示光源空间投射物覆盖；当前没有可用的同帧 receiver
  shadow factor/mask，屏幕受影比例为 unavailable。
- 普通离屏需由资源所有者 opt-in 并满足托管 pass/图形队列契约；相机、viewport
  不可证明时不伪造。普通离屏的提交序号、SSBO 派发序号和交换链帧号不能直接互认。
- 当前分析器限制单图 64 MiB、2,097,152 像素；连通区域分析上限 262,144 像素。
  普通比较尚未验证场景/设备身份；只有基线工具按其契约检查设备和场景。
  跨设备容差需要自行实测，不代表其他硬件已认证。

## 验证记录

以下是此前在 Windows、Intel Arc 140T、驱动 101.9175 上的结果，不代表新机器已通过：

- 定向 C++ 构建和 `TestFrameDebugger`、`TestDebugBridgeLifecycle`、
  `TestDebugViewLifecycle` 通过；Python 单元测试与完整 shader 编译/反射门禁通过。
  部分需要启动引擎或指定夹具的集成测试在普通单测运行中跳过。
- 启用 Vulkan validation 和 synchronization validation 的实机测试覆盖 Debug View、
  alpha-test、法线贴图、同桶实例、非白 alpha、Position/深度 mask、主帧不变、
  模式切换与关闭，未见 VUID/SYNC-HAZARD；dither 拒绝路径有测试。
- 固定场景同设备严格比较无像素变化；相机、法线、材质、几何覆盖和
  shadow caster 故障均被拒绝。Position RT/反投影在夹具中的最大误差约
  `7.19e-5` 世界单位，仅为该次实测，不是通用精度保证。
- DrawCount 真实 compute 写入抓取连续两次为 5；通用普通离屏示例此前出现
  `BuildResourceContract` 失败，另一次退出异常也曾在原基线复现，不能记为通过。
  非一致性内存硬件、跨设备容差及完整多世界 GPU 场景尚未认证。
- 曾在隔离场景比较关闭诊断和仅开启空闲桥的 API trace，未发现额外调试 RT、
  绘制或图像读回；不能据此推断所有场景、设备或未来修改均无开销。

## 新机器恢复与检查

需要 Visual Studio C++ 工具链、CMake、Python、Vulkan 运行环境及项目所需依赖。
本机曾使用 Visual Studio 18 2026、x64、vcpkg toolchain、`BUILD_TESTING=ON`
和 `ULRE_ENABLE_SHADER_PROGRAM_BUILD=ON`；新机器按实际安装调整生成器和路径，
不要使用旧 `CMakeCache.txt`。

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -DBUILD_TESTING=ON -DULRE_ENABLE_SHADER_PROGRAM_BUILD=ON
cmake --build build --config Debug --target BasicLitMeshes RenderDiagnosticsFixture TestFrameDebugger TestDebugBridgeLifecycle TestDebugViewLifecycle ShaderResourceSchemaRegressionGate
ctest --test-dir build -C Debug -R "^(TestFrameDebugger|TestDebugBridgeLifecycle|TestDebugViewLifecycle|shader_resource_schema_regression_gate)$" --output-on-failure
python -B -m unittest discover -s tools\ulre-mcp-server -p "test_*.py"
python -B tools\ulre-mcp-server\selftest.py
```

部分测试要求可用 Vulkan GPU、窗口会话和 validation layer；跳过、不可用或失败均不是通过。
完整 BasicLitMeshes/MCP 顺序及固定场景基线 `create`、`compare`、`update` 示例见设计文档和 Skill。
基线绑定实际设备/驱动和场景输入；新机器无匹配基线应返回 `NOT_COVERED`，只在用户明确操作时创建或更新。

## 新机器（AMD RX 6700 XT）迁移后验证记录

环境：AMD Radeon RX 6700 XT、驱动 32.0.21045.29003、Vulkan SDK 1.4.363.0、
Visual Studio 18.12.12412.261、MSVC 工具集 14.52.37025、CMake 4.4.4、
Windows SDK 10.0.26100.0、vcpkg `x64-windows`、静态运行库 `/MT`。

- 工具集小版本升级（14.51 → 14.52）会让既有 `build/CMakeCache.txt` 指向已被删除的
  `cl.exe`，必须 `cmake --fresh` 重新配置；否则配置阶段直接失败。
- 并行构建偶发 `MSB3073 / vcpkg z-applocal 退出码 32`，同名命令单独执行返回 0，
  重试或 `--parallel 4` 即可，与本分支改动无关。
- Debug 全量构建 0 错误；`ctest -C Debug -E "BidirectionalMapStressTest$"`
  **253/253 全部通过**。`BidirectionalMapStressTest` 在 Debug 下运行超过 15 分钟，
  属既有耗时问题，非回归。
- `GPUModelChessboardStress`（729 + 350 = 3995 立方体）通过，自校验
  `[CSTRESSSELF] PASS`；新设备金标准摘要为 `0x701368BFE0E60060`。像素摘要与
  批次/桶容量判据绑定具体 GPU 与驱动，换设备必须重新录制，不能沿用旧值。
- 修复 `VUID-07069`：`VKQueryProfiler` 只有在
  `GetMeshShaderFeatures().meshShaderQueries` 为真时才写入 task/mesh 统计查询位。
- `PhysicsScenarios` 的箱体堆叠场景曾依赖退化构型（等距贴面垒放、上层箱心正跨拼缝、
  且 100 枚落箱中最近的几枚正好砸在金字塔上），判定结果由浮点末位决定，会随编译器
  版本翻转。现改为：金字塔每块箱体带亚分米级错位使支撑面积不对称，落箱阵列整体移到
  +Y 侧、与金字塔互不干扰。判据未放宽，仍为「顶层箱在金字塔附近且 z > 1.0」和
  「100 枚落箱全部 z > 0」。
- 修复 `VUID-08963 / 06179 / 08914`（`CascadeShadowMap` 每帧刷屏，画面看似正常）：
  `VkPipelineRenderingCreateInfo`（颜色附件数与格式、深度格式）在
  `RenderPass::CreatePipeline*` 里**按 RenderPass 烘进管线**，`pipeline_memo` 也是
  per-RenderPass 的；而 `TextRenderPipeline` 的 `RenderResources::pipeline` 是
  **按字体缓存、只判空建一次**，结果在零颜色附件的 CSM 阴影 pass 里建出的文字管线
  被拿到 swapchain 前向 pass 上绑定（颜色附件数 0 vs 1、深度 `D32F` vs `D32F_S8U`、
  `pColorAttachmentFormats[0]` 为 `VK_FORMAT_UNDEFINED`）。
  修法：`RenderResources` 记录 `pipeline_render_pass`，绘制前与
  `world->GetRenderTarget()->GetRenderPass()` 判等，不等就重建；建管线另加
  `GetColorCount() > 0` 前置条件（文字 FS 静态写 `outColor`，零颜色 pass 里连创建都不合法），
  `TextRenderLayer::RenderLayer` 在零颜色 pass 直接早退。
  验证：同场景约 14 秒捕获，`VUID` / `pColorAttachments` / `colorAttachmentCount` /
  `pColorAttachment` 命中数全为 0（修复前 68 条），管线 5 Created / 0 Reuse，文字程序
  `program-18131365001569992837` 的管线改在前向
  `RenderPass_2_130_1000001002_3_1_64` 内创建。
- **通则**：`graph::Pipeline*` 只在其创建时的 `graph::RenderPass` 上有效；任何
  「按资源缓存管线」的代码都必须记录并比较来源 RenderPass。`LineRenderPipeline` /
  `SimpleLineRenderPipeline` 已同时改为按实际 RenderPass 延迟创建并重建，
  零颜色 pass 直接跳过线段绘制。
- 该示例在 Debug + validation 下帧率低，且**首帧之后几乎不再输出日志**：日志停在
  ~480 KB 不代表卡死。用 `cdb -p <pid> -c "~0k 12; qd"` 采样主线程栈，多次分别落在
  `ECSContext::Tick` / `ECSContext::Render` / `ECSContext::RenderGraph` 执行处，
  可确认仍在连续出帧。退出码与退出期泄漏见 `doc/backlog.md`。

## 后续工作意向

以下是分析建议，**尚未实现或作为本次迁移的执行计划**：

1. 补齐健康账本生产者。当前 `src`/`inc` 静态检索仅发现报告方法定义和桥接读取，
   空告警不能证明健康；需要事件来源、帧/对象身份、未覆盖状态和线程安全快照。
   账本 JSON 还需正确转义文本，当前纹理告警本身就包含引号。
2. 修正 profiler/桥接快照并暴露真实 pass 身份、统计项、可用性及丢样信息；
   当前 MCP 仅提供 pass 名称和时间，pass 名称粗分 swapchain/offscreen，
   每帧超过 32 个 pass 会被截断，采集与桥接读取的同步需审查。
3. 建立跨主帧、离屏、buffer、世界及相机的关联标识；提供统一能力发现、
   可重试/不可重试错误和完整证据包，便于 AI Agent 与离线分析器共享结果。
   对象/材质/批次/剔除链的只读归因可先做；逐像素获胜实体归因是更大的工作。
4. 探索将 **BUG 查询模式**与**性能检测模式**分开：前者可按配置逐帧采集并采用有界
   环形保留或显式全量保存；后者不抓图像/SSBO、不启用 Debug View，仍以低干扰、
   非阻塞方式采集少量时间与计数。结果应标注模式，避免交叉比较。
   全量抓取可能改变时序并掩盖竞态类问题，因此仍需低干扰诊断选项。
5. 按实际需求再考虑异步 readback、4K 分块分析、HDR、更多生产 SSBO 和真实 receiver
   shadow factor；当前不能把上述意向描述为已支持。
