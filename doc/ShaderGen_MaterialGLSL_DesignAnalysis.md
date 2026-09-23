# ShaderGen 材质 GLSL 生成器设计分析

> 本文分析 ULRE 引擎的材质 GLSL 生成器（`src/ShaderGen` 为主），从示例程序
> `example/Basic/PBRSpheres.cpp` 入口出发，沿调用链覆盖
> `inc/hgl/mtl`、`inc/hgl/graph/ssbo`、`src/ShaderGen`、`ShaderLibrary` 四个目录，
> 说明其分层架构、完整工作链与核心设计原理。
>
> 状态：**当前实现说明**（描述符退役后的 BDA 终态）
> 更新：2026-09-12；2026-09-24 按当前代码校正符号与 `path:line`
>
> 材质数据已收敛为 `GlobalSSBOBinding` + `GlobalSSBOBufferRegistry` 的
> 共享行与 BDA 地址表（`inc/hgl/graph/ssbo/GlobalSSBOTypes.h:64`、
> `inc/hgl/graph/module/GlobalSSBOBufferRegistry.h`）；旧的 Materialization、
> recipe 多 slot、provider `@ulre ssbo`、材质 set 2 与 per-material 描述符集
> 路径不再属于当前架构——描述符集只剩 Scene(0)/Bindless(1)
> （`inc/hgl/common/DescriptorSetTypeDef.h:42-50`）。

---

## 一、总体架构：五层职责分离

从 `PBRSpheres.cpp` 入口到最终 SPV，整条链可以切成五个清晰的分层，
每层只对相邻层暴露"契约"（struct 或可哈希的 Contract），不共享运行时句柄：

```
┌─ L1 作者层（声明意图）──────────────────────────────────────────┐
│  MaterialDefinition（能力超集，.material.toml 文件）            │
│  MaterialRecipe（实例输入：mtl_def_id + 纹理/SSBO 绑定 + 状态覆盖）│
├─ L2 构建请求层 ─────────────────────────────────────────────────┤
│  MaterialDefinitionBuildRequest = Recipe + 几何格式 + purpose + 设备 │
├─ L3 生成层（src/ShaderGen）─────────────────────────────────────┤
│  BuildGenericMaterial：契约推导 → MS 组装 → FS 组装 → BDA 行声明 │
│  （VertexABIBuilder / MeshTemplateEmitter /                     │
│    FragmentTemplateComposer / DescriptorContract / MaterialShaderCompiler）│
├─ L4 产物层 ─────────────────────────────────────────────────────┤
│  ShaderBuildContext{ShaderCreateInfoMap, ShaderResourceSchema,  │
│    ShaderLinkSpec, ShaderProgramArtifactMetadata} + SPV 字节    │
├─ L5 运行时层 ───────────────────────────────────────────────────┤
│  ShaderProgramManager（程序缓存）/ RenderPrimitiveCollectSystem  │
│  （recipe 物化）/ VKBindlessTextureManager（bindless 句柄池）    │
└─────────────────────────────────────────────────────────────────┘
```

关键设计点：**L1 不含任何 Vulkan 句柄**（`MaterialRecipe.h` 注释明确
"纯声明式材质输入"）。L5 由收集系统直接校验 effective recipe：材质数据通过
`GlobalSSBOBinding`（`GlobalSSBOType`、共享 `ssbo_id`、实例 `data_index`，
`inc/hgl/graph/ssbo/GlobalSSBOTypes.h:64`）映射到 `GlobalSSBOBufferRegistry`
的共享行，并按 draw item 写入 `MaterialInstanceAddresses` 行
（`payload_index` / `texture_reference_index`，`inc/hgl/graph/ShaderBufferSources.h`）；
纹理则继续通过 bindless 注册。运行时不再存在材质 binding table 中间层，
也没有 per-material 描述符集。

---

## 二、完整工作链（从示例入口逐步追踪）

### 第 1 步：示例作者装配材质（example/Basic/PBRSpheres.cpp）

`InitMaterialDataSSBO()` 为每个 PBR 材质实例从
`GlobalSSBOBufferRegistry` 获取 `GlobalSSBODataAccessor`
（`GetManager<GlobalSSBOBufferRegistry>()->GetAccessor<ssbo::PBRSurfaceRow>()`，
`example/Basic/PBRSpheres.cpp:277`/`:286`），
写入材质字段并取得 `GlobalSSBOBinding{ssbo_type, ssbo_id, data_index}`
（`GlobalSSBODataAccessor::GetGlobalSSBOBinding()`，
`inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:67`）。
创建实体时，示例把该 binding 写入 `MaterialDataAuthoringResource`，再通过
名称化的 `SetMaterialTextureResource()` 绑定 `base_color` 和 `normal`，
最后交给 `PrimitiveComponent` 的 recipe 合并与运行时收集流程。

### 第 2 步：ECS 收集系统触发编译（src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:520-590）

每帧收集时做**两级脏检查**：

- `recipe_hash`（recipe 内容 FNV）与 `build_context_hash`
  （`HashMaterialProgramBuildContext` = primitive_type + 几何格式 + 设备 profile，
  `src/ShaderGen/material_definition/MaterialDefinitionRegistry.cpp:113`）→ 任一变化即 `program_dirty`；
- 构造 `MaterialDefinitionBuildRequest`（把 `PrimitiveVariantPurpose` 转成
  `ShaderProgramPurpose`：DepthOnly/ShadowCaster 覆盖默认 ForwardColor）；
- 调 `ShaderProgramManager::AcquireShaderProgram(request)`。

### 第 3 步：程序缓存查询（src/SceneGraph/module/ShaderProgramManager.cpp:689）

`NormalizeRecipe`（幂等）把 recipe 规范化（解析 mtl_def_id → 文件注册表 →
回写 definition 的默认渲染状态），随后按 `ShaderProgramKey` 查
`shader_program_cache`——**命中即返回，完全不触碰 ShaderGen**。未命中才进入编译。

### 第 4 步：BuildGenericMaterial —— 生成器核心（src/ShaderGen/builder/GenericMaterialBuilder.cpp:702-806）

这是整条链的枢纽，按序执行：

1. **解析 definition**：`mtl_def_id="Lit"` → `ShaderLibrary/material/lit.material.toml`
   （文件注册表懒加载整目录，`src/ShaderGen/material_definition/MaterialDefinitionRegistry.cpp:184`）→ 得到
   `MaterialDefinition`：`[transform]` 五元组（source/mapping/orientation/scale/projection）、
   `[fragment]` provider 模块路径、`[pipeline]` family/profile 与可选 `[render_state]`、
   `[vertex]` 语义需求 + varyings、`[resources]` UBO/采样器声明
   （`src/ShaderGen/material_definition/MaterialDefinitionFile.cpp:343-1030` 逐字段解析）。
2. **目的派发**：`pass → ShaderProgramPurpose`；DepthOnly 会**裁剪**——把 fragment
   语义需求缩到 `IsVertexSemanticRequiredForVarying` 允许集、清掉 provider 根、
   剔除不需要的描述符（Phase 1 `ResolvePurposeAndCoverage`，src/ShaderGen/builder/GenericMaterialBuilder.cpp:117-238），
   这就是"深度 pass 不采样纹理"的来源。
3. **契约推导**（全部是无副作用的纯函数，产出可哈希结构）：
   - `MaterialCoverageContract`：alpha_test/dither/A2C 与所需 varyings/纹理槽
     （MaterialCoverageContract.cpp）；
   - `MaterialStageInterface`：varying 配置 → `InterStageSemanticContractEntry[]`
     （mesh stage 输出与 FS 输入的公共语言，MaterialStageInterface.cpp）；
   - `MaterialOutputContract`：purpose → 输出附件契约（location/类型）。
4. **顶点 ABI 解析**：`BuildResolvedMaterialVertexABI`（src/ShaderGen/builder/VertexABIBuilder.cpp:288）
   ——把 definition 的语义需求逐条与 `GeometryVertexFormat` 匹配
   （geometry.Find(semantic)），生成 `layout(location=N) in vec3 Position;` 声明 +
   `SerializedVertexEntry[]`（VkFormat 列表），同时 `GetShaderCodeModuleProviderGraphHash`
   算出 **provider 图哈希**（provider 选择快照，参与缓存 key；
   `src/ShaderGen/glsl_module/ShaderCodeModuleCapabilityResolver.cpp:28`）。
5. **资源清单**：`BuildShaderCodeResourceManifest`
   （src/ShaderGen/builder/DefinitionDescriptorBuilder.h:41，闭包实现在
   src/ShaderGen/builder/DescriptorBuilderCommon.h:158）
   ——从 definition 的 UBO 需求 + provider 根（material_source/ntb 模块）的
   `@ulre` 纹理引用声明聚合资源元数据，再 `BuildDescriptorsFromDefinition`
   （src/ShaderGen/builder/DefinitionDescriptorBuilder.h:20）生成 `SerializedDescriptorEntry[]`。
   材质 payload 类型只来自 `MaterialDefinition.material_private_data`
   （inc/hgl/mtl/MaterialRecipe.h:299），通过 BDA 行访问，不由 provider 元数据声明。
6. **网格着色器组装**：`EmitMeshTemplateDocument`（src/ShaderGen/meshgen/MeshTemplateEmitter.h）
   按 `vertex_node_config` 五元组把 vertex/ 下的 s1/s2/s3 模块拼成完整 mesh shader。
7. **片段着色器组装**：render preparation 先由
   `SceneRenderTemplateResolver` 选择 scene `RenderTemplateRequest`，再调用
   `AppendMaterialRenderTemplateRoots` 将 MaterialDefinition 的 material provider
   capabilities 追加到该模板。`FragmentTemplateComposer::Compose` 按模板 slots、
   coverage 和 output contract 直接写入 `ShaderDocument`，随后序列化 GLSL。
8. **编译**：`CompileMaterial`（src/ShaderGen/compile/MaterialShaderCompiler.cpp:459）——
   把完整 MS/FS GLSL 交给 `ShaderBuildContext`（`SetShaderResourceSchema` /
   `SetProgramLink` 承载全局资源契约，inc/hgl/mtl/ShaderBuildContext.h；
   材质 payload 的 buffer_reference 行声明由
   `MaterialShaderEmitter::BuildMaterialSSBODeclarations` 生成），
   `FinalizeShaderBuildContext`（MaterialShaderCompiler.cpp:27）先查 `ShaderArtifactStore`
   磁盘 SPV 缓存，
   未命中才 `CreateShaderDirect()` → GLSLCompiler 插件（动态库 `GLSLCompiler.dll`，
   C 函数指针接口，src/ShaderGen/compile/GLSLCompiler.cpp:46）编译 SPV，并回写缓存。

### 第 5 步：运行时落地

- `ExecuteRuntimeMaterialBuildPipeline`（src/SceneGraph/module/ShaderProgramManager.cpp:526）：
  `ShaderCreateInfoMap` → 各 stage SPV → `ShaderProgram`；
  `ShaderResourceSchema` 继续描述全局 Scene/Bindless 资源；材质 payload 不进入
  per-material descriptor set；
- 收集系统直接校验 `MaterialRecipe` 的唯一 `GlobalSSBOBinding` 和材质定义，
  从 `GlobalSSBOBufferRegistry` 验证 active `data_index`，并按 draw item 写入
  `MaterialInstanceAddresses` 行的 `payload_index`；不再构建或缓存材质
  binding table；
- bindless 侧 `VKBindlessTextureManager` 把
  `SetMaterialTextureResource` 注册的纹理写入全局 descriptor 池；
  `MaterialTextureReferencePool` 为每个 material definition 保存
  `uvec2(descriptor_index, array_layer)` 引用行，`MTL_TEX(i)` 通过
  `MaterialInstanceAddresses.texture_reference_index` 取得该行
  （`inc/hgl/graph/ShaderBufferSources.h`；旧字段名 `texture_reference_address`）。

---

## 三、核心设计原理

### 原理 1：三层"意图"分离（Definition / Recipe / Request）

- `MaterialDefinition` = **能力超集**（"这个材质能做什么"），纯静态声明，
  可被任意 recipe 复用（inc/hgl/mtl/MaterialRecipe.h:285）；
- `MaterialRecipe` = **本次渲染意图**（"这次要什么"），`mtl_def_id` 是唯一对接点
  （MaterialRecipe.h:499-509，mtl_def_id 在 :502）；
- `MaterialDefinitionBuildRequest` = **构建期上下文**（几何格式/设备/purpose，
  inc/hgl/mtl/MaterialDefinitionRegistry.h:41-56）。

`NormalizeRecipe`（src/ShaderGen/material_definition/MaterialDefinitionRegistry.cpp:244）是三层间的合流点：
把 definition 的默认状态解析进 recipe，且**解析结果写回 `render_state_overrides`
成为权威值**——之后所有下游（哈希、管线状态）只读 recipe，不再回看 definition。

### 原理 2：GLSL 代码模块自描述（@ulre 元数据）

ShaderLibrary 的每个 `.glsl` 头部有一段 `// @ulre begin/end` 注释块，把模块的
能力、依赖和纹理引用声明为结构化数据（ShaderCodeModuleFile.cpp）：

```glsl
// @ulre name pbr_surface_source
// @ulre kind Utility
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic UV0
// @ulre texture_reference base_color Fragment optional fallback
// @ulre uses material_source_interface
// @ulre uses bindless_textures
```

解析器产出 `ShaderCodeModuleDefinition`（名字、GLSL 源码指针、能力需求数组、
纹理引用、依赖和冲突）。provider 不再声明材质 payload SSBO；
材质 payload 类型只来自 `MaterialDefinition.material_private_data`，
BDA 行声明由编译器统一发射。`@ulre ssbo` 已删除并会被解析器拒绝为未知 directive。
注册表来源收敛为两条：显式注册（`Register`）+ **目录递归扫描**
（`LoadDirectory`，src/ShaderGen/glsl_module/ShaderCodeModuleRegistry.cpp:57-373，
`uses/conflicts` 两阶段名字→模块解析，收敛循环剔除悬空依赖）；
`ShaderCodeModuleID` 数字 ID 表已删除——模块身份即名字
（inc/hgl/mtl/ShaderCodeModule.h:14-17）。这是整个系统的"编译器前端"：
**GLSL 文件本身同时是源码与清单**，改一行 shader 代码 = 改缓存 key，
无手工同步。

### 原理 3：语义需求驱动的顶点模块选择（通用能力解析已退役）

> 状态标注：**已退役 + 现载体**。旧的通用能力解析（四类来源匹配、
> `CandidateFeasible` 前向可行性 + in_progress 环检测、
> `ShaderCodeModuleCapabilityResolver.cpp:510-606`）已删除——
> `src/ShaderGen/builder/VertexABIBuilder.cpp:165-166` 原文注明
> "无模块能力匹配（能力解析系统已删）…resolved 恒为 true"。
> 设计意图（语义需求驱动、不按模块名硬编码）保持不变，机制按现载体描述如下。

材质定义不直接说"用哪个法线模块"，而是声明
`[vertex].requirements = ["Position","UV0","Normal"]` +
`ntb_module = "ntb/ntb_tangent_vbo_normalmap.glsl"`。

现载体：`BuildResolvedMaterialVertexABI`（src/ShaderGen/builder/VertexABIBuilder.cpp:288）
遍历 `definition.vertex_semantic_requirements`，按语义直接选 `vertex/s1_*` 模块
（"格式=模块"：法线 `VK_FORMAT_R8G8_UNORM` → `vertex/s1_ntb_rg8.glsl`、
UV `VK_FORMAT_R16G16_SFLOAT` → `vertex/s1_uv_rg16f.glsl`、位置按
`out_position_format` 选 `s1_position_vec3/vec2/vec2i`），拼成顶点输入 GLSL
（VertexABIBuilder.cpp:172-285）。

`ShaderCodeModuleResolutionResult` 的 `selections` / `diagnostics`
（inc/hgl/mtl/ShaderCodeModuleCapabilityResolver.h:76）仍被
`ComposeShaderCodeModuleProviderGraph`（ShaderCodeModuleCapabilityResolver.cpp:85）
消费，`GetShaderCodeModuleProviderGraphHash`（同文件 :28）产出参与顶点 stage key 的
**provider 图哈希**——这就是"同一个材质换一个法线模块，缓存自动失效"的机制。

### 原理 4：契约化 = 可序列化、可哈希、可校验

所有推导结果都收敛为带 tag 的契约结构（CanonicalShaderContract.cpp）：现役只剩
`OutputContract`（OUT1，src/ShaderGen/contract/CanonicalShaderContract.cpp:12）——
`ResolvedModuleGraph`（RMG1）在当前代码树 0 命中（留待确认，见文末）；
`ShaderInterfaceContract`（SFI1）聚合体已退役（src/ShaderGen/contract/MaterialStageInterface.cpp:20：
"原经 ShaderInterfaceContract 聚合体校验的往返已删，规则逐条保留"）。
流程统一为 **Validate → CanonicalSort（与插入顺序无关）→ 序列化 →
FNV 哈希**。这带来两个关键收益：

1. **多级缓存 key 体系**（ShaderStageKey →
   ShaderLinkSpec → ShaderProgramKey，inc/hgl/mtl/ShaderProgramKey.h:12-19 **六维**哈希；
   原 `ShaderStageBuildContext.h` 已不在生产头中）：
   - vertex_stage_digest = 源码哈希 + provider 图哈希 + 顶点接口哈希 +
     资源契约哈希 + 编译器哈希；
   - fragment_stage_digest 同理；程序级 key 再叠加 vertex_input_hash /
     `resource_layout_hash`（旧字段名 `pipeline_state`）/ `render_target_hash` / `compiler_hash`。
   - **任何输入变化（几何格式、设备 profile、GLSL 库文件、材质 TOML、
     输出目标）都精确传导到程序 key**，命中 `ShaderProgramManager` 内存缓存
     或 `ShaderArtifactStore` 磁盘缓存（stage .spv 文件 + program 元数据），
     从根上避免无谓重编译。
2. **契约即文档**：校验已按域就地化（`ValidateShaderInterfaceContract` 聚合体已删）——
   inter-stage 条目由 `ValidateInterStageEntries`
   （src/ShaderGen/contract/MaterialStageInterface.cpp:43）、descriptor 条目由
   `ValidateDescriptorContract`（src/ShaderGen/contract/DescriptorContract.cpp:160）校验，
   覆盖拓扑序合法、descriptor 域合法、mesh stage 输出覆盖 FS 输入，
   在生成期而不是运行期暴露不一致。

### 原理 5：Fragment 模板 + 槽位占位符（生成器与手写模板的分工）

模板现为 `ShaderLibrary/fragment/*.glsl.tmpl`（`forward_lit` / `forward_unlit` / `sky`，
均含手写 `main()`）——原 `main_forward_surface.frag.glsl` + `SURFACE_FUNCTION_FILE`
宏 + `// ULRE_*_CONTRACT` 标记 + `ReplaceLightingModuleIncludes` 的替换路径已退役。
模板保留 `{{...}}` 占位符（`forward_lit.glsl.tmpl` 共 6 个：`defines` /
`module_includes` / `fragment_inputs` / `output_declarations` / `code_modules` /
`surface_input_wiring`），由 `FragmentTemplateComposer::ApplyFragmentTemplateSlot`
（src/ShaderGen/template/FragmentTemplateComposer.cpp:128）在 C++ 侧以字符串替换；
槽位 `#include` 文本由 `BuildSlotIncludeText`（同文件 :323）按
`RenderTemplateDefinition.slots` 顺序生成。GLSL 不允许 `#include MACRO`，因此模块
路径替换必须在 C++ 侧完成——生成器不生成算法，只做**选择与装配**，
算法全部留在可读的 .glsl 里。

### 原理 6：Bindless 纹理 + 统一 Sampler 注册表

- 纹理走 `set=1（BINDLESS_SET）binding=0 texture2DArray bindless_tex[]`
  （**2D/2DArray 统一为数组**；`set=3` 为旧集号，Bindless 2→1 已收敛——真源
  inc/hgl/common/DescriptorSetTypeDef.h:42-50，GLSL 声明见
  ShaderLibrary/common/bindless_textures.glsl:36-38），sampler 独立池 `binding=1`，
  另有 `binding=2 textureCubeArray bindless_cube_array[]`；
  `Sample2DArray(handle, idx, uv, layer)` 用 `nonuniformEXT` 索引，
  handle=0 返回 vec4(0) 表示无纹理。
- sampler 以 `ShaderLibrary/sampler.toml` 为**单一数据源**（出现顺序即索引）：
  ShaderGen 生成 `#define TrilinearSampler 2u` 宏（`BuildSamplerMacros`，
  src/ShaderGen/compile/MaterialShaderEmitter.cpp:112），运行时按序 `vkCreateSampler`
  ——GLSL 侧索引与运行时池
  天然对齐。
- `MaterialTextureReferencePool` 按 material definition 分配纹理引用行；
  ShaderGen 依据 `.material.toml` 的 `resources.textures` 声明顺序发射
  `uvec2 tex_<texture_name>` 字段，运行时只写 descriptor index 和 array layer。
- provider GLSL 只通过 `MTL_TEX(i).tex_<texture_name>` 取得引用，再调用
  `Sample2DArray(handle, sampler, uv, array_layer)`；不再依赖固定 `TextureSlot`、独立 texture-layer SSBO
  或 provider SSBO 元数据。

### 原理 7：顶点三段式管线（s1/s2/s3）+ 配置五元组

`[transform]` 的五个字段直接映射 `VertexShaderNodeConfig`，驱动 vertex/ 目录的
模块选择：

| 维度 | 模块 | 示例 |
|---|---|---|
| source（输入） | s1 | 按语义选 `ShaderLibrary/vertex/s1_*.glsl`：`s1_position_vec3/vec2/vec2i`（位置，按 `out_position_format` 选）、`s1_uv`/`s1_uv_rg16f`、`s1_ntb`/`s1_ntb_rg8`/`s1_ntb_rg16f`/`s1_ntb_a2bgr10`、`s1_color`、`s1_luminance`、`s1_palette_index`、`s1_transform_id`、`s1_size`、`s1_text_char_quad`（旧文的 `s1_input_procedural` 与 `layout(location=…) in vec3 Position;` 属 VS 时代，mesh stage 无顶点属性输入） |
| mapping（局部位置） | s2 | `s2_lift_xy0`（XY 平面）/`s2_lift_x0y`（地面）/`s2_passthrough3d`（3D） |
| orientation/scale/projection | s3 | `s3_world_camera_vp`（标准 3D）/`s3_ortho_viewport`（2D UI）/`s3_camera_facing_world`（billboard）/`s3_camera_facing_fixed_pixels`（固定像素 billboard，NDC 偏移按 `2/viewport_resolution` 缩放） |

vertex/helpers/ 提供 `GetL2W()`（含 `HGL_L2W_FROM_VERTEX_ATTR` 的 TransformID
属性分支）与固定像素缩放。**同一套 material 机制因此覆盖 3D 网格、2D UI、
Text、Sky、billboard**——差异只在五元组与模块路径。

---

## 四、设计评价要点（供后续改进参考）

**优点：**

- 声明式三层解耦干净；模块自描述让"加一个新材质/新法线算法"= 加一个 .toml +
  几个 .glsl，无需改 C++；
- 多级契约哈希把缓存做到输入精确；
- 生成器与算法分离（GLSL 可读性高，算法全在库文件里）。

**值得注意：**

- `BuildGenericMaterial` 现为约 105 行的单函数
  （src/ShaderGen/builder/GenericMaterialBuilder.cpp:702-806；原
  MaterialDefinitionRegistry.cpp:218-603 已拆出 5 个 Phase 辅助函数 +
  `VertexABIBuilder` / `DefinitionDescriptorBuilder` / `MaterialShaderEmitter`），
  仍承担目的裁剪/契约推导/资源清单/组装/哈希全流程的编排，是继续拆分的候选点；
- 模块身份已单轨化：`ShaderCodeModuleID` 数字 ID 枚举与运行时自增 ID 均已删除，
  身份即名字，stable ID = FNV1a(name)（inc/hgl/mtl/ShaderCodeModule.h:14-17；
  实现 src/ShaderGen/glsl_module/ShaderCodeResourceManifest.cpp:20）——这是缓存
  正确性的关键。原文提到的 `GetCanonicalShaderCodeModuleContentHash` 在当前代码树
  0 命中（留待确认，见文末）；
- shader 库路径靠运行时可执行文件向上搜索 `ShaderLibrary/` 目录
  （inc/hgl/mtl/ShaderLibraryPath.h:33 `GetShaderLibraryPath`，可用
  `ULRE_SHADERLIBRARY_PATH` 覆盖），部署时需保证目录可达。

---

## 附录：关键文件索引

| 层 | 文件 | 职责 |
|---|---|---|
| L1 | `inc/hgl/mtl/MaterialRecipe.h` | Definition/Recipe 数据结构、渲染状态解析 |
| L1 | `inc/hgl/mtl/MaterialDefinitionFile.h` + `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp` | .material.toml 解析 |
| L1 | `inc/hgl/mtl/MaterialDefinitionRegistry.h` + `src/ShaderGen/material_definition/MaterialDefinitionRegistry.cpp` | 材质定义文件注册表 + NormalizeRecipe / 构建上下文哈希 |
| L1 | `src/ShaderGen/builder/GenericMaterialBuilder.cpp` | BuildGenericMaterial 五阶段主流程（702-806） |
| L2 | `inc/hgl/mtl/ShaderBuildContext.h` | 编译产物容器（全局资源契约/Stage Map/链接规格） |
| L3 | `src/ShaderGen/glsl_module/ShaderCodeModuleFile.cpp` | @ulre 元数据解析 |
| L3 | `src/ShaderGen/glsl_module/ShaderCodeModuleRegistry.cpp` | 模块注册表（显式注册+目录扫描，无数字 ID 轨道） |
| L3 | `src/ShaderGen/glsl_module/ShaderCodeModuleCapabilityResolver.cpp` | provider 图哈希与组合（通用能力解析已退役） |
| L3 | `src/ShaderGen/builder/VertexABIBuilder.cpp` | 顶点 ABI + s1_* 模块选择 / provider 图哈希 |
| L3 | `src/ShaderGen/builder/DefinitionDescriptorBuilder.h`（+ `DescriptorBuilderCommon.h`） | 模块依赖闭包 / 资源清单 / 描述符条目 |
| L3 | `src/ShaderGen/template/FragmentTemplateComposer.cpp` | FS 模板装配（模板 slots/契约/main） |
| L3 | `src/ShaderGen/meshgen/MeshTemplateEmitter.h` | Mesh shader 三段式组装 |
| L3 | `src/ShaderGen/compile/MaterialShaderCompiler.cpp` | 最终编译流水线（CompileMaterial）+ 全局资源契约装配 |
| L3 | `src/ShaderGen/compile/MaterialShaderEmitter.cpp` | GLSL 发射（MTL_ROW / MTL_TEX / BDA 行声明） |
| L3 | `inc/hgl/graph/module/MaterialTextureReferencePool.h` + `src/SceneGraph/module/MaterialTextureReferencePool.cpp` | 按 material definition 管理纹理引用行 |
| L3 | `src/ShaderGen/compile/GLSLCompiler.cpp` | GLSLCompiler 插件加载与 SPV 编译 |
| L4 | `src/ShaderGen/compile/ShaderArtifactStore.cpp` | SPV 磁盘缓存（stage/program 两级） |
| L4 | `inc/hgl/mtl/ShaderProgramKey.h` | 程序级缓存 key（六维哈希） |
| L5 | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` | 收集/脏检查/触发编译；recipe 校验、共享材质行物化与 MaterialInstanceAddresses 行写入 |
| L5 | `src/SceneGraph/module/ShaderProgramManager.cpp` | 程序缓存 + 运行时落地 |
| 库 | `ShaderLibrary/`（common/compositor/fragment/lighting/material/mesh/ntb/sky/surface/ubo/vertex，另有 ao/shadow） | GLSL 代码模块库 |
| 库 | `ShaderLibrary/sampler.toml` | Sampler 预设注册表（单一数据源） |

---

## 附：未能核实、留待确认（2026-09-24）

以下符号在 `inc/` `src/` `example/` `ShaderLibrary/` `res/` 中 0 命中，且未找到
确定替代名，按「不猜测」原则**未改动**：

| 位置 | 符号 | 核实结果 |
|---|---|---|
| 原理 4 | `ResolvedModuleGraph`（RMG1） | 全仓 0 命中；现役契约 tag 只剩 `OutputContract`（OUT1）。替代载体未确认 |
| 原理 4 / 四 | `GetCanonicalShaderCodeModuleContentHash` | 全仓 0 命中；现役模块身份为 FNV1a(name)，是否有等价的「模块内容哈希」入口待确认 |
