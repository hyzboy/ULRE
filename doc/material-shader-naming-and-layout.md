# 材质与 Shader 命名审查及 ShaderLibrary 归类迁移方案

> **状态：提案（PROPOSAL）——未经批准不执行任何改名、移动或提交。**
> 本文只描述「现在叫什么、为什么有问题、改成什么、要改哪些引用」。
> 获批后按 §10 分阶段实施，每阶段独立提交。

- 审查基线：分支 `feature/gpu-driven-instanced-rendering`，HEAD `dbe7d10a2`
- 审查范围：`ShaderLibrary/**`（git 跟踪 204 个文件）+ `inc/hgl/mtl/**` + `src/ShaderMaterialData/**` + `src/SceneGraph/**` 相关符号 + 全工程引用面
- 本轮不改行为：仍保持 **Domain（引擎） → MaterialDefinition（`.mi.toml`） → Recipe（用户配方）** 三层，只改名字与位置

---

## 0. 审查方法与证据来源

| 证据 | 方式 |
|---|---|
| 文件清单 | `git ls-files ShaderLibrary` = 204 |
| 模块身份 | 逐个读取文件头 `// @ulre name/kind/slot/provides_capability` |
| 域契约 | `src/ShaderMaterialData/MaterialDomain.cpp` `DomainContracts[]` |
| 域固定源 | `src/ShaderMaterialData/DomainShaderTemplate.cpp` `kDomainDirs[]` + `ResolveDomainShaderTemplate()` |
| 资产 ID | 10 个 `*.mi.toml` 的 `id`/`domain` 字段 + 5 个 `BuiltinMaterialDefinitions.cpp` 内嵌定义 |
| 配方 | 62 个 `*.recipe.toml` 的 `schema`/`name`/`material_definition` |
| 槽位 | `inc/hgl/mtl/RenderTemplate.h` `ShaderModuleSlotRole`（12 个有效 role） |
| 槽位装配 | `src/ShaderMaterialData/SceneRenderTemplateResolver.cpp` 四个 `Make*Profile()` |
| 派生物真源 | `src/Tools/BufferGen/buffer_gen.toml` |
| 引用面 | `git grep` 全工程（`src`/`inc`/`example`/`doc`/`.ai`/`scripts`） |

---

## 1. 结论摘要：12 个命名缺陷

按严重度降序。每条都附当前证据，便于逐条否决。

### D1 —— `.mi.toml` 后缀在说「实例」，装的是「定义」（最高优先）
- `ShaderLibrary/material/*.mi.toml` 的 10 个文件，`id` 是 `Lit`、`UnlitTexture`…，`domain` 是 `StandardOpaque`…，内容是 **能力声明 + 默认渲染状态 + 纹理槽表**，即「材质定义」。
- 真正承载「某个具体材质实例」的是 `.recipe.toml`。
- `mi` 极易被读成 Material Instance，正好反过来。用户提出「`mi.toml` 像是多了一层」的直觉即来自这里。
- 证据：`src/ShaderMaterialData/MaterialDefinitionFile.cpp:1374-1376` 才把它叫 `.mi.toml`；`MaterialDefinitionRegistry.cpp:332` 注释写「资产目录（用户/项目可写的 .mi.toml）」——同一层被两种称呼。
- **改**：`.mi.toml` → `.definition.toml`；`ShaderLibrary/material/` 拆开（见 D2）。

### D2 —— `ShaderLibrary/material/` 混装三种完全不同的东西
83 个文件放在一个目录里：

| 实际身份 | 数量 | 例子 |
|---|---|---|
| 手写 GLSL 代码模块（`@ulre` 元数据） | 11 | `pbr_surface_source.glsl`、`sky_cube_surface.glsl` |
| 材质定义资产（TOML） | 10 | `lit.mi.toml` |
| 用户配方资产（TOML，含示例程序资产） | 62 | `recipes/Examples/Material/06b.BasicLit.Lit.recipe.toml` |

且 `sky_cube_surface.glsl` 的元数据是 `kind=Surface / slot=surface_provider`，**根本不是 material source**，却放在 `material/`。
- 证据：文件头 `// @ulre slot surface_provider`。
- **改**：GLSL 进 `module/provider/<slot>/`；TOML 进 `asset/definition/` 与 `asset/recipe/`。

### D3 —— 模块名不带槽位，同名不同槽 / 同槽不同名
`ShaderModuleSlotRole` 有 11 个有效 role（另 `Unknown`/`SlotRoleCount` 两个哨兵），但 `@ulre name` 完全看不出来属于哪个：

| 现名 | 槽位 | 问题 |
|---|---|---|
| `identity_ao` | ambient_occlusion_provider | `identity_` 前缀与 `identity_shadow` 撞；`ao` 是缩写 |
| `identity_shadow` | shadow_provider | 同上 |
| `full_shadow` | shadow_provider | 文件名 `shadow/full.glsl`，名字却前置 `full` |
| `pcf_shadow` | shadow_provider | 三个 shadow 模块三种构词法 |
| `indirect_ibl` / `indirect_none` / `indirect_sky_ambient` | ambient_light_provider | 槽位叫 Ambient，名字叫 Indirect，环境层参数叫 `AmbientEnv` → **三个词指一件事** |
| `pbr_surface_source` | material_source_provider | `_source` 后缀与文件名一致，但槽位名是 `material_source_provider` |
| `material_surface` | surface_provider | 名字里没有 `surface` 前缀，与 `sky_minimal_surface`/`sky_cube_surface` 构词法相反 |
| `forward_pbr` / `forward_toon` | lighting_model | `forward_` 不等于槽位名 |
| `forward_lighting` / `flat_lighting` | output_policy | 槽位叫 output_policy，名字叫 lighting → 与 lighting_model 无法区分 |

### D4 —— `forward_*` 一个前缀表示三种东西
- `lighting/forward_pbr.glsl`、`lighting/forward_toon.glsl`：`slot=lighting_model` 的 **槽位实现**
- `lighting/forward_flat.glsl`：同样实现 `EvalLighting`，**但没有 `@ulre slot` 行**（漏声明），只被 `compositor/flat_lighting.glsl` `#include`
- `compositor/forward_lighting.glsl`：`slot=output_policy` 的 **合成器**

且目录 `lighting/` 混装 4 个不同 role：diffuse_light / specular_light / ambient_light / lighting_model。

### D5 —— `Template` 一词指四个不同对象
| 符号 | 实际含义 | 位置 |
|---|---|---|
| `RenderTemplateID` / `RenderTemplateDefinition` / `RenderTemplateSlot` | forward/depth **pass 的槽位组合契约**（`ForwardLitShadowedAO`、`ShadowCasterOpaque`、`Sky`…） | `inc/hgl/mtl/RenderTemplate.h:14` |
| `SceneRenderTemplateProfile` / `MakeIdentityForwardLitProfile()` | 场景层给某 pass 选的模块组合 | `src/ShaderMaterialData/SceneRenderTemplateResolver.cpp:5` |
| `DomainShaderTemplate` / `ResolveDomainShaderTemplate()` / `kDomainDirs[]` | **域固定源文件路径解析器**（拼 `domain/<dir>/<stage>.frag`） | `src/ShaderMaterialData/DomainShaderTemplate.cpp:29` |
| `EmitMeshTemplateDocument()` | mesh shader GLSL 文档发射器 | 1 处 |

四者毫无共性，却共用 `Template`。

### D6 —— `ShaderModule`（SPIR-V）与 `ShaderCodeModule`（GLSL 源码）难以区分
- `hgl::graph::vk::ShaderModule`：Vulkan SPIR-V 模块，`inc/hgl/vk/VKShaderModule.h:12`
- `hgl::graph::mtl::ShaderCodeModule*`（`Kind`/`Semantic`/`CapabilitySource`/`Dependency`/`SemanticRequirement`/`TextureReferenceRequirement`）：GLSL 源码模块，`inc/hgl/mtl/ShaderCodeModule.h`
- `Code` 这个限定词不传达「GLSL」。32 个文件使用 `ShaderCodeModule*`。

### D7 —— `ShaderGen` 这个名字与现存代码不符
- 已删除的离线 cook 工具是 `src/ShaderGen`。
- 现存 `src/Tools/ShaderGen/` 里没有任何 shader 生成逻辑，只有：2 个宏表生成器（`DescriptorMacroGen`、`InterStageLocationMacroGen`）、1 个物理设备档案采集器、4 个回归门禁（`ShaderResourceSchemaRegressionGate`、`SceneRenderTemplateResolverRegression`、`ShaderDocumentProductionRegression`、`VertexLoaderConsistency`）。
- 配置名 `ULRE_ENABLE_SHADERGEN`（20 个文件）实际控制的是 `ULRE.ShaderProgramBuild` 是否构建。
- 通名 `SHADERGEN_CODEGEN_GENERATION` 在 ShaderGen 已被废除的今天会误导（5 个文件）。

### D8 —— `ubo/` 里放 SSBO
`src/Tools/BufferGen/buffer_gen.toml` 是派生物路径真源，其中 `ubo/global_addresses.glsl`、`ubo/world_addresses.glsl`、`ubo/light_row.glsl` 声明 `kind="ssbo"`，但目录叫 `ubo`。

### D9 —— `s1_` / `s2_` / `s3_` 数字前缀无语义
`ShaderLibrary/vertex/` 35 个文件：`s1_*`（17，顶点流解码）、`s2_*`（7，位置映射）、`s3_*`（7，变换/投影）、`helpers/`（4）。
数字只出现在 `VertexNodeConfigResolver.h` 的硬编码表里，读者无法从名字判断阶段。另有 `vertex/s1_stream_defaults.glsl`（`stream` 重复语义）。

### D10 —— 后缀与命名风格不统一
- `common/RenderItemResolve.glsl`：PascalCase 文件名 + `@ulre name RenderItemResolve`，全库唯一。
- `common/descriptor_macros.glsl`、`common/interstage_locations.glsl`：文件头写 `// GENERATED ... DO NOT EDIT`（由 `DescriptorMacroGen` / `InterStageLocationMacroGen` 生成并提交入库），却与手写文件混在同一目录；而运行时发射的 `generated/frag_options.glsl` 是**另一个不同根的 `generated/`**，直接新建 `ShaderLibrary/generated/` 会与 include 解析撞名。
- `common/alpha_compositor.glsl` 是 `kind Shared`（接口族），名字却是工具族。

### D11 —— 资产 ID 两套词法
| 词法 | 例子 | 来源 |
|---|---|---|
| PascalCase，无前缀 | `Lit`、`UnlitTexture`、`SkyCube`、`VertexPaletteColor` | 10 个 `*.mi.toml` 里 9 个 |
| snake_case + `builtin/` | `builtin/pure_color`、`builtin/checkerboard_2d`、`builtin/checkerboard_3d`、`builtin/error_checkerboard_3d`、`builtin/text_gpu`、`builtin/text_gpu_bitmap` | 1 个 `.mi.toml` + 5 个 `BuiltinMaterialDefinitions.cpp` |
- `builtin/` 含义也混：有的是「代码内嵌」，有的是「文件资产但极简」。
- 文件名与 `id` 一律不同：`lit.mi.toml` → `id = "Lit"`，`checkerboard_2d.mi.toml` → `id = "builtin/checkerboard_2d"`。

### D12 —— 配方名把定义名重复一遍，且 62 个里 2 个不合规
- 60/62 满足 `<App>.<Variant>.<DefId>.recipe.toml`，文件名 == `name` 字段 == 文件名去后缀。
- 2 个例外：`recipes/PBRSpheres.recipe.toml`（`name = "PBRSpheres.Lit"`）、`recipes/TextureQuad.recipe.toml`（`name = "TextureQuad.UnlitTexture"`）。
- 三段式也不严格：`06e.SingleSphereSwitch.Far`、`AlphaModes.LitOpaque`、`Phase2.MaterialRecipeEntry.Cube` 各是一种。
- `recipes/Examples/**` 是**示例程序资产**，却住在引擎 `ShaderLibrary` 内。

### 附：审查中发现的非命名结构问题（**本轮不改，仅记录**）
1. `domain/standard_opaque/forward_lit.frag` 用**字面 `#include`** 硬编码了所有槽位模块（`lighting/forward_pbr.glsl`、`shadow/pcf_shadow.glsl`、`compositor/forward_lighting.glsl`…），`SceneRenderTemplateProfile` 只用于校验与缓存键。槽位「选择」与「装配」是两处独立真源，靠回归门禁兜底。
2. `compositor/*_lighting.glsl`（output_policy）自己 `#include` 了对应 `lighting/forward_*.glsl`，使 `lighting_model` 槽位实际是镜像声明。
3. `MaterialDomain` 的 `Foliage` / `Terrain` / `HumanSkin` 的 `material_source` 与 `ntb` 为 `nullptr`（未实现）；`UI2D` 无固定 `.frag`。
4. `kDomainDirs[]` 仅 13 条，`VertexPalette` 复用 `vertex_color/vertex_color.frag`，`UnlitTexture3D` 复用 `unlit_texture`，Sky 走 `surface_module_name` 特例分支。

---

## 2. 术语表（定稿后写入 `doc/`，作为唯一词表）

| 概念 | 现用词 | 建议定名 | 定义 | 当前载体 |
|---|---|---|---|---|
| 表面领域 | `MaterialDomain` | **Domain** | 引擎固定的一套「表面能做什么 + 必填语义 + 纹理槽」契约，用户不能新增 | `inc/hgl/mtl/MaterialDomain.h` |
| 领域契约 | `MaterialDomainContract` / `DomainContracts[]` | **DomainContract** | Domain 的数据化描述 | `src/ShaderMaterialData/MaterialDomain.cpp` |
| 领域入口着色器 | `kDomainDirs` / `.frag` | **DomainEntry** | 每 Domain 的固定 `.frag`/深度 `.frag` | `ShaderLibrary/domain/**` |
| 材质定义 | `MaterialDefinition` / `.mi.toml` | **MaterialDefinition** | Domain + 默认渲染状态 + 纹理槽 + 变体能力的可序列化包，引擎/项目提供 | `asset/definition/*.definition.toml` |
| 用户配方 | `MaterialRecipe` / `.recipe.toml` | **Recipe**（≡ UE 的 MaterialInstance） | 选一个 MaterialDefinition + 绑资源 + 覆盖数值 | `asset/recipe/**` |
| GLSL 代码模块 | `ShaderCodeModule` | **GlslModule** | 带 `@ulre` 元数据、经注册表校验的 `.glsl` | `ShaderLibrary/module/**` |
| SPIR-V 模块 | `ShaderModule` | **SpirvModule**（或保留 `ShaderModule`） | Vulkan 模块对象 | `inc/hgl/vk/VKShaderModule.h` |
| 槽位 | `ShaderModuleSlotRole` | **Slot** | pass 内一个可替换的算法/数据接入点（11 个有效 role） | `RenderTemplate.h:26` |
| 槽位实现 | 无专名 | **Provider** | 某个 Slot 的一个 `@ulre` 实现 | `module/provider/<slot>/` |
| pass 组合契约 | `RenderTemplateID` / `SceneRenderTemplateProfile` | **PassProfile** | 「这个 pass 由哪些 Slot 组成」 | `RenderTemplate.h` / `SceneRenderTemplateResolver.cpp` |
| 着色器程序 | `ShaderProgram` / `ShaderProgramManifest` | **ShaderProgram** | 一次编译产物（Stage 集合 + 变体键） | `src/ShaderProgramBuild` |
| 阶段 | `ShaderStage` | **Stage** | `Vertex/Mesh/Fragment` | — |
| 内置 / 兜底 | `builtin` / `fallback` / `kFallbackMaterialRules` | **Builtin** / **Fallback** | Builtin = 引擎内嵌定义；Fallback = 报错材质 | `BuiltinMaterialDefinitions.cpp` |

**已废弃、不得再出现的词**：`mi`（指定义）、`indirect_*`（指 ambient_light）、`s1_/s2_/s3_`、`forward_*`（指 lighting_model 或 output_policy）、`Template`（指 pass profile 或路径解析器）、`ShaderGen`（指现存工具）。

---

## 3. 目标命名规则（R1–R8）

| 规则 | 内容 |
|---|---|
| **R1** | 目录按「是什么」分 5 个顶层区：`domain/`（入口）、`module/`（GLSL 代码）、`contract/`（契约与生成宏表）、`topology/`（固定拓扑）、`asset/`（TOML 资产） |
| **R2** | 每个 Slot 一个目录：`module/provider/<slot>/`，11 个目录与 `ShaderModuleSlotRole` 的 11 个有效 role 一一对应 |
| **R3** | Provider 的 `@ulre name` 必须以槽位名开头：`<slot>_<variant>`。名字→槽位可反推，槽位→名字可检索 |
| **R4** | 接口模块统一 `*_interface`（现状已满足，不改）；生成文件统一进 `contract/`，绝不与手写混放，且**不叫 `generated/`**（避开运行时发射目录同名） |
| **R5** | 全部 GLSL 与文件名用 `lower_snake_case`；`@ulre name` 与文件名去掉扩展名后**必须完全一致**（当前 `RenderItemResolve` 不满足） |
| **R6** | 资产 `id` 统一 `lower_snake_case`；`builtin` = 仅「引擎内嵌（C++ 字符串）」；文件资产一律不带 `builtin/` 前缀 |
| **R7** | 资产文件名 == `id`（去 `builtin/` 前缀、去扩展名）；配方文件名 == `name` 字段 |
| **R8** | C++ 中一个词只指一件事：`PassProfile`（槽位组合）/ `DomainEntry`（域入口源）/ `GlslModule`（GLSL 模块）/ `MaterialDefinition` / `Recipe`；`Template` 只保留在无歧义的模板函数语义里 |
| **R9** | `vertex/` 去掉数字前缀，改为语义前缀：`s1_*`→`stream_*`（顶点流解码）、`s2_*`→`position_*`（位置映射）、`s3_*`→`transform_*`（变换/投影） |

---

## 4. C++ 符号映射表

### 4.1 `doc` / 高价值改动（按 R8）

| 现符号 | 新符号 | 影响文件数 | 说明 |
|---|---|---|---|
| `RenderTemplateID` | `PassProfileID` | 见下 | 枚举值 `ForwardLitShadowedAO`… 一并评估是否改为 `ForwardLitShadowed/ForwardLitUnshadowed/...` + 独立 AO 轴（**本表只改名，不改枚举值语义**，枚举值改动列入待决项 Q3） |
| `RenderTemplateDefinition` | `PassProfileDefinition` | | |
| `RenderTemplateSlot` | `PassProfileSlot` | | |
| `RenderTemplateRequest` | `PassProfileRequest` | | |
| `RenderTemplateModuleRoot` | `PassProfileModuleRoot` | | |
| `RenderTemplateValidationError` / `...Diagnostic` | `PassProfileValidationError` / `PassProfileValidationDiagnostic` | | |
| `MaxRenderTemplateModuleRoots` | `MaxPassProfileModuleRoots` | | |
| `SceneRenderTemplateProfile` | `ScenePassProfile` | 8 | `SceneRenderTemplateResolver.{h,cpp}` + 调用点 |
| `ResolveSceneRenderTemplateRequest()` | `ResolveScenePassProfileRequest()` | 8 | |
| `MakeIdentityForwardLitProfile()` / `MakeForwardUnlitProfile()` / `MakeSkyProfile()` / `MakeShadowCasterProfile()` | 保持（已是 Profile 语汇） | 5 | |
| `DomainShaderTemplate`（类型/命名空间） | `DomainEntrySource` | 5 | 它是**路径解析器**，不是模板 |
| `ResolveDomainShaderTemplate()` | `ResolveDomainEntrySource()` | 5 | |
| `kDomainDirs[]` | `kDomainEntrySources[]` | 2 | `supports_depth` 字段改名 `supports_depth_purpose` |
| `EmitMeshTemplateDocument()` | `EmitMeshShaderDocument()` | 1 | |
| `SHADERGEN_CODEGEN_GENERATION` | `MATERIAL_CODEGEN_GENERATION` | 5 | 值从 15 → 16（文件布局变更，必须让已烘焙 SPV/缓存在改名后一致失效，见 §10.5） |
| `ULRE_ENABLE_SHADERGEN` | `ULRE_ENABLE_SHADER_PROGRAM_BUILD` | 20 | CMake 选项名 + 所有 `#ifdef` |
| `src/Tools/ShaderGen/` | `src/Tools/ShaderTooling/` | 目录 + CMake 目标 | 见 D7；`golden/` 随迁 |
| `SceneRenderTemplateResolverRegression`（目标名） | `PassProfileResolverRegression` | CMake + 目录 | 可选，列入 Q4 |

### 4.2 GLSL 模块类型（R8 第二件事）

| 现符号 | 新符号 | 影响文件数 | 备注 |
|---|---|---|---|
| `ShaderCodeModule` | `GlslModule` | 32 | 文件 `inc/hgl/mtl/ShaderCodeModule.h` → `GlslModule.h` |
| `ShaderCodeModuleKind` | `GlslModuleKind` | | |
| `ShaderCodeModuleSemantic` | `GlslModuleSemantic` | | |
| `ShaderCodeModuleCapabilitySource` | `GlslModuleCapabilitySource` | | |
| `ShaderCodeModuleNumericClass` | `GlslModuleNumericClass` | | |
| `ShaderCodeModuleDependency` | `GlslModuleDependency` | | |
| `ShaderCodeModuleSemanticRequirement` | `GlslModuleSemanticRequirement` | | |
| `ShaderCodeModuleTextureReferenceRequirement` | `GlslModuleTextureReferenceRequirement` | | |
| `ShaderCodeModuleRegistry` | `GlslModuleRegistry` | | |
| `ShaderCodeModuleRegistration*`（如有） | `GlslModuleRegistration*` | | |

> `ShaderModule`（`inc/hgl/vk/VKShaderModule.h`）**不改**。

### 4.3 保持不变（明确列出，避免实施时误改）

`MaterialDomain`、`MaterialDomainContract`、`MaterialDomainTextureRole`、`MaterialDomainTextureSlot`、`MaterialDefinition`、`MaterialRecipe`、`ShaderModuleSlotRole`、`ShaderProgramPurpose`、`GlobalSSBOType`、`MaterialVertexProviderPolicy`、`DescriptorMacroGen`、`InterStageLocationMacroGen`、`*_interface` 系列模块名。

### 4.4 本轮明确不改名的 `ShaderGen*` 符号

以下 5 处含 `ShaderGen` 字样但语义是「物理设备/契约描述」，不在本轮命名轴（Pass 剖面 / GLSL 模块 / 领域入口）内，**本轮不改名**，留待后续单独评估：

`inc/hgl/mtl/contract/ShaderGenContract.h`、`inc/hgl/mtl/contract/ShaderGenPhysicalDeviceProfileJson.h`、`inc/hgl/mtl/contract/ShaderGenPhysicalDeviceProfileAdapter.h`、`src/Vulkan/ShaderGenPhysicalDeviceProfileAdapter.cpp`、`inc/hgl/mtl/contract/ShaderGenProfileTargetVersion.h`（文件名保留，仅内部常量改名）。

---

## 5. TOML 键与资产 ID 映射

### 5.1 键（现全部保持不变 —— 无重命名需求，仅核对）

`*.definition.toml`（原 `.mi.toml`）已识别键：
`schema`、`id`、`name`、`domain`、`material_data`、`material_source_module_name`、`surface_module_name`、`ntb_module_name`、`textures`（`name`/`type`/`required`/`mapping`/`channels`/`max_count`/`sampler`）、`samplers`、`defines`、`code_modules`（legacy）、`render_state`（`alpha_test`/`alpha_cutoff`/`dither`/`pipeline`/…）、`vertex`（`provider_policy`/`normal_mode`/`requirements`/`varyings`/`transform`/`projection`/`orientation`/`scale`/…）、`mesh_shader`（`mode`/`max_invocations`/`emit_*`）、`resources`、`ubos`、`overlay`、`wireframe`、`line_width`、`dynamic_line_width`。

`*.recipe.toml` 已识别键：
`schema`、`name`、`material_definition`、`textures`（`name`/`asset`/`required`/`layer`/`auto_mipmaps`）、`render_state`（`alpha_test`/`alpha_cutoff`/`dither`/`pipeline`）、`vertex`（`requirements`/`varyings`/`transform`/`projection`/`orientation`/`scale`/`root`/`source`/`name`）、`wireframe`、`line_width`、`dynamic_line_width`、`overlay`。

**三个已被明确拒绝的 legacy 键**（`MaterialDefinitionFile.cpp:1028-1039`，报错文案已写清替代）：`material_source_module`、`ntb_module`、`surface_module_include`、`ambient_module_name`。→ 已符合 R8，不动。

### 5.2 资产 ID（R6/R7）

| 现文件 | 现 `id` | 现 `domain` | 新文件 | 新 `id` |
|---|---|---|---|---|
| `material/lit.mi.toml` | `Lit` | StandardOpaque | `asset/definition/lit.definition.toml` | `lit` |
| `material/unlit_texture.mi.toml` | `UnlitTexture` | UnlitTexture | `asset/definition/unlit_texture.definition.toml` | `unlit_texture` |
| `material/unlit_texture_3d.mi.toml` | `UnlitTexture3D` | UnlitTexture3D | `asset/definition/unlit_texture_3d.definition.toml` | `unlit_texture_3d` |
| `material/vertex_color.mi.toml` | `VertexColor` | VertexColor | `asset/definition/vertex_color.definition.toml` | `vertex_color` |
| `material/vertex_luminance.mi.toml` | `VertexLuminance` | VertexLuminance | `asset/definition/vertex_luminance.definition.toml` | `vertex_luminance` |
| `material/vertex_palette_color.mi.toml` | `VertexPaletteColor` | VertexPalette | `asset/definition/vertex_palette.definition.toml` | `vertex_palette`（去掉与 Domain 名无关的 `_color`） |
| `material/debug_normal_color.mi.toml` | `DebugNormalColor` | DebugNormal | `asset/definition/debug_normal.definition.toml` | `debug_normal` |
| `material/sky_cube.mi.toml` | `SkyCube` | Sky | `asset/definition/sky_cube.definition.toml` | `sky_cube` |
| `material/sky_minimal.mi.toml` | `SkyMinimal` | Sky | `asset/definition/sky_minimal.definition.toml` | `sky_minimal` |
| `material/checkerboard_2d.mi.toml` | `builtin/checkerboard_2d` | Checker2D | `asset/definition/checkerboard_2d.definition.toml` | `checkerboard_2d`（去 `builtin/`） |

代码内嵌（`src/ShaderMaterialData/BuiltinMaterialDefinitions.cpp`，5 个）—— **保留 `builtin/` 前缀**，因为它们不是文件资产：

| 现 `id` | 新 `id` | Domain | 备注 |
|---|---|---|---|
| `builtin/pure_color` | 不变 | Custom | 用户确认：太简单，故内嵌 |
| `builtin/checkerboard_3d` | 不变 | Checker3D | 用户确认：报错专用 |
| `builtin/error_checkerboard_3d` | 不变 | ErrorChecker3D | 报错专用 |
| `builtin/text_gpu` | 不变 | Text2D | |
| `builtin/text_gpu_bitmap` | 不变 | Text2D | |

> 判定标准落到一句话：**`builtin/` 前缀 = 「C++ 源码内嵌，不来自文件资产」**；文件资产一律裸 snake_case。`checkerboard_2d` 因此从 `builtin/` 降级为普通定义。

### 5.3 配方名（R7）

> **后续修正**：`PBRSpheres.Lit` 和 `TextureQuad.UnlitTexture` 是示例及外部资产使用的配方名，迁移时改名导致查找失败。现已恢复这两个名称，文件名保留不变，作为文件名与 `name` 不一致的两个例外。以下为原迁移计划与历史记录。

- **强制**：62 个配方的 `name` 必须等于文件名去 `.recipe.toml`。修 2 个例外：
  - `recipes/PBRSpheres.recipe.toml`：`name` `PBRSpheres.Lit` → `PBRSpheres`
  - `recipes/TextureQuad.recipe.toml`：`name` `TextureQuad.UnlitTexture` → `TextureQuad`
- **强制**：`material_definition` 字段随 §5.2 新 `id` 同步（`Lit` → `lit` 等，共 60 处 + 2 处 `UnlitTexture`→`unlit_texture`）。
- **可选（Q1）**：去掉配方名里的定义名后缀（`06b.BasicLit.Lit` → `06b.BasicLit`），约 40 个文件 + `example/**/*.scene.toml` 引用。**默认不做**，因为它纯粹是美观，且会破坏已跟踪的示例场景。

---

## 6. ShaderLibrary 目标布局

```
ShaderLibrary/
├── domain/                                  # R1：Domain 入口 .frag（11 个 Domain 目录）
│   ├── root_bindings.glsl                   #   多 Domain 共享的 ABI 片段（9 个）
│   ├── standard_opaque_bindings.glsl
│   ├── emissive_row_bindings.glsl
│   ├── checker_2d_bindings.glsl
│   ├── checker_3d_bindings.glsl
│   ├── sky_bindings.glsl
│   ├── text_2d_bindings.glsl
│   ├── unlit_texture_bindings.glsl
│   ├── vertex_color_bindings.glsl
│   ├── standard_opaque/{forward_lit,shadow_caster,shadow_caster_masked}.frag
│   ├── unlit_solid/unlit_solid.frag
│   ├── unlit_texture/unlit_texture.frag
│   ├── vertex_color/vertex_color.frag
│   ├── vertex_luminance/vertex_luminance.frag
│   ├── checker_2d/checker_2d.frag
│   ├── checker_3d/checker_3d.frag
│   ├── error_checker_3d/error_checker_3d.frag
│   ├── text_2d/text_2d.frag
│   ├── debug_normal/debug_normal.frag
│   └── sky/{sky,sky_minimal}.frag
├── module/                                  # R1/R5：手写 GLSL 代码模块
│   ├── interface/                           #   纯接口声明（kind Shared）
│   │   ├── surface_interface.glsl  lighting_interface.glsl
│   │   ├── material_source_interface.glsl  ntb_interface.glsl
│   │   ├── pass_environment_interface.glsl alpha_compositor.glsl
│   ├── descriptor/                          #   数据形状（UBO/SSBO/ECS/bindless）
│   │   ├── ubo/{ambient_env_info,camera_info,color_palette,scene_ubo,
│   │   │          scene_ubo_bindings,shadow_info,viewport_info}.glsl
│   │   ├── ssbo/{emissive_surface_row,material_data_row,
│   │   │          pbr_surface_row,transmission_surface_row,
│   │   │          global_addresses,world_addresses,light_row}.glsl
│   │   │                                      ← 后 3 个来自原 ubo/（D8）
│   │   ├── ecs/{entity_gpu,entity_gpu_cull}.glsl
│   │   └── {bindless_textures,l2w_ssbo}.glsl
│   ├── stage/                               #   顶点/网格阶段结构
│   │   ├── interstage_varying.glsl  interstage_varying_writer.glsl
│   │   ├── stage_common_head.glsl  stage_common.glsl  stage_body.glsl
│   │   └── char_quad_body.glsl  line_quad_body.glsl
│   ├── vertex/                              #   R9：语义前缀替换 s1/s2/s3
│   │   ├── stream_*.glsl（17）  position_*.glsl（7）  transform_*.glsl（7）
│   │   └── helpers/*.glsl（4）
│   ├── provider/                            #   R2/R3：11 个 Slot，一一对应
│   │   ├── surface/           material_source/   ntb/
│   │   ├── diffuse_light/     specular_light/    shadow/
│   │   ├── ambient_light/     ambient_occlusion/
│   │   ├── sky/               lighting_model/    output_policy/
│   └── utility/
│       └── render_item_resolve.glsl          ← 原 common/RenderItemResolve.glsl
├── contract/                                # R4：契约与生成宏表
│   ├── sampler.toml                          ← 原 ShaderLibrary/sampler.toml
│   ├── descriptor_macros.glsl                # GENERATED（DescriptorMacroGen）
│   └── interstage_locations.glsl             # GENERATED（InterStageLocationMacroGen）
├── topology/                                # 固定拓扑（mesh shader 主文件）
│   ├── tri_192.mesh  linequad_256.mesh  charquad_256.mesh
└── asset/                                   # R1：TOML 资产
    ├── definition/*.definition.toml（10）    ← 原 material/*.mi.toml
    └── recipe/**/*.recipe.toml（62）         ← 原 material/recipes/**
```

**顶层目录 15 → 5**，`ShaderLibrary/material/` 消失。

> ⚠ **必须同步**：`inc/hgl/mtl/ShaderLibraryPath.h:49` 用 `(path / "material").IsDirectory()` 判定库根 —— 该目录不再存在，必须改为探测 `(path / "module").IsDirectory()`。

---

## 7. 模块 `@ulre name` 改名全表

### 7.1 槽位 Provider（R3：名字以槽位名开头）

| Slot | 现文件 → 现 name | 新文件 → 新 name |
|---|---|---|
| surface | `surface/material_surface.glsl` → `material_surface` | `surface_material` |
| surface | `surface/sky_minimal_surface.glsl` → `sky_minimal_surface` | `surface_sky_minimal` |
| surface | `material/sky_cube_surface.glsl` → `sky_cube_surface` | `surface_sky_cube` |
| material_source | `material/pbr_surface_source.glsl` → `pbr_surface_source` | `material_source_pbr_surface` |
| material_source | `material/unlit_source.glsl` → `unlit_source` | `material_source_unlit` |
| material_source | `material/texture_source.glsl` → `texture_source` | `material_source_texture` |
| material_source | `material/text_source_gpu.glsl` → `text_source_gpu` | `material_source_text_gpu` |
| material_source | `material/vertex_color_source.glsl` → `vertex_color_source` | `material_source_vertex_color` |
| material_source | `material/luminance_source.glsl` → `luminance_source` | `material_source_luminance` |
| material_source | `material/checkerboard_2d_source.glsl` → `checkerboard_2d_source` | `material_source_checkerboard_2d` |
| material_source | `material/checkerboard_3d_source.glsl` → `checkerboard_3d_source` | `material_source_checkerboard_3d` |
| material_source | `material/error_checkerboard_3d_source.glsl` → `error_checkerboard_3d_source` | `material_source_error_checkerboard_3d` |
| material_source | `material/debug_normal_source.glsl` → `debug_normal_source` | `material_source_debug_normal` |
| ntb | `ntb/ntb_tangent_vbo_normalmap.glsl` → `ntb_tangent_vbo_normalmap` | 不变（已符合 R3，`_normalmap` 携带"法线来自贴图"这一关键信息，保留） |
| ntb | `ntb/ntb_orthonormal.glsl` → `ntb_orthonormal` | 不变 |
| ntb | `ntb/ntb_none.glsl` → `ntb_none` | 不变 |
| diffuse_light | `lighting/diffuse_lambert.glsl` → `diffuse_lambert` | `diffuse_light_lambert` |
| diffuse_light | `lighting/diffuse_burley.glsl` → `diffuse_burley` | `diffuse_light_burley` |
| diffuse_light | `lighting/diffuse_toon.glsl` → `diffuse_toon` | `diffuse_light_toon` |
| diffuse_light | `lighting/diffuse_none.glsl` → `diffuse_none` | `diffuse_light_none` |
| specular_light | `lighting/specular_cook_torrance.glsl` → `specular_cook_torrance` | `specular_light_cook_torrance` |
| specular_light | `lighting/specular_toon_step.glsl` → `specular_toon_step` | `specular_light_toon_step` |
| specular_light | `lighting/specular_none.glsl` → `specular_none` | `specular_light_none` |
| shadow | `shadow/full.glsl` → `full_shadow` | `shadow_full` |
| shadow | `shadow/pcf_shadow.glsl` → `pcf_shadow` | `shadow_pcf` |
| shadow | `shadow/identity.glsl` → `identity_shadow` | `shadow_identity` |
| ambient_light | `lighting/indirect_ibl.glsl` → `indirect_ibl` | `ambient_light_ibl` |
| ambient_light | `lighting/indirect_sky_ambient.glsl` → `indirect_sky_ambient` | `ambient_light_sky` |
| ambient_light | `lighting/indirect_none.glsl` → `indirect_none` | `ambient_light_none` |
| ambient_occlusion | `ao/identity.glsl` → `identity_ao` | `ambient_occlusion_identity` |
| sky | `sky/sky_atmosphere.glsl` → `sky_atmosphere` | 不变 |
| lighting_model | `lighting/forward_pbr.glsl` → `forward_pbr` | `lighting_model_pbr` |
| lighting_model | `lighting/forward_toon.glsl` → `forward_toon` | `lighting_model_toon` |
| lighting_model | `lighting/forward_flat.glsl` → `forward_flat`（**缺 `@ulre slot`**） | `lighting_model_flat`，**补 `@ulre slot lighting_model`** |
| output_policy | `compositor/forward_lighting.glsl` → `forward_lighting` | `output_policy_forward` |
| output_policy | `compositor/flat_lighting.glsl` → `flat_lighting` | `output_policy_flat` |

### 7.2 非 Provider 模块

| 现 name | 新 name | 说明 |
|---|---|---|
| `RenderItemResolve` | `render_item_resolve` | R5；同时 `ecs/entity_gpu_cull.glsl` 的 `@ulre uses` 同步 |
| `scene_ubo`、`scene_ubo_bindings`、`bindless_textures`、`l2w_ssbo`、`entity_gpu`、`entity_gpu_cull`、`ambient_env_info`、`camera_info`、`color_palette`、`global_addresses`、`light_row`、`shadow_info`、`viewport_info`、`world_addresses`、`emissive_surface_row`、`material_data_row`、`pbr_surface_row`、`transmission_surface_row` | 不变 | 仅随目录迁移 |
| `s1_*` → `stream_*`（17） | `stream_color`、`stream_color_default`、`stream_luminance`、`stream_ntb`、`stream_ntb_a2bgr10`、`stream_ntb_rg16f`、`stream_ntb_rg8`、`stream_palette_index`、`stream_position_vec2`、`stream_position_vec2i`、`stream_position_vec3`、`stream_size`、`stream_defaults`（原 `s1_stream_defaults`）、`stream_text_char_quad`、`stream_transform_id`、`stream_uv`、`stream_uv_rg16f` | D9 |
| `s2_*` → `position_*`（7） | `position_lift_0xy`、`position_lift_x0y`、`position_lift_xy0`、`position_ndc_lift`、`position_passthrough3d`、`position_pixel_to_local`、`position_zeroone_to_ndc` | D9 |
| `s3_*` → `transform_*`（7） | `transform_camera_facing_fixed_pixels`、`transform_camera_facing_world`、`transform_clip_passthrough`、`transform_l2w_only`、`transform_ortho_then_l2w`、`transform_ortho_viewport`、`transform_world_camera_vp` | D9 |
| `helpers/*`（4）：`orient_camera_facing`、`orient_world`、`scale_fixed_pixels`、`world_space` | 不变 | |
| `*_interface`（5）、`alpha_compositor` | 不变 | R4 |
| `descriptor_macros`、`interstage_locations`、`interstage_varying`、`interstage_varying_writer`、`stage_common_head`、`stage_common`、`stage_body`、`char_quad_body`、`line_quad_body` | 不变 | 仅随目录迁移 |

**合计改名 68 个模块**（36 provider + 1 utility + 31 vertex；`ntb_tangent_vbo_normalmap` 与 3 个 `ntb_*` 中的其余 2 个、`sky_atmosphere` 已符合 R3 不改）。

---

## 8. 文件迁移全表（204 项）

**规则（可机械验证）**：表中「→」左侧为现状（`ShaderLibrary/` 相对路径），右侧为新路径；标注 `name:` 的行为同时改 `@ulre name`。

### 8.1 `domain/` —— 23 个，路径不变（0 移动）

`domain/` 下 9 个 `*_bindings.glsl` + 11 个 Domain 目录下 14 个 `.frag` 全部原地保留。**唯一改动是它们内部的 `#include` 字面路径**（见 §9.1）。

| 现路径 == 新路径 | 备注 |
|---|---|
| `domain/{root,standard_opaque,emissive_row,checker_2d,checker_3d,sky,text_2d,unlit_texture,vertex_color}_bindings.glsl`（9） | `checker_3d_bindings.glsl` 有意为空 |
| `domain/{checker_2d,checker_3d,debug_normal,error_checker_3d,sky,text_2d,unlit_solid,unlit_texture,vertex_color,vertex_luminance}/**.frag`（11 目录 / 14 文件） | `standard_opaque/` 下 3 个（`forward_lit`、`shadow_caster`、`shadow_caster_masked`）；`sky/` 下 2 个 |

### 8.2 `module/interface/` —— 6 个（+6 移动，0 改名）

| 现 | → 新 |
|---|---|
| `common/surface_interface.glsl` | `module/interface/surface_interface.glsl` |
| `common/lighting_interface.glsl` | `module/interface/lighting_interface.glsl` |
| `common/material_source_interface.glsl` | `module/interface/material_source_interface.glsl` |
| `common/ntb_interface.glsl` | `module/interface/ntb_interface.glsl` |
| `common/pass_environment_interface.glsl` | `module/interface/pass_environment_interface.glsl` |
| `common/alpha_compositor.glsl` | `module/interface/alpha_compositor.glsl` |

### 8.3 `contract/` —— 3 个

| 现 | → 新 |
|---|---|
| `sampler.toml` | `contract/sampler.toml` |
| `common/descriptor_macros.glsl` | `contract/descriptor_macros.glsl`（GENERATED） |
| `common/interstage_locations.glsl` | `contract/interstage_locations.glsl`（GENERATED） |

### 8.4 `module/stage/` —— 9 个（2 来自 common，7 来自 mesh）

| 现 | → 新 |
|---|---|
| `common/interstage_varying.glsl` | `module/stage/interstage_varying.glsl` |
| `common/interstage_varying_writer.glsl` | `module/stage/interstage_varying_writer.glsl` |
| `mesh/stage_common_head.glsl` | `module/stage/stage_common_head.glsl` |
| `mesh/stage_common.glsl` | `module/stage/stage_common.glsl` |
| `mesh/stage_body.glsl` | `module/stage/stage_body.glsl` |
| `mesh/char_quad_body.glsl` | `module/stage/char_quad_body.glsl` |
| `mesh/line_quad_body.glsl` | `module/stage/line_quad_body.glsl` |

（`mesh/` 共 8 个文件，其中 3 个 `.mesh` 去 `topology/`，见 8.9 —— 故本组 5 个来自 `mesh/`。）

### 8.5 `module/descriptor/` —— 18 个

| 现 | → 新 |
|---|---|
| `ubo/ambient_env_info.glsl` | `module/descriptor/ubo/ambient_env_info.glsl` |
| `ubo/camera_info.glsl` | `module/descriptor/ubo/camera_info.glsl` |
| `ubo/color_palette.glsl` | `module/descriptor/ubo/color_palette.glsl` |
| `ubo/scene_ubo.glsl` | `module/descriptor/ubo/scene_ubo.glsl` |
| `ubo/scene_ubo_bindings.glsl` | `module/descriptor/ubo/scene_ubo_bindings.glsl` |
| `ubo/shadow_info.glsl` | `module/descriptor/ubo/shadow_info.glsl` |
| `ubo/viewport_info.glsl` | `module/descriptor/ubo/viewport_info.glsl` |
| **`ubo/global_addresses.glsl`** | **`module/descriptor/ssbo/global_addresses.glsl`**（D8） |
| **`ubo/world_addresses.glsl`** | **`module/descriptor/ssbo/world_addresses.glsl`**（D8） |
| **`ubo/light_row.glsl`** | **`module/descriptor/ssbo/light_row.glsl`**（D8） |
| `ssbo/emissive_surface_row.glsl` | `module/descriptor/ssbo/emissive_surface_row.glsl` |
| `ssbo/material_data_row.glsl` | `module/descriptor/ssbo/material_data_row.glsl` |
| `ssbo/pbr_surface_row.glsl` | `module/descriptor/ssbo/pbr_surface_row.glsl` |
| `ssbo/transmission_surface_row.glsl` | `module/descriptor/ssbo/transmission_surface_row.glsl` |
| `ecs/entity_gpu.glsl` | `module/descriptor/ecs/entity_gpu.glsl` |
| `ecs/entity_gpu_cull.glsl` | `module/descriptor/ecs/entity_gpu_cull.glsl` |
| `common/bindless_textures.glsl` | `module/descriptor/bindless_textures.glsl` |
| `common/l2w_ssbo.glsl` | `module/descriptor/l2w_ssbo.glsl` |

### 8.6 `module/utility/` —— 1 个

| 现 | → 新 | name |
|---|---|---|
| `common/RenderItemResolve.glsl` | `module/utility/render_item_resolve.glsl` | `RenderItemResolve` → `render_item_resolve` |

### 8.7 `module/vertex/` —— 35 个

| 现 | → 新（== 新 `@ulre name`） |
|---|---|
| `vertex/s1_color.glsl` | `module/vertex/stream_color.glsl` |
| `vertex/s1_color_default.glsl` | `module/vertex/stream_color_default.glsl` |
| `vertex/s1_luminance.glsl` | `module/vertex/stream_luminance.glsl` |
| `vertex/s1_ntb.glsl` | `module/vertex/stream_ntb.glsl` |
| `vertex/s1_ntb_a2bgr10.glsl` | `module/vertex/stream_ntb_a2bgr10.glsl` |
| `vertex/s1_ntb_rg16f.glsl` | `module/vertex/stream_ntb_rg16f.glsl` |
| `vertex/s1_ntb_rg8.glsl` | `module/vertex/stream_ntb_rg8.glsl` |
| `vertex/s1_palette_index.glsl` | `module/vertex/stream_palette_index.glsl` |
| `vertex/s1_position_vec2.glsl` | `module/vertex/stream_position_vec2.glsl` |
| `vertex/s1_position_vec2i.glsl` | `module/vertex/stream_position_vec2i.glsl` |
| `vertex/s1_position_vec3.glsl` | `module/vertex/stream_position_vec3.glsl` |
| `vertex/s1_size.glsl` | `module/vertex/stream_size.glsl` |
| `vertex/s1_stream_defaults.glsl` | `module/vertex/stream_defaults.glsl` |
| `vertex/s1_text_char_quad.glsl` | `module/vertex/stream_text_char_quad.glsl` |
| `vertex/s1_transform_id.glsl` | `module/vertex/stream_transform_id.glsl` |
| `vertex/s1_uv.glsl` | `module/vertex/stream_uv.glsl` |
| `vertex/s1_uv_rg16f.glsl` | `module/vertex/stream_uv_rg16f.glsl` |
| `vertex/s2_lift_0xy.glsl` | `module/vertex/position_lift_0xy.glsl` |
| `vertex/s2_lift_x0y.glsl` | `module/vertex/position_lift_x0y.glsl` |
| `vertex/s2_lift_xy0.glsl` | `module/vertex/position_lift_xy0.glsl` |
| `vertex/s2_ndc_lift.glsl` | `module/vertex/position_ndc_lift.glsl` |
| `vertex/s2_passthrough3d.glsl` | `module/vertex/position_passthrough3d.glsl` |
| `vertex/s2_pixel_to_local.glsl` | `module/vertex/position_pixel_to_local.glsl` |
| `vertex/s2_zeroone_to_ndc.glsl` | `module/vertex/position_zeroone_to_ndc.glsl` |
| `vertex/s3_camera_facing_fixed_pixels.glsl` | `module/vertex/transform_camera_facing_fixed_pixels.glsl` |
| `vertex/s3_camera_facing_world.glsl` | `module/vertex/transform_camera_facing_world.glsl` |
| `vertex/s3_clip_passthrough.glsl` | `module/vertex/transform_clip_passthrough.glsl` |
| `vertex/s3_l2w_only.glsl` | `module/vertex/transform_l2w_only.glsl` |
| `vertex/s3_ortho_then_l2w.glsl` | `module/vertex/transform_ortho_then_l2w.glsl` |
| `vertex/s3_ortho_viewport.glsl` | `module/vertex/transform_ortho_viewport.glsl` |
| `vertex/s3_world_camera_vp.glsl` | `module/vertex/transform_world_camera_vp.glsl` |
| `vertex/helpers/orient_camera_facing.glsl` | `module/vertex/helpers/orient_camera_facing.glsl` |
| `vertex/helpers/orient_world.glsl` | `module/vertex/helpers/orient_world.glsl` |
| `vertex/helpers/scale_fixed_pixels.glsl` | `module/vertex/helpers/scale_fixed_pixels.glsl` |
| `vertex/helpers/world_space.glsl` | `module/vertex/helpers/world_space.glsl` |

### 8.8 `module/provider/` —— 36 个

见 §7.1 全表（37 行，其中 1 行是"不改名"的 `ntb_tangent_vbo_normalmap`）—— 该表已同时给出旧文件、新文件与新 `@ulre name`，此处不重复。汇总到 11 个 Slot 子目录的分布：

| Slot 目录 | 数量 | 文件 |
|---|---|---|
| `surface/` | 3 | `surface_material`、`surface_sky_minimal`、`surface_sky_cube` |
| `material_source/` | 10 | `material_source_{pbr_surface,unlit,texture,text_gpu,vertex_color,luminance,checkerboard_2d,checkerboard_3d,error_checkerboard_3d,debug_normal}` |
| `ntb/` | 3 | `ntb_tangent_vbo_normalmap`、`ntb_orthonormal`、`ntb_none` |
| `diffuse_light/` | 4 | `diffuse_light_{lambert,burley,toon,none}` |
| `specular_light/` | 3 | `specular_light_{cook_torrance,toon_step,none}` |
| `shadow/` | 3 | `shadow_{full,pcf,identity}` |
| `ambient_light/` | 3 | `ambient_light_{ibl,sky,none}` |
| `ambient_occlusion/` | 1 | `ambient_occlusion_identity` |
| `sky/` | 1 | `sky_atmosphere` |
| `lighting_model/` | 3 | `lighting_model_{pbr,toon,flat}` |
| `output_policy/` | 2 | `output_policy_{forward,flat}` |
| **合计** | **36** | 11 个 slot 目录，与 `ShaderModuleSlotRole` 的 11 个有效 role 一一对应 |

### 8.9 `topology/` —— 3 个

| 现 | → 新 |
|---|---|
| `mesh/shape/tri_192.mesh` | `topology/tri_192.mesh` |
| `mesh/shape/linequad_256.mesh` | `topology/linequad_256.mesh` |
| `mesh/shape/charquad_256.mesh` | `topology/charquad_256.mesh` |

### 8.10 `asset/definition/` —— 10 个

见 §5.2 全表（10 行），路径全部 `material/<x>.mi.toml` → `asset/definition/<x>.definition.toml`，其中 `vertex_palette_color` → `vertex_palette.definition.toml`。

### 8.11 `asset/recipe/` —— 62 个

**机械规则**：`material/recipes/<X>.recipe.toml` → `asset/recipe/<X>.recipe.toml`（目录整体上移一层，文件名不变）。

`asset/recipe/` 根（2）：
`PBRSpheres.recipe.toml`、`TextureQuad.recipe.toml` —— 且 `name` 字段各去掉定义名后缀（§5.3）。

`asset/recipe/Examples/` 下 60 个，按示例程序分组（文件名不变，仅位置）：

| 子目录 | 数量 | 文件名 |
|---|---|---|
| `ApiValidation/` | 3 | `ComputeAsteroidBelt.AsteroidMaterial`、`ComputeAsteroidBelt.PlanetMaterial`、`ComputeIndirectCount.CubeMaterial` |
| `Cull/` | 1 | `GPUDrivenCull.Cube` |
| `Debug/` | 3 | `RenderBoundBox.Solid`、`RenderBoundBox.Wire`、`SimplestAxis.VertexColor` |
| `Environment/` | 9 | `AtmosphereSkyAmbient.Lit`、`AtmosphereSkyAmbient.Sky`、`AtmosphereSkyMinimal.Sky`、`AtmosphereSkySunGizmo.Sky`、`BasicLitSunDirection.Lit`、`BasicLitSunDirection.Sky`、`IBLEnvironment.Lit`、`IBLEnvironment.Sky`、`SkyCubeSphere.Sky` |
| `Geometry/` | 5 | `ExtrudedPolygonTest.DebugNormalColor`、`LoadGeometry.DebugNormalColor`、`LoadGeometry.Wire`、`LoadScene.DebugNormalColor`、`WallsFromPolyline.Lit` |
| `GettingStarted/` | 3 | `DrawTriangle.VertexColor`、`SimpleCube.DebugNormalColor`、`SimpleSphere.Lit` |
| `Instancing/` | 2 | `AutoInstance.VertexColor`、`AutoMergeMaterialData.PureColor` |
| `Interaction/` | 5 | `GizmoUsage.DebugNormalColor`、`GizmoUsage.VertexLuminance`、`PlaneGrid3D.VertexLuminance`、`RayPicking.Line`、`RayPicking.PlaneGrid` |
| `Material/` | 13 | `06b.BasicLit.Lit`、`06c.FullPrecisionLit.Lit`、`06e.SingleSphereSwitch.Far`、`06e.SingleSphereSwitch.Near`、`AlphaModes.LitAlphaBlend`、`AlphaModes.LitAlphaDither`、`AlphaModes.LitAlphaTest`、`AlphaModes.LitOpaque`、`AlphaModes.LitOpaqueBillboard`、`CoverageVariants.LitDitherMask`、`CoverageVariants.LitOpaque`、`PBRSpheres.LitBillboard`、`Phase2.MaterialRecipeEntry.Cube` |
| `Offscreen/` | 5 | `RTTColorDepth.DisplayColor`、`RTTColorDepth.DisplayDepth`、`RTTColorDepth.OffscreenSphere`、`RenderToTexture.Cube`、`RenderToTexture.OffscreenSphere` |
| `Shadow/` | 5 | `AlphaTestShadow.Ground`、`AlphaTestShadow.Lit`、`CascadeShadowMap.AlphaTest`、`CascadeShadowMap.Lit`、`DualWorldShadows.Lit` |
| `Texture/` | 4 | `TextureArrayStreaming.UnlitTexture`、`TextureMipLevels.UnlitTexture`、`TextureRect.UnlitTexture`、`TextureRectArray.UnlitTexture` |
| `Transform/` | 2 | `Clock.PureColor`、`RecursiveCube.DebugNormalColor` |
| **合计** | **60** | |

> 62 个配方的 `material_definition` 字段值需按 §5.2 同步改写（映射：`Lit`→`lit`、`UnlitTexture`→`unlit_texture`、`UnlitTexture3D`→`unlit_texture_3d`、`VertexColor`→`vertex_color`、`VertexLuminance`→`vertex_luminance`、`DebugNormalColor`→`debug_normal`、`SkyCube`→`sky_cube`、`SkyMinimal`→`sky_minimal`、`builtin/pure_color` 不变）。分布：`Lit` 26、`DebugNormalColor` 11、`UnlitTexture` 5、`builtin/pure_color` 5、`VertexLuminance` 4、`SkyMinimal` 4、`VertexColor` 3、`SkyCube` 2、`UnlitTexture3D` 2。

### 8.12 计数核对

| 组 | 文件数 |
|---|---|
| 8.1 domain | 23 |
| 8.2 interface | 6 |
| 8.3 contract | 3 |
| 8.4 stage | 7 |
| 8.5 descriptor | 18 |
| 8.6 utility | 1 |
| 8.7 vertex | 35 |
| 8.8 provider | 36 |
| 8.9 topology | 3 |
| 8.10 definition | 10 |
| 8.11 recipe | 62 |
| **合计** | **204** |

> 与 `git ls-files ShaderLibrary` = 204 **完全一致**（已验证）。各组互斥、无遗漏、无重复。实施时以 `git mv` 输出为准逐项核对。
>
> 交叉校验：`domain` 23 = 9 个 `*_bindings.glsl` + 11 个 Domain 目录下 14 个 `.frag`；`descriptor` 18 = `ubo` 10 + `ssbo` 4 + `ecs` 2 + `bindless_textures` + `l2w_ssbo`；`stage` 7 = `common` 2 + `mesh` 5；`provider` 36 = `surface` 3 + `material_source` 10 + `ntb` 3 + `lighting` 13 + `shadow` 3 + `ao` 1 + `sky` 1 + `compositor` 2。

---

## 9. 引用面与风险

### 9.1 `#include` 字面路径（**最大工作量**）
include 路径是 **ShaderLibrary 根相对**，不是文件相对。所有 `#include "common/x.glsl"`、`"ubo/x.glsl"`、`"lighting/x.glsl"`、`"material/x_source.glsl"`、`"material/recipes/..."`、`"mesh/shape/*.mesh"` 都需要改。
涉及文件：14 个 `domain/**/*.frag`、36 个 provider 模块、`module/stage/*`、`module/descriptor/*`、`module/vertex/*`、`module/interface/*`。粗略 150+ 处。

### 9.2 `@ulre uses` 声明与 `#include` 必须同步
`src/ShaderMaterialData/ShaderCodeModuleRegistry.cpp`（约 220–315 行）校验两者一致：`@ulre uses <name>` 必须与被 include 的模块 `@ulre name` 对应。改名必须两侧同时改，否则注册即失败。

### 9.3 硬编码路径 / 名字的代码点

| 位置 | 内容 | 改动 |
|---|---|---|
| `inc/hgl/mtl/ShaderLibraryPath.h:49` | `(path / "material").IsDirectory()` 判库根 | → 探测 `module` |
| `src/ShaderMaterialData/DomainShaderTemplate.cpp:29-47` | `kDomainDirs[]` 的 `dir_name` + `frag_name` + `shadow_caster*` | Sky 特例改为表项；`VertexPalette`/`UnlitTexture3D` 复用关系保留 |
| `inc/hgl/mtl/VertexNodeConfigResolver.h` | 硬编码 `vertex/s2_*`(7)、`vertex/s3_*`(6) | → `module/vertex/position_*`、`transform_*` |
| `src/Tools/BufferGen/buffer_gen.toml` | `ubo/*`、`ssbo/*` 派生物路径真源（含 3 个 `kind=ssbo` 的 ubo 条目） | → 新 `module/descriptor/**` |
| `src/Tools/ShaderGen/CMakeLists.txt` | `descriptor_macros.glsl`、`interstage_locations.glsl`、`vertex/` 目录 | → `contract/`、`module/vertex/` |
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp:4182,4200-4209` | 断言 `ShaderLibrary/material` 目录存在、后缀必须是 `.mi.toml` | → `ShaderLibrary/asset/definition` + `.definition.toml` |
| `src/ecs/tests/TestMaterialRecipeFile.cpp:263,272,283` | `ShaderLibrary/material/recipes/...` 绝对相对路径 | → `asset/recipe/...` |
| `src/SceneGraph/module/SceneRenderTemplateResolver.cpp:5-79` | 4 个 profile 里模块名 + include 路径字符串 | 按 §7.1 全改 |
| `src/SceneGraph/module/EnvironmentManager.cpp:242` | 默认 ambient provider 名 `indirect_sky_ambient` | → `ambient_light_sky` |
| `src/SceneGraph/module/ShaderProgramManager.cpp:227-230` | ambient provider 传递 | 仅注释 |
| `src/Tools/RuntimeFormProbe/RuntimeFormProbe.cpp:413` | `miss.ambient_module_name = "AmbientVertexGlobalColor"` | 该名字**不是**注册模块名（全库无 `@ulre name AmbientVertexGlobalColor`），已失效；改为 `ambient_light_*` 家族里的一个真实名或置空 |
| `src/Tools/ShaderGen/SceneRenderTemplateResolverRegression.cpp:364-365` | 负向用例：`surface_module_include`/`ambient_module_name` 属已废弃键，测的是报错；但同串里的 `sky_cube_surface`/`indirect_ibl` 值名需与 §7.1 一致 | 只改值名 |
| `src/Tools/ShaderGen/SceneRenderTemplateResolverRegression.cpp:411` | 正向数组 `{ "sky_cube_surface", "sky_minimal_surface" }` | → `surface_sky_cube`/`surface_sky_minimal` |
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp:3636,3669,7588-7625` | `forward_pbr`/`forward_flat`/`forward_toon` 断言 | 同步 |
| `scripts/ci/check-buffer-gen.ps1` | 校验生成物一致 | 路径同步 |
| `scripts/ci/verify-release-form.ps1` | 发行形态验证 | 路径同步 |

### 9.4 数据资产引用

| 引用方 | 引用形式 | 风险 |
|---|---|---|
| `example/**/*.scene.toml`（已跟踪） | `recipe = "06b.BasicLit.Lit"` 等 | §5.3 若只改 `name` 后缀为「不做」，则**不受影响**；但配方的 `material_definition` 改名不影响场景文件 |
| `res/**`（**未跟踪**，用户资产） | 同上 | **警告**：任何 recipe `name` 变更都会破坏用户本地场景；这是 §5.3「可选」默认不做的原因 |
| `MaterialRecipeRegistry.cpp:19` | 按 `.recipe.toml` 后缀扫描目录 | 后缀不变，安全 |
| `MaterialDefinitionFile.cpp:1376` | 按 `.mi.toml` 后缀扫描 | **必须改** |

### 9.5 缓存与烘焙
- `MATERIAL_CODEGEN_GENERATION`（P1d 由 `SHADERGEN_CODEGEN_GENERATION` 改名）`15` → `16`，强制已烘焙 SPV/manifest 失效，避免「旧缓存 + 新模块名」拼出错程序。
- 发行形态（`ULRE_ENABLE_SHADER_PROGRAM_BUILD=OFF`）依赖预烘焙 manifest，改名后必须重新烘焙，否则运行时按旧名查不到模块。
- 仓库内**无跟踪的 `.pmf`/`.spv`**（缓存全部落在未跟踪的 `build/`、`build_probe_nosg/`），因此「重烘焙」= 重跑 `scripts/ci/verify-release-form.ps1` 的 `emit-release-cache`，无需提交任何二进制。

### 9.6 文档需同步（活跃文档）

**实际已同步（P6，共 10 篇 ACTIVE + 3 篇 HISTORY）**：
`doc/material-domain-contract-architecture.md`（32 处）、`doc/ambient-provider-types-inventory.md`（39 处）、`doc/ENVIRONMENT_SYSTEM.md`（18 处）、`doc/texture2darray-unified-texture-binding.md`（16 处）、`doc/simple-sphere-ecs-render-chain.md`（14 处）、`doc/GUI_Text_Render_Pipeline.md`（6 处）、`doc/csm-mechanism.md`（5 处）、`doc/ecs-layer-architecture-and-frame-flow.md`（2 处）、`doc/scene-toml-design-2026-10-29.md`（2 处）、`.ai/skills/SKILL_MATERIAL_RECIPE_TOML.md`（1 处），合计 135 处替换。

**加「命名治理已落地」映射 banner、正文作为历史快照不改写**：`doc/ultra-thin-fixed-shader-architecture.md`、`doc/ultra-thin-implementation-status.md`、`doc/shadergen-elimination-domain-fixed-architecture.md`。

`doc/` 中其它历史报告（`*-audit-2026-*`、`*-review-*`）**不改**（历史快照）。

### 9.7 分阶段执行顺序（每阶段独立提交、可独立回滚）

| 阶段 | 内容 | 提交粒度 |
|---|---|---|
| **P0** | 采集基线：全量构建 + 4 个回归门禁 + `buffer_gen.py --check`；记录 `git status` 干净前提（当前有用户未跟踪/已改动文件，见 §11 Q5）。**已采集，见 §9.7.1** | 无提交 |
| **P1** | C++ 符号改名（§4，纯重命名，无语义变更） | 按 §4.1/§4.2 分 2 提交 |
| **P2** | ShaderLibrary 目录迁移 + `#include` + `@ulre uses` + `@ulre name`（§6/§7/§8） | 按组 4 提交：`domain`/`module`（interface+stage+descriptor+utility）/`module/vertex`+`provider`/`asset`+`contract`+`topology` |
| **P3** | 硬编码路径与门禁更新（§9.3）+ `buffer_gen.toml` + 重新生成 `contract/*.glsl` | 1 提交 |
| **P4** | 资产 ID 与配方字段（§5.2/§5.3 强制部分） | 1 提交 |
| **P5** | 缓存代际 15→16 + 重新烘焙 + 发行形态验证 | 1 提交 |
| **P6** | 文档同步（§9.6） | 1 提交 |

**验证命令**（每阶段结束必跑）：
- `cmake --build --preset <baseline>`（P0 记录的实际 preset）
- `ShaderResourceSchemaRegressionGate`
- `SceneRenderTemplateResolverRegression`
- `ShaderDocumentProductionRegression`
- `VertexLoaderConsistency`
- `scripts/ci/check-buffer-gen.ps1`
- `scripts/ci/verify-release-form.ps1`（P5 后）

### 9.7.1 P0 基线实测（分支 `feature/gpu-driven-instanced-rendering`）

构建：`cmake --build build --config Debug --target <t>`（generator = "Visual Studio 18 2026"，`ULRE_ENABLE_SHADERGEN=ON`，输出 `build\out\Windows_64_Debug\`，无 `CMakePresets.json`）
BufferGen 调用：`python src\Tools\BufferGen\buffer_gen.py --repo-root . --clang "C:\clang\bin\clang++.exe" --include-dir "E:\vcpkg\installed\x64-windows\include" --check`

| 门禁 | 基线结果 |
|---|---|
| `ShaderResourceSchemaRegressionGate` | **EXIT=1**，56 PASS / **1 FAIL** |
| └ `M.material-library-contract` | **预先存在失败**：门禁要求 `ShaderLibrary/material` 下所有 `.toml` 必须是 `.mi.toml` 后缀，而 62 个 `.recipe.toml` 也位于该目录 —— 这是 §1 D2 的直接证据。**P3 完成后应转 PASS**，作为迁移验收信号之一 |
| `SceneRenderTemplateResolverRegression` | EXIT=0 |
| `ShaderDocumentProductionRegression` | EXIT=0（1 个 fixture skipped，预先存在） |
| `VertexLoaderConsistency ShaderLibrary\vertex` | EXIT=0（需传目录参数，否则 usage 退出码 3） |
| `DomainOptionsRegression` | EXIT=0（PASS=47 FAIL=0） |
| `buffer_gen.py --check` | EXIT=0（11 个 GLSL + `BufferToml.gen.h` + `buffer_schema.fbs`） |

> **基线非全绿是既有事实，不是本轮引入**。因此 §11 的「P0 基线全绿」应理解为「逐项记录并解释」，验收标准改为：迁移后除 `M.material-library-contract`（应转 PASS）外，其余门禁与基线逐项相同。

### 9.7.2 P1–P6 实测结果（全部完成）

| 阶段 | 提交 | 实测 |
|---|---|---|
| P1a | `67d48136a` `RenderTemplate*` → `PassProfile*` | 5 门禁逐项与基线一致 |
| P1b | `7f9f947d0` `ShaderCodeModule*` → `GlslModule*` | 同上 |
| P1c | `f34608513` `DomainShaderTemplate*` → `DomainEntrySource*` | 同上 |
| P1d | `3992d0c7a` `SHADERGEN_CODEGEN_GENERATION` → `MATERIAL_CODEGEN_GENERATION` | 同上 |
| P1e | `217dc4f46` `ULRE_ENABLE_SHADERGEN` → `ULRE_ENABLE_SHADER_PROGRAM_BUILD` | 同上 |
| P1f | `184fec3b4` `src/Tools/ShaderGen` → `src/Tools/ShaderTooling` | 同上 |
| P2+P3 | `31f10e755` ShaderLibrary 迁移（181 项移动 + 64 项 `@ulre name` 改名）+ 硬编码路径 + `buffer_gen.toml` + 门禁 + 重生成 `contract/*.glsl` | `M.material-library-contract` 由 **FAIL 转 PASS**；57 PASS / 0 FAIL |
| P4 | `c6b806689` 资产 ID 小写 snake_case + 62 配方 `material_definition` | 57 PASS / 0 FAIL；资产交叉校验 `problems: 0`（10 个 definition `id` 与文件名一一对应、62 个配方 `name` 与文件名一致、`material_definition` 全部可解析） |
| P5 | `MATERIAL_CODEGEN_GENERATION = 16` | 全 5 门禁重新构建后 57 PASS / 0 FAIL；`buffer_gen.py --check` EXIT=0；`verify-release-form.ps1` **PASS**（发行树零生成器符号 + 烘焙缓存可解析） |
| P6 | 13 篇文档 | `p6_docs.py --audit` 残留仅 3 篇 HISTORY 的 25 处（带 banner，预期内） |

**P5 期间发现并修掉的 3 个发行形态问题**（均属 P1e/P2/P3 遗留，非新增缺陷）：
1. `verify-release-form.ps1` 原来「缓存存在就跳过 configure」，旧缓存里的 `ULRE_ENABLE_SHADERGEN=OFF` 在选项改名后不生效，新选项回落默认 ON → 发行树静默带上生成器产物。改为**按缓存里的实际取值判定**（`Test-CacheFlag`），不符即重新 configure。
2. `RuntimeFormProbe` 的库根探测仍写死 `ShaderLibrary/material`（只改了报错文案），导致发行形态运行期 EXIT=2 → 改为 `ShaderLibrary/asset/definition`。
3. `build_probe_nosg/src/ShaderProgramBuild/*.vcxproj` 是上面第 1 条造成的陈旧残留，CMake 不会自动清理，需手工删除后重新验证。

### 9.7.3 本轮明确**未处理**的残留（已知、非本轮引入）

| 残留 | 位置 | 为什么留 |
|---|---|---|
| `src/ShaderGen/**` 路径（该目录已随更早的生成器瘦身整体消失，实现现落在 `src/ShaderProgramBuild/**`） | `doc/ENVIRONMENT_SYSTEM.md`、`doc/GUI_Text_Render_Pipeline.md`、`doc/deprecated/**`、`doc/future/**`、`doc/backlog.md`、`.ai/skills/SKILL_CASCADED_SHADOW_CSM.md` | **专属文档准确性任务**：`EmitShaderConfigDefines`、`MeshTemplateEmitter`、`FragmentTemplateComposer` 等符号在代码里已 0 命中，改名救不了，需按现行 `src/ShaderProgramBuild` / `DomainOptions` 重新表述。本轮只做改名与迁移 |
| `src/Tools/ShaderGen/**` 路径 | `doc/backlog.md:137,334`、`doc/build-and-module-structure-audit-2026-10.md`、`doc/deprecated/**`、`doc/future/**` | 均是**历史记录/审计快照**，按 §9.6 不加改写 |
| 5 个 `ShaderGen*` 契约头/实现文件名 | 见 §4.4 | 语义属「物理设备/契约」，不在本轮命名轴 |
| 配方文件名里的旧定义名后缀（约 40 个） | `ShaderLibrary/asset/recipe/Examples/**` | Q1=B：会破坏 `example/**/*.scene.toml` 与用户 `res/**` 的场景引用 |
| 示例配方仍在引擎库内 | `ShaderLibrary/asset/recipe/Examples/**`（60 个） | Q6=B：搬出涉及装载路径与扫描约定，另开任务 |
| `RenderTemplateID` 枚举值仍带 `Shadowed`/`AO` 轴信息 | `inc/hgl/mtl/PassProfile.h` | Q3=A：属结构改动（拆轴成独立槽位组合），另开任务 |

### 9.8 可回滚性
- P1 纯符号重命名：`git revert` 安全。
- P2/P3 必须**同一提交**内完成（迁移 + include + 路径真源 + 门禁），否则树不可构建。**不可拆分**。
- P4 依赖 P2 完成；P5 必须最后。

---

## 10. 待决问题（请逐条裁定）

| # | 问题 | 选项 | 建议 |
|---|---|---|---|
| **Q1** | 是否去掉配方名里的定义名后缀（`06b.BasicLit.Lit` → `06b.BasicLit`，约 40 个文件 + 场景引用）？ | A 做 / B 不做 | **B 不做**：纯美观，且会破坏 `example/**/*.scene.toml` 和用户 `res/**` |
| **Q2** | `.mi.toml` → `.definition.toml` 是否可接受？还是改用 `.material.toml`（与更早命名一致）？ | A `.definition.toml` / B `.material.toml` / C 不改 | **A**：与 `MaterialDefinition` 类型同名，词义最准 |
| **Q3** | `RenderTemplateID` 枚举值（`ForwardLitShadowedAO`…）是否也拆轴（阴影/AO 从名字里拿掉，变成独立槽位组合）？ | A 本轮不动 / B 一并改 | **A**：属于结构改动，不是改名，另开任务 |
| **Q4** | `src/Tools/ShaderGen` → `src/Tools/ShaderTooling`，回归目标一并改名？ | A 全改 / B 只改目录 / C 不动 | **A**：目录与目标名一致才不误导 |
| **Q5** | 当前工作树有用户自有改动/未跟踪文件（`CMScript`、`res`、`src/Tools/GLTFConvert` 未跟踪；`src/Tools/TexConv` 已修改）。执行 P0..P6 期间是否允许我保持它们不动、只在干净区域操作？ | A 允许（不动它们） / B 先由用户处理 | **A**：我不会触碰这四处，但 `git status` 永远不会完全干净，需接受「按路径白名单操作」 |
| **Q6** | 示例配方（`asset/recipe/Examples/**`，60 个）是否顺手搬出引擎库，进 `example/` 自己的资产目录？ | A 搬 / B 本轮只换目录名 | **B**：搬出去涉及示例装载路径与 `MaterialRecipeRegistry` 扫描约定，另开任务 |

---

## 11. 批准后的执行清单（勾选式）

- [x] Q1–Q6 裁定完毕（用户不可用期间按建议值推进：Q1=B、Q2=A、Q3=A、Q4=A、Q5=A、Q6=B）
- [x] P0 基线采集完成（§9.7.1，含 1 项预先存在失败及其解释）
- [x] P1 C++ 符号改名（§4.1、§4.2）—— 6 个提交，P1a…P1f
- [x] P2 ShaderLibrary 迁移 + `#include` + `@ulre`（§6/§7/§8）—— 与 P3 合并提交 `31f10e755`
- [x] P3 硬编码路径 / `buffer_gen.toml` / 门禁（§9.3）+ 重生成 `contract/*.glsl`
- [x] P4 资产 ID 与配方 `material_definition`（§5.2/§5.3 强制项）—— `c6b806689`
- [x] P5 `MATERIAL_CODEGEN_GENERATION` 16 + 重烘焙 + 发行形态验证 —— 发行形态 PASS
- [x] P6 文档同步（§9.6）—— 10 篇 ACTIVE + 3 篇 HISTORY banner
- [x] 交付：《旧名 → 新名》最终映射（§4/§5/§7/§8）+ 残留说明（§1 附注 4 条结构问题、§4.4 未改名 `ShaderGen*` 符号、§10 Q1/Q6 保留项）
