# ShaderGen 架构与优化重构报告

## 1. 结论

ShaderGen 的 ShaderDocument-first 迁移已完成：Material、Mesh、Fragment 都先生成
`ShaderDocument`，GLSL 只是离线序列化格式，随后由 GLSLCompiler 产生 SPV。

当前主要问题不再是文本拼接或 Legacy 兼容，而是**生成决策分散在 C++ 条件分支中**：

- `SurfaceType`、`PassType`、coverage、mesh mode 到 stage 骨架的映射不集中；
- Compositor 的模板、默认模块和 main wiring 被 `CompositorAssembler` 持有；
- Builder 仍包含 pass 重写、surface lighting 和 descriptor 裁剪策略；
- `MaterialShaderEmitter` 同时有通用 Document fragment 和 Material ABI 文本规则；
- ShaderCodeModule 已数据化模块能力，但尚未描述“某个 stage 的哪些角色应如何组合”。

推荐方向不是让模板任意生成 GLSL，而是建立**可验证、可哈希、可回归的 Stage Recipe
组合层**：

```text
MaterialDefinition + MaterialRecipe
        -> ResolvedMaterialPlan
             -> MeshStageRecipe / FragmentStageRecipe
             -> StageTemplate + ordered module roles
             -> StageDocumentComposer
             -> ShaderDocument
             -> GLSL serialize -> offline compiler -> SPV/cache
```

这能把大多数“材质组合差异”收敛为配表，同时保留 C++ 对 Vulkan 限制、ABI 和复杂算法
生成的强校验。

## 2. 当前代码结构

`src/ShaderGen/CMakeLists.txt` 将库划分为以下管线角色：

| 模块 | 主要文件 | 职责 |
|---|---|---|
| `document` | `ShaderDocument.cpp`、`DocumentFragmentBuilder.cpp` | block 有序 IR、来源、诊断、序列化与 hash |
| `compile` | `MaterialShaderCompiler`、`MaterialShaderEmitter`、`GLSLCompiler`、artifact store | 编译执行、已解析状态的发射、SPV/cache |
| `glsl_module` | registry、metadata、manifest、capability resolver | GLSL 代码模块与能力/依赖/资源图 |
| `material_definition` | registry、TOML loader | 材质定义加载和注册 |
| `contract` | descriptor、output、coverage、stage interface | ABI、资源、varying、输出与 hash 不变量 |
| `meshgen` | MeshShaderAssembler、mode/template headers | Mesh stage 的模式化发射 |
| `compositor` | `CompositorAssembler` | Fragment skeleton、模块角色、契约与 main 组装 |
| `builder` | `GenericMaterialBuilder`、Vertex ABI/descriptor builder | Definition 到编译计划的编排与求解 |
| `release` | release/readonly/audit shell | 开发期发布预检，不参与当前生成主链 |

实际 Material 主链是：

```text
ShaderProgramManager
  -> GenericMaterialBuilder
      -> purpose / coverage / interface / vertex ABI / manifest / descriptors
      -> GenerateMeshShaderDocument
      -> CompositorAssembler::AssembleDocument
      -> ShaderBuildContext + stage/program identity
  -> MaterialShaderCompiler
      -> MaterialShaderEmitter::BuildMaterialStageDocument
      -> GLSL serialization
      -> GLSLCompiler / ShaderArtifactStore
```

运行时目前 SPV 优先读取 artifact；开发期未命中才允许 GLSL 编译 fallback。SPV-only
发布开关不应成为本报告重构的前置条件。

## 3. `MaterialShaderEmitter` 与 `CompositorAssembler`

二者职责不重叠，当前**不能删除** `CompositorAssembler`。

`MaterialShaderEmitter` 的正确边界是“已解出的 layout/contract/manifest 到 Document
fragment 的无决策发射器”。它适合：

- Material SSBO、index table 和 sampler macro 的 ABI 声明；
- compile define、code module manifest 的稳定顺序写入；
- 最终 stage Document 的资源叠加。

`CompositorAssembler` 当前同时承担：

1. `surface + pass + coverage` 到 fragment skeleton 的选择；
2. lighting、sky、NTB、material source、surface function 等模块角色选择；
3. fragment input、output、coverage contract 和 main wiring 发射。

应拆分其决策与发射，而不是把它复制进 `MaterialShaderEmitter`：

```text
FragmentStageRecipeResolver      // 选择模板、角色和策略
StageDocumentComposer            // 按模板 slot 写 Document
MaterialShaderEmitter            // Material ABI fragments
FragmentMainStrategy             // 少量 C++ main wiring 策略
```

迁移完成后，`CompositorAssembler` 可以先变为仅委托这些组件的 façade，确认无调用方后
再删除。

## 4. 硬编码热点

### 4.1 应优先配表的生成选择

| 位置 | 当前硬编码 | 建议数据模型 |
|---|---|---|
| `CompositorAssembler.cpp` | `SurfaceType × PassType -> Forward/Depth/Sky skeleton` | `FragmentStageRecipe` |
| `CompositorAssembler.cpp` | PBR、flat、sky、NTB 等默认 include 路径与 slot 顺序 | `ModuleRoleRegistry` + recipe ordered slots |
| `GenericMaterialBuilder.cpp` | `SurfaceType -> SurfaceLightingConfig` switch | recipe resolver 的 surface policy 表 |
| `GenericMaterialBuilder.cpp` | DepthOnly/ShadowDepth 到 effective pass 的重写 | `PurposeVariantPolicy` |
| `GenericMaterialBuilder.cpp` | coverage 对资源、模块、pass 的影响 | policy predicates |
| `MeshShaderAssembler.h` | mode 对默认模块、topology、layout 的选择 | `MeshStageRecipe` |
| `MaterialShaderEmitter.cpp` | 可复用资源声明片段的格式 | ABI resource fragment registry |

### 4.2 必须保留 C++ 的约束

以下内容可由 recipe 参数化，但不应交给任意外部模板：

- Vulkan/physical-device mesh 输出上限和 workgroup 计算；
- VertexPassthrough 的三角形整除约束；
- CharQuad、LineQuad 的特定 ABI 和算法主体；
- descriptor set/binding 的最终分配与冲突检测；
- shader interface、coverage、output contract 的一致性验证；
- stage/program key、artifact hash 与 cache 兼容性。

原则是：**配置描述合法选择；C++ 验证合法性并执行不可替代的算法。**

## 5. 目标数据模型

### 5.1 `StageRecipe`

每个 recipe 只声明组合，不直接携带不受控的拼接逻辑：

```text
id, version
stage, purpose/pass, surface family, mesh mode, coverage predicates
template id
ordered slots: Version, Extension, Define, Resource, Interface, Module, Function, MainBody
module roles: vertex_input, stage2, stage3, surface, material_source,
              ntb, lighting, sky, alpha
required contracts: descriptor, interface, output, coverage
main strategy id
```

`id + version + resolved role module IDs + resolved contract hash` 必须参与 stage key 或独立
recipe hash；不得依赖 registry 的遍历顺序。

### 5.2 `StageTemplateRegistry`

以 template ID 替代 `ForwardSurface/DepthOnly/Sky` enum 及嵌套 switch。每个模板需声明：

- 允许的 stage 和最小 GLSL/extension 要求；
- 各 slot 的 block kind、顺序与可选性；
- 需要的 contract；
- 允许的 module role；
- template version 和稳定 hash。

第一阶段可继续由 C++ 静态注册，以保证字节不变；稳定后再转为受 schema 校验的
TOML/GLSL asset。模板应是**受限 slot 模板**，不是具有逻辑、循环或任意表达式的通用
文本引擎。

### 5.3 `ModuleRoleRegistry`

现有 `ShaderCodeModuleRegistry` 负责能力、依赖和资源需求。新增 role registry 负责：

```text
role + surface/purpose/capability predicate
        -> ordered candidate modules
        -> priority/conflict/fallback policy
```

例如 `lighting.algorithm` 在 Lit/Forward 解析为 PBR，在 Unlit/Forward 解析为 flat。
这样 module path 不再同时存在于 Builder 和 Compositor。

## 6. 推荐重构顺序

### Phase A：引入模型，不改行为

1. 新增 `ResolvedMaterialPlan`，使 GenericMaterialBuilder 输出完整的 stage 选择结果。
2. 新增 C++ 静态 `StageRecipeRegistry`，注册当前三种 fragment skeleton 和三种 mesh
   mode，不修改现有文本常量、block 顺序或模块路径。
3. 为 recipe、role resolution、template version 生成稳定 hash。
4. 扩充回归：按 builtin/TOML definition 比较 Document blocks、provenance、序列化 GLSL、
   stage/program key、descriptor/interface/output contracts 和 SPV artifact。

### Phase B：Fragment 决策与发射分离

1. 把 `CompositorAssembler` 中 skeleton 选择、surface function switch、默认 include
   迁至 `FragmentStageRecipeResolver`。
2. 提取 `StageDocumentComposer`，让其按 recipe slots 写入 Document。
3. 保留现有 main/coverage 发射代码为 `FragmentMainStrategy`，先确保逐字节等价。
4. 删除 GenericMaterialBuilder 中 `GetSurfaceLightingConfig`，由 resolver 提供结果。

### Phase C：资源和变体 policy 收敛

1. 将 depth/shadow descriptor/module 裁剪定义为 `PurposeVariantPolicy`。
2. 将 Material SSBO、index table 等稳定声明注册成 ABI resource fragments。
3. policy 与 contract 共用同一个 `ResolvedMaterialPlan`；模板不得隐式添加或删除资源。
4. 合并 manifest/provider graph 中分散的固定容量，改由 schema/profile limits 定义。

### Phase D：Mesh recipe 化

1. 配表化 `MeshShaderMode -> topology/capacity/default module/main strategy`。
2. 将默认 vertex input、stage2、stage3 module path 迁至 role registry。
3. 保留 mesh mode strategy 和 device-limit validator 为 C++。
4. CharQuad 保留显式 builtin recipe/strategy，避免以通用模板掩盖 ABI 特殊性。

### Phase E：资产化和删除 façade

1. 将稳定 recipe/template 转为版本化 TOML/GLSL metadata，C++ static registry 只作
   builtin fallback。
2. loader 校验 schema、角色、capability、contract、循环依赖与确定性排序。
3. 所有调用改为 `StageRecipeResolver + StageDocumentComposer` 后删除
   `CompositorAssembler` façade。

## 7. 验证与安全门禁

每一个 phase 都必须保持：

- 同输入的 GLSL 字节、Document block 顺序和来源稳定；
- source digest、stage/program key、schema/metadata 稳定；
- 输出 SPV 字节相同，或有明确 recipe/template version 引起的安全 cache 失效；
- Debug/Release 构建、ShaderDocumentRegression、ShaderLegacyDocumentCompare、
  ShaderResourceSchemaRegressionGate、ShaderCookSmoke 通过；
- 两个独立 artifact store 的路径、文件数、hash、metadata 对比一致；
- 缓存 registry/recipe 的排序完全显式，绝不依赖 map 注册或遍历顺序。

## 8. 优先级建议

| 优先级 | 工作 | 收益 | 风险 |
|---|---|---|---|
| P0 | StageRecipe 静态 registry 与回归 fixture | 建立单一真源、无行为改变 | 低 |
| P0 | Fragment recipe resolver | 移除 Builder/Compositor 双真源 | 中 |
| P1 | PurposeVariantPolicy | 收敛 depth/shadow 分支，降低 ABI 失配风险 | 中 |
| P1 | StageDocumentComposer | 统一 shader 组合结构，减少重复发射代码 | 中 |
| P1 | Mesh recipe 化 | 降低 mode 扩展成本 | 中 |
| P2 | TOML template/recipe 资产化 | 新增 shader 变体无需改 C++ | 中高 |
| P2 | 删除 CompositorAssembler façade | 清晰命名和依赖方向 | 低 |

## 9. 不建议做的事情

- 不把所有 GLSL 全部改成自由文本模板；这会削弱 descriptor、interface 与 Vulkan 限制的
  类型安全。
- 不将 shader 资源的选择留给模板隐式决定；资源必须先经 contract 求解。
- 不在同一次改动中改变 recipe、GLSL 字节格式、hash 输入和 cache 格式。
- 不因本次架构整理提前关闭离线 GLSL 编译或开发期 fallback。
- 不将 `MaterialShaderEmitter` 与 `CompositorAssembler` 粗暴合并；先拆“决策”和“发射”，
  再以调用收敛决定文件删除。

## 10. 固定前向渲染器的简化方案

### 10.1 适用边界

当前目标不是通用渲染框架或任意材质图，而是一套受控的 Vulkan 前向管线：

- 主材质：PBR + IBL，并可降级为 fake PBR 或 Blinn-Phong；
- 天光/IBL：低、中、高质量的固定实现；
- 表面输入：高质量 RGBA16F x2 到低质量 RG8 的有限 NTB 编码；
- 形态：静态网格、骨骼动画、天空球、ShadowMap、贴花；
- 后处理：SSAO、DOF 等独立全屏 pass。

这个边界意味着 ShaderGen 不需要成为“任意模块图求解器”。更适合的模型是：

```text
固定 Pipeline Family
    + 有限 Quality Profile
    + 有限 Geometry/Input Profile
    + 有限 Pass Variant
    -> 已批准的 Shader Variant
```

新增能力优先增加一条已批准的 profile 或 recipe，而不是让材质定义任意指定 GLSL 路径、
main 函数、include 顺序和资源集合。

### 10.2 推荐的固定 family

| Family | 需要的变体轴 | 不应承担的职责 |
|---|---|---|
| `ForwardLit` | lighting model、IBL quality、NTB format、skinning、alpha mode、shadow receive | 天空、贴花、后处理 |
| `ForwardUnlit` | vertex color/texture、alpha mode、skinning | 光照和 IBL |
| `ShadowCaster` | alpha mask、skinning、geometry mode | material lighting、IBL、颜色输出 |
| `Sky` | sky quality/profile | 任意 material provider、NTB、skin |
| `Decal` | decal blend/profile、normal write policy | mesh material 的所有组合 |
| `SSAO` / `DOF` | quality/profile、render target format | MaterialDefinition、Mesh/Compositor 组合 |

`SSAO`、`DOF` 和其他 post-process 不应进入 MaterialShaderCompiler 或
GenericMaterialBuilder。它们应是独立的 fixed fullscreen pipeline：一份固定
ShaderDocument/GLSL 模板配合少量 quality define，最终单独 cooker 成 SPV。

### 10.3 用 profile 代替自由 module 选择

建议将当前可自由填写的 `fragment_material_source_module`、
`fragment_ntb_module`、lighting 和 sky module 路径，收敛为少量枚举：

```text
LightingModel: Unlit, BlinnPhong, FakePBR, PBR
IBLProfile: Off, AmbientOnly, DiffuseSpecular
NTBEncoding: None, RG8, RGBA16F2
SkinningMode: Static, MatrixPalette
AlphaMode: Opaque, Masked, Blend, Dither, A2C
GeometryProfile: Mesh, LineQuad, CharQuad
```

`MaterialDefinition` 只声明其允许的 profile 集合；`MaterialRecipe` 只选择一个经验证
的 quality/profile，而非携带 GLSL 模块路径。仅内置 bootstrap 或开发实验性 definition
可保留显式模块 override，并必须以 feature flag 隔离。

这会显著缩小无效组合空间。例如：

- `PBR + RGBA16F2` 可以只允许 `DiffuseSpecular` IBL；
- `BlinnPhong` 可以禁止高成本 specular IBL；
- `Sky`、`ShadowCaster`、`SSAO` 无 NTB 选项；
- `CharQuad` 只能使用 Unlit；
- `ShadowCaster` 仅保留 Static/Skinning 与 Opaque/Masked。

### 10.4 使用 variant key，而不是通用模板解释器

可定义一个紧凑的 `FixedShaderVariantKey`：

```text
family | pass | lighting | ibl | ntb | skinning | alpha | geometry | profile version
```

它应由 C++ 的 `ResolveFixedPipelineVariant()` 生成，并同时返回：

- `StageRecipe`；
- 固定的 descriptor/interface/output contract；
- 允许的 module role 集合；
- pipeline state；
- 稳定 hash 与诊断。

`StageRecipe` 可先是 C++ 的静态表。表项指向已有 `ShaderCodeModule` 和受控 main strategy；
只有稳定后再迁为 TOML metadata。这样仍具备数据驱动的好处，但不会引入自由 DSL、复杂
解析或难以穷举的运行时组合。

### 10.5 Shader 组合的最佳简化层次

推荐把组合限制在三个层次：

1. **固定 stage 模板**：每个 family/pass 仅一份 Mesh/Fragment/Fullscreen 模板；
   模板包含 `ShaderDocument` block slot，不带循环、条件语言或任意 include。
2. **有限 profile slot**：由 variant key 在固定表中选择 lighting、IBL、NTB、skinning
   provider 和 main strategy。
3. **契约驱动 fragment**：descriptor、varying、output 仍由现有 contract 系统生成，
   但只能填入模板已声明的 slot。

不要把每一小段 GLSL 都模板化。稳定算法应直接存放为标准 `.glsl` module；模板只描述其
插槽顺序与可选 profile。`ShaderDocument` 负责最终顺序、来源、版本和 hash。

### 10.6 对当前模块的直接影响

| 当前模块 | 固定管线后的职责 |
|---|---|
| `GenericMaterialBuilder` | 解析 definition/recipe，调用 variant resolver；不再自己做 surface/pass/lighting 路径决策 |
| `CompositorAssembler` | 短期保留为 `FragmentStageComposer`；长期只根据已解析 recipe 填 Document |
| `MaterialShaderEmitter` | 继续负责 Material ABI、SSBO、binding/index table 等稳定 fragment |
| `ShaderCodeModuleRegistry` | 保留 capability/依赖校验；module 选择改由 fixed recipe 表，而非任意 definition 路径 |
| `MeshShaderAssembler` | 仅保留 Mesh/LineQuad/CharQuad 三种受控 strategy 和设备限制验证 |
| `MaterialDefinition` | 收敛为 family、允许 profile、资源能力和 render-state envelope |
| `ShaderCooker` | 遍历有限 variant manifest，离线编译所有批准组合并写 artifact manifest |

### 10.7 建议的实施顺序

1. 定义 `FixedShaderPipelineFamily` 与上述 quality/input enums，先只覆盖现有 Unlit、
   Lit、Sky、Shadow 路径。
2. 建立静态 `FixedPipelineVariantTable`，让每个已有 builtin definition 映射到一条表项；
   只替换选择逻辑，保持当前 GLSL 字节和 stage key 不变。
3. 将 `GetSurfaceLightingConfig`、Compositor skeleton switch 和默认 module path 收敛到
   表项；GenericMaterialBuilder 不再直接知道 PBR/flat 模块路径。
4. 将 `MaterialDefinition` 的 module path 字段标记为迁移兼容字段，并改由 profile
   选择；确认全量 builtin/TOML definition 迁移后删除。
5. 为 cooker 生成明确的 variant manifest，覆盖所有批准组合；SPV-only 发布时直接以该
   manifest 为 package 输入。
6. 将 SSAO、DOF、tone mapping 等后处理独立为 `PostProcessShaderLibrary`，不复用
   MaterialDefinition/mesh contract。

### 10.8 最终建议

对个人开发的固定前向渲染器，最有价值的简化不是继续扩展通用 `ShaderCodeModule`
自由组合能力，而是将其限制为**固定 family 内的实现模块库**，由一个小而明确的 variant
表统一选择。

这样既保留 PBR/IBL、低配光照、NTB 精度、骨骼、阴影、天空和贴花的演进空间，也可让
组合数、缓存 key、Cooker 输出和回归矩阵保持可枚举、可理解、可维护。

## 11. 已确认的组合模板边界

ShaderGen 不实现 Shader Graph，也不提供可由 MaterialDefinition 任意改写的执行图。
它是一个受控的模板装配器：配置只能选择预定义 slot 的实现，不能改变模板的控制流或
资源/接口契约。

标准逐像素前向光照模板固定为：

```glsl
SurfaceData surface = GetSurfaceData(input);

DirectLightData direct = GetDirectLight(input);
AmbientLightData ambient = GetAmbientLight(input);
ShadowFactor shadow = GetShadowFactor(input);
float ao = GetAmbientOcclusion(input, surface);

vec3 color =
    EvaluateDirectLighting(surface, direct) * shadow.value +
    EvaluateAmbientLighting(surface, ambient) * ao;

WriteFragmentOutput(surface, color);
```

各 slot 的职责和可替换边界如下：

| Slot | 可替换实现 | 固定边界 |
|---|---|---|
| `GetSurfaceData` | PBR texture、纯色、顶点色、贴花、RG8/RGBA16F x2 NTB 解码 | 输出固定 `SurfaceData` |
| `GetDirectLight` | 方向光、局部光列表、无直射光 | 输出固定 direct-light 输入 |
| `GetShadowFactor` | 无阴影、PCF ShadowMap、后续其他 shadow filter | 输出标准化遮蔽因子 |
| `GetAmbientLight` | 常量、天光、SH、EnvMap、IBL、GI/probe | 输出固定 ambient/environment 输入 |
| `GetAmbientOcclusion` | 常量 AO、材质 AO、SSAO、GTAO | 输出 `[0, 1]` 遮蔽因子 |
| `Evaluate*Lighting` | PBR、Fake PBR、Blinn-Phong | 只消费已声明 capability 的输入 |
| `WriteFragmentOutput` | HDR color、深度、透明/A2C 输出 | 由 pass/output contract 决定 |

`AmbientLightData` 是环境光来源与 lighting model 的解耦点。它可以包含 diffuse
irradiance、specular environment、BRDF LUT、GI radiance 等字段；具体 lighting model
只消费其声明支持的字段。`SurfaceData` 同理应声明 normal、base color、metallic、
roughness、emissive、opacity 等 capability，而非让各模块以隐式全局变量耦合。

模板以外的特例不是生成器 feature：

1. 若特例仍遵守上述 slot contract，增加一条受控 profile/table entry。
2. 若特例需要不同控制流、不同 contribution 合成或不同资源生命周期，新增或修改一个
   明确版本化的模板。
3. 不向 GenericMaterialBuilder、Compositor 或 MaterialShaderEmitter 添加“仅此材质”
   分支，也不允许 definition 注入自由 GLSL main。

因此 generator 的配置职责限定为：

```text
Template ID
  + Surface provider ID
  + Direct-light provider ID
  + Shadow provider ID
  + Ambient provider ID
  + AO provider ID
  + Lighting model ID
  + Output policy ID
```

模板负责标准流程；registry 负责检查 slot capability、descriptor/interface contract 和
确定性顺序；ShaderDocument 负责可审计地按 block 输出。这样既允许 PBR + IBL、
Blinn-Phong + IBL、PBR + SH/GI 等合法搭配，又不会让组合机制演化为 Shader Graph。
