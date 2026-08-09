# ULRE 数据驱动材质与 Shader 生成架构

> 状态：设计评审稿  
> 日期：2026-08-09  
> 适用范围：Material Definition、Material Recipe、多决策域 Material LOD、Shader Code Module、Shader Program 选择、VS/MS/FS 生成、Visibility Buffer Tile 着色选择、资源解析与加载、Descriptor Contract、Pass Output、Shader Artifact  
> 当前阶段：从 0 到 1，不保留旧资产格式兼容  

## 1. 文档目标

本文定义 ULRE 材质与 Shader 系统的目标架构，并给出从现有实现迁移到目标架构的重构计划。

ULRE 面向使用严格资产规格的中小规模团队。材质系统不是开放式 Shader 编程环境，
也不允许上层任意改写完整渲染流程。目标是一条由引擎控制、允许替换固定模块、
能够根据 Material、Geometry、设备能力和渲染目的确定最终程序的生成流水线。

本架构需要同时支持：

1. 当前 Vulkan Vertex Shader + Fragment Shader 路径。
2. 当前 VBO 顶点输入和 VS/FS Varying。
3. Material Definition 中面向未来 Material LOD 的完整配置。
4. Material Recipe 提供资源候选，但不强制所有资源都被当前程序使用。
5. 根据 ECS 批准的最终 Shader Program 或 Prepared Program Set 延迟加载真实需要的资源。
6. Forward、Depth Only、Shadow、VBuffer 和部分 Post Process。
7. 未来取消 VBO，迁移到 SSBO Geometry + Mesh Shader。
8. 未来 Visibility Buffer 下按 Tile/像素分布从 Prepared Set 中进一步选择着色 Profile。

本文不要求一次完成 Mesh Shader 迁移，但当前设计不能把 VBO 结构固化为长期材质 ABI。

## 2. 产品定位与非目标

### 2.1 产品定位

开发者负责的内容接近受约束的材质表达：

1. 声明材质可能提供哪些逻辑资源和配置。
2. 为 Recipe 绑定可用资产。
3. 选择或提供受支持的 Material Source、NTB、Lighting 等模块。
4. 将材质表达收敛到引擎定义的标准结构和函数签名。

引擎负责：

1. 根据 Material Definition、Recipe、Geometry、设备能力和 Shader Program Purpose
   选择 Material Program 与模块。
2. 完成模块依赖闭包和 Provider 匹配。
3. 得到当前程序实际需要的 Geometry semantic、Varying、Descriptor 和 Output。
4. 生成 Shader Stage，编译为 SPIR-V，并创建 ShaderProgram。
5. 在程序确定后生成资源加载计划和运行时绑定计划。
6. 由独立渲染执行系统决定队列、排序、Blend、Depth、Cull 等执行策略。

因此，本架构追求的是**受控组合能力、可验证契约和确定性生成**，不是无限扩展能力。

### 2.2 非目标

本文不计划：

1. 建立任意节点图或通用 Shader AST。
2. 允许材质资产直接指定 Vulkan binding、VBO location 或 SPIR-V 接口。
3. 让 Material Recipe 直接持有 Vulkan 对象。
4. 让 Opaque、Transparent 等执行分类天然产生不同 Shader。
5. 在当前重构中解决所有 Vulkan Varying 上限与压缩问题。
6. 为不存在的旧资产格式建立长期兼容层。

## 3. 关键术语

### 3.1 Material Definition

`MaterialDefinition` 是一个材质的完整定义，描述该材质在不同配置、Material LOD、
Shader Program Purpose 和设备条件下**可能使用**的能力与资源。

它是材质级别的配置全集，不是某个最终 Shader Program 的精确资源清单。

Material Definition 可以包含：

- 作者层 Surface Intent，例如 HumanSkin、ReptileSkin、AmphibianSkin、Leather、
  Wood、Stone、Metal。
- Surface Intent 对应的高质量配置和允许的降级策略。
- Material Program 候选。
- Material LOD 配置。
- 可选的 Material Source、NTB、Lighting、Coverage 和 Output 模块选择。
- 材质可能使用的纹理、UBO、SSBO 和参数声明。
- 默认渲染配置。
- 每个候选程序的适用条件和优先级。

Definition 中出现一个资源不代表最终程序一定使用它。只有完成 Program 选择和模块需求
闭包后，才能得到当前程序的真实资源需求。

### 3.2 Material Recipe

`MaterialRecipe` 是上层用户对一个材质实例或材质配置提供的声明式输入。

Recipe 表达：

- 使用哪个 Material Definition。
- 上层当前能够提供哪些逻辑资源。
- 每个逻辑纹理或数据槽对应哪个资产。
- 材质参数和作者层覆写。
- 请求的 Material LOD、质量或其他选择提示。

Recipe 不保证其中所有资源都会被最终程序使用，也不保证 Recipe 一定能够成功生成程序。

Recipe 中的资源绑定是**可用资源集合**，不是立即加载指令。目标架构下，Recipe 解析阶段
只检查逻辑绑定、资源元数据和能力，不创建 Vulkan 资源。

### 3.3 Surface Intent 与 Resolved Surface Profile

`SurfaceIntent` 表达材质在作者和资产层面的真实表面类型，例如：

- HumanSkin。
- ReptileSkin。
- AmphibianSkin。
- Leather。
- Wood。
- Stone。
- Metal。

它是 Material Definition 的稳定语义，不应因为用户降低画质而被改写。

`ResolvedSurfaceProfile` 表示在当前设备、质量等级和 Shader Program Purpose 下真正采用的
表面实现。例如：

```text
High:
  HumanSkin     → SkinSSS
  ReptileSkin   → ReptileSkinLighting
  Wood          → WoodFiberLighting
  Metal         → MetalSurfaceLighting

Medium:
  HumanSkin     → StandardPBR
  ReptileSkin   → StandardPBR
  Wood          → StandardPBR
  Metal         → StandardPBR

Low:
  HumanSkin     → BlinnPhongFakePBR
  ReptileSkin   → BlinnPhongFakePBR
  Wood          → BlinnPhongFakePBR
  Metal         → BlinnPhongFakePBR
```

多个不同 Surface Intent 可以收敛到同一个 Resolved Surface Profile。收敛后只要模块图、
接口和编译期 feature 相同，就必须允许共享同一个 Resolved Material Program 和
ShaderProgram，不能因为来源 Material Definition 不同而各自编译一份。

### 3.4 Material Program

`MaterialProgramDefinition` 是 Material Definition 内可被选择的一个程序候选。

它描述：

- Material LOD 或质量等级。
- 目标 Resolved Surface Profile。
- 适用的 Shader Program Purpose。
- 设备或质量条件。
- 根模块集合。
- 必需与可选的 Recipe 资源能力。
- 选择优先级和显式 fallback 关系。

一个 Material Definition 可以拥有多个 Material Program。最终只选择满足当前上下文的
候选；候选可以引用共享 Surface Profile 和共享模块定义，不表示该 Definition 独占最终
Shader。没有候选满足时必须明确失败。

### 3.5 Shader Program Purpose

`ShaderProgramPurpose` 表示为什么要生成这份 Shader，而不是如何执行 draw。

建议的最小集合：

- `ForwardColor`
- `DepthOnly`
- `ShadowDepth`
- `VBufferWrite`
- `VBufferShade`
- `PostProcess`

它决定：

- 哪些模块需要参与。
- 哪些数据需求可以裁剪。
- Fragment Shader 是否需要颜色输出。
- Output Contract 的结构。

它不直接决定：

- Render Queue。
- Opaque 或 Transparent 分类。
- 排序策略。
- Blend、Depth、Cull、MSAA 等完整执行状态。

上述内容由独立的 Render Execution / Pipeline 系统处理。

### 3.6 Shader Code Module

Shader Code Module 是拥有稳定 ID、正式元数据和固定边界的 GLSL 代码单元。

模块声明：

- `requires`：模块需要的 semantic、资源、能力或其他模块输出。
- `provides`：模块能够提供的 semantic、标准结构或函数入口。
- `dependencies`：必须组合的其他模块。
- `conditions`：有限且可验证的适用条件。
- `entry`：对外公开的标准函数入口。

模块不直接指定最终 descriptor binding、VBO location 或临时运行时资源地址。

### 3.7 Resolved Contract

Resolved Contract 是完成 Program 选择、模块选择、需求闭包和能力匹配后产生的规范化结果。

它描述当前 Shader Program **实际使用什么**，而不是 Definition 或 Recipe **可能提供什么**。

## 4. 两类 Vulkan Location

当前讨论中必须区分两个不同的 location 空间：

1. **Vertex Attribute Location**
   - 描述 VBO 顶点输入与 Vertex Shader 输入变量的映射。
   - 当前属于 Geometry Vertex Format / Vertex Input ABI。
   - 未来采用 SSBO Geometry + Mesh Shader 后可以消失。

2. **Inter-Stage Varying Location**
   - 描述 VS 或 Mesh Shader 输出到 Fragment Shader 输入的接口。
   - Mesh Shader 不会天然消除这一接口。
   - 只要仍使用 Fragment Shader，就仍需保证生产端与消费端一致。

本阶段不需要为 Location 上限和极限压缩过度设计。当前目标是：

- 禁止 VS 和 FS 各自分配 location。
- 使用统一 semantic contract 保证接口一致。
- Vertex Attribute 的物理映射保持可替换，为取消 VBO 做准备。
- Inter-Stage Varying 可以暂时使用全局稳定 location 注册表。

未来若需要紧凑分配，只替换 semantic 到物理 location 的映射策略，不改变模块使用的
semantic 身份和 Resolved Contract。

## 5. 当前主要问题

### 5.1 VS 与 FS 接口来自两套事实来源

当前 VS Varying 主要由 C++ 配置或 TOML 决定，FS Varying 又可能由 Compositor 和
`@ulre require ProducedSemantic` 决定。

可能产生：

- FS 声明输入但 VS 没有输出。
- VS 与 FS 为同一 semantic 分配不同 location。
- Pass 裁剪后两侧保留的 semantic 集合不同。

### 5.2 Definition、Recipe 与最终资源需求未明确分层

Definition 和 Recipe 都可能包含最终程序不使用的资源。如果在 Program 选择前加载所有
Recipe 纹理，会造成：

- Material LOD 未确定时加载错误资源集。
- Depth Only 或 Shadow 加载不需要的光照资源。
- 无法生成 Shader 的材质仍然发起资源加载。
- 资源身份不必要地污染 Shader 缓存。

### 5.3 Surface、Lighting、Coverage 与 Output 职责混合

如果材质数据、光照、Alpha Test 和 Attachment 输出固化在某个
`main_forward_*.frag.glsl` 中，会不断复制：

- Unlit/Lit 骨架。
- Shadow Mask 逻辑。
- Dither 逻辑。
- VBuffer 特殊输出。

### 5.4 Shader 生成目的与渲染执行分类混合

Opaque 和 Transparent 在很多情况下可以使用完全相同的 Shader 代码，差异存在于：

- 渲染队列。
- 排序。
- Blend State。
- Depth Write。

如果这些执行分类进入 Shader permutation，会制造没有代码差异的重复 Shader。

### 5.5 NTB 与 Geometry 输入策略耦合

法线和切线可能来自 VBO、SSBO、纹理、程序化生成或导数。外层材质流程只应消费统一
NTB 结果，不应知道 Provider 的实际存储方式。

### 5.6 缓存身份边界不清

Shader 代码、Pipeline 执行状态和运行时材质资源是三类不同身份。它们不能继续依赖
同一个笼统的 Recipe hash 或 Materialization hash。

## 6. 设计原则

### 6.1 区分“可能提供”与“实际需要”

- Definition：材质可能拥有的完整能力和资源声明。
- Recipe：上层当前可提供的逻辑资源。
- Selected Material Program：本次选择的程序候选。
- Resolved Contract：最终程序实际需要的能力。
- Resource Acquire Plan：为满足最终程序需要加载的资产。

只有 Resolved Contract 可以作为生成 Shader 接口的权威输入。

### 6.2 先选择程序，再加载资源

目标流程必须是：

```text
Definition + Recipe metadata + Geometry + Device + Purpose
    → 选择 Material Program
    → 解析模块和 Contract
    → 验证必要逻辑资源
    → 生成 Resource Acquire Plan
    → 加载实际需要的资源
```

Program 选择和 Contract 解析阶段原则上不执行资源 I/O，也不依赖 bindless handle 等
运行时分配结果。

### 6.3 需求与能力分离

模块只声明“需要什么”。Definition、Recipe、Geometry 和 Provider 声明“能提供什么”。
Resolver 负责匹配。

### 6.4 单一事实来源

每类事实只能有一个权威来源：

- Material 能力全集：Material Definition。
- 上层可用资源：Material Recipe。
- 作者表面语义：Surface Intent。
- 当前质量实现：Resolved Surface Profile。
- 当前请求解析结果：Material Resolution Result。
- 可共享有效程序：Resolved Material Program。
- 模块依赖闭包：Resolved Module Graph。
- 当前实际接口：Shader Interface Contract。
- Shader 代码身份：Shader Variant Contract。
- 当前资源加载集合：Resource Acquire Plan。
- 运行时绑定：Material Binding Plan。
- 渲染执行状态：Render Execution / Pipeline Contract。

Material Definition 不拥有 ShaderProgram。它只参与解析并得到一个
`EffectiveMaterialProgramKey`；任意其他 Definition 解析到相同有效身份时必须复用缓存结果。

### 6.5 先完成需求闭包，再生成任何 Stage

不能先生成 VS，随后在 FS 中发现新的需求。

正确顺序是：

```text
Program Selection
    → Module Graph Resolution
    → Requirement Aggregation
    → Provider Resolution
    → Contract Canonicalization
    → 同时生成所有 Shader Stage
```

### 6.6 使用需求 DAG，不固化为严格线性流水线

材质逻辑具有阶段关系，但不应强制所有程序执行完整链路。

例如：

- Forward Color 需要 Material Source、Surface、Lighting、Coverage 和 Color Output。
- Masked Shadow 只需要产生 Alpha 的 Material Source、Coverage 和 Depth Output。
- Opaque Depth Only 可以完全不执行 Material Source 和 Lighting。
- Unlit 不需要真实光照，但仍实现标准 Lighting 输出接口。

### 6.7 显式失败，显式 fallback

以下情况必须产生结构化错误：

- Material Program 无候选满足。
- 必需 Recipe 资源未提供。
- Provider 无法满足需求。
- semantic、资源或函数接口冲突。
- Output Contract 无法满足。
- 模块图存在循环依赖。

只有 Definition 或 Program 明确声明 fallback 关系时，Resolver 才能尝试下一个候选。
禁止无记录地切换到默认 Shader。

### 6.8 不保留旧资产兼容

项目处于从 0 到 1 阶段。重构期间可以存在短期代码适配器，用于分阶段验证当前生成结果，
但最终实现不保留：

- 旧 TOML schema。
- DirectInclude 模式。
- 旧 Varying bool 配置。
- FixedMaterialDef 核心编译路径。
- 新旧 Compositor 双路径。

现有测试资产和示例资产直接更新到最终格式，不建设长期转换工具。

## 7. 总体架构

```text
Material Definition
    - Surface Intent
    - Program candidates
    - Material LOD definitions
    - Possible resource declarations
    - Default policies
            +
Material Recipe
    - Available logical asset bindings
    - Author overrides
    - Requested LOD / quality hints
            +
Geometry Capabilities
            +
Device / Quality Profile
            +
Shader Program Purpose
            |
            v
Material Program Candidate Resolution
    - Enumerate candidates
    - Resolve Surface Profile
    - Resolve each candidate graph
    - Build EffectiveMaterialProgramKey
            |
            v
Resolved Program Cache Lookup
       | hit                         | miss
       |                             v
       |                   Resolved Module Graph
       |                             |
       |                             v
       |                   Requirement Aggregation
       |                   + Provider Resolution
       |                             |
       |                             v
       |                   Resolved Shader Contracts
       |                       - Interface Contract
       |                       - Variant Contract
       |                       - Output Contract
       |                             |
       |                             v
       |                   Store Resolved Program Cache
       |                             |
       +-----------------------------+
                     |
                     v
Resolved Material Program
                     |
                     +-------------------------------+
                     |                               |
                     v                               v
Shader Assembly                             Resource Acquire Plan
    - VS or MS                                  - Only used assets
    - FS                                        - Async load dependencies
                     |                               |
                     v                               v
SPIR-V / ShaderProgram                      Loaded runtime resources
                     |                               |
                     +---------------+---------------+
                                     v
                           Material Binding Plan
                                     |
                                     v
                           Render Execution System
```

## 8. Surface Profile、Material Program 与 Material LOD

### 8.1 Surface Intent 是作者事实

Material Definition 首先声明作者层 Surface Intent。HumanSkin 在任何画质下仍然是
HumanSkin，Wood 在任何画质下仍然是 Wood。

画质降低改变的是实现策略，而不是资产语义。不能把 Material Definition 本身改写成
StandardPBR Definition，否则会丢失：

- 恢复高画质时所需的原始意图。
- 专用资源和参数的作者语义。
- 诊断和编辑器显示信息。
- 后续新增质量级别时的重新解析能力。

### 8.2 Surface Profile 是共享实现

`SurfaceImplementationProfile` 是可被多个 Material Definition 引用的共享实现定义。

建议至少包含：

```text
SurfaceImplementationProfile
  - profile_id
  - profile_schema_version
  - lighting_model
  - root_modules
  - required_surface_fields
  - supported_purposes
  - device_quality_constraints
  - static_feature_schema
  - resource_projection_schema
  - content_hash
```

例如 `StandardPBR` 和 `BlinnPhongFakePBR` 都是共享 Profile，不属于某个 HumanSkin 或
Wood Definition。

Definition 负责将自己的作者数据投影到 Profile 需要的标准字段。投影可以说明：

- HumanSkin 的 BaseColor、Normal、Roughness 如何映射到 StandardPBR。
- Wood 的纹理和参数如何映射到同一组 StandardPBR 字段。
- 专用 SSS、fiber 或 scale 数据在降级后不再使用。

投影结果中只有会改变代码或接口的结构信息进入 Program key；具体数值和资产绑定留在
Material Binding。

### 8.3 Program 候选

Material Definition 包含一个或多个 `MaterialProgramDefinition`，但候选主要是对共享
Profile、模块和投影规则的引用，不是 Definition 私有 Shader。

建议每个候选至少包含：

```text
MaterialProgramDefinition
  - program_id
  - material_lod
  - surface_profile_id
  - resource_projection
  - supported_purposes
  - device_quality_constraints
  - optional_definition_modules
  - required_recipe_capabilities
  - optional_recipe_capabilities
  - priority
  - explicit_fallback_program
```

`program_id` 用于 Definition 内选择、诊断和编辑，不自动进入共享 Shader key。

### 8.4 Material LOD 的含义

Material LOD 不是简单的纹理分辨率切换，而是可以改变：

- Resolved Surface Profile。
- Material Source 模块。
- Lighting 算法或质量。
- Normal/NTB 策略。
- 使用的纹理和 Buffer 集合。
- Shader 代码复杂度。
- Output 精度或附加数据。

Recipe 可以提供多个 LOD 可能使用的资源。当前 Forward 路径只加载最终选中 Program 的
有效需求；未来允许 Tile/像素级动态降级时，加载范围是 ECS 预先批准的
`PreparedMaterialProgramSet` 的需求并集，而不是 Definition 的全部资源。

Material LOD 的常见收敛行为是：

```text
High    → 保留各 Surface Intent 的专用 Profile
Medium  → 多种 Surface Intent 收敛到 StandardPBR
Low     → 多种 Surface Intent 收敛到 BlinnPhongFakePBR
Lowest  → 可选 Flat / Unlit 或其他极低成本 Profile
```

收敛不是 fallback 错误，而是质量策略的正常解析结果。

### 8.5 Material LOD 的决策域

Material LOD 不是由单一整数一次决定，而是由多个层面逐步收敛。

#### 8.5.1 设备与全局质量上限

设备能力和用户画质选项决定当前运行环境允许的最高 Profile，例如：

- 设备是否支持某种 Shader 能力。
- 全局是否允许 SSS、复杂 BRDF 或高成本采样。
- 当前质量档是否只允许 StandardPBR 或更低 Profile。

这一层产生 `SurfaceQualityCeiling`，用于排除运行环境绝不会执行的 Profile。

#### 8.5.2 ECS / Primitive 准备决策

ECS 根据场景和实体信息进一步决定应该为当前 Primitive 准备哪些 Profile：

- 摄像机距离。
- 投影到屏幕后的大小。
- 实体或材质重要性。
- 当前 View 和 Shader Program Purpose。
- 场景质量预算和性能压力。
- 稳定性策略与 LOD hysteresis。

这一层不一定只选出一个 Program。对于未来 Visibility Buffer 路径，它可以产生一个有界的
`PreparedMaterialProgramSet`：

```text
preferred_profile
maximum_profile
admissible_profiles
prepared_programs
fallback_order
selection_reason
```

ECS 只允许后续阶段在该集合内选择，不允许 Tile 阶段临时请求未准备的 Shader 或资源。

#### 8.5.3 Tile / 像素执行决策

Visibility Buffer 延迟着色可以在看到实际屏幕覆盖后，根据 Tile 或 shading group 中的
像素分布决定实际执行 Profile。

例如一个 Tile 中绝大多数像素只需要 StandardPBR，只有一个 HumanSkin 像素偏好 SkinSSS，
质量策略可以将该 Tile 统一降级为 StandardPBR，以避免为极少数像素执行或调度高成本 SSS。

这一层产生的是**本帧执行选择**，不是新的 Material Definition、Shader Variant 或
Artifact。它只能选择 ECS 已准备并缓存的 ResolvedMaterialProgram。

#### 8.5.4 时间稳定性

距离、投影尺寸、性能预算和 Tile 像素组成都会随帧变化。质量系统必须提供：

- Primitive/ECS 层的 hysteresis 和最短驻留时间。
- Tile 层可选的历史 Profile。
- 升级与降级使用不同阈值。
- 性能压力下优先快速降级、恢复时渐进升级。
- Debug 可视化显示当前 Profile 和降级原因。

这些状态影响本帧选择和 Prepared Set 更新，但不进入 Shader Artifact key。

### 8.6 Profile 兼容图

Surface Profile 不应只建模为一个所有材质共用的整数 LOD。不同 Surface Intent 的降级链
可以不同，因此目标结构应是可验证的 fallback graph：

```text
SkinSSS             ─┐
ReptileSkinLighting ─┼→ StandardPBR → BlinnPhongFakePBR → Flat
WoodFiberLighting   ─┤
MetalSurfaceLighting─┘
```

每个可见材质提供一个按质量排序的 admissible profile 集合。Tile reducer：

1. 取得 Tile 内各 shading sample 的 admissible profile。
2. 求出所有 sample 均允许使用的公共 Profile。
3. 在公共集合中按质量预算选择最高可用项。
4. 如果不存在公共 Profile，则拆分 shading group 或执行多个 Profile，不能选择不兼容实现。

因此，“一个 SkinSSS 像素随 Tile 降为 StandardPBR”成立的前提是 HumanSkin Definition
明确声明 StandardPBR 是合法投影目标。

### 8.7 Forward 与 Visibility Buffer 的差异

Forward 路径通常以 Primitive/draw 为 Program 粒度：

```text
ECS decision → one EffectiveMaterialProgram → draw
```

Visibility Buffer 路径将 Geometry 可见性与材质着色解耦：

```text
ECS prepares bounded Profile set
    → Visibility Buffer records material/geometry identity
    → Tile classification
    → Runtime Profile reduction
    → Dispatch cached shading Program per Tile/group
```

两条路径共享 Surface Profile、Program key、Module Graph 和 BindingPlan 定义，但执行选择
粒度不同。

### 8.8 Program 选择输入

选择器消费：

```text
MaterialDefinition
MaterialRecipe metadata
GeometryCapabilities
DeviceQualityProfile
ShaderProgramPurpose
RequestedMaterialLOD
ViewQualityBudget
PrimitiveDistanceAndProjectedSize
PrimitiveImportance
```

Recipe metadata 只表达逻辑资源是否存在及其可验证属性，不要求资产已经加载到 GPU。

资源类型、维度、格式等选择期信息应来自 Asset Catalog 或等价的轻量 metadata，
而不是通过创建纹理和上传 GPU 数据获得。

### 8.9 候选解析顺序

Program 选择不是先选中一个候选、再假定其模块必然可解析。Resolver 应：

1. 保留 Definition 的 Surface Intent 作为来源事实。
2. 根据质量等级、设备能力和 Purpose 解析目标 Surface Profile。
3. 根据 Profile 和 Definition 投影规则形成有序 Program 候选。
4. 对当前候选解析完整 Module Graph。
5. 聚合 requirement，并匹配 Geometry、Recipe metadata 和 Provider。
6. 规范化所有会影响代码、接口和资源结构的有效输入。
7. 构造 `EffectiveMaterialProgramKey`。
8. 查询共享 Resolved Program Cache。
9. 缓存未命中时构造并验证 Contract，再写入共享缓存。
10. 只有上述步骤成功，才产生 Material Resolution Result。
11. 当前候选失败时，仅在 Definition 明确允许 fallback 的情况下尝试下一个候选。

因此，选择结果同时保留两类身份：

- **Resolution Provenance**：来自哪个 Definition、Surface Intent、program_id 和降级路径。
- **Effective Program Identity**：最终 Surface Profile、模块图、接口和静态 feature 的共享身份。

前者用于失效、诊断和编辑；后者用于共享 Program、Shader 和 Artifact。

### 8.10 质量收敛示例

假设存在：

```text
HumanSkinDefinition
ReptileSkinDefinition
WoodDefinition
MetalDefinition
```

高画质解析结果可以是：

```text
HumanSkinDefinition  → SkinSSS Program
ReptileSkinDefinition→ ReptileSkin Program
WoodDefinition       → WoodFiber Program
MetalDefinition      → MetalSurface Program
```

此时它们的 EffectiveMaterialProgramKey 不同，各自生成或命中对应 Shader。

中画质解析结果可以全部收敛：

```text
HumanSkinDefinition  ─┐
ReptileSkinDefinition ├→ StandardPBR Effective Program
WoodDefinition        ┤
MetalDefinition       ┘
```

只要实际模块图、接口、Purpose 和静态 feature 一致，它们命中同一个
EffectiveMaterialProgramKey 和 ShaderProgram。不同 Recipe 仍通过各自 MaterialBindingPlan
绑定不同纹理和参数。

如果某个材质启用了会改变代码或接口的静态 feature，例如 Alpha Test、Normal Map 路径或
特殊 Geometry Provider，它应形成另一个有效 key；不能为了强行共享而忽略真实差异。

### 8.11 选择失败

以下情况允许失败：

- 请求 LOD 没有兼容 Program。
- Recipe 缺少候选的必需逻辑资源。
- Geometry 或设备能力不满足。
- 候选模块无法形成完整依赖闭包。

失败结果必须列出所有候选及拒绝原因。

如果 Definition 明确提供 LOD fallback 顺序，可以依次尝试；fallback 行为必须进入诊断和
选择结果，不能静默发生。

## 9. 模块模型

### 9.1 Material Source Module

负责读取或构造材质层面的原始值，例如：

- 常量色。
- Vertex Color。
- Texture。
- Vertex Color + Texture。
- PBR 参数。
- 特殊资产编码。
- PCG 数据。
- VBuffer 后置重建数据。

建议统一入口：

```glsl
void EvalMaterialSource(in MaterialSourceContext context,
                        inout SurfaceInput surface);
```

Material Source 可以只生产当前 Purpose 需要的字段。Masked Shadow 可能只请求 Alpha，
不要求 BaseColor、Normal、Metallic 等完整字段。

### 9.2 Geometry / Attribute Provider

Provider 将物理 Geometry 数据转换为标准 semantic。

当前 Provider 可以读取：

- VBO attribute。
- Transform SSBO。
- 其他现有输入。

未来 Provider 可以读取：

- Geometry SSBO。
- Meshlet 数据。
- Mesh Shader payload。
- 程序化 Geometry。

模块 requirement 必须使用 `Position`、`Normal`、`TexCoord0` 等 semantic，不直接把
VBO location 当作长期契约。

### 9.3 NTB Provider Module

NTB Provider 根据消费者需求和可用能力产生统一结果：

```glsl
struct NTBData
{
    vec3 normal;
    vec3 tangent;
    vec3 binormal;
};

NTBData EvalNTB(in NTBContext context);
```

候选实现可以来自：

- RG8 压缩法线。
- RGB Normal。
- Normal + Tangent。
- Normal Map。
- 程序化函数。
- Fragment derivative。

如果当前 Shader Purpose 或 Lighting 不消费 NTB，则整个 Provider 不进入模块图。

### 9.4 Surface Evaluator

`SurfaceInput` 是 Material Source、NTB 与 Lighting 之间的稳定逻辑边界。

Surface 字段应采用按需求启用的标准 semantic，不要求每个 Program 都生产所有字段。

### 9.5 Lighting Algorithm Module

Lighting Module 消费标准 Surface 数据，产生标准着色结果。

候选包括：

- Flat / Unlit。
- PBR。
- Blinn-Phong。
- Toon。

Unlit 是一个 Lighting Algorithm，不是独立 Shader 生成架构。

### 9.6 Coverage Module

Coverage 负责：

- Alpha Test。
- Dither Mask。
- Alpha-to-Coverage 所需 coverage 值。
- Purpose 特定裁剪。

Coverage 不是固定发生在 Lighting 之后的线性步骤。它消费 Material Source 或 Surface
产生的 Alpha/Mask，并可以在不执行 Lighting 的程序中独立使用。

典型依赖：

```text
Masked Forward:
  Material Source ──> Coverage ──┐
        └──────────> Surface ──> Lighting ──> Color Output

Masked Shadow:
  Material Source(alpha only) ──> Coverage ──> Depth Output

Opaque Depth Only:
  Geometry/Transform ──> Depth Output
```

### 9.7 Output Module

Output Module 只负责 Shader 输出声明和写入，不负责完整渲染执行状态。

候选包括：

- Forward Color。
- Depth Only。
- VBuffer ID。
- VBuffer Shading Data。
- Post Process Color。

Opaque 与 Transparent 如果生成相同 FS 输出和代码，应复用同一个 Shader Variant。
Blend、排序和 Depth Write 的差异由执行系统处理。

如果某种 Transparent 模式确实需要不同代码，例如 premultiplied alpha 处理，则通过明确
模块或编译期开关产生不同 Shader，而不是仅因为执行分类不同就生成变体。

## 10. 模块元数据

### 10.1 元数据目标

`@ulre` 元数据必须能够在生成任何 Shader Stage 前完成解析。

它不是临时注释约定，而是一种需要版本化、验证和规范化的模块声明格式。

### 10.2 Module Identity

每个模块至少包含：

```text
module_id
metadata_schema_version
module_content_hash
module_kind
entry_points
dependencies
requires
provides
conditions
priority
```

### 10.3 Semantic Requirement

示例：

```glsl
// @ulre require ProducedSemantic WorldPosition
// @ulre require GeometrySemantic Normal numeric=UNorm components=2
```

Geometry 要求表达 logical semantic 与约束，不表达 VBO location。

### 10.4 Resource Requirement

资源 requirement 至少需要表达：

```text
semantic
resource_kind
required_or_optional
fallback_policy
array_count
access
shader_visibility
schema_or_struct_identity
texture_dimension
sample_type
image_format_if_required
```

模块不能填写裸 descriptor binding。

### 10.5 Provide

Provider 必须显式声明：

- 提供的 semantic。
- 输出类型。
- 适用阶段。
- 是否独占。
- 满足该输出所需的输入。

多个 Provider 均满足时，按显式优先级和选择规则决定；优先级相同且无法唯一选择时报告
歧义，不能依赖注册顺序。

### 10.6 条件需求

条件只支持有限、可验证的表达式，例如：

```glsl
// @ulre require [Purpose=ForwardColor] ProducedSemantic WorldNormal
// @ulre require [Coverage=Masked] Texture2D AlphaMask
```

不支持任意预处理器表达式。条件必须在 Requirement Aggregation 阶段求值，并进入
Resolved Module Graph 的诊断记录。

### 10.7 模块图规则

Resolver 必须：

- 检测循环依赖。
- 对 module ID 去重。
- 验证 entry point 和 symbol 冲突。
- 形成稳定拓扑顺序。
- 记录每个模块被选择的原因。
- 记录被拒绝 Provider 的原因。

## 11. Contract 分层

禁止建立一个承担所有职责的全能 Contract。目标系统使用以下相互关联但职责独立的结果。

### 11.1 MaterialResolutionResult

表示某个 Definition/Recipe 请求的解析结果，并保留完整来源：

```text
definition_id
definition_content_hash
surface_intent
program_id
resolved_material_lod
resolved_surface_profile_id
shader_program_purpose
selection_inputs_digest
quality_resolution_path
fallback_path
effective_material_program_key
```

`definition_id`、原始 Surface Intent 和选择路径用于诊断与失效，不表示 Definition 独占
最终 Program。

### 11.2 ResolvedMaterialProgram

表示可以跨 Definition 共享的有效程序：

```text
effective_material_program_key
resolved_surface_profile
normalized_static_features
resolved_module_graph_key
shader_interface_key
output_contract_key
shader_variant_key
```

它不得包含只表示来源、但不改变最终代码和接口的 Definition ID 或 program_id。

### 11.3 PreparedMaterialProgramSet

表示 ECS 已经允许后续运行时选择的有界 Program 集合：

```text
material_resolution_source
preferred_effective_program_key
maximum_surface_profile
admissible_effective_program_keys
fallback_order
combined_resource_requirements
quality_policy_version
stable_hash
```

当前 Forward 路径通常只含一个有效 Program。Visibility Buffer 路径可以包含专用高质量
Profile 及 StandardPBR、FakePBR 等已批准降级项。

Prepared Set 的职责是保证 Tile/像素选择发生前：

- 所有允许执行的 Shader Program 已缓存或可用。
- 所有允许执行 Profile 的必要资源已加入加载计划。
- 每个材质到各 Profile 的参数投影和 Binding view 已定义。

### 11.4 ResolvedModuleGraph

表示规范化模块依赖图：

```text
selected_modules
topological_order
module_content_hashes
resolved_conditions
provider_selections
aggregated_requirements
diagnostic_provenance
stable_hash
```

### 11.5 ShaderInterfaceContract

描述 Shader 与 Vulkan 接口：

```text
geometry_semantics
vertex_input_abi          // 当前 VBO 过渡字段
inter_stage_semantics
descriptor_requirements
stage_outputs
entry_points
stable_hash
```

未来 Mesh Shader 迁移可以删除或替换 `vertex_input_abi`，但保留 geometry semantic 和
inter-stage semantic。

### 11.6 OutputContract

描述 Shader 输出，而不是执行状态：

```text
shader_program_purpose
attachments
location
value_type
write_semantics
depth_only
stable_hash
```

输出声明和输出写入必须由同一 OutputContract 生成。

### 11.7 ShaderVariantContract

描述所有会改变生成代码或编译结果的内容：

```text
effective_material_program_key
resolved_surface_profile_hash
resolved_module_graph_hash
shader_interface_hash
output_contract_hash
compile_time_features
compiler_profile
device_target
stable_hash
```

它不包含：

- Texture asset ID。
- Bindless handle。
- Index table row。
- Render queue。
- 仅影响 Vulkan 执行而不影响 Shader 的 Blend/Depth/Cull。
- 仅作为来源标识、但已收敛到同一有效实现的 Definition ID。

### 11.8 ResourceAcquirePlan

从 Recipe 可用资源与 PreparedMaterialProgramSet 的有效 Contract 求交集，得到需要真正
加载的资产：

```text
required_assets
optional_assets
fallback_assets
resource_usage_reason
load_priority
stable_logical_hash
```

`resource_usage_reason` 应能指出哪个模块、哪个 requirement 导致资源被加载。

Forward 单 Program 时，它等于该 Program 的资源需求。Visibility Buffer 动态降级时，它是
Prepared Set 内可执行 Profile 需求的去重并集；不属于 Prepared Set 的 Definition 资源仍然
不能被加载。

### 11.9 MaterialBindingPlan

在资源加载完成后建立运行时绑定：

```text
shader_interface_hash
logical_resource_bindings
profile_binding_views
bindless_handles
ssbo_resource_ids
texture_layer_values
material_data_indices
instance_rows
```

运行时 handle 和 row 只属于 Binding/Instance 身份，不能影响 Shader Artifact。

`profile_binding_views` 为 Prepared Set 中不同 Surface Profile 提供相同 Recipe 数据的
规范化视图，例如 SkinSSS 专用数据视图和 StandardPBR 降级视图。

### 11.10 RenderExecutionContract

由 Shader 系统之外的渲染执行层负责：

```text
shader_program
render_queue
sort_policy
blend_state
depth_state
raster_state
msaa_state
render_target_compatibility
```

Opaque/Transparent 可以引用同一个 ShaderProgram，但建立不同执行配置或 Pipeline。

## 12. Varying Semantic Registry

### 12.1 当前目标

当前阶段建立统一 registry，解决 VS/FS 接口不一致。

每个 Inter-Stage semantic 至少包含：

- Semantic ID。
- 当前固定 location。
- GLSL 类型。
- 插值方式。
- 标准变量名。
- location 宽度。

### 12.2 使用规则

FS 根据 `ShaderInterfaceContract.inter_stage_semantics` 生成输入。

VS 或 Mesh Shader 根据同一集合生成输出和赋值。

禁止：

- VS 使用局部 `loc++`。
- FS 按注释出现顺序分配 location。
- TOML 填写裸 location。
- 两个 Stage 分别推导 semantic 集合。

### 12.3 当前简化

当前不把 location 极限优化作为重构前置条件。只需要：

- Semantic ID 唯一。
- Location 区间不重叠。
- 类型和插值非空。
- 生成端与消费端一致。

未来如果需要紧凑分配，可以在保持 semantic contract 不变的情况下替换 registry 的物理
映射策略。

## 13. Program 解析与资源生命周期

### 13.1 状态机

建议材质程序实例使用明确状态：

```text
Unresolved
    → ResolvingProgram
    → ProgramResolved
    → ResourcesPending
    → Ready

任意阶段
    → Failed
```

### 13.2 Program 解析阶段

此阶段只消费：

- Definition。
- Recipe metadata。
- Geometry capabilities。
- Device/quality profile。
- Shader Program Purpose。

输出：

- MaterialResolutionResult。
- EffectiveMaterialProgramKey。
- 共享 ResolvedMaterialProgram 引用。
- PreparedMaterialProgramSet。
- ResolvedModuleGraph 与 Shader contracts 引用。
- ResourceAcquirePlan。

不创建纹理、不分配 bindless handle、不写实例 index table。

如果多个 Definition 收敛到相同 EffectiveMaterialProgramKey，它们在此阶段得到同一个
ResolvedMaterialProgram 和 ShaderProgram 引用，但分别保留自己的 MaterialResolutionResult
与 ResourceAcquirePlan。

### 13.3 资源加载阶段

ECS 或资源调度系统在 `ProgramResolved` 后提交 `ResourceAcquirePlan`。

Forward 单 Program 路径只加载最终 Program 实际需要的资源。Visibility Buffer 路径加载
PreparedMaterialProgramSet 内允许执行 Profile 的需求并集。Recipe 中未被 Prepared Set
使用的绑定保持未加载或不增加该实例的引用。

Tile/像素执行选择发生时不得触发同步资源加载。

### 13.4 Ready 阶段

资源准备完成后：

1. 解析逻辑资源到实际 runtime resource。
2. 为 Prepared Set 中每个需要的 Profile 建立兼容 MaterialBinding view。
3. 分配 bindless、SSBO 和 index table 数据。
4. 验证 BindingPlan 满足 ShaderInterfaceContract。
5. 材质实例进入 Ready。

### 13.5 失败与重试

以下错误应区分：

- Program selection failed。
- Shader assembly failed。
- Shader compilation failed。
- Required asset metadata missing。
- Asset load failed。
- Runtime binding failed。

Definition、Recipe、Geometry、Purpose 或设备 profile 改变时，应使对应解析结果失效并重新
进入 `Unresolved`。纯实例 row 变化不应使 Shader Program 失效。

### 13.6 ECS 职责

ECS 负责提供当前实体或 Primitive 的解析上下文，并驱动状态转换：

- Definition/Recipe 变化。
- Geometry 能力变化。
- Shader Program Purpose 变化。
- Material LOD 或设备质量变化。
- Primitive 距离、投影尺寸或重要性跨越稳定阈值。
- View 质量预算变化。
- 资源加载完成或失败。

Program Resolver 本身应是无副作用、可缓存的服务。ECS 不应在没有依赖变化时逐帧重新完成
Program 选择和模块闭包；渲染收集阶段只消费已经解析的 Program、BindingPlan 和状态。

ECS 的结果是可执行 Profile 集合和质量上限，不替代 Visibility Buffer 阶段基于实际像素
分布做出的本帧执行选择。

### 13.7 Visibility Buffer 运行时选择

Visibility Buffer shading classifier 消费：

- Tile 内 material/primitive identity。
- 每个 sample 的 PreparedMaterialProgramSet。
- 当前帧 shading budget。
- 可选的历史选择和稳定性信息。

它输出 shading group：

```text
effective_program_key
tile_or_sample_mask
binding_view_set
downgrade_reason
```

该结果是帧内调度数据，不进入持久 Shader cache，也不能创建新的 ShaderVariant。若目标
Program 不在 Prepared Set 中，必须拆分 group、使用集合内合法 fallback 或报告策略错误。

## 14. Shader Assembly

### 14.1 组装输入

Shader Assembler 只消费已经规范化的：

- ResolvedModuleGraph。
- ShaderInterfaceContract。
- OutputContract。
- ShaderVariantContract。

它不再自行发现新的资源或 semantic requirement。

如果组装阶段发现新 requirement，视为模块元数据不完整并直接失败。

### 14.2 轻量 Module IR

不需要建设通用 Shader AST，但建议在字符串输出前使用轻量结构：

```text
ShaderModuleIR
  - module_id
  - ordered_source_fragments
  - entry_points
  - defines
  - declared_symbols
  - required_symbols
  - source_map
```

这样可以在最终拼接前检测：

- entry point 冲突。
- symbol 重复。
- 模块顺序错误。
- 缺少 required symbol。

### 14.3 Stage 生成

当前：

- Vertex Shader 从 Geometry semantic 和 Provider 生成。
- Fragment Shader 从 Surface/Lighting/Coverage/Output 图生成。

未来：

- Mesh Shader 替代 Vertex Shader 的 Geometry 获取和 primitive 输出。
- Fragment Shader 继续消费同一逻辑 inter-stage semantic contract。

## 15. Shader Purpose 与典型需求图

### 15.1 Forward Color

```text
Geometry Provider
    → Material Source
        ├→ Coverage
        └→ Surface
              → Optional NTB
              → Lighting
                    → Forward Color Output
```

Opaque 和 Transparent 默认共享该 Shader。只有代码或接口确实不同时才建立不同 Variant。

### 15.2 Shadow Depth

不透明材质：

```text
Geometry / Transform
    → Depth Output
```

Alpha Test 材质：

```text
Geometry / Transform
    → Material Source(alpha only)
    → Coverage(alpha test / dither)
    → Depth Output
```

不执行 Lighting，不声明 Color Attachment，也不加载无关 PBR 纹理。

### 15.3 Depth Only / Early-Z

与 Shadow 类似，但可以拥有不同 Purpose 条件、深度策略和 Coverage 规则。

HZB 通常消费已有深度，应使用独立固定骨架，不强行进入 Surface/Lighting 流程。

### 15.4 VBuffer Write

使用专用 OutputContract 写出 ID 或最小重建数据。

如果 Masked 材质需要裁剪，仍复用 Material Source 的 Alpha 子集和 Coverage。

VBuffer 至少需要保留或可重建：

- Material/Binding identity。
- Primitive/Geometry identity。
- Profile 选择所需的分类信息。
- 各 Prepared Profile 共同需要的基础 Surface 输入。

不得只编码高质量专用 Profile 才能理解、却无法投影到 StandardPBR fallback 的数据。

### 15.5 VBuffer Shade

Material Source 从 VBuffer 重建输入，而不是直接读取传统 Geometry Varying。Surface 和
Lighting 接口保持一致。

着色前先进行 Tile/group classification。分类器从各 sample 的 Prepared Set 中选择公共
有效 Profile；例如多数 StandardPBR 像素与少量 SkinSSS 像素可以统一使用 StandardPBR。

选择结果只决定本帧调度哪个已缓存 shading Program，不修改 Definition，不创建新的
ShaderVariant，也不在 Tile 阶段加载资源。

### 15.6 Post Process

Post Process 可以复用：

- Module metadata。
- Descriptor Contract。
- OutputContract。
- Shader assembly infrastructure。

但不强制复用 Surface/Lighting 模型。

## 16. 缓存与稳定哈希

### 16.1 Hash 分层

至少区分：

```text
MaterialSelectionRequestKey
EffectiveMaterialProgramKey
PreparedMaterialProgramSetKey
ResolvedModuleGraphKey
ShaderInterfaceKey
ShaderVariantKey
ShaderProgramKey
ResourceAcquirePlanKey
MaterialBindingKey
RenderPipelineKey
```

### 16.2 Material Selection Cache

第一层缓存回答：

> 这个 Definition/Recipe 上下文在当前质量、Purpose、Geometry 和设备下应解析成什么？

`MaterialSelectionRequestKey` 包含：

- Definition stable ID。
- Definition content hash。
- Recipe 的结构能力签名，不包含具体 runtime handle。
- Surface Intent。
- 请求质量或 Material LOD。
- ECS 量化后的屏幕尺寸、距离、重要性与 View budget class。
- Surface quality resolution policy version。
- Shader Program Purpose。
- Geometry capability class。
- Device/quality profile。

原始浮点距离、像素覆盖率和逐帧 Tile 组成不能直接进入持久 key。ECS 先将它们量化为稳定
质量决策类别，并应用 hysteresis；Tile 选择只产生帧内调度结果。

值为 `MaterialResolutionResult`，其中包含最终
`EffectiveMaterialProgramKey` 和完整选择来源。

这一层必须包含 Definition 身份和内容，因为不同 Definition 的降级路径和资源投影可能
不同。它只缓存“如何解析”，不表示最终 Shader 私有。

### 16.3 EffectiveMaterialProgramKey

第二层缓存回答：

> 质量解析完成后，这个有效材质程序是否已经由其他 Definition 构建过？

该 key 只能来自规范化的有效实现：

- Resolved Surface Profile 内容 hash。
- 规范化静态 feature。
- Material Source 投影的有效代码结构。
- Resolved Module Graph hash。
- 有效 Recipe/Geometry capability signature。
- Shader Program Purpose。
- Program resolver 与 Contract schema version。
- Device/quality profile 中影响模块选择的部分。

该 key 不包含：

- 仅用于来源追踪的 Definition ID。
- Definition 内局部 program_id。
- 原始 Surface Intent，除非它在降级后仍改变代码或接口。
- 未被有效 Profile 使用的专用资源。
- 运行时材质参数和具体资产身份。

HumanSkin、Wood、Metal 等在中画质解析为完全相同 StandardPBR 有效实现时，必须得到相同
`EffectiveMaterialProgramKey`。

ShaderInterfaceKey 和 OutputContractKey 是该缓存的输出，不作为查询前尚未生成的输入。
它们继续参与下游 ShaderVariantKey 和 ShaderProgramKey，保证接口生成变化会正确使
Shader Artifact 失效。

### 16.4 Resolved Program Cache

```text
EffectiveMaterialProgramKey
    → ResolvedMaterialProgram
    → ResolvedModuleGraph
    → ShaderInterfaceContract
    → OutputContract
    → ShaderVariantKey
    → ShaderProgram / Artifact reference
```

该缓存实现跨 Definition 的 Program 与 Shader 共享。

Definition 或 Recipe 实例仍各自持有 MaterialResolutionResult 和 MaterialBindingPlan，
不能把某个 Definition 的资源绑定写入共享 ResolvedMaterialProgram。

### 16.5 PreparedMaterialProgramSetKey

Prepared Set key 用于缓存 ECS 已批准的有效 Program 集合，包含：

- 有序且去重的 EffectiveMaterialProgramKey 集合。
- Preferred 与 maximum Profile。
- Fallback graph/policy version。
- Shader Program Purpose。
- 资源投影 schema。

距离、屏幕大小和重要性可以改变 ECS 选择哪个 Prepared Set，但不进入单个
EffectiveMaterialProgramKey。

Tile 内实际选择哪个 Program 是帧内调度结果，不建立持久 Artifact key，也不能导致
Shader 编译。

### 16.6 ShaderVariantKey

包含：

- EffectiveMaterialProgramKey 或其规范化构成。
- Resolved Surface Profile 内容 hash。
- Shader Program Purpose。
- Module graph 内容 hash。
- Interface contract hash。
- Output contract hash。
- 编译期 feature。
- Compiler/profile/device target。

不包含：

- 解析来源 Definition ID 和局部 program_id。
- 已经收敛且不再改变有效实现的原始 Surface Intent。
- 未被当前程序使用的 Definition/Recipe 资源。
- Texture asset ID，除非资产元数据明确影响代码选择。
- Bindless handle。
- SSBO runtime ID。
- Index table row。
- Render queue 和排序策略。

### 16.7 Recipe 对 Shader Key 的影响

Recipe 只有在以下信息改变模块或代码选择时才影响 ShaderVariantKey：

- 某种逻辑资源是否提供。
- 资源 metadata 是否满足候选 Program。
- 作者覆写是否对应编译期 feature。
- Material LOD 请求是否改变 Program 选择。

具体 asset identity 和运行时 handle 不影响 ShaderVariantKey。

### 16.8 Canonical Serialization

所有 Contract 在 hash 前必须：

- 使用稳定 semantic ID。
- 按稳定规则排序。
- 不包含指针。
- 不依赖注册顺序。
- 不包含未初始化 padding。
- 使用 HGL hash 工具。
- 包含 schema/version。

规范化过程必须消除来源差异。例如 HumanSkin 和 Wood 都降级为 StandardPBR，且有效
feature、接口和模块完全相同时，不能因为不同字符串 ID、候选顺序或未使用字段导致 key
不同。

### 16.9 Program Artifact

Program Artifact 至少记录：

```text
artifact_schema_version
shader_program_key
stage_keys
module_graph_hash
interface_hash
output_contract_hash
compiler_profile
device_target
generated_source_digest
```

缓存读取必须先验证 Program Metadata，再读取各 Stage SPIR-V。

## 17. 错误诊断

### 17.1 Program 选择失败

至少包含：

- Definition ID。
- Recipe ID。
- 请求 Material LOD。
- Shader Program Purpose。
- Device/quality profile。
- Geometry capability 摘要。
- 检查过的 Program 候选。
- 每个候选的拒绝原因。
- 是否尝试显式 fallback。

### 17.2 模块解析失败

至少包含：

- 当前候选 Program 或 MaterialResolutionResult。
- 当前模块图。
- 未满足 requirement。
- 检查过的 Provider。
- Provider 被拒绝原因。
- 循环依赖或 symbol 冲突。

### 17.3 资源错误

区分：

- Recipe 未提供必需逻辑资源。
- 资源 metadata 不满足要求。
- 资源加载失败。
- 资源加载成功但运行时绑定失败。

禁止用默认资源悄悄掩盖必需资源缺失。只有 requirement 明确允许 fallback 时才能使用
默认资源，并记录 fallback 来源。

### 17.4 运行时质量诊断

调试和性能工具至少能够查询：

- Primitive 的 Surface Intent、quality ceiling 和 Prepared Set。
- 当前 Preferred 与实际执行 Profile。
- ECS 降级原因：距离、尺寸、重要性、预算或设备限制。
- Tile/group 降级原因和公共 Profile 求交结果。
- 无公共 Profile 时的 group 拆分次数。
- 每个 Profile 的像素数、Tile 数和 dispatch 成本。
- Tile 请求了未准备 Program 的策略错误。

## 18. 与现有类型的目标关系

### 18.1 保留并演进

- `MaterialDefinition`
  - 扩展为 Program/LOD/资源全集定义。
- `MaterialRecipe`
  - 保持声明式可用资源输入，不持有 Vulkan 句柄；现有 `material_lod` 仅作为作者提示，
    不再被视为最终执行 Profile。
- `GLSLCodeModuleRegistry`
  - 演进为正式模块 registry。
- `GLSLCodeModuleCapabilityResolver`
  - 演进为通用 module/provider resolver。
- `MaterialResourceLayout`
  - 作为 ShaderInterfaceContract 的物理 descriptor layout 结果。
- `MaterializationSpec` 与 Pools
  - 收敛为 MaterialBindingPlan 和实例数据实现。
- `ShaderStageKey`、`ShaderProgramKey`、`ShaderArtifactContract`
  - 按 Contract 分层补齐。
- ECS 材质质量状态
  - 新增量化质量上下文、PreparedMaterialProgramSet 和 hysteresis 状态。
- Visibility Buffer shading classifier
  - 消费 Prepared Set，在帧内建立 Tile/shading group 执行选择。

### 18.2 降级为过渡适配器

- `MaterialVertexVaryingConfig`
- 旧 TOML `vertex.varyings`
- 旧 Compositor 模板选择字段
- `FixedVertexEntry`
- `FixedDescriptorEntry`
- `FixedMaterialDef`

这些类型在迁移期间可以用于 A/B 对照，但不能继续作为目标架构的核心事实来源。

### 18.3 最终替代

- `NewShaderPermutationKey`
  - 由 ShaderVariantContract 和 ShaderVariantKey 替代。
- 分散的 render state hash
  - 由 RenderExecution/Pipeline Contract 管理。
- Compositor 内部临时发现需求
  - 由预先解析的 ResolvedModuleGraph 替代。

## 19. 重构计划

每个阶段必须满足：

1. 阶段目标和非目标明确。
2. 相关回归测试通过。
3. 不提前切换下一阶段行为。
4. 失败时只回退当前阶段。
5. 不在同一阶段同时改数据模型、生成行为和运行时加载行为，除非验收明确要求。

### Phase A：冻结术语、元数据与 Contract 数据模型

目标：

- 定义 MaterialProgramDefinition。
- 定义 SurfaceIntent、SurfaceImplementationProfile 和质量降级映射。
- 定义 ShaderProgramPurpose。
- 定义模块 `require/provide/dependency/condition` schema。
- 定义 ResolvedModuleGraph。
- 定义 ShaderInterfaceContract、OutputContract 和 ShaderVariantContract。
- 定义 ResourceAcquirePlan 与 MaterialBindingPlan 的边界。
- 定义 MaterialResolutionResult、EffectiveMaterialProgramKey 和 ResolvedMaterialProgram。
- 明确 Program Selection 与 Provider Selection 诊断结构。

本阶段不改变 Shader 输出。

验收：

- 所有新结构可以对当前 PureColor、Text 等内建材质建立镜像描述。
- 多个不同 Surface Intent 可以构造收敛到 StandardPBR 的镜像用例。
- Contract 可以稳定序列化和 hash。
- 模块图循环、冲突和歧义有单元测试。
- 代码评审确认没有把 runtime handle 放入 Shader Contract。

### Phase B：建立 Semantic Registry 与旧配置适配

目标：

- 建立 Inter-Stage Varying Semantic Registry。
- 建立 Geometry Semantic Registry。
- 区分 Vertex Attribute 与 Inter-Stage Varying。
- 验证 semantic ID、location、类型、插值和宽度。
- 将旧 Varying bool 配置转换为 semantic set。

本阶段仍保持现有生成结果。

验收：

- Registry 完整性测试通过。
- 相同 semantic 的 VS/FS 类型和插值可统一查询。
- 现有 Shader 输出保持不变。

### Phase C：实现预解析 Module Graph 与 Requirement Closure

目标：

- 在生成 Shader 前解析全部模块元数据。
- 建立 ResolvedModuleGraph。
- 提前定义 NTB/Geometry Provider 的标准 require/provide 数据模型。
- 实现依赖拓扑、循环检测、Provider 选择和拒绝原因。
- 将现有 ShaderResourceManifest 纳入统一 requirement aggregation。

本阶段可以继续使用旧 Compositor 输出，但新 Graph 必须能镜像解释旧路径需要的资源。

验收：

- 当前内建材质均可产生确定的 Module Graph。
- 改变模块注册顺序不改变 Graph hash。
- 缺少 Provider 时产生完整诊断。
- 当前 Graph 推导资源与旧路径实际声明一致。

### Phase D：统一 Shader Stage 接口生成

目标：

- 从 ShaderInterfaceContract 同时生成 VS 输出与 FS 输入。
- 移除 VS `loc++`。
- 移除 FS 按注释顺序分配 location。
- 禁止 TOML 直接决定物理 location。
- BuildResolvedMaterialVertexABI 接入正式 Contract。

允许的过渡代码：

- 旧 Varying Config 到 semantic set 的单向适配器。
- VS 仍使用 VBO physical mapping。

验收：

- VS/FS interface contract 完全一致。
- Vulkan Validation 无 stage interface 错误。
- PureColor、Text、Texture、Sky 等现有路径通过。

### Phase E：完成 Contract Key 与 Artifact 闭环

目标：

- 实现 canonical serialization。
- 分离 StageKey、ShaderVariantKey、ShaderProgramKey。
- 实现 MaterialSelectionRequestKey 与 EffectiveMaterialProgramKey。
- 实现 PreparedMaterialProgramSetKey。
- 实现 Material Selection Cache 与跨 Definition Resolved Program Cache。
- 将 module graph、interface、output、compiler/profile 纳入正确 key。
- 实现 ProgramMetadata 落盘与验证。
- 保证 runtime resource identity 不进入 Shader key。

验收：

- 注册顺序变化不改变 key。
- 相同输入跨进程产生相同 key。
- 不同 Definition 收敛为同一有效 Profile 时产生相同 EffectiveMaterialProgramKey。
- 高画质专用 Surface Profile 仍产生不同 EffectiveMaterialProgramKey。
- 改变 bindless handle 不改变 Shader key。
- 改变模块内容、接口或 compiler profile 必须使缓存失效。
- ReadOnly、BuildIfMissing、损坏 artifact 测试通过。

### Phase F：引入 ShaderProgramPurpose、OutputContract 与 Coverage DAG

目标：

- 将 Shader 生成目的从 Opaque/Transparent 等执行分类中分离。
- 实现 Forward Color 和 Depth Only OutputContract。
- 将 Output 声明与写入统一生成。
- 将 Coverage 建模为需求 DAG 分支。
- 支持 Masked Shadow/Depth 只执行 Alpha 所需 Material Source 子集。
- Opaque Depth Only 裁剪 Material Source 和 Lighting。

验收：

- Depth Only 不声明 Color Output。
- Masked Shadow 正确执行 alpha test/discard。
- Opaque Shadow 不加载、不声明无关材质资源。
- Opaque 与 Transparent 在代码相同时共享 ShaderVariantKey。
- Blend/Depth 执行差异仍由现有渲染执行系统正确处理。

### Phase G：统一 Lit、Unlit 与 Material Source 骨架

目标：

- Unlit 作为 Flat Lighting Algorithm。
- 不同 Unlit 材质变为 Material Source Module。
- Lit/Unlit 使用同一 Surface、Coverage 和 Output 边界。
- 移除通过大型 `#if/#else` 选择材质类型的生成方式。

验收：

- Vertex Color。
- Texture 2D / 2D Array。
- Text。
- Palette。
- Lit PBR。
- Alpha Test / Dither。
- 新旧路径 A/B 结果一致。

### Phase H：Material Program、Material LOD 与延迟资源加载

目标：

- Material Definition 支持 Program candidates。
- Material Definition 保留 Surface Intent，并通过质量策略解析共享 Surface Profile。
- Recipe 只提供逻辑资源和 metadata。
- 根据 Purpose、LOD、Geometry、设备和 Recipe 能力选择 Program。
- 定义设备/全局上限、ECS 准备决策和未来 Tile 执行决策的边界。
- ECS 将距离、投影尺寸、重要性和 View 预算解析为 PreparedMaterialProgramSet。
- 将选择来源缓存与有效 Program 缓存分离。
- ProgramResolved 后按 Prepared Set 生成 ResourceAcquirePlan。
- ECS 在 Prepared Set 确定后发起资源加载。
- 资源完成后建立 MaterialBindingPlan。
- Recipe 未使用资源不加载。

验收：

- Recipe 可包含多个 Program/LOD 的资源，但只加载 Prepared Set 的需求并集。
- Forward 路径的 Prepared Set 默认只有一个有效 Program。
- 距离、投影尺寸和重要性跨越 hysteresis 阈值时才更新 Prepared Set。
- HumanSkin、ReptileSkin、Wood、Metal 在中画质收敛到相同 StandardPBR Program 时共享
  ShaderProgram，但保留各自 BindingPlan。
- 高画质恢复后，各 Surface Intent 重新命中各自专用 Program。
- Shader 生成失败时不提交资源加载。
- Shadow 不加载只供 Forward Lighting 使用的资源。
- Program 选择失败列出所有候选拒绝原因。
- 资源 Pending/Ready/Failed 状态正确。

### Phase I：NTB Provider 行为切换

目标：

- 将 Phase C 已冻结的 NTB Provider 数据模型接入真实生成。
- 根据消费者需求和 Geometry 能力选择 Provider。
- 支持当前需要的 RG8、RGB、Normal + Tangent、Normal Map 等路径。
- Provider 内部隐藏 VBO/SSBO/推导细节。

验收：

- 每种 Geometry 编码选择预期 Provider。
- 不消费 NTB 的程序不选择 Provider。
- 输出完整并归一化。
- 缺少 Provider 时明确失败。

### Phase J：扩展 Purpose

目标：

- ShadowDepth。
- Early-Z / DepthOnly。
- VBufferWrite。
- VBufferShade。
- 必要的 PostProcess skeleton。

每新增一种 Purpose，必须先定义：

- 根模块。
- 可裁剪阶段。
- OutputContract。
- Coverage 需求。
- ResourceAcquirePlan 预期。
- 执行系统边界。
- PreparedMaterialProgramSet 与 Tile classifier 输入。

验收：

- 每种 Purpose 有独立 Contract 测试。
- 不需要的模块和资源确实被裁剪。
- Shader Purpose 不错误决定 Render Queue 或 Blend State。
- VBuffer Tile reducer 只能选择 Prepared Set 内的 Program。
- 少量 SkinSSS 像素和多数 StandardPBR 像素可以按策略统一降为 StandardPBR。
- 不存在公共 Profile 时拆分 shading group，不能选择非法 Profile。
- Tile 选择不触发 Shader 编译或资源加载。

### Phase K：移除过渡路径

当前安全清理状态：

- 已删除 DirectInclude 模式、旧 `program_mode` TOML 字段和旧 fragment source 兼容别名。
- 已删除重复 Lit/Unlit Forward skeleton、对应旧 Surface 模块和旧 PureColor fragment。
- 已删除 `FixedMaterialDef` 核心编译入口，统一使用 `MaterialCompilerInput`。
- 已删除公开 migration 切换、生产 Shadow 对照分支和多 cache namespace。
- `MaterialVertexVaryingConfig` 与 `CompositorAssembler` 仍是当前生产生成链依赖；必须先用
  canonical ShaderInterface/module assembly 完整替代，不能仅为清理目标强制删除。

目标：

- 删除旧 Varying bool 配置。
- 删除旧 DirectInclude。
- 删除旧 Compositor 运行路径。
- 删除 FixedMaterialDef 核心编译路径。
- 删除 Lit/Unlit 重复 skeleton。
- 删除只服务旧路径的 TOML 字段。
- 更新全部测试资产和示例。

验收：

- 完整 Debug Build。
- 完整 Release Build。
- 全部 ShaderGen Regression Gate。
- 全部示例程序。
- 搜索确认不存在旧入口、双路径和迁移标记。

### Phase L：为 SSBO Geometry + Mesh Shader 做接口切换

该阶段属于后续大计划，本文只定义衔接要求：

- Material module 继续消费 Geometry semantic，不消费 VBO location。
- Geometry Provider 改为从 SSBO/Meshlet 获取数据。
- `vertex_input_abi` 从 ShaderInterfaceContract 中删除或替换。
- Mesh Shader 生产 Fragment Shader 所需 inter-stage semantic。
- Material、Lighting、Coverage 和 Output 模块不因 Geometry 存储方式改变而重写。

验收标准在 Mesh Shader 专项设计中定义。

## 20. 测试策略

### 20.1 数据模型与解析

- Definition Program/LOD 解析。
- Recipe 逻辑资源解析。
- Module metadata schema。
- Semantic 字符串到 ID。
- 条件 requirement。
- 显式 fallback。

### 20.2 Module Graph

- Requirement 去重。
- 类型和资源冲突。
- Provider 唯一选择。
- Provider 歧义。
- 循环依赖。
- 稳定拓扑和 hash。

### 20.3 Contract

- VS/FS semantic 一致。
- Descriptor requirement 一致。
- Output 声明与写入一致。
- Depth Only 无颜色输出。
- Runtime identity 不进入 Shader Contract。

### 20.4 Program 与 LOD

- 请求 LOD 命中。
- Surface Intent 到专用高质量 Profile 的解析。
- 多种 Surface Intent 在中画质收敛到 StandardPBR。
- 多种 Surface Intent 在低画质收敛到 BlinnPhongFakePBR。
- 收敛后 EffectiveMaterialProgramKey 与 ShaderProgram 共享。
- Definition 专用静态 feature 仍能阻止错误共享。
- 显式 LOD fallback。
- Recipe 资源不足。
- Geometry 不兼容。
- Device profile 不兼容。
- 所有候选拒绝原因完整。

### 20.5 多决策域 LOD

- 设备和全局画质正确限制 maximum Profile。
- 距离、投影尺寸和重要性生成预期 Prepared Set。
- Hysteresis 避免阈值附近逐帧抖动。
- Tile 内 Profile admissible set 求交正确。
- 少量 SkinSSS sample 可按策略统一降为 StandardPBR。
- 无公共 Profile 时正确拆分 shading group。
- Tile 选择只引用已有 Program，不创建 ShaderVariant。
- 帧内选择不污染持久 Artifact key。

### 20.6 资源生命周期

- Program 确定前不加载资源。
- Forward 只加载单 Program active requirement。
- Visibility Buffer 只加载 Prepared Set requirement 并集。
- 未使用 Recipe 资源不加载。
- Tile 切换 Profile 不触发同步加载。
- Shader 失败不加载。
- Asset load failure。
- BindingPlan 验证。

### 20.7 Shader 与 Artifact

- 最终 VS/FS GLSL。
- Stage interface。
- Program metadata。
- 缓存命中与失效。
- Material Selection Cache 命中。
- 跨 Definition Resolved Program Cache 命中。
- PreparedMaterialProgramSet Cache 命中。
- 不同 BindingPlan 共享同一个 Shader Artifact。
- 跨进程稳定 key。
- 损坏 artifact。

### 20.8 Vulkan

- Pipeline 创建无 Validation Error。
- Descriptor Layout 与 Shader 声明一致。
- 当前 Vertex Input 与 Geometry Format 一致。
- Depth Only、Shadow 和 Forward 输出兼容。

### 20.9 视觉

至少覆盖：

- Vertex Color。
- Texture 2D / 2D Array。
- Text。
- Palette。
- Lit PBR。
- Normal Map。
- Alpha Test。
- Dither。
- Sky。
- Opaque/Transparent 共享 Shader 的对照。
- Masked Shadow。
- Material Profile LOD 可视化。
- Visibility Buffer Tile 降级与 shading group 边界。

## 21. 最终形态

目标系统不再以“2D Shader”“3D Shader”“Unlit Shader”“Lit Shader”作为相互独立的
生成路径，而是按 Program、模块图和 Contract 组织：

```text
Definition + Recipe + Context
        ↓
Surface Intent
        +
Device / Global Quality Ceiling
        +
ECS Distance / Size / Importance / View Budget
        ↓
Material Resolution Result
        ↓
Effective Material Program Cache
        ↓
Resolved Module Graph
        ↓
Shader Contracts
        ├── Geometry / Attribute Provider
        ├── Material Source
        ├── Optional NTB
        ├── Surface
        ├── Optional Lighting
        ├── Optional Coverage
        └── Output
        ↓
Prepared Material Program Set
        ├── Cached ShaderPrograms
        ├── Resource Acquire Plan
        └── Profile Binding Views
        +
Material Binding Plan
        ↓
Execution Selection
        ├── Forward: Primitive/draw chooses one Program
        └── Visibility Buffer: Tile/group chooses cached Program
        ↓
Independent Render Execution System
```

架构最终应满足：

1. Definition 和 Recipe 可以描述资源全集，但最终程序只消费真实需求子集。
2. Definition 始终保留 HumanSkin、Wood、Metal 等 Surface Intent。
3. Material LOD/质量解析可以让不同 Surface Intent 收敛到共享 Surface Profile。
4. 收敛后的有效 Program 和 Shader 按内容 key 跨 Definition 共享。
5. 设备、画质、距离、屏幕尺寸、重要性和 View 预算共同决定 Prepared Profile 集合。
6. Visibility Buffer 可以在 Prepared Set 内按 Tile/像素分布进一步降级。
7. Tile/像素降级不会触发 Shader 编译、Artifact 创建或同步资源加载。
8. Shader Program/Set 确定前不加载具体纹理和材质资源。
9. Alpha Test 可以被 Forward、Shadow、Depth 和 VBuffer 复用。
10. Opaque/Transparent 等执行分类不会无理由制造 Shader 变体。
11. Shader、Pipeline 和 Material Binding 使用不同缓存身份。
12. 当前 VBO 输入可以被未来 SSBO Geometry + Mesh Shader 替换。
13. 所有组合失败都在资源加载和 Vulkan Pipeline 创建之前提供结构化诊断。
