# 过时历史文档归档目录 (doc/deprecated)

本目录存放引擎演进过程中已被**彻底废除、取代或与当前进度偏差过大**的历史设计/计划文档（最早一批是 ShaderGen / 材质方案，后续陆续并入其余失效计划）。
保留这些文档仅供翻阅历史背景与设计演进脉络，**严禁作为现行实现或未来设计的参考依据**。

---

## 现行权威架构设计文档（请阅读以下最新文档）

1. **`doc/shadergen-elimination-domain-fixed-architecture.md`**（2026-10 最新权威）
   - **彻底废除 ShaderGen 终极架构设计：基于 MaterialDomain 的固定着色器方案**
   - 确立 `MaterialDomain` 物理固定着色器，消灭 C++ AST 与拼接生成器，C++ 仅输出极简 `options.glsl`。
2. **`doc/ultra-thin-fixed-shader-architecture.md`**
   - 超薄极简着色器架构设计正文（Domain 契约、多态包含契约、消灭 `#if` 条件编译）。
3. **`doc/material-domain-contract-architecture.md`**
   - 表面领域契约层设计。
4. **`doc/shadergen-reduction-closure-report.md`**
   - ShaderGen 削减工程实测结项报告。
5. **`doc/ecs-layer-architecture-and-frame-flow.md`** / **`doc/world-addresses-and-camera-model-plan.md`**
   - ECS 分层与帧流权威口径、世界地址与相机模型权威口径（取代归档的 RenderTarget / 多 RT 旧设计）。
6. **`doc/archive/async-texture-upload-and-bindless.md`**
   - 纹理异步上传与 bindless 现行机制（取代归档的纹理资产 ID streaming 计划）。⚠ 该档本身已在 2026-10-28 按时间窗封存，机制未变、仍可参考。
7. **`doc/csm-mechanism.md`** / **`doc/visibility-authority-chain.md`**
   - 阴影（CSM）与可见性权威链路的现行设计（取代归档的 ShadowMask / 混合阴影草案）。

---

## 归档文档清单及废弃说明

| 文档名称 | 原主题 | 废弃原因说明 |
|---|---|---|
| `ShaderGen_MaterialGLSL_DesignAnalysis.md` | 早期材质 GLSL 生成器设计分析 | 基于旧版 5 层生成器模型与复杂 recipe 处理，现行架构已走向固定 Domain 着色器。 |
| `ShaderGen_ComposableTemplate_TechnicalDesign.md` | 受控组合模板技术方案 | 详述动态 AST 拼装与 FragmentTemplateComposer 拼接机制，该机制已整体定性为待废除。 |
| `simple-sphere-material-shadergen-overview.md` | SimpleSphere 到 ShaderGen 的工作链分析 | 详细追踪旧版 ShaderGen 生成流程，已过时。 |
| `material-lit-recipe-to-shadergen-dataflow.md` | Lit 材质数据流全链路分析 | 描述了通过生成器解析材质与生成专属着色器的历史链路，不符合「资产仅填参、不编译 SPV」的现行设计。 |
| `shadergen-generator-diagnostics-layering.md` | 真实生成器与诊断工具分层设计 | 试图对旧版 ShaderGen 做生成态与诊断态分层，现行方向为整体剔除生成器。 |
| `shadergen-refactoring-methodology.md` | 复杂度削减方法论 (2026-08) | 早期局部优化方法论，现行架构已升级为顶层固定 Domain 体系。 |
| `texture-asset-id-streaming-plan.md` | 纹理资产 ID 化与全异步加载（TextureStreaming，v2） | 基线分支 `TextureStreaming` 从未合入；六项待拍板 P-1…P-6 全未定案；`TextureAssetID` 全仓零命中；引用的 `CMCore/inc/hgl/thread/Workflow.h` 已不存在（现为 `thread/WorkerPool.h` + `concurrency/SpscQueue.h`）。纹理上传机制真源仍为 `doc/archive/async-texture-upload-and-bindless.md`（未归档）。 |
| `meshlet-geometry-pipeline-implementation.md` | Meshlet 几何管道与压缩顶点格式 | 档首自述 **⚠ 已作废（2026-10-05）**：用户拍板本轮不支持 meshlet，引擎侧 meshlet 路径整体删除，loader 改为显式拒绝。 |
| `mesh-shader-thread-model-and-loadvertexdata-conflicts.md` | Mesh Shader 迁移踩坑实录 | 关联代码 `src/ShaderGen/meshgen/` 整目录已删除；分支 `to_mesh_shader` / 提交 `ad547e843` 为一次性迁移记录，全仓无反向引用。 |
| `annotation-vs-include-tree-assessment.md` | 「用静态 include 树取代注记驱动模块表」立项评估 | 档首自述已完成历史使命；评估对象（旧 ShaderGen 注记驱动模块表）随生成器废除而消失，现行方向见 `doc/shadergen-elimination-domain-fixed-architecture.md`。 |
| `shadow-mask-and-hybrid-shadows-design.md` | 基于 ShadowMask 的多层次混合阴影架构 | 设计草案 / 预研规划；`ShadowMask` / `HybridShadow` 在 `inc/`+`src/`+`example/` 全部 0 命中；阴影路线已定为 CSM + 世界私有 `ShadowInfo`。 |
| `render-target-standardization-design.md` | RenderTarget 标准化设计 | 档首「**别再按本文去实现**」：核心字段 `RenderGraph::Pass::renderTarget` 已整体删除（A3 `34af98709`），多 RT 权柄改为 `ECSContext::RenderTo()` + RenderPassOptions。 |
| `terrain-rendering-implementation-plan.md` | GPU-Driven 地形渲染实现方案（MeshShader / BDA / LOD） | 状态「设计定稿，未执行」，T1–T5 全未开工；`Terrain` 在代码中仅剩 `MaterialDomain.h` 一个枚举值，`ShaderLibrary/` 与 `example/` 下无任何地形实现。 |
| `terrain-vulkan-standalone-experiment.md` | Vulkan 独立工程地形渲染实验方案 | 档首定位「为**独立 Vulkan 工程**（不依赖 ULRE）设计」，本仓从无该工程，不属于 ULRE 的进度面。 |

> 归档判据与逐档取证见 `doc/backlog.md` §「📦 文档归档记录（2026-10-19）」。
