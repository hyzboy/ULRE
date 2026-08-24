# ShaderGen 材质 GLSL 生成器设计分析

> 本文分析 ULRE 引擎的材质 GLSL 生成器（`src/ShaderGen` 为主），从示例程序
> `example/Basic/BasicLitMeshes.cpp` 入口出发，沿调用链覆盖
> `inc/hgl/mtl`、`inc/hgl/shadergen`、`src/ShaderGen`、`ShaderLibrary` 四个目录，
> 说明其分层架构、完整工作链与核心设计原理。
>
> 状态：**分析文档（只读调研，未修改任何代码）**
> 日期：2026-08-16
> 相关文档：`doc/material-recipe-and-materialization-spec.md`（材质契约宪法，
> 本文聚焦生成器实现原理，二者互补）

---

## 一、总体架构：五层职责分离

从 `BasicLitMeshes.cpp` 入口到最终 SPV，整条链可以切成五个清晰的分层，
每层只对相邻层暴露"契约"（struct 或可哈希的 Contract），不共享运行时句柄：

```
┌─ L1 作者层（声明意图）──────────────────────────────────────────┐
│  MaterialDefinition（能力超集，.material.toml 文件）            │
│  MaterialRecipe（实例输入：mtl_def_id + 纹理/SSBO 绑定 + 状态覆盖）│
├─ L2 构建请求层 ─────────────────────────────────────────────────┤
│  MaterialDefinitionBuildRequest = Recipe + 几何格式 + purpose + 设备 │
├─ L3 生成层（src/ShaderGen）─────────────────────────────────────┤
│  BuildGenericMaterial：契约推导 → MS 组装 → FS 组装 → 描述符分配  │
│  （ResolvedModuleGraphBuilder / MeshShaderAssembler /              │
│    CompositorAssembler / DescriptorContract / MaterialShaderCompiler）│
├─ L4 产物层 ─────────────────────────────────────────────────────┤
│  ShaderBuildContext{ShaderCreateInfoMap, ShaderResourceSchema,  │
│    DescriptorSetLayoutAllocator, ShaderLinkSpec} + SPV 字节      │
├─ L5 运行时层 ───────────────────────────────────────────────────┤
│  ShaderProgramManager（程序缓存）/ RenderDescriptorBindingSystem │
│  （描述符绑定）/ VKBindlessTextureManager（bindless 句柄池）      │
└─────────────────────────────────────────────────────────────────┘
```

关键设计点：**L1 不含任何 Vulkan 句柄**（`MaterialRecipe.h` 注释明确
"纯声明式材质输入"），句柄在 L5 通过 `ResolvedBindingTable`
（`inc/hgl/mtl/MaterialBindingContract.h`）把 recipe 资产映射到 GPU 资源——
生成器与运行时通过"语义"（`DescriptorSemantic`/`TextureSlot`/`SSBOType`）
而非指针解耦。

---

## 二、完整工作链（从示例入口逐步追踪）

### 第 1 步：示例作者装配材质（example/Basic/BasicLitMeshes.cpp）

`InitMISSBO()`（BasicLitMeshes.cpp:118-140）先向 `ResourceDomainManager`
申请一块 PBR 材质 SSBO 数组，拿到运行时身份 `SSBOBinding{ssbo_type=PBRSurface,
ssbo_id}`；`InitMaterial()`（:82-116）再构造 recipe 并把 SSBO 绑定挂进去：

```cpp
mesh_recipe.recipe_name = "06b.BasicLit.Lit";
mesh_recipe.mtl_def_id  = "Lit";                       // ← 唯一对接材质定义的键
mesh_recipe.domain      = "06b.BasicLit";              // 缓存域
mesh_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
UpsertRecipeSSBOAssetBinding(mesh_recipe, "mtl",       // data_slot "mtl"
                             mtl_data_ssbo_accessor->GetSSBOBinding());
```

纹理/Sampler 走 `SetMaterialTextureResource(TextureSlot::BaseColor/Normal/Roughness,
...)`（bindless 注册），最终 `PrimitiveAsset(geometry, &mesh_recipe, Triangles)`
把 recipe 挂到网格资产上，交给 ECS 的 `PrimitiveComponent`。

### 第 2 步：ECS 收集系统触发编译（src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:520-590）

每帧收集时做**两级脏检查**：

- `recipe_hash`（recipe 内容 FNV）与 `build_context_hash`
  （`HashMaterialProgramBuildContext` = primitive_type + 几何格式 + 设备 profile，
  MaterialDefinitionRegistry.cpp:723）→ 任一变化即 `program_dirty`；
- 构造 `MaterialDefinitionBuildRequest`（把 `PrimitiveVariantPurpose` 转成
  `ShaderProgramPurpose`：DepthOnly/ShadowCaster 覆盖默认 ForwardColor）；
- 调 `ShaderProgramManager::AcquireShaderProgram(request)`。

### 第 3 步：程序缓存查询（src/SceneGraph/module/ShaderProgramManager.cpp:432）

`NormalizeRecipe`（幂等）把 recipe 规范化（解析 mtl_def_id → 文件注册表 →
回写 definition 的默认渲染状态），随后按 `ShaderProgramKey` 查
`shader_program_cache`——**命中即返回，完全不触碰 ShaderGen**。未命中才进入编译。

### 第 4 步：BuildGenericMaterial —— 生成器核心（src/ShaderGen/MaterialDefinitionRegistry.cpp:218-603）

这是整条链的枢纽，按序执行：

1. **解析 definition**：`mtl_def_id="Lit"` → `ShaderLibrary/material/lit.material.toml`
   （文件注册表懒加载整目录，MaterialDefinitionRegistry.cpp:921-945）→ 得到
   `MaterialDefinition`：`[transform]` 五元组（source/mapping/orientation/scale/projection）、
   `[fragment]` 四个模块路径、`[compositor]` 表面/混合/pass、
   `[vertex]` 语义需求 + varyings、`[resources]` UBO/采样器声明
   （MaterialDefinitionFile.cpp:429-600 逐字段解析）。
2. **目的派发**：`pass → ShaderProgramPurpose`；DepthOnly 会**裁剪**——把 fragment
   语义需求缩到 `IsVertexSemanticRequiredForVarying` 允许集、清掉 provider 根、
   剔除不需要的描述符（MaterialDefinitionRegistry.cpp:249-422），
   这就是"深度 pass 不采样纹理"的来源。
3. **契约推导**（全部是无副作用的纯函数，产出可哈希结构）：
   - `MaterialCoverageContract`：alpha_test/dither/A2C 与所需 varyings/纹理槽
     （MaterialCoverageContract.cpp）；
   - `MaterialStageInterface`：varying 配置 → `InterStageSemanticContractEntry[]`
     （mesh stage 输出与 FS 输入的公共语言，MaterialStageInterface.cpp）；
   - `MaterialOutputContract`：purpose → 输出附件契约（location/类型）。
4. **顶点 ABI 解析**：`BuildResolvedMaterialVertexABI`（MaterialDefinitionRegistry.cpp:818）
   ——把 definition 的语义需求逐条与 `GeometryVertexFormat` 匹配
   （geometry.Find(semantic)），生成 `layout(location=N) in vec3 Position;` 声明 +
   `SerializedVertexEntry[]`（VkFormat 列表），同时 CapabilityResolver 算出
   **provider 图哈希**（顶点语义提供模块的选择快照，参与缓存 key）。
5. **资源清单**：`Build3DShaderResourceManifest`（src/ShaderGen/3d/DefinitionDescriptorBuilder3D.h）
   ——从 definition 的 ubo 需求 + provider 根（material_source/ntb 模块）的
   `@ulre` 资源声明聚合出 UBO/SSBO/纹理层需求，再 `Build3DDescriptorsFromDefinition`
   生成 `SerializedDescriptorEntry[]`。
6. **网格着色器组装**：`GenerateMeshShader`（src/ShaderGen/common/MeshShaderAssembler.h）
   按 `vertex_node_config` 五元组把 vertex/ 下的 s1/s2/s3 模块拼成完整 mesh shader。
7. **片段着色器组装**：`CompositorAssembler::Assemble`（CompositorAssembler.cpp:318）：
   - 按 surface/pass 查模板路径（Lit→`compositor/main_forward_surface.frag.glsl`，
     深度 pass→`main_depth_only.frag.glsl`，Sky→`main_forward_sky.frag.glsl`）；
   - 在 `#version` 后注入 permutation 宏（`HGL_USE_SCENE_LIGHTING`/
     `HGL_USE_NTB_PROVIDER`/`HGL_ALPHA_TEST`…，InjectDefines 会先找出 `#version`
     行再插入，CompositorAssembler.cpp:105-155）；
   - 把 7 个可配置 `#include "..."` 替换为 definition 指定的模块路径
     （ReplaceLightingModuleIncludes）；
   - `#include SURFACE_FUNCTION_FILE` → `#include "surface/material_surface.glsl"`；
   - 在 `// ULRE_FRAGMENT_INPUT_CONTRACT`/`ULRE_OUTPUT_CONTRACT`/
     `ULRE_SURFACE_INPUT_CONTRACT` 三处标记**注入由契约生成的声明**
     （varying in / 输出 location / SurfaceInput 构造）。
8. **编译**：`CompileCompositorMaterial`（MaterialShaderCompiler.cpp:289）——
   把完整 MS/FS GLSL 交给 `ShaderBuildContext`（AddStruct/AddUBO/AddSSBO 填描述符
   分配器，data_slot 声明逐槽注入 SSBO 结构与 buffer 声明），
   `FinalizeShaderBuildContext` 先查 `ShaderArtifactStore` 磁盘 SPV 缓存，
   未命中才 `CreateShaderDirect()` → GLSLCompiler 插件（动态库 `GLSLCompiler.dll`，
   C 函数指针接口，GLSLCompiler.cpp:46-62）编译 SPV，并回写缓存。

### 第 5 步：运行时落地

- `ExecuteRuntimeMaterialBuildPipeline`（ShaderProgramManager.cpp:307）：
  `ShaderCreateInfoMap` → 各 stage SPV → `ShaderProgram`；
  `ShaderResourceSchema` → `MaterialDescriptorManager`（描述符池）；
- 收集系统再调 `BuildBindingTable`（src/ShaderGen/BindingTableBuilder.cpp /
  MaterialBindingContract.cpp）：把 recipe 的纹理/SSBO 绑定与 schema 逐条对照，
  产出 `ResolvedBindingTable`（每项带 `BindingSource::Asset/DirectValue/Missing`），
  `IsRuntimeReady()` 全绿才允许渲染；
- `RenderDescriptorBindingSystem` 按 `DescriptorSemantic` 把域 SSBO（材质数据、
  纹理层表）绑到描述符；bindless 侧 `VKBindlessTextureManager` 把
  `SetMaterialTextureResource` 注册的纹理灌进 `texture2DArray bindless_tex[]`，
  句柄写回 `mtl_texture_layer_rows` SSBO——GLSL 侧
  `Sample2D(handle, TrilinearSampler, uv)` 直接按句柄采样。

---

## 三、核心设计原理

### 原理 1：三层"意图"分离（Definition / Recipe / Request）

- `MaterialDefinition` = **能力超集**（"这个材质能做什么"），纯静态声明，
  可被任意 recipe 复用（inc/hgl/mtl/MaterialRecipe.h:211-276）；
- `MaterialRecipe` = **本次渲染意图**（"这次要什么"），`mtl_def_id` 是唯一对接点
  （MaterialRecipe.h:305-317）；
- `MaterialDefinitionBuildRequest` = **构建期上下文**（几何格式/设备/purpose，
  inc/hgl/mtl/MaterialDefinitionRegistry.h:41-56）。

`NormalizeRecipe`（MaterialDefinitionRegistry.cpp:972）是三层间的合流点：
把 definition 的默认状态解析进 recipe，且**解析结果写回 `render_state_overrides`
成为权威值**——之后所有下游（哈希、管线状态）只读 recipe，不再回看 definition。

### 原理 2：GLSL 代码模块自描述（@ulre 元数据）

ShaderLibrary 的每个 `.glsl` 头部有一段 `// @ulre begin/end` 注释块，把模块的
能力声明为结构化数据（GLSLCodeModuleFile.cpp:436-728）：

```glsl
// @ulre name pbr_surface_source
// @ulre kind Utility
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic UV0
// @ulre ssbo mtl PBRSurface 0 Fragment optional fallback
// @ulre texture_layer base_color Fragment optional fallback
// @ulre uses material_source_interface
// @ulre uses bindless_textures
```

解析器产出 `GLSLCodeModuleDefinition`（名字、GLSL 源码指针、需求数组、依赖、
条件、冲突）。注册表三来源：显式注册 + 内置表（`GLSLCodeModuleID` 枚举）+
**目录递归扫描**（GLSLCodeModuleRegistry.cpp:68-374，`uses/conflicts` 两阶段
名字→ID 解析，收敛循环剔除悬空依赖）。这是整个系统的"编译器前端"：
**GLSL 文件本身同时是源码与清单**，改一行 shader 代码 = 改缓存 key，
无手工同步。

### 原理 3：语义需求驱动的 Provider 解析（CapabilityResolver）

材质定义不直接说"用哪个法线模块"，而是声明
`[vertex].requirements = ["Position","UV0","Normal"]` +
`ntb_module = "ntb/ntb_tangent_vbo_normalmap.glsl"`。
CapabilityResolver 把需求按四类来源处理（GLSLCodeModuleCapabilityResolver.cpp:510-606）：

- `GeometryAttribute` → 与几何格式能力表匹配（语义 + 数值类掩码 + 分量数区间，
  `GetNumericClassFromVkFormat` 把 VkFormat 归类为
  Float/Normalized/Signed/Unsigned/Packed）；
- `Resource`/`Option` → 集合包含性检查；
- `ProducedSemantic` → 递归选 provider：按 priority 降序，`CandidateFeasible`
  前向可行性 + in_progress 环检测。

选中的 provider 集合会被拼装成**顶点 provider GLSL**
（`ComposeGLSLCodeModuleProviderGraph`）并参与顶点 stage key——
这就是"同一个材质换一个法线模块，缓存自动失效"的机制。

### 原理 4：契约化 = 可序列化、可哈希、可校验

所有推导结果都收敛为带 tag 的契约结构（CanonicalShaderContract.cpp）：
`ResolvedModuleGraph`（RMG1）/`ShaderInterfaceContract`（SFI1）/`OutputContract`
（OUT1），流程统一为 **Validate → CanonicalSort（与插入顺序无关）→ 序列化 →
FNV 哈希**。这带来两个关键收益：

1. **多级缓存 key 体系**（ShaderStageBuildContext → ShaderStageKey →
   ShaderLinkSpec → ShaderProgramKey，ShaderProgramKey.h:15-25 八维哈希）：
   - vertex_stage_digest = 源码哈希 + provider 图哈希 + 顶点接口哈希 +
     资源契约哈希 + 编译器哈希；
   - fragment_stage_digest 同理；程序级 key 再叠加 vertex_input_hash /
     pipeline_state / render_target / compiler_hash。
   - **任何输入变化（几何格式、设备 profile、GLSL 库文件、材质 TOML、
     输出目标）都精确传导到程序 key**，命中 `ShaderProgramManager` 内存缓存
     或 `ShaderArtifactStore` 磁盘缓存（stage .spv 文件 + program 元数据），
     从根上避免无谓重编译。
2. **契约即文档**：`ValidateShaderInterfaceContract` 等校验（拓扑序合法、
   descriptor 域合法、mesh stage 输出覆盖 FS 输入，ShaderStageBuildContext.h:142）
   在生成期而不是运行期暴露不一致。

### 原理 5：Compositor 模板 + 标记注入（生成器与手写模板的分工）

`main_forward_surface.frag.glsl` 是**可读的手写模板**（含 `main()`），只在 4 个
位置留了机器可替换的标记：`#version` 后插宏、`SURFACE_FUNCTION_FILE` 宏替换
surface 模块、三处 `// ULRE_*_CONTRACT` 注释插入声明。GLSL 不允许
`#include MACRO`，所以模块路径替换（ReplaceLightingModuleIncludes）必须在
C++ 侧以字符串完成——生成器不生成算法，只做**选择与装配**，
算法全部留在可读的 .glsl 里。

### 原理 6：Bindless 纹理 + 统一 Sampler 注册表

- 纹理走 `set=3 binding=0 texture2DArray bindless_tex[]`
  （**2D/2DArray 统一为数组**，最近提交的合并），sampler 独立池 `binding=1`；
  `Sample2D(handle, idx, uv)` 用 `nonuniformEXT` 索引，handle=0 返回 vec4(0)
  表示无纹理（material/pbr_surface_source.glsl:34-46 因此可以"可选纹理"）。
- sampler 以 `ShaderLibrary/sampler.toml` 为**单一数据源**（出现顺序即索引）：
  ShaderGen 生成 `#define TrilinearSampler 2u` 宏（MaterialShaderCompiler.cpp:134-149
  BuildSamplerMacros），运行时按序 `vkCreateSampler`——GLSL 侧索引与运行时池
  天然对齐。
- `mtl_texture_layer_rows` 这个"每材质一行、每槽一列"的 SSBO 由 C++ **动态生成**
  （MaterialShaderCompiler.cpp 按 `TextureSlot::RANGE_SIZE` 展开 struct），
  行索引 = `fragDataIndexID`，渲染侧按行写入 bindless 句柄。

### 原理 7：顶点三段式管线（s1/s2/s3）+ 配置五元组

`[transform]` 的五个字段直接映射 `VertexShaderNodeConfig`，驱动 vertex/ 目录的
模块选择：

| 维度 | 模块 | 示例 |
|---|---|---|
| source（输入） | s1 | `s1_input_procedural`（无属性，gl_VertexID 派生）或直接生成 `layout(location=0) in vec3 Position;` |
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

- `BuildGenericMaterial` 是 ~390 行的单函数（MaterialDefinitionRegistry.cpp:218-603），
  承担了目的裁剪/契约推导/资源清单/组装/哈希全流程，是后续拆分的候选点；
- `GLSLCodeModuleID` 仍是手写枚举（TestProviderA/PBRSurface）与目录扫描并存的
  双轨制，文件模块的 ID 是运行时自增，稳定性依赖名字哈希
  （stable ID = FNV1a(name)）——这是缓存正确性的关键，值得关注
  `GetCanonicalGLSLCodeModuleContentHash` 的字段覆盖完整性；
- shader 库路径靠运行时可执行文件向上搜索 `ShaderLibrary/` 目录
  （ShaderLibraryPath.h:27-76），部署时需保证目录可达。

---

## 附录：关键文件索引

| 层 | 文件 | 职责 |
|---|---|---|
| L1 | `inc/hgl/mtl/MaterialRecipe.h` | Definition/Recipe 数据结构、渲染状态解析 |
| L1 | `inc/hgl/mtl/MaterialDefinitionFile.h` + `src/ShaderGen/common/MaterialDefinitionFile.cpp` | .material.toml 解析 |
| L1 | `inc/hgl/mtl/MaterialDefinitionRegistry.h` + `src/ShaderGen/MaterialDefinitionRegistry.cpp` | 定义注册表 + BuildGenericMaterial 主流程 |
| L2 | `inc/hgl/shadergen/ShaderBuildContext.h` | 编译产物容器（描述符分配器/Stage Map/链接规格） |
| L3 | `src/ShaderGen/common/GLSLCodeModuleFile.cpp` | @ulre 元数据解析 |
| L3 | `src/ShaderGen/common/GLSLCodeModuleRegistry.cpp` | 模块注册表（内置+目录扫描） |
| L3 | `src/ShaderGen/common/GLSLCodeModuleCapabilityResolver.cpp` | 语义需求 → provider 解析 |
| L3 | `src/ShaderGen/ResolvedModuleGraphBuilder.cpp` | 模块依赖图（闭包/拓扑/聚合/哈希） |
| L3 | `src/ShaderGen/CompositorAssembler.cpp` | FS 模板装配（宏注入/模块替换/契约标记） |
| L3 | `src/ShaderGen/common/MeshShaderAssembler.h` | Mesh shader 三段式组装 |
| L3 | `src/ShaderGen/MaterialShaderCompiler.cpp` | 最终编译 + 描述符/SSBO 声明生成 |
| L3 | `src/ShaderGen/GLSLCompiler.cpp` | GLSLCompiler 插件加载与 SPV 编译 |
| L4 | `src/ShaderGen/ShaderArtifactStore.cpp` | SPV 磁盘缓存（stage/program 两级） |
| L4 | `inc/hgl/shadergen/ShaderProgramKey.h` | 程序级缓存 key（八维哈希） |
| L5 | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` | 收集/脏检查/触发编译 |
| L5 | `src/SceneGraph/module/ShaderProgramManager.cpp` | 程序缓存 + 运行时落地 |
| L5 | `src/ecs/systems/render/RenderDescriptorBindingSystem.cpp` | 描述符绑定（SSBO/纹理层表） |
| 库 | `ShaderLibrary/`（common/compositor/lighting/material/ntb/sky/surface/ubo/vertex） | GLSL 代码模块库 |
| 库 | `ShaderLibrary/sampler.toml` | Sampler 预设注册表（单一数据源） |
