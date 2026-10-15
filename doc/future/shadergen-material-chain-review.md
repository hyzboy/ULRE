# ShaderGen → ECS 材质链评审（MaterialRecipe 到材质行）


> **快照**：HEAD = `73fa8ddae`（2026-10-11）；工作区在 `src/ShaderGen`、`inc/hgl/mtl`、`inc/hgl/ecs/**Material*`、`ShaderLibrary` 面**干净**。
> **范围**：`ShaderLibrary/material/*.toml`（15 份）→ `MaterialDefinitionRegistry` → `ShaderProgramManager::AcquireShaderProgram`
> → `BuildGenericMaterial`（5 相位）→ `CompileMaterial`（7 步）→ `ShaderBuildContext` → `FinalizeShaderBuildContext`（SPV 缓存/glslang）
> → `ShaderProgram`；ECS 侧 `RenderPrimitiveCollectSystem` → `MaterialData::BuildResolvedRecipe` + `MaterialVariantTable::Intern`
> + `MaterialRuntimeTable::Intern`/`AssignSlotRow` → 共享行/slot → `PipelineMaterialRenderer`。
> **规模**：`src/ShaderGen` 12,242 行（36 个 .cpp/.h）+ `inc/hgl/mtl` 材质头 + 回归门 5,649 行。
> **方法**：三路机械枚举（死代码 / 特例 / 平行真源）→ **逐条本机复核**（子代理结论不作为依据；下面每条都带可复跑的判据）。
> **口径**：评审只做分析，本档不含任何代码改动。

---

## 1. 结论摘要

| 级别 | 发现 | 一句话 |
|---|---|---|
| **高** | **A. 描述符契约层"活着但全空转"** | `BuildDescriptorsFromDefinition()` 忽略入参恒返回空 ⇒ `DescriptorContract.cpp` 296 行、`SerializedDescriptorEntry`、`ShaderResourceSchema.resources`、契约哈希全在空 vector 上跑；且入参路径无生产者 |
| **高** | **B. `MaterialTextureSamplingOptions` 双端空转** | 解析 ~220 行 + 结构/hash ~75 行；**0 个材质 TOML 授权**（15 份全无）、**0 个运行时消费者**（30 处命中 = 23 门 + 3 解析 + 1 声明 + 3 无关注释） |
| **中** | **C. 平行解析表** | 同一枚举的名字↔值映射被实现 2–3 份（`ParseCompareOp` ×2、filter/wrap ×2、采样器类型名 ×3）；T1 单一真源注册表**漏掉 7 个枚举** |
| **中** | **D. 双份真值（同事实多处表达）** | 模板必需槽（表 vs 硬编码）、`double_sided` vs `cull_mode`、`vertex_node_config` ×3、recipe"已归一化"两态两套纪律 |
| **中** | **F-1. 只写不读字段 5 处** | `material_row_cpu`/`material_texture_row_cpu`/`error_module_name`/`item_index`/`queue_family_count` —— 有赋值、**全仓 0 读取** |
| **中** | **G. 隐式顺序协议 4 条无守卫** | sampler.toml 顺序=索引、模板 slot 顺序=发射顺序、模板 define 早于 include、5 相位语句顺序=哈希输入 |
| **低** | **E. 特例簇** | CharQuad 特判、`MaterialMeshIndexTables` 字面量 3 处 strcmp、shadow 合成器手写、Sky routing 嵌套三元 |
| **低** | **F-2. 零消费者函数 23 / 类型与常量 8** | 含整个 `enum class BlendMode`、`ShaderArtifactSPVExtension`（artifact 契约真源常量却无人用） |
| **正面** | 数据驱动已成立者 | `s_descriptors`（mesh 模式表）、`kEmittedSemantics` 白名单、sampler 名→索引查表、`Templates[]`、回退规则表 + `static_assert`、`ShaderDocument` 块序协议（**带**诊断） |

---

## 2. 分模块发现

### A. 描述符契约层：活着但全空转（高）

| # | 位置 | 事实（机制） | 判据 |
|---|---|---|---|
| A1 | `src/ShaderGen/builder/DefinitionDescriptorBuilder.h:14-27` | `BuildDescriptorsFromDefinition(const MaterialDefinition &definition)` **忽略入参**（`(void)definition`）**恒返回空 vector**；函数名承诺"从 definition 构建描述符" | 函数体只有 `(void)definition; std::vector<...> descriptors; return descriptors;` |
| A2 | `src/ShaderGen/builder/GenericMaterialBuilder.cpp:387-408` | Phase 3 仍为它保留一步：`plan.descriptors = BuildDescriptorsFromDefinition(...)` → `BuildDescriptorContract(...)` + 失败路径 + 日志 | 该 Phase 的产物恒为空契约 |
| A3 | `src/ShaderGen/contract/DescriptorContract.cpp:56-99 / 133-158 / 160-202 / 250-295` | `AppendEntry`（语义推导、`LocalToWorldIndex`/`MaterialPrivateData` 特判）、`BuildEffectiveDescriptorContract` 的条目改写循环、`ValidateDescriptorContract` 的两两碰撞检查、`GetDescriptorContractHash` 的排序+逐字段哈希 —— **全部在空 vector 上运行**；哈希对构建键只贡献常量（`module_manifest_hash` ⊕ 空 entries hash） | 输入恒空 ⇒ 循环体不可达 |
| A4 | `src/ShaderGen/compile/MaterialShaderCompiler.cpp:301` | 注释"**契约恒 Scene UBO 条目**"与事实**相反**：`git grep -n "DescriptorSemantic::Scene\|DescriptorSetType::Scene" -- src inc` = **0 命中**（唯一同名物是无关的 `RenderSceneUBOSystem`） | 0 命中 |
| A5 | `inc/hgl/mtl/MaterialShaderCompiler.h:78-79` → `MaterialShaderCompiler.cpp:254-260` | `input.descriptor_entries` / `descriptor_entry_count` **全仓无生产者**（只有声明与这一处读取）⇒ `PrepareBaseDescriptorContract` 的 fallback 分支**不可达** | `git grep -n "descriptor_entries\|descriptor_entry_count" -- src inc` = 声明 2 + 读取 2 |
| A6 | `src/ShaderGen/compile/MaterialShaderCompiler.cpp:130-141` | `HasDescriptorSemantic()` 是 **static 且全仓零调用**（连本文件内也没有；`/W3` 不报 C4505 故构建 0 warning 掩盖了它） | 1 命中 |
| A7 | `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | **5,649 行**门里有 **58 处**描述符相关断言 —— 其中大部分在验证上述空机制（哪些用例仍有意义需逐条判） | `grep -c "descriptor\|Descriptor"` = 58 |
| ✅ | `MaterialShaderCompiler.cpp:150-173` `ValidateDefinitionCapabilitySubset` | **仍真有价值**：唯一合法态是"definition 声明的描述符为空"，任何一条即回归（现态：循环无条件为每条 push 诊断） | 生产路径每次编译都跑 |
| ✅ | `ShaderResourceSchema::requires_runtime_data_rows`（`ShaderResourceSchema.h:43`） | **仍真有价值**：行表需求信号（Step 6 直判），渲染侧建表/绑定表的判据 | `IsRuntimeDataRowsRequired()` 读它 |

**成因**：描述符集（Scene 集 / per-material 分配器 / L2W / mesh_draw_params 契约条目）逐轮退役时，**只删了生产点，没删载体**——契约类型、校验器、哈希器、门用例都留了下来，于是"永远为空的容器"继续参与每材质的构建与键计算。

**改进方向（供拍板，不在本档执行）**：把"描述符层"收敛为两个函数——`ValidateDefinitionCapabilitySubset`（必须为空）+ `requires_runtime_data_rows` 直判；
`DescriptorContract`/`SerializedDescriptorEntry`/`ShaderResourceSchema.resources`/`DescriptorSemantic` 的**条目载体与哈希**可整体删除，
`BuildDescriptorsFromDefinition` / `AppendEntry` / `BuildEffectiveDescriptorContract` / `BuildResourceSchemaFromContract` / `GetDescriptorContractHash` / `HasDescriptorSemantic` 随之消失；
`MaterialShaderCompiler.cpp:301` 的错误注释必须同批改掉（否则下一个会话按它去找 Scene UBO 条目）。

### B. `MaterialTextureSamplingOptions`：双端空转〔2026-10-11 **已整删**：见 §9〕（高）

| # | 位置 | 事实 | 判据 |
|---|---|---|---|
| B1 | `inc/hgl/mtl/MaterialRecipe.h:107-180` | 结构（17 字段：filter×3 / wrap×3 / swizzle×4 / compare / lod×3 / anisotropy / 2×has_*）+ `operator==` + `HashMaterialTextureSamplingOptions` ≈ **75 行** | 定义处 |
| B2 | `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp:493-710` | TOML 解析（`mag_filter`/`min_filter`/`mipmap_mode`/`u`/`v`/`w`/`r`/`g`/`b`/`a`/`compare_op`/`mip_lod_bias`/`min_lod`/`max_lod`/`max_anisotropy`/`anisotropy`/`swizzle` 等逐键 if-else）≈ **220 行** | 解析处 |
| B3 | **消费者 = 0** | `git grep -nw sampling -- src inc example ShaderLibrary` = 30 处 → 门 23 + 解析 3 + 声明 1 + GLSL/文本里无关词的注释 3 | 运行时绑定链（`RenderPrimitiveCollectSystem`、`MaterialData`）、GLSL 发射（`MaterialShaderEmitter` 的纹理通道宏只读 `channels`）、`VkSampler` 创建（`SamplerPresetLibrary` 读**另一个文件** `ShaderLibrary/sampler.toml`）都不读它 |
| B4 | **授权面 = 0** | 15 份材质 TOML 中 `sampling` / `anisotropy` / `swizzle` / `compare_op` 各 **0 个文件**（只有 `channels` 3 个文件在用，且那是另一条通路） | 逐键 `grep -l <key> ShaderLibrary/material/*.toml \| wc -l` = 0 |

**结论**：这是一条**没有作者写、也没有运行时读**的完整机制——只有回归门在替它维持存在。真实采样器由 `ShaderLibrary/sampler.toml`（预设名 → 索引 → `vkCreateSampler`）驱动，与 per-material 的 sampling 选项是**两套互不相通的东西**。

### C. 平行解析表（中）

**2026-10-11 处置**：`纹理 filter`/`纹理 wrap`/`纹理 compare` 三行的实现 A 随 **S2** 整删（材质侧那套双端空转）；`VkCompareOp` 的两份同名实现 + 采样器侧 filter/mipmap/address/compare 四份手写解析器随 **S3a** 收敛到 `inc/hgl/mtl/VkEnumText.h`（单一真源，见 §10）；`采样器类型名` 的三张表与四个注册表盲区 ⇒ **S3b 待办**。
**另（新发现）**：`src/Vulkan/VKString.{h,cpp}` 的整套 Vk↔文本注册表（9 个枚举 + DynamicState 大表、`String2VkEnum`）**全仓 0 消费者**——它覆盖的 compare/blend/cull 正是 ShaderGen 手写的那几份 ⇒ 真死代码，交用户拍板（删 / 日后并入公共头）。

| 枚举 | 实现 A | 实现 B（同语义第二份） | 备注 |
|---|---|---|---|
| `VkCompareOp` | `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp:112-133`（`ParseCompareOp`，8 项） | `src/ShaderGen/glsl_module/SamplerPreset.cpp:34-45`（`ParseCompareOp`，8 项） | **同名同语义两份实现** |
| 纹理 filter | `MaterialDefinitionFile.cpp:394-409`（`ParseTextureFilterMode` → `MaterialTextureFilterMode`，2 项） | `SamplerPreset.cpp:14-17`（`ParseFilter` → `VkFilter`，1 分支+默认） | 取值空间重叠、真源分离 |
| 纹理 wrap | `MaterialDefinitionFile.cpp:411-435`（`ParseTextureWrapMode`，5 项） | `SamplerPreset.cpp:25-32`（`ParseAddressMode`，4 显式 + default） | 同上 |
| 采样器类型名 | `MaterialDefinitionFile.cpp:377-392`（`ParseSampler`，6 项索引序） | `inc/hgl/mtl/MaterialRecipe.h:54-66`（`ToGLSLSamplerTypeName`，**零调用**）；另与 `inc/hgl/common/TextureSamplerTypeDef.h:105-152`（`SamplerTypeName` 数组 + `ParseSamplerType`）重叠 | **同一枚举反/正/第三方三张表** |
| 纹理 compare | `MaterialDefinitionFile.cpp:464-491`（`ParseTextureCompareOp` → `MaterialTextureCompareOp`，8 项） | 上面两处 `VkCompareOp` 解析（第三份） | 三个平行实现 |

**T1 单一真源注册表的覆盖盲区**（`枚举定义处` 提供 `Get*Name`/`Parse*` 的模式已用于
`ShaderCodeModuleSemantic`、`GlobalSSBOType`、`ShaderModuleSlotRole`、`DescriptorSemantic`）：
**未纳入** `GLSLSamplerType`〔S3b 待办：与 `inc/hgl/common/TextureSamplerTypeDef.h` 的 `SamplerType` 收敛〕、~~`MaterialTextureFilterMode`~~、~~`MaterialTextureWrapMode`~~、~~`MaterialTextureCompareOp`~~〔三者已随 S2 整删，2026-10-11〕、
`MaterialVertexProviderPolicy`（`MaterialDefinitionFile.cpp:58-74` 只有单向解析）、`ShaderCodeModuleKind`（`ShaderCodeModuleFile.cpp:153-163` 单向）、
`MeshShaderMode`（`inc/hgl/mtl/MeshShaderMode.h:18-29` 单向）⇒ 与既定口径（T1/T2 之后"名字表收敛到枚举定义处"）不一致。

### D. 双份真值（中）

| # | 事实 | 表达 1 | 表达 2 | 是否自动同步 |
|---|---|---|---|---|
| D1 | "模板的哪些槽是必需的" | `inc/hgl/mtl/RenderTemplate.h:83` `required` 字段（默认 true；`RenderTemplate.cpp:54` 有唯一显式 false），在 `FragmentTemplateComposer.cpp:355` **已生效** | `FragmentTemplateComposer.cpp:493-500` 按模板**硬编码**必需槽（ForwardUnlit 三项 / Sky 两项） | 否。硬编码与表当前对 ForwardUnlit 恰好同集合 ⇒ 冗余；与 `ShadowCasterOpaque` 的 `MaterialSourceProvider=false` 不一致的风险已存在 |
| D2 | 剔除方向 | `ResolvedMaterialRenderState.double_sided`（`PipelineConfig.h:59`） | `MaterialPipelineConfig.cull_mode`（`PipelineConfig.h:17`） | 否。TOML 可同时给、无一致性校验；最终 `src/Vulkan/VKRenderPass.cpp:166-168` 用 `double_sided` 覆盖成 `VK_CULL_MODE_NONE`。**两种键在 15 份材质 TOML 里均 0 授权** ⇒ 目前仅代码可达（`RenderPrimitiveCollectSystem.cpp:1110` 注释承认"仅 double_sided/cull 变化"也要重建 program 身份） |
| D3 | 顶点节点配置 | `MaterialDefinition.vertex_node_config`（`MaterialRecipe.h:322`） | `MaterialRecipe.vertex_node_config`（`:633`）+ `BuildRequest.vertex_node_config_override` | 否。优先级 request > recipe(非默认) > definition(非默认)，判"作者是否设置"用 `IsDefault3DNodeConfig`（`VertexShaderNodeConfig.h:85-92`）⇒ **默认值即缺席**的隐式三态 |
| D4 | "recipe 已归一化" | `IsRecipeNormalized`（`MaterialRecipe.h:693-701`，5 个 `has_*` 全真） | 消费侧纪律两套：`VKRenderPass.cpp:149-154` **报错拒绝**未归一化 recipe；`MaterialRecipe.h:678-690` `GetNormalizedRecipeRenderState` 对未归一化 recipe **静默返回全零** | 否。同一不变量，一个 fail-fast、一个静默给错值 |
| D5 | 回退材质定义 ID | `kFallbackMaterialRules`（`MaterialRecipe.h:540-560`，唯一权威 + `static_assert` 自检） | 门 `ShaderResourceSchemaRegressionGate.cpp:435/465/4052-4069/4224` 独立硬钉字面量 | 否。档内注释自述"换外观必然要动这两处" |
| D6 | TOML 允许键集合 | 解析器实际读取的键（`MaterialDefinitionFile.cpp:738-1068`） | 白名单 `ValidateMaterialDefinitionKeys`（`:1113-1174`） | 否。手工逐项对应（`:1112` 注释自述）；漏改白名单 ⇒ UnknownKey 拒绝（**有**保护，但需双维护） |

### E. 特例（低–中）

| # | 位置 | 内容 | 判据 / 定性 |
|---|---|---|---|
| E1 | `inc/hgl/mtl/MeshShaderMode.h:32`（`IsCharQuadMode`，注释自认"CharQuad 特判"）、`GenericMaterialBuilder.cpp:269`（CharQuad 跳过顶点 ABI）、`:427-455`（三分支硬编码各模式 max_invocations）、`MeshShaderModeCharQuad.h:21/39`（专属 include 与 .tmpl 名） | CharQuad 特判簇 | 触发源是 definition 字段（TOML `mode="CharQuad"`，仅 1 份材质）；"领域固有"（字符三层数据结构）**但**为单模式开的口已散成 4 处 |
| E2 | `src/ShaderGen/meshgen/MeshModeDescriptor.h:3` 自述目标"**消除** MeshTemplateEmitter 编排器内的特殊分支（如 `if (mode == CharQuad)`）"；实际 `MeshTemplateEmitter.h:264` 仍写 `if (mode == VertexPassthrough \|\| mode == LineQuad) … else（CharQuad 走 rows[gl_DrawID]）` | 目标已声明、**未达成**的残留分支 | 表 `s_descriptors`（`MeshModeDescriptor.h:183`）已建，编排器内仍有 mode 直比 |
| E3 | `MaterialShaderEmitter.cpp:388` 生产字面量 `"MaterialMeshIndexTables"`；`:609`/`:776`/`:788` 三处 `std::strcmp(..., "MaterialMeshIndexTables")` | 内部块名用字符串比较，无共享常量 | 1 生产 + 3 消费、字面量重复 4 次 |
| E4 | `FragmentTemplateComposer.cpp:536-694` `ComposeShadow`（整段 shadow GLSL 在 C++ 内逐块拼写）被 `:695` 唯一调用 | 唯一**不走 `.tmpl`** 的模板族 | 与"模板外置"口径不一致；新增 shadow 变体只能改 C++ |
| E5 | `SceneGraph/module/ShaderProgramManager.cpp:228-235` 模板路由嵌套三元（含"无 source 且无 ntb ⇒ Sky"隐含策略）；`:242`/`:263` 为 SkyCube / IBL 开的 surface/ambient 覆盖口 | 能力→模板的映射未表化 | 覆盖口的字段是通用的，但当前各只有 1 个声明者（`sky_cube.material.toml:14-15`、`lit_ibl.material.toml:17-18`） |
| E6 | `src/ShaderGen/template/SceneRenderTemplateResolver.cpp:5-60` | ForwardLit / Sky profile 逐行 `AddModule(role, name, path)` 硬编码（8 个槽） | 字面量各对应唯一模块文件；表化后即可消 |
| E7 | `src/ShaderGen/contract/MaterialOutputContract.cpp:37-60 / 61-…` | `switch (pass)`→purpose、`switch (purpose)`→附件 | 领域固有（pass↔purpose），但形式是 switch 而非表 |
| E8 | `DescriptorContract.cpp:77-83` | 为**恰好一个**语义（`LocalToWorldIndex`）硬写 `ssbo_type` 推导，紧邻又为 `MaterialPrivateData` 单列校验 | 属 A 层空转机制的一部分（随 A 一并处置） |

### F. 死代码与零消费者（低，逐符号，全部本机复核）

**F-1 只写不读的字段（有赋值、0 读取）**

| 字段 | 位置 | 写入点 | 读取 |
|---|---|---|---|
| `MaterialRuntimeRow::material_row_cpu` | `inc/hgl/ecs/support/MaterialRuntimeTable.h:145` | `RenderPrimitiveCollectSystem.cpp:745/1463/1520` | **0** |
| `MaterialRuntimeRow::material_texture_row_cpu` | `:151` | `RenderPrimitiveCollectSystem.cpp:747/1753/1812` | **0** |
| `ShaderCodeResourceManifest::error_module_name` | `inc/hgl/mtl/ShaderCodeResourceManifest.h:40` | `ShaderCodeResourceManifest.cpp:100/113/120/127/162/170` | **0**（错误文案走 `error` 枚举 + 调用方自带 `definition->name`） |
| `ShaderCodeModuleDiagnostic::item_index` | `inc/hgl/mtl/ShaderCodeModuleMetadata.h:38` | `ShaderCodeModuleMetadata.cpp:26` | **0** |
| `ShaderGenContract.h` `queue_family_count` | `inc/hgl/mtl/contract/ShaderGenContract.h:68` | `ShaderGenPhysicalDeviceProfileJson.h:189`、`Vulkan/ShaderGenPhysicalDeviceProfileAdapter.cpp:60` | **0** |

**2026-10-11 处置**：上表 5 处**已全部删除**（含 `item_index` 的形参与 12 处调用点实参、`queue_family_count` 删后变死的 `CountQueueFamilies` 整函数）+ 5 条禁复活针 —— 见 §12。

**F-2 零调用函数（除声明/定义外全仓 0 命中）**
`CaptureFinalDocuments`（`MaterialShaderCompiler.h:63`）· `ConfigureMaterialVertexSemanticContract`（`MaterialRecipe.h:474`）·
`ToGLSLSamplerTypeName`（`:54`）· `ParsePassType`（`PassType.h:39`）· `GetShaderCompilerTargetVersions`（`ShaderCompilerProfileAPI.h:12`）·
`SetCacheMode`/`GetRootPath`（`ShaderArtifactStore.h:31/33`）· `BuildStageInterfaceDocument`（`MaterialStageInterface.h:58`）·
`ValidateShaderSemanticRegistries`（`ShaderSemanticRegistry.h:141`）· `GetNormalizedShaderCodeModuleDependencyCount/Dependency`（`ShaderCodeModuleMetadata.h:44/47`）·
`WriteCString`/`WriteI32`/`WriteU16`（`CanonicalContractWriter.h:31/45/56`）· `EmitMeshShaderHeader`（`MeshShaderHeaderGen.h:103`）·
`AppendStageResourceBlocks`（`MaterialShaderEmitter.cpp:596`）· `HasDescriptorSemantic`（`MaterialShaderCompiler.cpp:130`）·
`GetGLSLVertexInputType`（`VertexABIBuilder.cpp:115`）· `DocumentFragmentBuilder::SetStage/SetMaterial`（`DocumentFragmentBuilder.cpp:13/18`；该类本体**活的**，被 `MaterialShaderEmitter.cpp:434` 使用）·
`MaterialData::ClearAuthoredResources`（`MaterialData.h:132`，仅注释与测试针提及）· `MaterialData::GetMaterialRowID`（`:129`，仅 2 处注释引用）·
`CalcMeshGroupCount`（`PipelineMaterialRenderer.h:54`，唯一消费者在 `example/ApiValidation/_Obsolete/`；而 `ShaderLibrary/ecs/entity_gpu_cull.glsl:385` 的注释仍以它为"同式"锚点）

**2026-10-11 处置**：23 项中 `HasDescriptorSemantic` 已在 **S1** 删除，其余 **22 项已全部删除**（含级联 `GetPassTypeName`、`GetNumericClassFromVkFormat` 与 4 条禁复活针）—— 见 §13。

**F-3 零消费者类型 / 枚举 / 常量**
`enum class BlendMode`（`inc/hgl/mtl/BlendMode.h:7`，全仓 `BlendMode::` **0 命中**；文件同时被列进 `ShaderGen/CMakeLists.txt:155`）·
`ResourceClass::CombinedImageSampler`（`ShaderGenContract.h:19`）· `ShaderArtifactSPVExtension`（`ShaderArtifactContract.h:17`）—— **升级为"真源被绕过"**：该常量是 artifact 契约里".spv"扩展名的**单一真源**（技能口径：`ShaderArtifactContract.h` 是缓存目录/扩展名/头布局唯一真源），但唯一使用点 `src/ShaderGen/compile/ShaderArtifactStore.cpp:23` **硬编码 `OS_TEXT(".spv")`** 而非引用它 ⇒ 处置 = **让它引用常量**（不是删除常量）·
`kMeshVertexPassthroughMaxPrimitives`（`MeshShaderLimits.h:15`）· `ShaderCodeModuleResolutionRequest`（`ShaderCodeModuleCapabilityResolver.h:61`，整类型 + 7 成员无读写）·
`ShaderCodeModuleGeometryCapability`（`:17`，仅被上面那个死类型引用）· `ShaderCodeModuleResolutionRequest::resource_count`（`:72`）

**2026-10-11 处置**：F-3 六项已全部处理（5 删 1 改）+ 门里三处同族硬编码改引用契约常量 + 1 条"必须存在"针 / 3 条禁复活针；`ShaderArtifactContract.h` 路径订正为 `inc/hgl/mtl/…` —— 见 §14。

**F-4 死参数（名不符实）**
`ApplyBaseMaterialInfoDefaults(MaterialRecipe&, const MaterialDefinition&, const bool overwrite_existing = false)`
（`inc/hgl/mtl/MaterialRecipe.h:746-752`）：**函数体从不读 `overwrite_existing`**，行为恒为"仅当 `mtl_def_id` 为空才填"；
唯一调用点 `MaterialDefinitionRegistry.cpp:296` 传 `false` ⇒ 参数既无用也无第二取值，名字承诺的"覆盖已有"不存在。

### G. 隐式顺序协议（中）

| # | 协议 | 位置 | 现有守卫 |
|---|---|---|---|
| G1 | `sampler.toml` 的 `[[samplers]]` **出现顺序 = sampler 索引**（宏 `#define <name>Sampler <idx>u` 与运行时 `vkCreateSampler` 同源） | 生产/消费 `src/ShaderGen/glsl_module/SamplerPreset.cpp:131-214`、`MaterialShaderEmitter.cpp:112-136`；表 `ShaderLibrary/sampler.toml` | **仅注释警告**（"插在中间会让所有已编译 shader 里的宏错位"），无任何校验 |
| G2 | fragment 模板的 **slot 数组顺序 = `#include` 发射顺序**，并隐含"SurfaceProvider 必须在 MaterialSourceProvider/NTBProvider 之后"（否则 GLSL 调用未声明函数） | 约定 `inc/hgl/mtl/RenderTemplate.h:92-104`（逐字写明"硬约束"）；消费 `FragmentTemplateComposer.cpp:340-364` | **仅注释 + 数组书写顺序**；无断言（重排会改生成文本 ⇒ program hash 变，但不会报错） |
| G3 | 模板 **define 必须早于 module_includes**（`#ifndef` 取值依赖顺序） | `FragmentTemplateComposer.cpp:399-417`（`{{defines}}` 块位置）、`:508-516`（占位符回填） | **仅注释**；正确性靠 `.tmpl` 内占位符书写位置 |
| G4 | `GenericMaterialBuilder.cpp` 的**语句顺序 = stage key / resource contract 的哈希输入序**（文件头自述"Do not reorder statements here"） | `src/ShaderGen/builder/GenericMaterialBuilder.cpp:1-8`（5 相位、Phase 3 注释同） | **仅注释**（`src/Vulkan/VKRenderPass.cpp:149-154` 的 recipe 归一化哨兵是同类做法的正例：那里是 fail-fast） |
| ✅ | 反例（有守卫的良好范例） | `ShaderDocument` 块序：顺序表 `document/ShaderDocument.cpp:42-57` + 强制点 `:115-131` + `DocumentFragmentBuilder.cpp:57-62`，倒退即 `block-order` 诊断且 `Serialize` 失败；code-module 依赖 DFS 序（`ShaderCodeResourceManifest.cpp:136-174`）+ 环检测（`:124-128`、`ShaderCodeModuleMetadata.cpp:84-131`）+ 依赖缺失检测（`ShaderCodeModuleRegistry.cpp:200-207`），顺序计入 `BuildStableHash` | **有显式校验** |

---

## 3. 已验证为真（可直接复跑）

```bash
# A. 描述符层空转
git grep -n "DescriptorSemantic::Scene\|DescriptorSetType::Scene" -- src inc | wc -l          # 0
git grep -n -w HasDescriptorSemantic -- src inc example | wc -l                              # 1（仅 static 定义）
git grep -n "descriptor_entries\|descriptor_entry_count" -- src inc                          # 声明 2 + 读取 2（无生产者）
grep -c "descriptor\|Descriptor" src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp   # 58（门长 5649 行）
# B. sampling 双端空转
git grep -n -w sampling -- src inc example ShaderLibrary | wc -l                             # 30（门 23 + 解析 3 + 声明 1 + 无关注释 3）
for k in sampling anisotropy swizzle compare_op; do echo -n "$k: "; grep -l "$k" ShaderLibrary/material/*.toml 2>/dev/null | wc -l; done   # 全 0（共 15 份材质）
# C/F. 逐符号计数
git grep -nw BlendMode -- src inc example        # 3（枚举声明 + 1 注释 + CMake 清单；BlendMode:: 全仓 0）
git grep -nw ShaderArtifactSPVExtension          # 1
git grep -nw kMeshVertexPassthroughMaxPrimitives # 1
git grep -nw material_row_cpu -- src inc example                    # 4（1 声明 + 3 赋值，0 读）
git grep -nw material_texture_row_cpu -- src inc example            # 4（同上）
git grep -nw error_module_name -- src inc example                   # 7（1 声明 + 6 赋值，0 读）
git grep -nw ClearAuthoredResources -- src inc example              # 4（声明/定义/2 注释；无调用）
# D/E. 双份真值与特例
sed -n '48,56p;76,78p' src/ShaderGen/template/RenderTemplate.cpp    # 槽表（含唯一 required=false）
sed -n '488,502p' src/ShaderGen/template/FragmentTemplateComposer.cpp  # 硬编码必需槽 vs 表
sed -n '264,275p' src/ShaderGen/meshgen/MeshTemplateEmitter.h       # mode 直比（与 MeshModeDescriptor.h:3 自述矛盾）
grep -n "MaterialMeshIndexTables" src/ShaderGen/compile/MaterialShaderEmitter.cpp   # 1 生产 + 3 比较
```

**构建/测试基线**：本档为只读评审，未改任何文件；`ctest -L ecs` 12/12、`TestCSMIncrementalPass` 1/1 为上一批（§4）状态。

---

## 4. 任务表（建议执行顺序：**先删遗留/未使用 → 再处理改进**，与既定口径一致）

| 批 | 任务 | 判据（红的样子） | 依赖 / 备注 |
|---|---|---|---|
| **S1** | **A 层收敛**：`ValidateDefinitionCapabilitySubset` + `requires_runtime_data_rows` 保留为唯一两条；删 `BuildDescriptorsFromDefinition` / `DescriptorContract`（含 `AppendEntry` / `BuildEffective*` / `BuildResourceSchemaFromContract` / `GetDescriptorContractHash` / `HasDescriptorSemantic` / `input.descriptor_entries*`）与 `plan.descriptors` 字段 | 全仓 grep 上述符号 = 0；`GenericMaterialBuilder` Phase 3 只剩 manifest + 验证；门的 58 处描述符断言同批收敛（保留"必须为空"反证针：往 definition 声明一条描述符 ⇒ 编译失败） | 改动面含 `inc/hgl/mtl/*` 公共头 ⇒ 先 purge 依赖 TU 再全量重编；`MaterialShaderCompiler.cpp:301` 错误注释同批改 |
| **S2** | **B 层整体删除**：`MaterialTextureSamplingOptions`（结构 + hash + `operator==`）+ `MaterialTextureDeclaration::sampling` + `MaterialDefinitionFile.cpp:493-710` 解析块 + `ParseTextureFilterMode/WrapMode/CompareOp`（**已核**：`MaterialTextureCompareOp` 仅出现在 `MaterialRecipe.h` 与 `MaterialDefinitionFile.cpp:466-473`，**没有**独立通路——真是源的 compare 走 `ShaderLibrary/sampler.toml:57` → `SamplerPreset.cpp:34` 的 `VkCompareOp`） | 15 份材质 TOML 无 `sampling` 键（已成立）；删后门用例同批收敛；`git grep -nw sampling` 只剩无关注释 | `channels` **不在**本批范围（活：3 份 TOML 授权 + 纹理通道宏消费者） |
| **S3** | **C 层平行解析表收敛**：`ParseCompareOp` 合一份；filter/wrap 的名字↔值收敛到枚举定义处（T1 模式：`Get*Name` + `Parse*`）；补 `GLSLSamplerType`/`MaterialVertexProviderPolicy`/`ShaderCodeModuleKind`/`MeshShaderMode` 的注册表 | 每个枚举的 name→value 只有 1 处实现（`git grep -c` 计数）；`ToGLSLSamplerTypeName` 要么被用、要么删；门用例同步 | 与 T1/T2 既定口径一致，可独立成批 |
| **S4** | **D 层去双真值**：D1（删硬编码必需槽，改读表 + 表里显式标 required）；D2（`double_sided` 与 `cull_mode` 二选一，另一条删或加一致性校验；TOML 侧两者 0 授权 ⇒ 可直接收敛）；D4（`GetNormalizedRecipeRenderState` 加同 `VKRenderPass` 的 fail-fast 或删除该口）；F-4 死参数删除。**〔2026-10-12 全部完成：F-4 并入并完成（`694ab2002`）、S4a=D1（§19.6）、S4b-D4（§19.7）、S4b-D2（§19.8）〕** | 删 D1 后：把某模板槽设为 `required=false` 且不提供 ⇒ 不再被硬编码判死；D4 反证：传未归一化 recipe ⇒ 报错而非静默全零 | D2/D4 触及 recipe/normalize 语义，建议单独一批并附反证 |
| **S5** | **F 层逐符号删除**（F-1 五字段 / F-2 二十三个零调用函数 / F-3 类型与常量 / F-4 已并入 S4） | 每个符号：`git grep -nw <sym> -- src inc example` 归 0（注释与测试针同批改写为"禁复活"） | 与 §1.2/§1.3 那一轮的删除纪律完全同构；`ShaderArtifactSPVExtension` 处置前先核 `".spv"` 是否有别处硬写（若有 ⇒ 改为用常量而非删） | **〔2026-10-11 已完成：F-1 §12 / F-2 §13 / F-3 §14 / F-4 §15，见 §16 小结〕**
| **S6** | **G 层顺序协议加守卫**：G1 sampler 顺序（加载时校验索引连续性/或改为显式 index 字段）；G2 slot 顺序（对 `SurfaceProvider` 早于 provider 的排列加断言）；G3/G4 至少加注释+哨兵（模板占位符位置校验、相位序哈希输入校验） | 反证：手工错排 sampler.toml / 调换 slot 顺序 ⇒ 加载或编译**报错**，而非静默产出错 GLSL | G1 影响已编译 shader 的宏索引 ⇒ 需与 SPV 缓存失效策略一起定 |
| **S7** | **E 层特例收敛**（可选，按用户口径"真需要才留"）：E2 把剩余 mode 分支推进 `s_descriptors` 表；E3 抽共享常量；E4 shadow 合成器外置为 `.tmpl`；E5/E6 profile 表化 | E2：编排器内 `mode ==` 直比归 0；E4：`CompositionShadow` 走 `.tmpl` 后与其它模板同路径 | E1 的 CharQuad 特判若判定"领域固有"则保留（4 处已收敛到 `IsCharQuadMode` + 表行）；不要为消除而过度抽象 |

### 勿动清单（评审认为现态合理，勿被下一轮当死代码删）

- `MeshModeDescriptor::s_descriptors` 表、`kEmittedSemantics` 白名单、`SamplerPresetLibrary` 的名→索引表、`Templates[]` 槽表、`kFallbackMaterialRules` + `static_assert`、`ShaderDocument` 块序协议与 code-module DFS 序（**有**守卫的两处）；`MaterialRuntimeTable` 的 `DetachOwned`（文件头已写明"别当死代码删"及其使用条件）。
- `MaterialData` 的 `authored_generation` 与行内 revision 的"镜像"关系（已由 §1.1 审计裁定不是双真值）。
- `IsCharQuadMode` 之外的 CharQuad 表行与专属 GLSL（`s1_text_char_quad.glsl`、`char_quad.glsl.tmpl`）——领域固有。

---

---

## 6. S1 执行记录（2026-10-11）—— 描述符契约层整体删除 + 载体塌缩为单信号

### 6.1 落地内容（33 档，净 **−1,367 / +72** 行）

**删除（7 档）**：`inc/hgl/mtl/ShaderResourceSchema.h`、`inc/hgl/mtl/SerializedDescriptorEntry.h`、
`inc/hgl/mtl/DescriptorContract.h`、`inc/hgl/mtl/DescriptorSemantic.h`、
`src/ShaderGen/contract/DescriptorContract.cpp`、`src/ShaderGen/builder/DefinitionDescriptorBuilder.h`、
`src/ShaderGen/builder/DescriptorBuilderCommon.h`。

**新增/改名**：`src/ShaderGen/builder/MaterialResourceManifestBuilder.h`（原 `DescriptorBuilderCommon.h` +
`DefinitionDescriptorBuilder.h` 合一；namespace `descriptor_builder_common` → `resource_manifest_builder`，
函数 → `BuildMaterialShaderCodeResourceManifest`）—— 一并去掉 `CStrEqual` 里「指针 < 0x10000 视为损坏」的装饰性守卫
（x64 上合法 C 字符串不可能落在低 64KB，该分支恒不触发）。

**载体塌缩**：材质编译产物只剩 **一个布尔** —— `ShaderBuildContext::requires_recipe_runtime_rows` /
`ShaderProgram::RequiresRecipeRuntimeRows()`；消费者 5 处（`PrimitiveBatchPipeline` ×2、`RenderPrimitiveCollectSystem` ×3）
由自由函数包装改读成员，行为逐字不变。

**生成器侧**：`MaterialShaderCompiler` 的 Step 1/3b/6 与 `HasDescriptorSemantic`、`ValidateDefinitionCapabilitySubset`、
`PrepareBaseDescriptorContract`、`BuildEffectiveDescriptorEntries`、`BuildAndValidateResourceSchema` 全删；
`GenericMaterialBuilder` 的 `plan.descriptors` 推导 + `BuildDescriptorContract` 步骤删除、Phase 3 改名
（resource manifest）；门的 `W1.material-descriptor-contract` 用例、`RunValidationCase` 及其 A/B1/B2 三用例删除，
`AE.binding-macro-single-source` 去掉合成条目输入。

**ECS/graph 侧（本批**新发现**的死消费者，评审档 §2-A 未列全）**：
- `RenderPrimitiveCollectSystem` 里 **3 处遍历 `schema.resources`（恒空）** 的循环全删 —— 其中 `:1214` 那处是
  `RenderSceneUBOSystem::RegisterMaterialStructLayout` 的**唯一调用点**；
- `RegisterMaterialStructLayout` 一并删除 —— 它把**同一张 stride 表的取值与自己比**（`byte_stride` 由调用方
  从 `GetGlobalSSBOTypeStructStride(type)` 取、再与 `GetGlobalSSBOTypeStructStride(type)` 比）⇒ **恒通过**，
  属装饰性校验（与「恒真守卫」同族）。真校验应落在**行写入侧**（那里才有实际写入字节数）⇒ 记为欠账。
- `GizmoResource::InitColorSSBO(gr, const ShaderResourceSchema&)` 的该参数**从未使用**，且其唯一调用点先花一次
  材质编译去拿这份 schema ⇒ 同批删除（生产路径上的死调用）。
- `ShaderProgramManager::BuildShaderResourceSchema` 删除（唯一生产消费者 = 上面那处死调用）。
- `ULRE_ARENA_DEBUG` 的诊断字段 `schema_reqs=%u` 恒为 0 ⇒ 改为只打印 `material_binding_valid`。

**缓存键等价式**：Phase 5 的 `resource_contract_hash` 原 = `GetDescriptorContractHash(空契约, manifest_hash)`
= `FNV1a(manifest_hash, FNV1a(uint32(0)))`；改为同名局部函数 `ComputeResourceLayoutHash(depth, manifest_hash)`
**逐位复现**原算式 ⇒ stage key / `resource_layout_hash` 不变，**不触发 SPV 与管线缓存整体失效**。
（`ShaderProgramArtifactBuilder` 的 artifact interface hash 含原 schema 哈希，改用单布尔 ⇒ 该值变化属预期。）

### 6.2 验收（实测）

| 项 | 结果 |
|---|---|
| 全量 Release（先清整棵 obj 树 1,804 个 + 全量重编；`-j 8` 撞已知 applocal 竞态 MSB3075 ⇒ `-j 1` 收敛） | `BUILD rc=0` / `error C*｜LNK*` = **0** / `error MSB3073｜3075` = **0** / `warning C*` = **0** / **393 目标** |
| `ctest -L ecs` | **12/12** |
| `TestCSMIncrementalPass` | **1/1** |
| ShaderGen 门 `ShaderResourceSchemaRegressionGate.exe`（cwd = 仓根） | **rc=0**，42 PASS / 0 FAIL（连跑两次：首跑重生成 golden、二跑走 golden 比对路径） |
| `ShaderCooker.exe --store <tmp>`（真生产路径，全变体） | `cooked=40 failed=1 skipped=17` |
| 运行时冒烟 | `SimpleSphere.exe` 18s / 107 帧 / 0 真错误；`PBRSpheres.exe` 18s / **2,630 帧** / 0 错误（每球一行材质数据的关键用例，`materialize(ok/fail)=1/0`） |

### 6.3 反证（针有牙）

往 `inc/hgl/vk/VKShaderProgram.h` 临时写回旧读口名 ⇒ `Test 11` **精确变红**：
`Test 11 Failed: 资源条目 schema 已按 S1 删除：… 'GetShaderResourceSchema' 重新出现在 VKShaderProgram.h（禁复活）`；
逐字节还原后复绿。新增 **4 条禁复活针**（`GenericMaterialBuilder.cpp` 的 `SerializedDescriptorEntry`、
`VKShaderProgram.h` 的 `GetShaderResourceSchema`、`RenderSceneUBOSystem.h` 的 `RegisterMaterialStructLayout`、
`PrimitiveBatchPipeline.cpp` 的 `MaterialRequiresRecipeRuntimeRows`）。

### 6.4 本批发现（未擅自处理）

1. **评审档 §2-A 的三处判断被本批订正**（详见 §6.5）。
2. **既存红（非本批引入）**：`ShaderCooker` 的 40/41 里那 1 例是
   `ShaderLibrary/material/shadow_receiver_source.glsl` 组成后的 GLSL 语法错
   （`glslang: 'const' : no qualifiers allowed for function return`，报在组成文本的 `:233`）。
   判据：该档**工作区与 HEAD 逐字节相同**、本批**未改动任何 GLSL 发射路径**（改动集中在契约/schema/哈希/dump）
   ⇒ 组成文本与 HEAD 逐字节相同 ⇒ 该例在 HEAD 即失败。
   **为何门没抓到**：门里 `ShadowReceiver` 只出现在「注册表覆盖」的文件 ID 列表断言里，**没有编译用例**；
   门绝大多数用例以 `defer_finalize=true` 构建（**不过 glslang**）⇒ 语法错在门里不可见。这正是技能里那条
   「门绿 ≠ 能编译」的实例；是否补一个 `ShadowReceiver` 的真实编译用例由用户拍板。
3. **门仍有两处「名不符实」未动**（避免与验证改动混批）：门文件名/目标名 `ShaderResourceSchemaRegressionGate`
   与其分组名 `descriptor`（该组现存用例已与描述符无关，只剩材质契约/SSBO 绑定键等活用例）。

### 6.5 评审档三处订正（旧说法 → 新结论）

| 位置 | 旧说法（本档早先写的） | 订正 |
|---|---|---|
| §2-A ✅ 行 | 「`ValidateDefinitionCapabilitySubset` 仍真有价值：唯一合法态是 definition 声明的描述符为空」 | **该函数是空的**：它检查的是「有效契约 → schema.resources」，而契约恒空、`resources` 恒空 ⇒ 任一条目都不可能存在，永远通过。S1 里删除；「不许描述符复活」改由**禁复活针**承担（可验证、可复现红） |
| §2-A A3/A5 行 | 「`input.descriptor_entries` 无生产者 ⇒ 分支不可达」 | 「**生产不可达，仅门/测试可达**」：门的 `AE.binding-macro-single-source` 与已删的 `W1` 用例曾手工构造条目喂它。S1 同批删掉这两处输入 |
| §4-S1 行 | 只列了生成器侧删除 | **补三类 ECS/graph 死消费者**（三处恒空循环、自比校验 `RegisterMaterialStructLayout`、`InitColorSSBO` 死参数）——它们是「同一机制的下游」，纯删生成器侧会留下一半 |

## 7. 阴影采样路实测（2026-10-11，承接"把 ShadowMap 改成硬件比较"一问）

### 7.1 对象先界定：`ShadowMap` 手工比较路是**孤儿**

| 事实 | 证据 |
|---|---|
| 示例地面实体名 `ShadowReceiverPlane`，但材质是 **Lit** | `example/Shadow/ShadowMap.cpp:916`（实体）× `:654`（`scene_recipe.mtl_def_id = "Lit"`）× 运行日志 `[MaterialTextureReferences] owner=ShadowReceiverPlane definition=Lit` |
| shadow map 现在经**世界 shadow 行**下发（不再走材质纹理槽） | `:1289` `shadow_info->shadow_tex = (shadow_map_handle, 0, 0, 0)`；消费侧 `ShaderLibrary/shadow/pcf_shadow.glsl` |
| `material/shadow_receiver*.{toml,glsl}` 无任何示例使用，且**长期编不过** | cook 于 2026-10-11 前为 `40/41`（唯一失败 = 该材质） |
| 文档与代码漂移 | `ShadowMap.cpp:71/90/128/139` 的注释仍写"地面改用 ShadowReceiver 材质" |

⇒ **活路径本来就是硬件比较**（`pcf_shadow.glsl` 的 `EvalPoissonPCF` 用 `Sample2DArrayShadow` + `ShadowPCF`）；
`shadow_receiver_source.glsl` 里的 3×3 手动比较属于被取代的旧设计。**用户 2026-10-11 决策：整条删除（见 §8）。**

### 7.2 拆掉的地雷：`#define shadow`（单词级宏）

`ShaderLibrary/ubo/scene_ubo.glsl` 原有一行 `#define shadow ShadowInfoRef(world_addresses.addr_shadow)` ——
**一个单词级小写宏**，注入到每个 include 该头的 shader：任何名为 `shadow` 的**局部变量**都会被整词改写成
buffer_reference 表达式。`material/shadow_receiver_source.glsl:233` 的 `const float shadow = EvalShadowReceiverMask(...)`
正是被它改写成 `const float ShadowInfoRef(...)` ⇒ `'const' : no qualifiers allowed for function return`
（用 `glslangValidator -E` 从预处理文本取证）。

**已修**：宏改名 `shadow` → `shadow_info`（`pcf_shadow.glsl` 34 处使用点同步），并补**禁复活针**
（`TestCSMIncrementalPass` 钉 `scene_ubo.glsl` 不得出现 `#define shadow `）+ 同步 Test 7C/Test 14 两处
钉文本的针。验证：**cook 41/41（原 40/1）**、门 42 PASS/0 FAIL、CSM 1/1、`ctest -L ecs` 12/12。

**零行为变化证据（A/B）**：暂存回退这两个 GLSL（旧宏）与新宏各跑一次示例自检，帧摘要**逐位相同**
（`0x7B8593FC7D72F968`）。

### 7.3 `ShadowPCF` 的 Linear 过滤：实测**有效**，不要改成 Nearest

`sampler.toml` 的 `ShadowPCF` 用 `mag/min/mipmap = Linear` + `compare_op = GreaterOrEqual`。
`shadow_receiver_source.glsl` 里写着"纯深度格式（D32F）不支持线性过滤 ⇒ 用 Linear 采样器读深度图属未定义行为"，
且引擎**没有任何 `SAMPLED_IMAGE_FILTER_LINEAR` 能力检查**（全仓 0 命中）。本机实测（`SM_SELFCHECK=1`，同树同机）：

| 配置 | 帧摘要 |
|---|---|
| `ShadowPCF` = Linear（现状） | `0x7B8593FC7D72F968`（两次跑一致 ⇒ 确定性） |
| `ShadowPCF` = Nearest（实验） | `0xD253E1BC8A24752A` |

⇒ ① **Linear 在比较采样语境下确实生效**，上面那句"不支持线性过滤"**不是普适法则**（更像是特定厂商/非比较采样路径的行为），
该注释需要限定；② 换 Nearest 会**改变画面**（不是无害实验），且对 CSM 的 32-tap Poisson PCF 而言 **Linear 是更强的配置**
（每 tap 自带 2×2 双线性比较）⇒ 现状保留，不建议改 Nearest。

### 7.4 既存红：`ShadowMap` 示例自检基线已陈旧

`SM_SELFCHECK=1` 报 `FAIL（期望 fnv=0xA8A369516CB04571，实测 0x7B8593FC7D72F968）`。
该基线被多份计划档当"逐位不变"判据引用（`doc/future/entity-rows-gpu-cull-plan.md:1389/1509/1600`、
`gpu-cull-flicker-postmortem.md:113/403`、`lighting-shadow-domain-split-plan.md:158/200`、
`offscreen-onetime-world-stall-analysis.md:119`、`sky-domain-and-provider-params-plan.md:271`）。
已证**与 S1 及宏改名无关**（旧宏/新宏摘要逐位相同）⇒ 属既存漂移；重新钉基线需用户在真机确认画面后统一改这些档。

---

## 8. ShadowMap 技术示例删除批（2026-10-11，用户决策）

**决策依据**：`ShadowMap` 是旧 shadow-map 直采路线的技术验证示例，CSM 已全面取代；单张阴影图的兜底 =
把 `example/Shadow/CascadeShadowMap.cpp:1057` 的 `cfg.cascade_count` 设 1（**该值可配**，实测；宏/采样器预设无需改动）。

### 8.1 删除面

| 项 | 说明 |
|---|---|
| `example/Shadow/ShadowMap.cpp` | 示例本体 |
| `ShaderLibrary/material/shadow_receiver.material.toml` + `shadow_receiver_source.glsl` | 该示例**独占**材质（全仓引用仅此一处）；亦是那颗单词级宏地雷的唯一受害者 |
| `sampler.toml` 的 `ShadowMap` 预设（第 8 项，末尾） | 唯一消费者是上面那份材质源；随附"D32F 不支持线性过滤"的注释一并退场（该观点已由 §7.3 实测限定） |
| `lit.material.toml` / `lit_ibl.material.toml` 的 `samplers` | 去掉 `"ShadowMap"`（否则落"预设查不到"告警） |

### 8.2 同批连带同步（每条都对应"红的样子"）

| 位置 | 改动 |
|---|---|
| `example/Shadow/CMakeLists.txt` | 删 `CreateProject(ShadowMap …)`；注释写明"要单张阴影图就把 cascade_count 设 1" |
| 门 `ShaderResourceSchemaRegressionGate.cpp` | 材质文件数 `15 → 14`；GLSL 模块数 `71 → 70`（两处 + 诊断串）；采样器预设表 8 → 7 项；`expected_file_ids` 去 `ShadowReceiver` |
| `TestCSMIncrementalPass.cpp` | 删 2 条指向已删文件的针（世界级访问器针；T6b 光源相机方向针 —— 其意图仍由 `EnvironmentInfo.h` 的 `sky.sun_direction` 禁复活针覆盖）；**新增 1 条禁复活针**钉 `example/Shadow/CMakeLists.txt` 不得再出现 `CreateProject(ShadowMap ` |
| 注释面 | `pcf_shadow.glsl` / `scene_ubo.glsl` 里对已删文件的交叉引用改为不依赖路径的表述（机制留在文里，指向消失） |
| 文档面 | 6 份计划档的 ShadowMap 台架基线行 + 计数行加 dated 标注（含 `15→14`、`71→70` 的现值订正） |

### 8.3 验收（实测）

| 项 | 结果 |
|---|---|
| 全量 Release | `rc=0` / `error C*｜LNK*` = **0** / **392 目标**（少 1 = 删掉的示例目标） |
| `ctest -L ecs` / `TestCSMIncrementalPass` | **12/12** / **1/1** |
| ShaderGen 门 | **42 PASS / 0 FAIL** |
| `ShaderCooker` | `cooked=38 failed=0`（原 41/1 里 ShadowReceiver 的 3 个变体退场） |
| 活路径冒烟 `CascadeShadowMap.exe` | 15s / **3,107 帧** / 0 错误 / `materialize(ok/fail)=4/0` |
| 陈旧产物 | `build/out/…/ShadowMap.exe` 手工清除（删 target 不会删旧 exe） |

### 8.4 本批踩的坑（已写进技能）

1. **锚点切片传空结束锚点 = 静默空操作**：`splice(p, start, "", "")` 等于 `t[:i] + "" + t[i:]` ⇒ 什么也没删却打印"成功"；
   本批因此漏删一条针，靠 `Test 27` 变红才发现。切片 helper 必须**拒绝空结束锚点**。
2. **`| head -N` 截断取证**：一次 `git grep … | head -12` 把测试文件里的命中截掉，得出"测试里没有引用"的错误结论。
   计数/穷举类取证不要接 `head`（同一天踩两次）。
3. **禁复活针的禁用串要防子串误撞**：首版钉 `"ShadowMap.cpp"`，而 `CascadeShadowMap.cpp` 含该子串 ⇒ 假红；
   改用 `CreateProject(ShadowMap ` 才准。凡是"禁用旧名"的针，先想一遍它与活名字的子串关系。

---

## 9. S2 执行记录：材质侧纹理采样选项双端死路径整删（2026-10-11）

**靶**：`MaterialTextureSamplingOptions` 及其 4 个枚举 + 解析链 + authoring 键。判据：**0 授权 + 0 消费者**
（15 份材质 TOML 对 `filter/wrap/swizzle/anisotropy/max_anisotropy/mip_lod_bias/min_lod/max_lod/compare_op` 全部 0 命中；
运行期采样器真源是 `ShaderLibrary/sampler.toml` 预设表）。删除面 4 档：

| 档 | 净删 | 内容 |
|---|---|---|
| `inc/hgl/mtl/MaterialRecipe.h` | −112 | `MaterialTextureFilterMode/WrapMode/Swizzle/CompareOp` 4 枚举 + `MaterialTextureSamplingOptions` + `operator==` + `HashMaterialTextureSamplingOptions` + `MaterialTextureDeclaration::sampling` 字段 |
| `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp` | −334 | `ParseTextureFilterMode/WrapMode/CompareOp/Swizzle/SwizzleOptions/TextureSamplingOptions` 6 个函数 + 调用点 + `ValidateTextureDeclarationKeys` 里 9 个 authoring 键 |
| 门 `ShaderResourceSchemaRegressionGate.cpp` | −96/+新 1 用例 | 采样 TOML 输入、`S.material-definition-file-schema` 的采样断言、`changed_sampling` ABI 用例；2 处聚合初始化改 3 元 |
| `doc/texture2darray-unified-texture-binding.md:216` | 标注 | dated 订正：该组 authoring 键整删，材质侧再写会被键校验拒绝 |

**意图不丢**：原先那条"非法 swizzle **值**必须被拒"的用例**改造成**"已退役 authoring **键**必须被键校验拒绝"
（机制换了、意图保留：畸形纹理声明不得静默通过）+ 一条禁复活针（`MaterialDefinitionFile.cpp` 不得再出现 `key != "swizzle"`）。

**反证（判据有牙）**：把 `key != "swizzle"` 放回白名单 ⇒ 门精确报
`[FAIL] S.material-definition-file-schema — retired texture sampling keys must be rejected as unknown keys`；还原后 42 PASS。

**验收**：全量 Release `rc=0`/0 error/392 目标；`ctest -L ecs` **12/12**；CSM **1/1**；门 **42 PASS/0 FAIL**；
cook `38/0`；`PBRSpheres` 15s 0 错误；`CascadeShadowMap` 12s 0 错误。

### 9.1 本批踩坑：定向 purge 不足以覆盖"改结构大小"（重要）

删 `MaterialTextureDeclaration` 的一个字段改变了**公共头的结构大小**。按老习惯对 `MaterialRecipe.h` 跑定向 purge
（`purge-stale-deps.sh` 命中 57 TU / 95 obj）后全量构建 `rc=0`、`ctest` 全绿、门 42 PASS、cook 38/0 ——**但**
`PBRSpheres.exe` 启动即崩（`0xC0000005` @ `PBRSpheres+0x16f450`：`mov r8,[rax-8]`，`rax=7`）。

排查顺序与结论：
1. 冷缓存复现（挪走 `shader-cache`）⇒ **仍崩** ⇒ 排除"SPV 缓存里烘焙了已删采样器索引 7"。
2. `find build -name "*.obj" -delete` + 全量重编 ⇒ **立刻恢复**（rc=124 正常出帧、0 错误）。
⇒ 结论：**改结构大小/字段集时，obj 必须「整树」清**；`grep` 出的包含者列表会漏掉间接包含的 TU，
漏一个就是"库用新头、某 TU 用旧头"的布局错位（本次崩在首个材质绑定路径）。

**注**：定向 purge 只能用于"只改实现/只加不删"的小改；凡动 `struct` 字段（增删改序/改类型）一律整树清。

---

## 10. S3a 执行记录：Vk 枚举 name↔value 收敛为单一真源（2026-10-11）

**靶**：ShaderGen 侧 **7 份手写 Vk 枚举解析器**——`MaterialDefinitionFile.cpp` 的 `ParseCullMode`/`ParseCompareOp`/`ParseBlendFactor`
与 `SamplerPreset.cpp` 的 `ParseFilter`/`ParseMipmapMode`/`ParseAddressMode`/`ParseCompareOp`（其中 `ParseCompareOp` 是**同名同语义两份**，§C 首行）。
判据：一个枚举的名字↔值只有 1 处实现，且**未知名必须失败**（不得静默落默认值）。

### 10.1 落地

| 项 | 内容 |
|---|---|
| **新增** `inc/hgl/mtl/VkEnumText.h`（单一真源，154 行） | 6 张表 + 6 对 `ParseVk*`/`GetVk*Name`：`VkCullModeFlags`/`VkCompareOp`/`VkBlendFactor`（材质 TOML）与 `VkFilter`/`VkSamplerMipmapMode`/`VkSamplerAddressMode`（sampler.toml）。拼写只此一份、精确匹配；未知名/空名 ⇒ `false` |
| `MaterialDefinitionFile.cpp` | 删 3 个解析器（−69 行），4 处调用改 `ParseVk*(value.c_str(), …)`；语义等价（本来就是未知名即失败） |
| `SamplerPreset.cpp` | 删 4 个解析器（−32 行），4 处调用改严格解析：未知名 ⇒ 打印 `非法 <key>: <值>` 并 `return false`（原先静默落 Linear/Repeat/Never） |
| 门 | 新增 `C.vk-enum-text`：**for 遍历全部 6 张表**（正查命中且值一致 + 反查非空；新增条目自动纳入，不手列断言）+ 8 条未知名/空名必须被拒 |
| 禁复活针 | `MaterialDefinitionFile.cpp` 不得含 `LessOrEqual`；`SamplerPreset.cpp` 不得含 `ClampToEdge`（防名字字面量散回解析文件） |

### 10.2 验收（实测）

| 项 | 结果 |
|---|---|
| 全量 Release | `rc=0` / 0 error / 392 目标 |
| `ctest -L ecs` / `TestCSMIncrementalPass` | **12/12** / **1/1**（含两条新针 ⇒ 字面量确已消失） |
| 门 | **43 PASS / 0 FAIL**（42 + `C.vk-enum-text`） |
| cook / `PBRSpheres` | 38/0 / rc=124 正常出帧、0 错误 |

### 10.3 反证（两条，各一次真实报红）

1. **表中拼写就是活路径**：改 `"GreaterOrEqual"`→`"GreaterEqual"` ⇒ 门 `[FAIL] AC.sampler-preset-library — sampler.toml load failed`（`ShadowPCF.compare_op` 断链）⇒ 还原 43 PASS。
2. **未知名必须失败**：`sampler.toml` 的 `UI.mag_filter` 改 `"Bogus"` ⇒ 同一处报 `sampler.toml load failed`（新行为；旧实现会静默变 Linear）⇒ 还原 43 PASS。

### 10.4 本批踩坑

1. **头文件破坏点的反证必须先 purge，否则假绿**：反证① 第一次直接 `cmake --build` 得到 `rc=0` + 门仍 43 PASS（MSBuild 没重编 include 该头的 TU）⇒ 差点误判"表不是真源"。补 `purge-stale-deps.sh … -- inc/hgl/mtl/VkEnumText.h`（`TUs=4 objs=6`）后同一破坏立刻精确报红。⇒ 破坏与还原**两侧**都要先 purge 再编。
2. **本档曾整篇 CRCRLF（doc/ 89 档里唯一一档，且已入库）**：早前写盘时对已是 CRLF 的内容又做了一次换行扩展，且叠出三层 CR（205/426 行）。已收敛为 CRLF。判据除 `loneLF==0` 外，必须同时判 `count(b"\r\r\n")==0`。

### 10.5 新发现（未擅自处理）

- **`src/Vulkan/VKString.{h,cpp}` 整套 Vk↔文本注册表 0 消费者**：`VkEnum2String`/`String2VkEnum`/`VkCullMode2String` 全仓无调用点，`IVulkanEnumStringList`/`VkGetEnumStringList` 仅出现在自身文件内。⇒ 真死代码（覆盖 compare/blend/cull，正是 S3a 收敛掉的那几份）。处置：删除（含其公共头）或日后并入公共头复用 —— **待拍板**。
- **S3b 待办**：`GLSLSamplerType`（6 项，`ParseSampler` 按**索引序**映射名字 ⇒ 枚举顺序一改即静默错位）与 `inc/hgl/common/TextureSamplerTypeDef.h` 的 `SamplerType`（18 项，已带 `GetSamplerTypeName`/`ParseSamplerType`）是同一概念的两套；`ToGLSLSamplerTypeName` 0 调用。另有 3 个注册表盲区：`MaterialVertexProviderPolicy`、`ShaderCodeModuleKind`、`MeshShaderMode`（均只有单向解析）。

---

## 11. S5-VKString：Vk↔文本注册表（死代码）整删（2026-10-11）

**靶**：`src/Vulkan/VKString.{h,cpp}` —— 覆盖 9 个 Vk 枚举（PolygonMode/CullMode/FrontFace/StencilOp/CompareOp/LogicOp/
BlendFactor/BlendOp/DynamicState）的 name↔value 注册表，含 `VkEnum2String`/`String2VkEnum`/`VkGetEnumStringList`/
`IVulkanEnumStringList`/`VkCullMode2String`/`String2VkCullMode`。

**判据（全仓 0 消费者，逐符号核实）**：7 个符号的 `git grep` 命中**只在自己两档内**；无人 `#include<hgl/vk/VKString.h>`；
`VkEnum2String` 等模板特化没有任何调用点。它覆盖的 compare/blend/cull 正是 S3a 收敛掉的那几份手写解析器 ⇒ 平行且更死的第二套。

### 11.1 删除面

| 项 | 内容 |
|---|---|
| `inc/hgl/vk/VKString.h`（公共头） | 删除（含 4 个导出声明） |
| `src/Vulkan/VKString.cpp` | 删除（含注册表实现 + `VK_ENUM_STRING_LIST` 宏 + DynamicState 大表） |
| `src/Vulkan/CMakeLists.txt` | 去掉两行（`${VK_INCLUDE_PATH}/VKString.h` 头文件登记；`VKString.cpp` 源文件，`)` 收在 `VKFormat.cpp` 行上） |
| `inc/hgl/mtl/VkEnumText.h` 注释 | 「另有 `src/Vulkan/VKString.cpp` 一套…」改为「…**已于 2026-10-11 整删**」（否则注释把读者指向已删文件） |
| 禁复活针 | `src/Vulkan/CMakeLists.txt` 不得再出现 `VKString.cpp` |

### 11.2 验收（实测）

| 项 | 结果 |
|---|---|
| 全量 Release（含 CMake 重配） | `rc=0` / 0 error / 392 目标 |
| `ctest -L ecs` / `TestCSMIncrementalPass` | **12/12** / **1/1** |
| 门 | **43 PASS / 0 FAIL** |
| cook | 38/0 |
| 示例冒烟（Vulkan 库重链后） | `PBRSpheres` rc=124 / 0 错误；`CascadeShadowMap` rc=124 / 0 错误 |

### 11.3 反证（针有牙）

把 `VKString.cpp` 加回 `src/Vulkan/CMakeLists.txt` ⇒ `TestCSMIncrementalPass` **Test 11 精确报红**：
`'VKString.cpp' 重新出现在 Vulkan/CMakeLists.txt（禁复活）`；还原后 1/1。

**残留判据**：7 个符号在 `src/inc/example/check/ShaderLibrary` 内只剩两处**有意保留**的文本
（本档的删除记录 + 新针自身的禁用串与文案），源码面零引用。

### 11.4 另注（未处理，待用户拍板）

- `bin/PipelineCompiler.exe`（2025-03 入库的**预编译二进制**，全仓无任何 CMake target 引用它）二进制里含这些符号名
  ⇒ 它是更早的构建产物，与本次删源无关；但**陈旧二进制入库**本身是卫生问题（本项目既有 `GLSLCompiler.dll` 这类
  **有意**入库的运行时插件，也有 `bin/PipelineCompiler.exe` 这种无 target 的遗留件）。删/留交用户。

---

## 12. S5a 执行记录：F-1 五个“只写不读”字段整删（2026-10-11）

**靶与判据**：5 个字段逐符号核实 = **只有赋值、全仓 0 读取**（`git grep -nw <字段>` 命中仅 声明 + 写点）：

| # | 字段 | 位置 | 写点 | 读取 |
|---|---|---|---|---|
| 1 | `MaterialRuntimeRow::material_row_cpu` | `inc/hgl/ecs/support/MaterialRuntimeTable.h` | `RenderPrimitiveCollectSystem.cpp` ×3 | 0 |
| 2 | `MaterialRuntimeRow::material_texture_row_cpu` | 同上 | 同上 ×3 | 0 |
| 3 | `ShaderCodeResourceManifest::error_module_name` | `inc/hgl/mtl/ShaderCodeResourceManifest.h` | `ShaderCodeResourceManifest.cpp` ×6 | 0 |
| 4 | `ShaderCodeModuleMetadataValidationDiagnostic::item_index` | `inc/hgl/mtl/ShaderCodeModuleMetadata.h` | 同上 `.cpp` ×1 | 0 |
| 5 | `PhysicalDeviceProfileLite::queue_family_count` | `inc/hgl/mtl/contract/ShaderGenContract.h` | JSON 读取侧 ×1 + Vulkan 适配器 ×1 | 0 |

### 12.1 落地

| 项 | 内容 |
|---|---|
| 1/2 | 删 2 个字段声明 + **6 处写点**（2 处 `= nullptr` 重置各 2 次；一处三行三元表达式；一处两行赋值）。**无级联**：`new_allocation.cpu_row` 另有活消费者（池的 `RowCPU` 校验），`material_buffer`/`row_offset` 仍被 GPU 侧赋值使用 |
| 3 | 删字段 + **6 处赋值**（全在失败路径）；诊断仍由 `error` 枚举 + 调用方自带的模块名承担 |
| 4 | 删字段 + 赋值 + `SetMetadataFailure` 的**形参** + **12 处调用点的第 4 实参**（14 个调用点中 12 处传了它）——刻意连参数一起删，不留“参数已死”的中间态 |
| 5 | 删契约字段 + 2 处写点 + **`CountQueueFamilies` 整函数（23 行）**（删掉唯一调用者后变死）。JSON 里的 `queue_families` 数组由采集器直接产出、**不经过本字段** ⇒ 产出文本与任何哈希都不受影响 |
| 针 | 5 条禁复活针（每个字段一条，钉在其定义处所属头文件），文案写明“何时删 / 替代表达是什么” |

### 12.2 验收（实测）

| 项 | 结果 |
|---|---|
| 全量 Release | **整树清 obj + 全量重编**（4 个公共头结构变化）；`-j 8` 撞 applocal 竞态（`env=10`，落点全在无关目标 `CMCore/examples/…/UnorderedSetMoveSemantics`）⇒ **`-j 1` 收敛 `rc=0 / code=0 / env=0` / 392 目标** |
| `ctest -L ecs` / `TestCSMIncrementalPass` | **12/12** / **1/1**（含 5 条新针 ⇒ 字段在源码面确已归零） |
| 门 | **43 PASS / 0 FAIL** |
| cook / `PBRSpheres` | 38/0 / rc=124 正常出帧、0 错误 |
| 残留 | `git grep` 5 个字段名在 `src inc example` 只剩针自身的禁用串与文案；`CountQueueFamilies` 归零 |

### 12.3 反证（逐针过）

- 把 `material_row_cpu` 字段加回 `MaterialRuntimeTable.h` ⇒ `TestCSMIncrementalPass` **Test 11 精确报红**：
  `… 'material_row_cpu' 重新出现在 MaterialRuntimeTable.h（禁复活）`。
- 把 `queue_family_count` 加回 `ShaderGenContract.h` ⇒ 同处精确报红（文案带"禁复活"）。
- 两次还原后 `ctest` 1/1 且**逐字节一致**（`sha` 级比对）。

---

## 13. F-2 执行记录：零调用函数整删（2026-10-11）

**靶**：§F-2 清单 23 项 —— 其中 `HasDescriptorSemantic` 已在 **S1** 同批删掉（清单为旧稿），本批实做 **22 项**。
判据：`git grep -nw <符号>` 除自身声明/定义外**全仓 0 命中**；每一处先复算（评审是前一天写的，S2/S3a 之后状态可能已变）。

### 13.1 删除面（按形态）

| 形态 | 符号 |
|---|---|
| 声明 + 定义（跨 .h/.cpp） | `CaptureFinalDocuments` · `GetShaderCompilerTargetVersions` · `BuildStageInterfaceDocument` · `ValidateShaderSemanticRegistries` · `GetNormalizedShaderCodeModuleDependencyCount` · `GetNormalizedShaderCodeModuleDependency` · `DocumentFragmentBuilder::SetStage` / `SetMaterial` · `MaterialData::ClearAuthoredResources` |
| 头内 inline（声明即定义） | `ConfigureMaterialVertexSemanticContract` · `ToGLSLSamplerTypeName` · `ParsePassType` · `SetCacheMode` · `GetRootPath` · `WriteU16` / `WriteI32` / `WriteCString` · `EmitMeshShaderHeader` · `CalcMeshGroupCount` |
| .cpp 内 static | `AppendStageResourceBlocks` · `GetGLSLVertexInputType` |

### 13.2 级联（删掉目标后随之变死，同批删除）

| 符号 | 变死原因 |
|---|---|
| `GetPassTypeName`（`PassType.h`） | `ParsePassType` 是它**唯一**调用者（PassType 枚举本身仍活：`MaterialOutputContract` 用其作形参类型） |
| `GetNumericClassFromVkFormat`（`VertexABIBuilder.cpp`，**94 行**） | `GetGLSLVertexInputType` 是它唯一调用者（`ShaderCodeModuleNumericClass` 枚举仍活：`ShaderCodeModule` 的 `numeric_class_mask` 字段在用） |

### 13.3 连带同步（不留指向已删符号的引用）

| 位置 | 改动 |
|---|---|
| `MaterialData.h` | 授权入口清单去掉 `ClearAuthoredResources`（唯一入口纪律本身不变） |
| `MaterialAuthoringSideTable.h` / `graph/ssbo/MaterialDataRows.h` | 两处注释 `MaterialData::GetMaterialRowID()` → `MaterialData::material_row_id`（字段仍活：内部 20+ 处使用） |
| `ShaderCodeModuleMetadata.cpp` | 内联访问处的注释删掉旧函数名（改为"边界由循环条件保证"） |
| `ShaderLibrary/ecs/entity_gpu_cull.glsl:385` | 注释去 `CalcMeshGroupCount`，改为"组大小见 `MeshDispatchGroupSize`"（该函数是活真源，被 `PrimitiveBatchPipeline` 使用） |
| `example/ApiValidation/_Obsolete/ComputeAsteroidBelt.cpp` | **停放源码**（不在任何 CMake 构建内，`example/ApiValidation/CMakeLists.txt` 只留一行说明）：调用点就地展开组数公式，保持该停放文件自洽 |
| 针 | 删 1 条（绑定 `ClearAuthoredResources` 的"唯一入口"针随函数退场；`SetDataResource`/`SetTextureResource` 两条同类针仍在）；**新增 4 条禁复活针**（`CalcMeshGroupCount` / `ToGLSLSamplerTypeName` / `GetNormalizedShaderCodeModuleDependency` / `GetMaterialRowID`），文案写明替代表达 |

### 13.4 验收（实测）

| 项 | 结果 |
|---|---|
| 全量 Release（先 `purge-stale-deps.sh`：101 TU / 142 obj） | `rc=0` / 0 error / 0 MSB / **392 目标** |
| `ctest -L ecs` / `TestCSMIncrementalPass` | **12/12** / **1/1**（含 4 条新针） |
| 门 | **43 PASS / 0 FAIL** |
| cook | 38/0 |
| 示例冒烟 | `PBRSpheres` / `CascadeShadowMap` / `TextDraw` 三个 rc=124 正常出帧、各 0 错误 |

### 13.5 反证（逐针单独跑，避免"第一处即停"掩盖第二条）

- `CalcMeshGroupCount` 加回 `PipelineMaterialRenderer.h` ⇒
  `Test 11 Failed: … 'CalcMeshGroupCount' 重新出现在 PipelineMaterialRenderer.h（禁复活）`；
- `ToGLSLSamplerTypeName` 加回 `MaterialRecipe.h` ⇒ 同处精确报红。
  两次还原后 `TestCSMIncrementalPass` **1/1**，且两个头文件逐字节回到改写前状态（0 残留）。

**残留判据**：23 个符号在 `src inc example ShaderLibrary` 内源码面**全部归零**；仅 4 条针的禁用串与文案保留。

### 13.6 剩余

- **F-3**：零消费者类型 / 枚举 / 常量（`BlendMode`、`ResourceClass::CombinedImageSampler`、`kMeshVertexPassthroughMaxPrimitives`、`ShaderCodeModuleResolutionRequest` 一族；`ShaderArtifactSPVExtension` 属"让它引用常量"而非删除）。
- **F-4**：死参数 `ApplyBaseMaterialInfoDefaults(..., overwrite_existing)`（函数体从不读该参数，唯一调用点传 `false`）。

---

## 14. F-3 执行记录：零消费者类型 / 枚举 / 常量（2026-10-11）

**靶**：§F-3 清单 6 项，全部处理（其中 1 项是"改引用常量"而非删除）。

| 项 | 处置 |
|---|---|
| `enum class BlendMode`（`inc/hgl/mtl/BlendMode.h`，全仓 `BlendMode::` **0 命中**） | **删文件** + `ShaderGen/CMakeLists.txt` 的条目 + `ShaderLinkSpec.h` 注释里去名（改为"混合/深度/模板"）——混合状态由运行时管线状态管，材质侧不建平行类型 |
| `ResourceClass::CombinedImageSampler` | **连带删整个 `ResourceClass` 枚举 + `ResourceRequirement` 结构**：后者全仓 0 消费者，枚举失去唯一字段引用（描述符契约层退役后的遗留） |
| `ShaderCodeModuleGeometryCapability`（含 `operator==`） | 整类型删除 |
| `ShaderCodeModuleResolutionRequest`（7 成员，其中 `resource_count`） | 整类型删除（它是上面那个类型的唯一引用者）。**同文件其余仍活**：`ResolutionResult` / `ProviderSelection` / `RejectDiagnostic` / `GetShaderCodeModuleProviderGraphHash` / `ComposeShaderCodeModuleProviderGraph` 一律保留 |
| `kMeshVertexPassthroughMaxPrimitives` | 删除（0 消费者；同族 `kMeshVertexPassthroughMaxInvocations` / `kMeshVertexPassthroughMaxVertices` / `kMeshLineQuadMaxInvocations` 仍活，保留） |
| `ShaderArtifactSPVExtension` | **不删**（它是 `.spv` 扩展名的单一真源）：把 `ShaderArtifactStore.cpp:23` 的硬编码 `OS_TEXT(".spv")` 改为 `ToOSString(ShaderArtifactSPVExtension)` |

**顺带修正同族第二处"真源被绕过"**：门 `ShaderResourceSchemaRegressionGate.cpp` 里 `"shader-cache"` / `"program"` / `".meta"`
三处硬编码 ⇒ 改为引用契约常量 `ShaderArtifactCacheDirectory` / `ShaderArtifactProgramDirectory` / `ShaderArtifactMetadataExtension`。
门的"只读完整 program artifact"用例照旧通过、产物目录名仍为 `shader-cache/{stage,program}` ⇒ 路径等价、行为零变化。

**针**：**1 条"必须存在"针**（钉 `ShaderArtifactStore.cpp` 含 `ToOSString(ShaderArtifactSPVExtension)`——防再退回硬编码字面量）
+ **3 条禁复活针**（`BlendMode.h` / `ResourceClass` / `ShaderCodeModuleResolutionRequest`）。

### 14.1 验收（实测）

| 项 | 结果 |
|---|---|
| 全量 Release（先 purge 19 TU / 22 obj） | `rc=0` / 0 error / 0 MSB / **392 目标** |
| `ctest -L ecs` / `TestCSMIncrementalPass` | **12/12** / **1/1**（4 条新针在内） |
| 门 | **43 PASS / 0 FAIL** |
| cook | 38/0 |
| 示例冒烟 | `PBRSpheres` / `CascadeShadowMap` 各 rc=124、0 错误 |
| 产物目录 | 仍为 `shader-cache/{stage,program}`（常量拼出的路径逐字节等价） |
| 残留 | 6 项源码面归零（`CombinedImageSampler` / `GeometryCapability` / `kMeshVertexPassthroughMaxPrimitives` 0 命中；其余命中只在针条目文案里） |

### 14.2 反证（逐针过）

- 把 `ShaderArtifactStore.cpp` 退回硬编码 `.spv` ⇒ `Test 11 Failed: … 'ToOSString(ShaderArtifactSPVExtension)' not found in ShaderArtifactStore.cpp`
  （**"必须存在"针生效**：真源用法回退立刻被抓住）。
- `BlendMode.h` 加回 `ShaderGen/CMakeLists.txt` ⇒ `Test 11 Failed: … 'BlendMode.h' 重新出现在 ShaderGen/CMakeLists.txt（禁复活）`。
- 两次还原后 `TestCSMIncrementalPass` **1/1** 且两个文件逐字节回原。

### 14.3 订正

§F-3 原文把 `ShaderArtifactContract.h` 的路径写作 `src/ShaderGen/contract/…`，**实际是 `inc/hgl/mtl/ShaderArtifactContract.h`**（本批按实际路径处置）。

**剩余**：**F-4** 死参数 `ApplyBaseMaterialInfoDefaults(..., overwrite_existing)`（函数体从不读该参数、唯一调用点传 `false`，名字承诺的"覆盖已有"不存在）。

---

## 15. F-4 执行记录：死参数（复核后升级为「删整个函数 + 调用点」）+ **S5 / F 层收官**（2026-10-11）

**靶**：`ApplyBaseMaterialInfoDefaults(MaterialRecipe &, const MaterialDefinition &, const bool overwrite_existing = false)`
（§F-4 记为"死参数：函数体从不读 `overwrite_existing`，唯一调用点传 `false`"；函数实际在 `MaterialRecipe.h:604`，行号漂移）。

**复核发现（比评审结论更强）**：唯一调用点在 `MaterialDefinitionRegistry.cpp:296`，而**紧邻的上一行**已经
`recipe.mtl_def_id = definition.definition_id;`。函数体是：

```cpp
if (recipe.mtl_def_id.empty())
    recipe.mtl_def_id = definition.definition_id;
```

⇒ 无论 `definition.definition_id` 是否为空，该调用**恒为 no-op**（非空 ⇒ 条件为假；空 ⇒ 赋回去的还是空）。
故处置**不是"删参数了事"**，而是删掉整个函数与那次调用 —— 参数问题随之消失。

### 15.1 落地

| 位置 | 改动 |
|---|---|
| `inc/hgl/mtl/MaterialRecipe.h` | 删函数本体（8 行，含 `overwrite_existing` 形参） |
| `src/ShaderGen/material_definition/MaterialDefinitionRegistry.cpp` | 删调用行（保留其上一行的直接赋值——那才是真正的来源） |
| `TestCSMIncrementalPass.cpp` | 新增 1 条禁复活针（文案写明"先证恒 no-op 再删，不是删参数了事"） |

### 15.2 验收（实测）

| 项 | 结果 |
|---|---|
| 全量 Release（先 purge 57 TU / 95 obj） | `rc=0` / 0 error / 0 MSB / **392 目标** |
| `ctest -L ecs` / `TestCSMIncrementalPass` | **12/12** / **1/1** |
| 门 | **43 PASS / 0 FAIL** |
| cook / 示例 | 38/0 / `PBRSpheres` rc=124、0 错误 |
| 残留 | `ApplyBaseMaterialInfoDefaults` / `overwrite_existing` 源码面归零（仅针条目文案保留） |
| 反证 | 函数加回 `MaterialRecipe.h` ⇒ `Test 11 Failed: … 'ApplyBaseMaterialInfoDefaults' 重新出现在 MaterialRecipe.h（禁复活）`；还原后 1/1 |

---

## 16. S5（F 层）收官小结

| 子批 | 内容 | 结果 |
|---|---|---|
| F-1（§12） | 5 个只写不读字段 + `item_index` 形参/12 处实参 + `CountQueueFamilies` | 全删 + 5 条禁复活针 |
| F-2（§13） | 22 个零调用函数 + 级联 `GetPassTypeName` / `GetNumericClassFromVkFormat` | 全删 + 4 条禁复活针 |
| F-3（§14） | `BlendMode` / `ResourceClass`+`ResourceRequirement` / `ResolutionRequest`+`GeometryCapability` / `kMeshVertexPassthroughMaxPrimitives`；`ShaderArtifactSPVExtension` 改回真源引用（含门里 3 处硬编码） | 5 删 1 改 + 1 条"必须存在"针 + 3 条禁复活针 |
| F-4（§15） | `ApplyBaseMaterialInfoDefaults`（复核为恒 no-op ⇒ 连函数带调用点删） | 删 + 1 条禁复活针 |

**F 层剩余遗漏项**：无（原清单逐条复核；`HasDescriptorSemantic` 归 S1、`ShaderArtifactSPVExtension` 归类为"改引用"）。
**计划内剩余任务**：`S3b`（`GLSLSamplerType` 与 `SamplerType` 收敛 + 3 个注册表盲区）、`S4`（D 层去双真值）、
`S6`（G 层顺序协议加守卫）、`S7`（E 层特例收敛，可选）。

---

## 17. 材质配置写错的处置口径落地：LOG 报错 + 走错误材质路径（2026-10-11，用户口径）

**用户口径（2026-10-11）**：发售用**全预编译 SPV**，届时不再有 shader 合成器 ⇒ sampler 配错这类问题**直接 LOG 报错**，
材质侧**走错误材质处理路径**（"也就是棋盘格渲染"）。

### 17.1 实测到的真实现状（与口径相反）

任何一份材质 TOML 配置写错，整个进程**立即中止**：`src/ShaderGen/material_definition/MaterialDefinitionRegistry.cpp:213`
把 `error_count > 0` 也当成致命条件，命中即 `GLogFatal(...) + std::abort()`。
指纹：cdb 抓到 `code c0000409 / Subcode 0x7 FAST_FAIL_FATAL_APP_EXIT`；因为 stdout 还压在缓冲里，
**门与 cooker 表现为"零输出 + 非零 rc"的静默早退**（此前 §14 反证时踩到过这个现象，当时误以为是工具偶发）。

### 17.2 落地

| 位置 | 改动 |
|---|---|
| `MaterialDefinitionFileRegistry::LoadFile` | 三类失败各自 LOG：`打不开 / 文件为空 / 读取失败`；**解析失败**打 `材质定义解析失败: <完整路径> (<ParseResultName>)`；**ID 重复**打 `材质定义 ID 重复: <ID> (file=<完整路径>)` |
| `MaterialDefinitionFileRegistry::LoadDirectory` | 有失败时补一行汇总：`材质库加载完成：成功 N 份、失败 M 份（失败项不注册 ⇒ 引用者走错误材质路径）` |
| `MaterialDefinitionRegistry` | **fatal 条件收窄**：只有"整库不可用"（`file_count == 0 || 已解析 == 0`）才 `GLogFatal + abort`；单份失败 ⇒ `GLogError` + 跳过 |
| `ShaderCodeModuleRegistry`（同族） | 5 处日志的**路径**格式串 `%s` → `%ls`（`OSString` 是宽字符；此前只打出首字符，例如 `E`） |

链路语义（落地后）：写错的材质**不注册** ⇒ 引用它的配方分类为 `MaterialErrorKind::UnknownMaterialDefinition`
⇒ 走 `kFallbackMaterialRules` 的保底材质 + 标记色（现状见 §17.4-a）。

### 17.3 验证（实测）

| 项 | 结果 |
|---|---|
| 全量 Release | `rc=0` / 0 error / 392 目标 |
| `ctest -L ecs` / `TestCSMIncrementalPass` / 门 / cook | **12/12** / **1/1** / **43 PASS / 0 FAIL** / 38/0 |
| 示例 | `PBRSpheres` rc=124、0 错误 |

**探针（把 `lit.material.toml` 的 `sampler` 改成 `"SamplerBogus"`）**：

- 日志（完整路径 + 原因，路径修 `%ls` 后正确）：
  `[MaterialDefinitionFileRegistry] 材质定义解析失败: E:\ULRE\ShaderLibrary\material\lit.material.toml (InvalidValue)`
  → `材质库加载完成：成功 13 份、失败 1 份（失败项不注册 ⇒ 引用者走错误材质路径）`
  → `[ShaderGen] 1 份材质定义加载失败（逐份原因见上）——这些定义不注册，引用它们的配方会走错误材质路径。`
- 门：**不再静默早退** ⇒ 35 PASS / 8 FAIL，其中 `[FAIL] S.material-definition-file-schema — material file registry bulk load failed`（可读的红），其余用例照跑。
- cooker：`cooked=35 failed=0 skipped=15`（照编其余，不中止）。
- 示例：打日志后**继续运行**（不再 abort）。
- 还原后：43 PASS / 38-0 / 0 错误，`SamplerBogus` 归零。

### 17.4 待办 / 待拍板

- **a. 保底材质接棋盘格**：**已完成（§18.2）** —— 两条错误种类改用 `builtin/checkerboard_3d`（只依赖 Position +
  GeometryOnly ⇒ 任意几何可画），`None` 仍为 `builtin/pure_color`；新增门 case `S.material-fallback-definitions-exist`。
  标记色仍不可见（棋盘格源不读 MaterialData），按用户口径**只留 TODO**（`MaterialRecipe.h` 规则表上方注释）。
- **b. 日志粒度到键/值**：**已完成（§18.3）** —— `ParseMaterialDefinitionFile` 回传 表/键/值/行号，
  日志报到配置项级别；顺带补了纹理声明形态、`material_data` 形态两个静默失败洞。
- **c. S3b-2 注册表归位**：**已完成（§18.1）** —— 三个盲区归位到枚举定义旁 + 2 条禁复活针；
- **本档剩余项去哪找**：S4 / S3b-3 / 棋盘格上色 与 ECS 待拍板、既存红已统一折进 `doc/backlog.md` 的「当前未结项索引」（2026-10-12）——本档不再单独维护清单。
  本轮又发现 5~6 个同类盲区（顶点节点模式枚举，全仓无 `Get*Name`），列为 **S3b-3** 待办。

---

## 18. 三个任务：S3b-2 注册表归位 / 错误材质接棋盘格 / 解析失败回传细节（2026-10-12，用户拍板）

用户当轮三项指令（原文）：①「棋盘格就先不管颜色吧！改让它支持颜色的事未来再说，现在仅留个注释说明未来要做就行了」
②「那就增加 ParseMaterialDefinitionFile 回传细节吧」③「S3b-2 可以做了」。执行顺序：S3b-2 → 棋盘格 → 解析细节（前一项独立、后两项都动公共头）。

### 18.1 S3b-2：三个注册表盲区归位（名字↔值只此一处）

| 枚举 | 归位处（枚举定义旁） | 删掉的本地单向解析器 |
|---|---|---|
| `MaterialVertexProviderPolicy` | `inc/hgl/mtl/MaterialRecipe.h` | `MaterialDefinitionFile.cpp::ParsePolicy` |
| `ShaderCodeModuleKind` | `inc/hgl/mtl/ShaderCodeModule.h` | `ShaderCodeModuleFile.cpp::ParseKind` |
| `MeshShaderMode` | `inc/hgl/mtl/MeshShaderMode.h` | 无（`ParseMeshShaderMode` 原地重写为走 `GetMeshShaderModeName`） |

形态：`Get*Name`（switch，枚举 → 名字）+ `Parse*`（线性查表，名字 → 枚举）+ `*Count = static_cast<uint32>(末值) + 1`
——**新增枚举值自动纳入**（不必补断言/补表项）。`MeshShaderMode` 的空串 = `VertexPassthrough` 缺省语义保留。
判据：`git grep -n "ParsePolicy(\|ParseKind("` 只剩 2 条**禁复活针**（`TestCSMIncrementalPass.cpp`）⇒ 本地单向解析器不得回来。
反证：把 `ParseKind(` 重新写进 `ShaderCodeModuleFile.cpp` ⇒ `Test 11 Failed: ShaderCodeModuleKind 的名字表已归位到枚举定义处（…，2026-10-11 S3b-2） -- 'ParseKind(' 重新出现（禁复活）`。

**本轮新发现（S3b-3 待办，同一形态，本次未动）**：`MaterialDefinitionFile.cpp` 里还有 **5 个本地单向解析器**
（`ParseVertexInput`/`ParseMapping`/`ParseOrientation`/`ParseScale`/`ParseProjection` + `ParseVertexNormalMode`）
对 `VertexInputMode`/`MappingMode`/`OrientationMode`/`ScaleMode`/`ProjectionMode`/`MaterialVertexNormalMode` 做单向映射，
这 6 个枚举**全仓没有 `Get*Name`**（`git grep -c Get*Name` = 0）⇒ 名字只存在于这些本地表里。归位需要动各自枚举所在的头
（其中几个属 `hgl::graph` 顶点节点侧，blast radius 比 S3b-2 大），故另起一批做。

### 18.2 错误材质接棋盘格（颜色留 TODO）

`kFallbackMaterialRules`（`inc/hgl/mtl/MaterialRecipe.h`）两条**错误种类**的定义 ID
由 `builtin/pure_color` 改为 **`builtin/checkerboard_3d`**；`None`（非错误默认）**仍是** `builtin/pure_color`
⇒ 门 `F.fallback-dimension-neutral`（钉无参回退 ID）与既有契约**不破**。

选 `checkerboard_3d` 的依据（不是拍脑袋）：它 `provider_policy = GeometryOnly`、顶点需求只有 `Position`、
`normal_mode = OptionalFaceFallback` ⇒ **无需 UV/法线即可在任意几何上画出来**（`checkerboard_2d` 走 NDC/LocalToWorldOnly，面向 2D 绘制）。
深度/阴影 pass **不受影响**：program 由 `SelectCurrentSceneRenderTemplateRequest` 按 **purpose** 选模板
（`ShadowCasterOpaque/Masked`），与本材质的 fragment 源无关。

新增门 case **`S.material-fallback-definitions-exist`**（数据驱动遍历整张规则表）：
① 定义 ID 必须能在材质库里解析；② 顶点需求里**几何输入**（`GeometryAttribute`）只允许 `Position`
（`ProducedSemantic` 是着色器内部产出，不依赖顶点数据 —— 第一版判据漏了这条，被自己的用例当场报红，
日志为 `fallback definition for kind 'none' must not require vertex attributes beyond Position`）；
③ `provider_policy == GeometryOnly`。

反证两条：① 规则 ID 改成 `builtin/checkerboard_typo` ⇒ 门报
`fallback definition for kind 'missing_material_source' is not registered: builtin/checkerboard_typo`；
② 规则 ID 改成合法的 `builtin/checkerboard_2d` ⇒ 门仍绿（它是合法的棋盘格材质），但
`TestRenderStrategyTable` 的**意图针**报红：`Test 6 Failed: 材质错误必须走棋盘格保底材质 builtin/checkerboard_3d`。
两道闸互补：门兜"打错字/定义不存在"，测试钉"拍板的那一款"。

颜色仍**看不到**：`checkerboard_{2d,3d}_source.glsl` 没有 `@ulre require Resource MaterialData`，
棋盘格两色写死灰度（`vec3(0.0)`/`vec3(0.35)`）⇒ 规则表里的 `marker_color` 照旧下发到材质数据行但无人读。
按用户口径**只留 TODO**（`MaterialRecipe.h` 规则表上方注释）：未来 ① 两套 checkerboard 源读 EmissiveSurface 标记色；
② 2D/3D 分派（届时回退 ID 需按绘制维度区分）；③ 门/契约同步。

### 18.3 `ParseMaterialDefinitionFile` 回传失败细节（表 / 键 / 值 / 行号）

**动因**（用户口径）：发售用全预编译 SPV、不再有 shader 合成器 ⇒ 材质配置写错只能靠**日志**定位，
所以失败必须报到"哪个表、哪个键、什么值"，而不是只说"某文件解析失败"。

机制（`MaterialDefinitionFile.h` 新增 `MaterialDefinitionFileData::ParseErrorDetail`：
`table`/`key`/`value`/`detail`/`line`）：

- 解析器内部失败点极多且都以 `return false` 收敛 ⇒ **不逐点改**，而是引入**诊断上下文** `ParseErrorContext`
  （文件内匿名命名空间 + `thread_local` 指针 + RAII `ParseErrorContextScope`）；上下文**不参与任何控制流判定**，
  只记**第一个**失败点（根因优先）。
- 记录点：① `ReadString/ReadBool/ReadFloat` 失败 ⇒ 键；② 各枚举/名字解析失败（`sampler`/`cull_mode`/`depth_compare_op`/
  `blend_src`/`blend_dst`/`transform` 五键/`provider_policy`/`normal_mode`/`requirements`/`material_data.type`/
  `mesh_shader.mode`）⇒ **键 + 值 + 期望提示**；③ 键白名单（`ValidateKnownKeys`，8 个段落各 `SetParseTable`）⇒ 键；
  ④ TOML **语法错误** ⇒ 行号 + 库原文（`toml::syntax_error::errors()`）。

> **订正（2026-10-12，S4b/D2 收尾时实测）**：③ 里「8 个段落各 `SetParseTable`」**当时并未落盘** ——
> 那批脚本在 `mesh_shader` 锚点上报错中止、重试又栽在 `rs.contains("pipeline")` 锚点，最终只写进 `render_state.pipeline` 一条；
> 后果是**诊断日志的表前缀是残留值**（实测：把 `double_sided` 写进 `[render_state]` 时报成 `resources.textures.double_sided`）。
> 已在 S4b/D2 批补齐 7 条（校验器内现共 **8** 条，实测计数），复验报 `render_state.double_sided` ✓。
> **教训**：`apply()` 里 `assert` 中止 ⇒ 该次调用**整体不写盘** ⇒ 脚本打印的那些 `ok:` 行**不代表整批落盘成功**；
> 落盘后必须按**结果计数 / 运行输出**复验，不能凭 apply 的打印。
- **顺带补了两个静默失败洞**（实测发现的）：纹理声明的形态契约（`name`/`sampler` 必须字符串、`required` 必须布尔）
  与 `material_data` 的形态检查原先直接 `return false` ⇒ 现在逐项报到键级
  （例如缺 `required` 现在报 `resources.textures.required（必须有布尔 required（显式声明，不取默认值））`）。

日志形态（`MaterialDefinitionFileRegistry::LoadFile`）：

```
[#10][MaterialDefinitionFile.cpp:1084] [MaterialDefinitionFileRegistry] 材质定义解析失败:
  E:\ULRE\ShaderLibrary\material\lit.material.toml (InvalidValue) resources.textures.sampler = "SamplerBogus"
  (未知采样器类型名（见 inc/hgl/common/TextureSamplerTypeDef.h）)
```

新增门 case **`S.material-definition-parse-detail`**（自造 TOML，3 个子项）：
① 名字写错 ⇒ `InvalidValue` + `key == "sampler"` + `value == "SamplerBogus"`；
② 未知键 ⇒ `UnknownKey` + `key == "bogus_root_key"`；③ TOML 语法错 ⇒ `line > 0`。

反证两条（都是当场真实出现过的红）：① 测试 TOML 缺 `required` 时（先于 sampler 失败）
报 `bogus sampler name must report key+value, got key='' value=''` ⇒ 促使我补上纹理形态的记录点；
② 去掉 `ValidateKnownKeys` 里的记录 ⇒ `unknown root key must report its name, got ''`。

### 18.4 验收（本轮最终状态）

| 项 | 结果 |
|---|---|
| 全量 Release | `rc=0` / 0 error / **392** 目标 |
| `ctest -L ecs` / `TestCSMIncrementalPass` | **12/12** / **1/1** |
| ShaderGen 门 | **45 PASS / 0 FAIL**（43 → +`S.material-fallback-definitions-exist` → +`S.material-definition-parse-detail`） |
| `ShaderCooker` | **38/0** |
| 示例冒烟 | `PBRSpheres` rc=124、0 错误 |

### 18.5 本轮自己踩的坑（供下次）

1. **blob 重铺会吃掉未提交改动**：编辑 helper 从 `git show HEAD:` 取内容重铺 ⇒ 把 `MaterialRecipe.h` 里
   **未提交**的 S3b-2 改动整块抹掉（编报 `ParseMaterialVertexProviderPolicy: identifier not found`）。
   正确口径：**内容取工作区、BOM/行尾取 blob**；只有"从干净形态重建"才用 blob 内容。
2. **反证假绿 = 二进制没重编**：改的是**头文件里的表**，MSBuild 不跟踪头依赖 ⇒ 门/测试仍跑旧二进制、反证"不红"。
   必须 `touch` 目标 TU（或按依赖 purge）后再反证 —— 本轮两次反证都栽在这上面，补 `touch` 后精确报红。
3. 锚点全是"我手抄的原文"，两处多打了一个 `)` 导致命中 0（`mesh_shader`/`pipeline`）⇒
   锚点一律从文件里 `repr()` 出来核对，别凭记忆抄。

---

## 19. S4 施工单：D 层去双真值（2026-10-12 建立，开工前）

> **范围**：批表 §196 原列 D1 / D2 / D4 + F-4。**F-4 已并入并在 `694ab2002` 完成**
> （`ApplyBaseMaterialInfoDefaults` 恒 no-op ⇒ 连函数带调用点删）⇒ **本施工单只剩 D1 / D2 / D4**。
> **§196 批表里的行号已随重构漂移，下面全部是本轮实测行号**（每条都写了复核方式）。
> 分批结论见 §19.4；边界见 §19.5。

### 19.1 D1 —— 「模板哪些槽必需」的双真值（模板层：最小、判据现成）

**取证（实测）**

| 角色 | 位置 | 说明 |
|---|---|---|
| 真源 | `src/ShaderGen/template/RenderTemplate.cpp:16-70` | 6 张槽表（`ForwardLitSlots` / `ForwardLitUnshadowedSlots` / `ForwardUnlitSlots` / `ShadowCasterOpaqueSlots` / `ShadowCasterMaskedSlots` / `SkySlots`），每槽一个 `required`；字段定义 `inc/hgl/mtl/RenderTemplate.h:83`（默认 true）；**全仓唯一显式 false** = `ShadowCasterOpaqueSlots` 的 `MaterialSourceProvider`（`RenderTemplate.cpp:54`） |
| 表驱动校验（**正确形态**） | `src/ShaderGen/template/RenderTemplate.cpp:290-300` | `if (slot.required && !request.FindModuleRoot(slot.role))` ⇒ `RenderTemplateValidationError::MissingRequiredSlot` |
| 第二处表达（**硬编码**） | `src/ShaderGen/template/FragmentTemplateComposer.cpp:492-500` | `if (id == ForwardUnlit && (output_policy_inc.empty() \|\| material_source_inc.empty() \|\| surface_inc.empty())) return false;` + `if (id == Sky && (sky_provider_inc.empty() \|\| surface_inc.empty())) return false;` |
| 耦合自证（同一事实要手工对齐的痕迹） | `RenderTemplate.cpp:32-34` | 注释："Composer 对 ForwardLit 系列一律要求 MaterialSource 与 NTB 非空，因此这两个 slot 必须存在——此前缺失会让该模板永远校验失败" |
| 回归工具 | `src/Tools/ShaderGen/ShaderDocumentRegression.cpp:148` / `:174` | 钉 `MissingRequiredSlot`；用 `ForwardUnlit` 造 unexpected slot 请求 |

**语义差异（决定不能"直接删分支"）**：表看的是「槽有没有 module root」，Composer 看的是「生成的 include 文本是否为空」——
后者额外覆盖「module 在、但 include 产出为空」这一类失败。⇒ 收口方式是**把知识收回表里**，不是删掉那一层判断。

**施工步骤**

1. `FragmentTemplateComposer.cpp:492-500` 的 2 个 `if (id == …)` ⇒ 换成**表驱动**：取当前模板的 slots，对 `required == true` 的槽调用既有 `BuildSlotIncludeText(...)`，**为空即失败**（保留原「include 非空」语义）。
2. 删掉那 2 个硬编码分支；`RenderTemplate.cpp:32-34` 的耦合注释改写为「必需槽由表声明，Composer 按表校验 include 非空」（**不复述被删分支原文**，避免污染禁复活判据）。
3. 复查 `*_inc` 局部变量：确认不再存在任何"按模板 ID 特判必需性"的旁路。

**判据（红/绿）**

- 绿：`ShaderDocumentRegression` rc=0（`MissingRequiredSlot` 仍红得出来）+ 门 45 PASS/0 FAIL + `ctest -L ecs` 12/12 + `ShaderCooker` 38/0 + 示例 0 错误。
- **反证**（必须打印预期的失败用例名与文案）：① 取任一模板某槽置 `required=false` 且不提供 ⇒ 改后**不再**被硬编码判死（改前死在这 2 个 if 上）；② 移除一个必需槽的 module ⇒ 仍报 `MissingRequiredSlot`。
- 规模：1 处改动；无 TOML / GPU 侧影响。

### 19.2 D2 —— 剔除方向的双真值（`double_sided` vs `cull_mode`）

**取证（实测）**

| 角色 | 位置 |
|---|---|
| `cull_mode`（管线键，粒度更细） | `inc/hgl/mtl/PipelineConfig.h:17`（`MaterialPipelineConfig`，默认 `VK_CULL_MODE_BACK_BIT`） |
| `double_sided`（覆盖键） | `PipelineConfig.h:68-69`（overrides 的 `has_double_sided` / 值）、`:59`（`ResolvedMaterialRenderState.double_sided`） |
| 覆盖点（两者汇合处） | `src/Vulkan/VKRenderPass.cpp:170-172`：`if (render_state.double_sided) render_state.pipeline_config.cull_mode = VK_CULL_MODE_NONE;` |
| 解析/键 | `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp`（render_state 段含 `double_sided`；pipeline 段含 `cull_mode`） |
| 哈希输入 | `inc/hgl/mtl/MaterialRecipe.h:669-671`：`h << overrides.has_double_sided << overrides.double_sided` |
| 门用例 | `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp:2451-2484`（5 处） |
| 影响面 | `double_sided` 共 **8 档**：`MaterialRecipe.h`(6) / `PipelineConfig.h`(4) / `MaterialDefinitionFile.cpp`(4) / `MaterialDefinitionRegistry.cpp`(2) / 门(5) / `VKRenderPass.cpp`(2) / `RenderPrimitiveCollectSystem.cpp`(1) |
| TOML 授权 | `grep -rn "double_sided\|cull_mode" ShaderLibrary/` ⇒ **0**（只有代码可达 ⇒ 收敛成本极低） |
| **保留**（独立规则，不动） | `VKRenderPass.cpp:166-168`「Lines 图元强制 cull off」——按**图元类型**而非材质语义 |

**可证问题**：TOML 可同时给两键且无一致性校验 ⇒ `cull_mode = "Back"` + `double_sided = true` 时，
作者写的 `cull_mode` 被 `double_sided` **静默覆盖**（渲染结果与配置字面不符）。

**施工步骤（建议：删 `double_sided` 全链，剔面语义只留 `cull_mode`）**

1. 把现有 `double_sided = true` 的**代码写入点**改写成显式 `cull_mode = VK_CULL_MODE_NONE`；
2. 删 overrides 的 `has_double_sided` / `double_sided`、`ResolvedMaterialRenderState.double_sided`、解析键与键白名单条目、`HashMaterialRecipe` 的两个输入项、`VKRenderPass.cpp:170-172` 的覆盖分支；
3. 门 `:2451-2484` 的用例改写为 **`cull_mode` 断言**（意图保留：override 生效 + `has_` 语义）；
4. 注释同步：`RenderPrimitiveCollectSystem.cpp:1079-1083` 里"仅 double_sided/cull 变化"的举例改成 `cull_mode` 说法（**机制不动**）。

**判据（红/绿）**

- `git grep -nw double_sided` 归零（注释里同样不得复述）；
- 门 45 PASS/0 FAIL（用例已换断言）+ `ctest` 12/12 + cook 38/0 + 示例 0 错误；
- **反证**：材质 TOML 写 `double_sided = true` ⇒ 必须是**未知键报错**（不得静默忽略）；
- ⚠️ **哈希变化声明**：删哈希输入后**所有**配方的 program 身份键都会变 ⇒ 一次性冷缓存重建（首次运行会重编 stage GLSL）。
  这条要写进当批验收说明，否则后续 bisect 会把它当成回归。

### 19.3 D4 —— 「recipe 已归一化」的两套消费纪律

**取证（实测）**

| 角色 | 位置 | 行为 |
|---|---|---|
| 检查口 | `inc/hgl/mtl/MaterialRecipe.h:608-615` `IsRecipeNormalized` | 5 个 `has_*` 全真 |
| fail-fast 侧 | `src/Vulkan/VKRenderPass.cpp:152-157` | 未归一化 ⇒ `GLogError` + 返回 `nullptr` |
| **静默侧** | `inc/hgl/mtl/MaterialRecipe.h:593-605` `GetNormalizedRecipeRenderState` | **不检查** `has_*`，直接拷 overrides |
| 唯一调用者 | `VKRenderPass.cpp:159` | 先检查后取值 ⇒ 当前无可达错值路径，但**不变量两套纪律** |

**订正**：评审档记「静默返回全零」，实测更准确的说法是**返回结构体默认值 + 未写回的 overrides**
（`alpha_cutoff = 0.5`、`cull_mode = BACK`、`double_sided = false`…）——作者定义的默认值可能不同 ⇒ **错值**而非零值，性质相同。

**施工步骤**：把「检查 + 取值」合并成一个口
`bool TryGetNormalizedRecipeRenderState(const MaterialRecipe &, ResolvedMaterialRenderState &out)`（内部即 `IsRecipeNormalized` 判定），
调用方沿用现有报错文案；删 `GetNormalizedRecipeRenderState`（**不留兼容重载**）。

**判据 / 反证**：构造未归一化 recipe 调新口 ⇒ 返回 `false`（改前静默给默认值）；
反证挂门或 `TestCSMIncrementalPass`，**失败用例名与文案必须打印出来**才算数。

### 19.4 分批（每批自编译 + 测试绿 + 反证）

| 批 | 内容 | 为什么这么切 |
|---|---|---|
| **S4a** | **D1** | 只在模板层，不碰 recipe 语义/哈希；回归工具现成 |
| **S4b** | **D4 + D2** | 同属 recipe/normalize 语义（§196 批表亦建议"单独一批并附反证"）；D2 改哈希输入，与 D4 的语义收敛同批验收最省事 |

若要更小步：**D4 可单独一批**（2 个符号 + 1 个调用点）。
**不建议**把 D1 与 D2/D4 合并 —— 三者无共享文件，合并只会模糊"哪一处导致契约变化"。

### 19.5 边界（不做什么）

- 不碰 GPU 侧行载荷 / 描述符（D 层是纯 CPU 侧语义收敛）；
- 不保留旧键别名 / 双写期（零兼容）；
- 不动 `RenderPrimitiveCollectSystem` 那条"配方内容变了 ⇒ 重建管线"的机制（只改注释里的举例）。

### 19.6 S4a 执行记录（2026-10-12）

**处置**：`src/ShaderGen/template/FragmentTemplateComposer.cpp` 里那两处「按模板 ID 硬编码必需槽」的 8 行**整删**；
原位置留 5 行说明（指向唯一执行点，不复述被删分支原文）；表侧耦合注释（`RenderTemplate.cpp`）改写为
「ForwardLit 的 defines 恒为 1 ⇒ 表中这两个槽必须保持 required=true；必需性判定唯一执行点 = `BuildAllSlotIncludeText`」；
`src/ecs/tests/TestCSMIncrementalPass.cpp` 加 **2 条禁复活针**（`output_policy_inc.empty()` / `sky_provider_inc.empty()`）。

**开工复核（比施工单预期更省）**：施工单原计划"把硬编码换成表驱动循环"，实测**不需要新代码** ——
`BuildAllSlotIncludeText`（同文件 `:340`）**已经是表驱动**（`if (definition.slots[index].required) return false;`），
且在同一函数里**先于**那 8 行执行；而 `BuildSlotIncludeText` 的 `*_inc.empty()` ⟺ `ResolvedInclude()` 返回空
⟺ 同一次 `FindModuleRoot(role)` 查找（`ResolvedInclude` 已含 `include_path` 非空判定）⇒ 两处**同条件**，
对现有表（ForwardUnlit 三槽、Sky 的 slot 全 `required=true`）那 8 行**永不触达** ⇒ 处置定为整删。

**验收（全绿）**：`shader_document_regression`（钉 `MissingRequiredSlot`）/`scene_render_template_resolver_regression`/
`shader_document_production_regression`/`ShaderCookSmoke` **4/4 Passed**；门 **45 PASS / 0 FAIL**；
`ctest -L ecs` **12/12**（含 2 条新针）；`TestCSMIncrementalPass` **1/1**；`ShaderCooker` **38/0**；
示例 `CascadeShadowMap` / `TextDraw` **0 错误**。残留判据：`git grep "output_policy_inc.empty()\|sky_provider_inc.empty()"`
仅剩 2 条针本身。

**红的样子**（针反证，实测原文）：

```
Test 11 Failed: 必需槽强制已收归模板表（BuildAllSlotIncludeText，2026-10-12 S4a）：
Composer 里不得再出现按模板 ID 硬编码的必需槽判断
-- 'output_policy_inc.empty()' 重新出现在 FragmentTemplateComposer.cpp（禁复活）
```

**诚实边界**：硬编码块与表的分歧场景（表标 `required=false` + 该模块缺失 + 模板属 ForwardUnlit/Sky）目前**没有任何生产者**
（表中无此条目，请求路径也不会缺该模块）⇒ 该分歧无法被现有工具触发，故本轮的红只有"针"这一条 + 上述静态不可达性论证。
若将来要这道保险，应加门 case「表中 optional 槽缺失 ⇒ 组合器不得失败」，并先造出生产者。

**未做**：S4b 的 D2 —— 见 §19.2 施工步骤（D4 已完成，见 §19.7）。

### 19.7 S4b/D4 执行记录（2026-10-12）

**处置**：`inc/hgl/mtl/MaterialRecipe.h` 的 `GetNormalizedRecipeRenderState`（静默拷 overrides）**整删**，
换成 **`TryGetNormalizedRecipeRenderState(const MaterialRecipe &, ResolvedMaterialRenderState &out)`** ——
内部先过 `IsRecipeNormalized`，未归一化直接返回 `false`（**不留兼容重载**）。
为让内联可见性成立，`IsRecipeNormalized` 上移到 `TryGet…` 之前（两者相邻、语义不拆）。
调用点 `src/Vulkan/VKRenderPass.cpp`：「先判定 + 后取值」收成**一次调用**，报错文案/位置与 `return nullptr` 语义不变。
门 `Y1.resolved-material-render-state` 扩两条断言：① 未归一化 recipe 必须被拒；
② 归一化后与 `ResolveMaterialRenderState(definition, recipe)` **等价**（逐字段 + `HashResolvedMaterialRenderState` 一致）。
**1 条禁复活针**：串取**声明形态** `ResolvedMaterialRenderState GetNormalizedRecipeRenderState(` ——
刻意避开新名字（`TryGetNormalizedRecipeRenderState(` 会包含旧名子串 ⇒ 取调用形态会误撞，这是既有教训）。

**验收（全绿）**：门 **45 PASS / 0 FAIL**（`Y1` PASS）；`ctest -L ecs` **12/12**；`TestCSMIncrementalPass` **1/1**；
`ShaderCooker` **38/0**；示例 `PBRSpheres` / `TextDraw` **0 错误**。
残留判据：`git grep -n GetNormalizedRecipeRenderState` 只剩 `TryGet…` 四处调用/声明与针本身。

**红的样子**（把判定短路成 `if (false && …)` 后，实测原文）：

```
[FAIL] Y1.resolved-material-render-state
  - un-normalized recipe must be rejected by TryGetNormalizedRecipeRenderState
```

**D2 已完成**，见 §19.8。

---

### 19.8 S4b/D2 执行记录（2026-10-12）

**处置**：`double_sided` **全链删除**，剔面语义唯一表达 = `MaterialPipelineConfig::cull_mode`：

| 档 | 改动 |
|---|---|
| `inc/hgl/mtl/PipelineConfig.h` | 删 `ResolvedMaterialRenderState.double_sided`、`MaterialRenderStateOverrides.has_double_sided/double_sided`、`HashResolvedMaterialRenderState` 对应输入 |
| `inc/hgl/mtl/MaterialRecipe.h` | 删覆盖应用分支、`IsRecipeNormalized` 合取项、`TryGetNormalizedRecipeRenderState` 的赋值、`HashMaterialRecipe` 两个输入 |
| `MaterialDefinitionFile.cpp` | 删 `render_state.double_sided` 解析块 + 键白名单条目 ⇒ **写该键 = 未知键报错** |
| `MaterialDefinitionRegistry.cpp` | 删归一化写回两行 |
| `src/Vulkan/VKRenderPass.cpp` | 删「double_sided ⇒ cull NONE」覆盖分支（**Lines 图元**那条按图元类型的独立规则保留） |
| `VKCommandBufferRender.cpp` / `RenderPrimitiveCollectSystem.cpp` | 两处注释改写，**不复述被删符号** |
| 门 `Y1.resolved-material-render-state` | 5 处断言改写成 **`cull_mode`**（定义默认 `NONE` + 覆盖为 `BACK_BIT`），三条原意图逐条保留；D4 块的等价断言去掉该合取项 |
| `TestCSMIncrementalPass.cpp` | **4 条禁复活针**（PipelineConfig.h / MaterialRecipe.h / MaterialDefinitionFile.cpp / VKRenderPass.cpp） |

**⚠️ 哈希变化声明**：`HashMaterialRecipe` / `HashResolvedMaterialRenderState` 的输入集合变了 ⇒ **所有**配方的
program 身份键都会变（一次性冷缓存重建；首次运行会重编 stage GLSL）。这是设计内的键空间收敛，**不是回归**。

**验收（全绿）**：**整树清 obj（1809 个）+ `*.module.json`** 后全量重编 ⇒ `rc=0 / 0 error / **392** 目标`
（本轮删了 struct 字段集合，按仓库规矩必须整树清）；门 **45 PASS / 0 FAIL**（`Y1` PASS）；`ctest -L ecs` **12/12**；
`TestCSMIncrementalPass` **1/1**；四个 ShaderGen 回归工具 **4/4**；`ShaderCooker` **38/0**；
示例 `PBRSpheres` / `CascadeShadowMap` / `TextDraw` **0 错误**。残留判据：`git grep -n double_sided -- src inc` **只剩 4 条针**。

**红的样子（四条，均为实测原文）**

① 针（把该名写回任一被针文件）：
```
Test 11 Failed: 剔面语义唯一表达 = MaterialPipelineConfig::cull_mode（S4b/D2，2026-10-12）：
double_sided 全链已删，不得复活 -- 'double_sided' 重新出现在 PipelineConfig.h（禁复活）
```
② 解析键（材质 TOML 写 `[render_state]` + `double_sided = false`）：
```
[MaterialDefinitionFileRegistry] 材质定义解析失败:
  E:\ULRE\ShaderLibrary\material\lit.material.toml (UnknownKey)
  render_state.double_sided (未知键（拼写错误或已删字段残留）)
```
③ 门（把定义默认 cull 改回 `BACK_BIT`）：
```
[FAIL] Y1.resolved-material-render-state
  - definition render defaults were not preserved by resolution
```
④ D4（把归一化判定短路成 `if (false && …)`）：
```
[FAIL] Y1.resolved-material-render-state
  - un-normalized recipe must be rejected by TryGetNormalizedRecipeRenderState
```

**顺带修正**：§18.3 记录的「键白名单 8 个段落各 `SetParseTable`」当时**并未落盘**（只有 `render_state.pipeline` 一条）
⇒ 诊断日志的表前缀是残留值；本批补齐 7 条（校验器内现共 8 条）并复验 ② 输出为 `render_state.double_sided` ✓。详见 §18.3 的订正框。

**S4 收口**：S4a（§19.6）/ S4b-D4（§19.7）/ S4b-D2（本节）全部完成 ⇒ 材质链评审计划的**计划内批次全部结清**。

## 5. 本档边界

- 评审（§1–§4）为只读静态取证（grep / 行号 / 结构）；**§6 是其后 S1 批的落地记录**（该批改了引擎代码，验收数字见 §6.2）。
- 视觉验证：S1 只做删除，未逐像素目视；运行时证据为帧数与错误计数（§6.2）。
- 未覆盖：`inc/hgl/graph/**` 的材质表头与 `src/Vulkan` 的 sampler/pipeline 创建细节（仅按调用面引用）、`doc/` 文档与代码的一致性（另属 `repo-docs-sync` 范畴）。
- 数字均为本机 `HEAD=73fa8ddae` 实测；行号会随重构漂移，**符号名是稳定锚点**。
- 子代理的枚举结论已逐条复核：其中 2 处计数与 1 处定性被本档修正（`sampling` 命中 30 而非"仅定义+解析"；`MaterialMeshIndexTables` 消费点 3 处而非 2；`RenderTemplateSlot.required` **是活的**、硬编码必需槽属**重复表达**而非"表未使用"）。
