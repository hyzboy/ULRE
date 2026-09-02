# ShaderGen 受控组合模板迁移计划

## 1. 目标

将当前 ShaderDocument-first 的生成链，从“`MaterialDefinition` 指定模块路径，
`GenericMaterialBuilder` 和 `CompositorAssembler` 以条件分支决定组合”迁移为：

```text
ECS/render preparation: 模板、全局/场景质量和光照 module roots
MaterialDefinition:    材质能力、允许的管线 profile、资源包络
MaterialRecipe:        某实例的资源绑定、LOD、允许的质量选择
BuildRequest:          几何格式、设备 profile、实际 pass
        ->
FixedShaderVariantKey
        ->
FixedPipelineVariantTable
        ->
resolved stage recipes
        ->
ShaderDocument
```

目标是固定前向渲染模板的受控组合，而非 Shader Graph。ECS 选择完整 template/module
roots；ShaderGen 只验证和装配。最终标准 Forward Lit 主流程固定为：

```text
SurfaceData
  -> direct lighting * shadow
  + ambient lighting * AO
  + emissive
  -> output policy
```

PBR、Fake PBR、Blinn-Phong、SH、EnvMap、IBL、GI、RG8/RGBA16F x2 NTB、静态/骨骼
等均是已批准 slot 的实现；特例通过新增版本化模板实现。每一个模板 slot 必须有实际
module，资源缺失即为错误，不得用 no-op fallback 伪装为合法组合。

## 2. 现有能力与迁移映射

| 现有能力 | 当前位置 | 迁移后位置 | 处理方式 |
|---|---|---|---|
| `MaterialDefinition.definition_id/name` | `MaterialRecipe.h` | Definition identity | 保持不变 |
| `ubo_requirements`、`texture_slot_decls`、`sampler_names`、私有 SSBO | `MaterialDefinition` | Definition resource envelope | 保持为资源能力上限 |
| `code_module_requirements` | `MaterialDefinition` | 共享/必需 module roots | 保持，禁止承载 template slot 选择 |
| `fragment_source`、surface/material/NTB module path | `MaterialDefinition` | 迁移兼容字段 | 逐步由 profile/table 替代，最后删除 |
| `compositor_surface/blend/pass` | `MaterialDefinition` | family/default policy | 迁移为默认 family/coverage policy |
| `mesh_shader_mode`、`VertexShaderNodeConfig` | `MaterialDefinition`/`MaterialRecipe` | geometry profile + geometry strategy input | 保留现有实际生成器 |
| `material_lod` | `MaterialRecipe` | quality/profile preference | 扩展为有限 quality tier，不存 GLSL path |
| textures、SSBO asset binding | `MaterialRecipe` | instance binding | 完全保留，不参与模板控制流 |
| physical-device profile、geometry format、purpose override | Build request | runtime/build context | 完全保留 |
| manifest、descriptor/interface/output/coverage contract | 当前 contract 层 | resolved variant validation | 保留并成为唯一 ABI 真源 |
| `ShaderDocument`、artifact/store、GLSLCompiler | document/compile | 输出与离线编译 | 不改职责 |

当前 `MaterialRecipe` 已正确地区分“实例资源绑定”与 Definition 的静态能力；本计划不把
shader module path、template source 或任意 GLSL 注入带回 Recipe。

## 3. 最终数据职责

### 3.1 `MaterialDefinition`

Definition 描述材质**允许做什么**，不描述每次应如何拼 shader：

```text
definition_id
pipeline_family
allowed_profile_mask
default_quality_tier
resource envelope
vertex semantic requirements
geometry capability envelope
default render state
```

建议新增而非立即替换的字段：

```text
FixedPipelineFamily pipeline_family
FixedShaderProfileMask allowed_profiles
FixedShaderProfile default_profile
```

原 `fragment_*_module` 字段仅在迁移期作为 legacy override 读取。新 Definition 只选择
profile，不保存 lighting、ambient、NTB 或 surface 的任意路径。

### 3.2 `MaterialRecipe`

Recipe 继续描述“该实例绑定什么资源”，只增加受限质量偏好：

```text
quality_tier: Default | Low | Medium | High
```

`NormalizeRecipe()` 根据 Definition 的 `allowed_profiles` 将 quality tier 归一化为有效
profile。若请求档位不可用，选择 Definition 明确指定的 fallback，并记录诊断；不得默默
改用不兼容的 shader 组合。

### 3.3 `MaterialDefinitionBuildRequest`

Build request 仍提供 geometry format、physical-device profile 和实际 purpose。这些是构建
上下文，不应写入 Definition 或 Recipe。新增的 variant 选择结果应存入构建内部 plan，
避免扩大频繁传递的 request ABI。

### 3.4 `ResolvedFixedShaderVariant`

新增内部、只读的解析结果，加入 `GenericMaterialBuildPlan`：

```text
FixedShaderVariantKey key
const FixedPipelineVariant *variant
const StageRecipe *mesh_recipe
const StageRecipe *fragment_recipe
ResolvedMaterialRenderState render_state
```

它是 Fragment、Mesh、descriptor、interface 和 artifact key 的共同输入，禁止各阶段
重新解析 profile。

## 4. 目标固定表

第一版使用 C++ 静态只读表；不应先设计 TOML DSL。

```text
FixedPipelineVariantTable:
  key:
    family, purpose/pass, quality tier, geometry profile, alpha mode
  selects:
    mesh recipe
    fragment template
    surface provider
    direct-light provider
    shadow provider
    ambient provider
    AO provider
    lighting model
    output policy
  declares:
    required module capabilities
    descriptor/interface/output/coverage policy
    deterministic slot order
    template/profile version
```

首批只登记以下家族：

| Family | 首批 profile |
|---|---|
| `ForwardLit` | PBR+IBL+RGBA16F2、PBR+SH+RG8、FakePBR+SH、Blinn-Phong+EnvMap |
| `ForwardUnlit` | PureColor、VertexColor、Texture |
| `ShadowCaster` | Static/Skinned + Opaque/Masked |
| `Sky` | Constant、EnvMap、Atmosphere |
| `Decal` | 一个固定投影模板和有限 blend policy |
| `PostProcess` | SSAO、DOF 分别独立 family/template |

不登记的组合即为不支持，而不是让 resolver 拼凑“最接近”的路径。

## 5. 分阶段计划

### Phase 0：冻结迁移基线

**目标**：在不改生产代码前固定现有行为。

1. 为所有 builtin/TOML material capture Mesh/Fragment `ShaderDocument`。
2. 记录每个 fixture 的序列化 GLSL hash、stage key、program key、descriptor contract、
   interface contract、output contract、SPV artifact hash。
3. 固定现有 `GenericMaterialBuildPlan` 的五阶段输出作为对照。
4. 将当前 `fragment_source`、`fragment_surface_module`、
   `fragment_material_source_module`、`fragment_ntb_module` 的实际值列为迁移映射表。

**完成条件**：每个当前 material 都有可重复运行的基线 fixture；无基线的 material 不进入
后续迁移。

### Phase 1：引入类型与静态 table，不接管生成

**目标**：建立类型系统和单一选择入口，零 shader 输出变化。

1. 新增 `FixedPipelineFamily`、`FixedShaderProfile`、`FixedShaderQualityTier`、
   `FixedShaderVariantKey` 和 `FixedPipelineVariant`。
2. 新增 `FixedPipelineVariantTable` 与 `ResolveFixedPipelineVariant()`。
3. 让 table 先返回当前 Definition 已有的 fragment/module path，确保旧组合结果可逐项映射。
4. `MaterialDefinition` 新增 `pipeline_family`、`allowed_profiles`、`default_profile`；
   file loader 支持可选字段，缺失时由旧字段构造临时 legacy profile。
5. `GenericMaterialBuildPlan` 保存 resolver 结果，但仍由现有 Compositor 路径发射。

**完成条件**：同一输入的输出 GLSL、contract、stage/program key、SPV artifact 与基线一致。

### Phase 2：将 Definition 和 Recipe 切换为 profile 选择

**目标**：停止让普通 Definition 指定 fragment 模块路径。

1. `NormalizeRecipe()` 解析 `quality_tier -> FixedShaderProfile`，并验证 Definition 允许该
   profile。
2. `MaterialDefinition` 的资源能力仍按现有 `ubo_requirements`、texture/SSBO/module
   manifest 声明；profile 只确定需要哪些能力。
3. 将新的 builtin/TOML Definition 改为 family/profile 写法。
4. 保留 `fragment_*` 路径字段为 legacy fallback；当 profile 与 legacy path 同时存在时，
   要求 table 解析结果逐项一致，否则构建失败。
5. 将 profile 和 template version 纳入 build-context/stage key；与旧路径等价期间保持
   原 key 输入，首次有意变更时通过版本号使 cache 安全失效。

**完成条件**：所有 builtin Definition 已使用 profile；legacy path 仅保留给尚未迁移的
file Definition。

### Phase 3：Fragment 模板组合

**目标**：把 `CompositorAssembler` 的选择逻辑迁入 table，保留其可靠的 Document 发射代码。

1. 新增 `FragmentStageRecipe`：模板 ID、ordered slots、provider module ID、coverage/output
   policy。
2. 将 `GetSurfaceLightingConfig`、`SurfaceType × PassType -> skeleton`、默认 PBR/flat/sky
   模块路径迁入 variant table。
3. 将现有 `CompositorAssembler` 拆为：
   - `FragmentStageRecipeResolver`：从 `ResolvedFixedShaderVariant` 返回 recipe；
   - `FragmentStageComposer`：根据 recipe 将已解析 contract 写入 `ShaderDocument`。
4. 先将现有 forward/depth/sky `main` 文本原样保留为 versioned template main。
5. 对 Forward Lit 建立固定 slot：surface、direct、shadow、ambient、AO、lighting、output。
   每一个 slot 必须有实际实现；不使用 shadow/AO 的路径采用不同版本化模板。

**完成条件**：`GenericMaterialBuilder` 不再知道 lighting/skeleton/module path；每个
fragment document 的 block source 能追溯到 template 和 slot module。

### Phase 4：契约化 slot capability

**目标**：允许受控替换 PBR/Blinn、SH/IBL/GI、RG8/RGBA16F2，而不接受错误组合。

1. 扩展 `ShaderCodeModuleDefinition` metadata：`slot_role`、输入/输出 capability、
   template compatibility。
2. 定义稳定 `SurfaceData`、`DirectLightData`、`AmbientLightData`、`ShadowFactor` 的 GLSL
   contract version。
3. 让 surface provider 声明能否提供 normal/metallic/roughness/opacity；
   lighting model 声明所需字段和 ambient capability。
4. 让 ambient provider 声明 diffuse/specular IBL/GI 等 capability；
   PBR+IBL、Blinn+EnvMap 等合法关系由 table 显式登记。
5. descriptor manifest 继续从 selected provider roots 构建；模板不可私自添加资源。

**完成条件**：slot 缺失、重复、capability 不匹配、descriptor/interface 不匹配都在 GLSL
生成前报告带 module/template 名称的诊断。

### Phase 5：Mesh 策略接入

**目标**：让 Mesh 选择也由同一 variant 解析结果控制，但保留算法安全边界。

1. 新增 `MeshStageRecipe`：`GeometryProfile -> MeshShaderMode`、input strategy、
   local-deform strategy、skinning strategy、local-to-world strategy、varying policy。
2. 将现有 `MeshShaderAssembler` 中默认 input path 与 mode 选择移到 recipe。
3. 保留以下 C++ 实现和验证：physical-device invocation clamp、VertexPassthrough 三角形
   整除、LineQuad/CharQuad main、CharQuad 专用资源。
4. 从 fragment recipe 反推 required varyings，并以现有 `MaterialStageInterface` 生成
   mesh/fragment 共享 contract。

**完成条件**：Static、Skinned、LineQuad、CharQuad 的现有 GLSL 与 artifact 基线一致；
不支持的 family/geometry 组合在 resolver 阶段失败。

### Phase 6：独立 family 与 legacy 收尾

**目标**：避免后处理和特殊 pass 扩大 Material 主路径。

1. ShadowCaster、Sky、Decal 使用独立 template family；共享的 environment resource 可以
   复用，但不共享 Forward Lit 的 surface/main。
2. SSAO、DOF、tone mapping 建立独立 fullscreen template/cooker entry，不接入
   `MaterialDefinition`、`MaterialRecipe` 或 mesh contract。
3. 全部 Definition file 完成 profile 化后，删除 `fragment_source` 和各
   `fragment_*_module` legacy 字段、loader 逻辑与 compatibility test。
4. 当 `CompositorAssembler` 只剩委托时删除 façade，保留 `FragmentStageComposer`。

**完成条件**：`GenericMaterialBuilder` 只编排 resolver、contract、stage composer；
任何特殊流程均能定位到明确 template，而不是 builder/compositor 条件分支。

### Phase 7：Cooker 与发布准备

**目标**：把有限变体集变成可枚举的离线 artifact 清单。

1. ShaderCooker 遍历 `FixedPipelineVariantTable` 的批准 entries，而非依赖运行期偶然触发。
2. 输出包含 variant key、template/profile version、module hashes、SPV、descriptor/schema
   metadata 的 artifact manifest。
3. cooker 验证每个批准组合至少有一个代表性资源 binding fixture。
4. SPV-only package 仅在 renderer freeze 时独立启用；本计划不提前关闭开发期 GLSL
   fallback。

**完成条件**：完整 cooker 能生成和验证所有批准 variant，且 artifact manifest 可用于
后续 SPV-only packaging。

## 6. 每阶段验证

每个 Phase 都执行：

1. Debug 与 Release 的 ShaderGen target build。
2. `ShaderDocumentRegression`、`ShaderLegacyDocumentCompare`、
   `ShaderResourceSchemaRegressionGate` 与 `ShaderCookSmoke`。
3. 对受影响 fixture 比较 Document block 顺序/source、GLSL hash、stage/program key、
   descriptor/interface/output contract 和 SPV artifact hash。
4. 在两个独立 artifact store 运行 cooker，比较路径、文件数、payload hash 与 metadata。
5. 确认 ReadOnly artifact load 成功；刻意缺失/损坏 artifact 应产生明确诊断且不 fallback。

允许 shader 文本变化仅限于有意修改模板或 profile version 的 Phase；此时必须同步更新
fixture、key/cache version 和变更原因。

## 7. 风险控制

| 风险 | 控制措施 |
|---|---|
| table 成为新的双真源 | 先由 table 返回当前 Definition 路径；迁移后 Definition 不再保存同类 path |
| profile 产生组合爆炸 | 仅登记批准条目，禁止按枚举自动笛卡尔积 |
| template 偷偷引入资源 | resource manifest 只由 selected provider metadata/contract 生成 |
| PBR 与 ambient provider 错配 | slot capability + variant table 双重校验 |
| Mesh/Fragment varying 不一致 | 统一由 resolved variant 的 interface policy 生成 |
| hash/cache 意外复用 | template/profile/module version 进入 key 或显式使 cache 失效 |
| 特例重新污染 builder | 特例只能新增 template family 或 versioned template |

## 8. 不应在本计划中做的事情

- 不引入 Shader Graph、通用模板语言、动态表达式或用户可注入 `main()`；
- 不把 Recipe 变成 shader 结构描述；
- 不删除现有 contract/manifest/artifact store；
- 不将 Mesh mode 的 Vulkan 限制、ABI 或复杂 body 配置化为任意 GLSL；
- 不与 SPV-only release 切换合并实施。
