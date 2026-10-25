# 彻底废除 ShaderGen 终极架构设计：基于 MaterialDomain 的固定着色器方案

> 状态：**终极架构规范（指导彻底删除 ShaderGen）**
> 核心命题：**彻底废除 C++ Shader 生成器与 AST 拼装，主着色器走向纯物理固定 GLSL，C++ 仅输出极简 options.glsl**
> 日期：2026-10-25

---

## 0. 核心定位与概念纠偏（必须牢记）

在 ULRE 的物理级极简架构中，核心分层与概念必须严格对齐，严禁混淆：

```
+-----------------------------------------------------------------------------------+
| 引擎核心: MaterialDomain (母材质 / 表面领域)                                         |
| - 例: StandardOpaque, UnlitSolid, VertexColor, Sky, Text2D, Foliage 等 16 个领域    |
| - 等价于 Unreal 中的 "Material" (母材质)                                            |
| - 它是真正持有 GLSL 算法与 Pipeline 契约的实体！                                     |
| - 全引擎仅有这十几个固定的 Domain，完全由引擎内部写死，上层不可新建、不可改！           |
+-----------------------------------------------------------------------------------+
                                         │ 实例化 (填数值、配贴图)
                                         ▼
+-----------------------------------------------------------------------------------+
| 用户资产: MaterialRecipe / .mi.toml (材质实例)                                      |
| - 例: lit.mi.toml, checkerboard_2d.mi.toml, builtin/pure_color                    |
| - 等价于 Unreal 中的 "MaterialInstance" (材质实例)                                  |
| - 资产绝不包含任何 Shader 代码！绝不编译为独立 SPV！                                 |
| - 它只是向 MaterialDomain 填参的一行数据，运行时直接作为 SSBO 行 + Bindless 纹理槽传递 |
+-----------------------------------------------------------------------------------+
```

### 关键纠偏结论
1. **编译 SPV 的单位从来都不是用户资产（`.mi.toml`）**！
2. **唯一需要对应着色器的实体是 `MaterialDomain`**（与其组合的渲染 Pass，如 ForwardLit / ShadowCaster）。
3. 现有的 `src/ShaderGen`（8000+ 行代码，包含 AST、Registry、DFS 依赖图、动态 Varying 拼接、OutputContract 拼接）之所以臃肿，是因为它**把原本属于固定 Domain 的东西，用 C++ 人肉充当了一套极度复杂的“动态预处理器”**。

---

## 1. 终极架构原则：彻底消灭 ShaderGen

### 1.1 物理级固定主着色器（Fixed Master Shaders）
每个 `MaterialDomain` 在结合对应渲染 Pass 时，其主着色器是**物理级写死的完整编译单元**，不存在任何运行时代码拼接。文件路径为：

```
ShaderLibrary/domain/root_bindings.glsl                       全局固定前导（pc_root / 材质地址 / 阴影标志位）
ShaderLibrary/domain/standard_opaque_bindings.glsl            StandardOpaque 专属绑定（采样器索引 / PBRSurfaceRow / 纹理表）
ShaderLibrary/domain/sky_bindings.glsl                        Sky 专属绑定（单槽纹理表）
ShaderLibrary/domain/standard_opaque/forward_lit.frag         ForwardColor（PBR 光照）
ShaderLibrary/domain/standard_opaque/shadow_caster_masked.frag ShadowDepth（含 alpha 覆盖判定）
ShaderLibrary/domain/standard_opaque/shadow_caster.frag       ShadowDepth（无覆盖判定，空体占位）
ShaderLibrary/domain/sky/sky.frag                             ForwardColor（天空盒）
```

要点：

- **一个文件就是一次编译**。文件里没有 `{{占位符}}`、没有插槽、没有需要 C++ 端回填的锚点。
- **`#version 450` 必须是文件的第一行**——实测 glslang 在 `#version` 之前连注释都不接受
  （`ERROR: '#version' : must occur first in shader`）。所以所有说明性注释都写在 `#version` 之下。
- `pc_root`（push constant）只允许在 `root_bindings.glsl` 中声明一次。它是全引擎唯一的
  push constant 块，重复声明会直接编译失败，因此每个 domain 的绑定文件都不许再声明它。
- 采样器预设索引（`TrilinearSampler` / `LinearSampler` / `ShadowPCFSampler`）**物理写死在
  `<domain>_bindings.glsl`**。这些数字是 `sampler.toml` 的全局序号，不是域内序号，写错不会
  报错、只会静默按最邻近采样。
  原实现由 `BuildSamplerMacros(definition->sampler_names)` 逐次生成，实测**同一 domain 两次运行
  会产出不同的宏集合**（第二次整个漏掉采样器宏，`sky` 因此编不过）——因为它取决于 definition
  的加载顺序。采样器槽表本来就在 `DomainContracts[]` 里是静态数据，物理固定即可彻底消除该隐患。

### 1.2 唯一允许的动态物：极简 `options.glsl`

全套着色器编译中，C++ 端**唯一允许动态生成**的产物是一个不到 20 行的配置头，落到
`<include-root>/generated/options.glsl`。主着色器用固定的一行包含它：

```glsl
#include "generated/options.glsl"
```

发射器只有一处：`hgl::graph::mtl::EmitDomainOptions(const DomainOptionsInput &, AnsiString &)`
（`inc/hgl/mtl/DomainOptions.h` + `src/ShaderMaterialData/DomainOptionsEmitter.cpp`）。
它**不接收** `MaterialDomain` / `MaterialRecipe`：recipe 只是在 BDA 行里描述实例，
反查 recipe 会让这一层重新长出领域知识。调用侧先用
`ApplyRenderStateToDomainOptions()` 把 `ResolvedMaterialRenderState` 压平进输入，
再用 `ResolveDomainOptionsProfile()` 定 profile。

协议内容只有两类，**没有任何算法逻辑、没有任何 `const` 变量、没有任何非 `#define` 之外的声明**：

```glsl
// options.glsl —— StandardOpaque / ForwardColor 的一个具体实例（真实发射结果）
#define MTL_TEX_NORMAL_CHANNELS 2
#define HGL_SHADOW_PCF_POISSON_TAPS 16
#define HGL_SHADOW_NORMAL_OFFSET 1
#define HGL_ALPHA_TEST 1
#define HGL_ALPHA_CUTOFF 0.500000
#include "lighting/indirect_sky_ambient.glsl"     // ← 环境光 provider（多态 include）
```

**发射顺序即 program 身份**（`ComputeShaderConfigHash` 是对该文本做哈希），改动必须过
`scripts/ci/verify-domain-master-shaders.ps1`：

1. `#define HGL_COVERAGE_ONLY 1` —— 仅 `ShadowCaster`
2. `#define HGL_MATERIAL_FACE_NORMAL_FALLBACK 1` —— 顶点无法线时
3. `#define MTL_TEX_<SLOT>_CHANNELS <n>` —— `ShadowCaster` 整段跳过；`channels == 0` 整条跳过
4. `#define HGL_SHADOW_PCF_POISSON_TAPS` / `HGL_SHADOW_NORMAL_OFFSET` —— 非 `ShadowCaster` 且启用 PCF
5. `#define HGL_ALPHA_TEST 1` + `#define HGL_ALPHA_CUTOFF <%f>`
6. `#define HGL_ALPHA_DITHER 1`
7. `#define HGL_ALPHA_OUTPUT 1`
8. `#include "<ambient>"` —— 仅 `ForwardLit`，全文件唯一一行 include

`DomainOptionsProfile` 路由（镜像 `SelectSceneRenderTemplateRequest`）：

| 条件 | profile | 输出 |
|---|---|---|
| 深度用途 | `ShadowCaster` | 覆盖标记 + alpha |
| 非深度、有 NTB provider | `ForwardLit` | 通道数 + 阴影参数 + alpha + ambient include |
| 非深度、有 surface provider、无 NTB | `ForwardUnlit` | 同上去掉 ambient include |
| 都没有 | `None`（Sky） | **空文件，一个字节都不发** |

**已删除的两个宏**：`HGL_USE_MATERIAL_SOURCE_PROVIDER` / `HGL_USE_SCENE_LIGHTING`
在 `ShaderLibrary` 里**零消费者**，属于旧生成期的死宏，不再发射。

硬约束（实测得出，不可再犯）：

1. **多态 include 必须是字面路径写死在 `options.glsl` 文本里**。
   `#define AMB "lighting/indirect_sky_ambient.glsl"` 配合 `#include AMB` **编译失败**：
   `ERROR: '#include' : must be followed by a header name`。GLSL 预处理器不支持宏展开成头文件名。
2. 它只允许出现 `#define` 与 `#include`。任何需要计算的逻辑都不许进来，否则它又会退化成
   一个 C++ 端的代码生成器。
3. 它只承载**按 program 变化**的量（覆盖判定开关、阴影采样参数、法线贴图通道数、环境光
   provider）。材质业务参数（颜色、粗糙度乘数、贴图索引）一律走运行时 BDA，**绝不进这里**。
4. `HGL_ALPHA_CUTOFF` 用 `#define`（`alpha_compositor.glsl` 在家守卫内直接比 `alpha < HGL_ALPHA_CUTOFF`）。
5. 数字格式锁死：整数 `%u`；浮点 `%f`（六位小数）。这是逐字节兼容旧产物的前提。
6. `channels == 0` **必须整条跳过**，因为下游全部按"宏存在即非零"判断
   （`ntb_tangent_vbo_normalmap.glsl` 只用 `== 2` 才走双通道重建）。
7. 成功路径**整体覆盖**输出（`None` ⇒ 清空）；任何拒绝（缺 ambient 路径、非法槽名）都不得
   留下半截文本。

`HGL_MATERIAL_HAS_VERTEX_NORMAL` **不在本协议内**：它描述顶点格式而非 recipe，走
`MaterialDefinition::compile_defines` 那条独立通道（`MaterialShaderEmitter::BuildCompileDefineDocument`）。

回归门：`DomainOptionsRegression`（独立 target，只链接 `ULRE.ShaderMaterialData`，
ShaderGen 删除后照常构建）26 条断言；`--write-options <dir>` 供等价性脚本直接消费发射结果。
## 1.3 绝大部分分支走“多态 include”与“空对象（Null Object）”，彻底消灭 `#if`
除硬件管线必须依赖的 Alpha 覆盖（`discard` / Dither）外，所有光照与环境算法**绝不使用 `#if / #else` 条件编译**，而是包含一个「空对象」模块。现已具备的空对象（实测存在的文件名，不要臆造其它名字）：

| 关闭的东西 | 包含的文件 | 语义 |
|---|---|---|
| 漫反射 | `lighting/diffuse_none.glsl` | 返回零贡献 |
| 高光 | `lighting/specular_none.glsl` | 返回零贡献 |
| 间接光 / 环境光 | `lighting/indirect_none.glsl` | 返回零贡献 |
| 阴影 | `shadow/identity.glsl` | 返回全亮（无阴影） |
| AO | `ao/identity.glsl` | 返回 1.0 |
| 切线空间构造 | `ntb/ntb_none.glsl` | 退化正交基 |

编译器通过内联与死代码消除（DCE）在微秒内生成最优代码。

保留 `#if` 的例外只有一处：`#if defined(HGL_ALPHA_TEST) || defined(HGL_ALPHA_DITHER)`。
它守的是一条**管线状态必须特化**的 `discard`/dither 路径，无法用 include 多态表达。

### 1.4 材质属性全部通过运行时 BDA 传参（零编译开销）
贴图索引号、粗糙度乘数、基础颜色等业务参数，全部通过 `materialDataIndex` 直接从 BDA 内存（`MaterialDataSSBO`）中获取。换参数只需要更新 Buffer 数据，**完全不触发着色器重新编译**。

### 1.5 实测矩阵（StandardOpaque + Sky，已固化并验证）

varying 的**集合**按 (domain, pass, 覆盖判定) 三者确定，**不是按 pass 确定**——这是把「通用模板」思路彻底否定的关键证据。

| 主着色器 | pass | `options.glsl` 提供的宏 | varyings |
|---|---|---|---|
| `standard_opaque/forward_lit.frag` | ForwardColor | `HAS_VERTEX_NORMAL` `USE_MATERIAL_SOURCE_PROVIDER` `USE_SCENE_LIGHTING` `MTL_TEX_NORMAL_CHANNELS` `SHADOW_PCF_POISSON_TAPS` `SHADOW_NORMAL_OFFSET`（+可选 `ALPHA_TEST`/`ALPHA_CUTOFF`） | `fragDataIndexID`@0 `fragWorldPos`@1 `fragWorldNormal`@2 `fragUV0`@3 |
| `standard_opaque/shadow_caster_masked.frag` | ShadowDepth（覆盖判定） | `COVERAGE_ONLY` `HAS_VERTEX_NORMAL` `ALPHA_TEST` `ALPHA_CUTOFF` | `fragDataIndexID`@0 `fragUV0`@3 |
| `standard_opaque/shadow_caster.frag` | ShadowDepth（无覆盖判定） | `COVERAGE_ONLY`（+`HAS_VERTEX_NORMAL`） | **无**（连 include 都没有，纯空 `main()`） |
| `sky/sky.frag` | ForwardColor | 无 | `fragDataIndexID`@0 `fragDirection`@**6** |

三个反直觉但已确认的事实：

1. **无覆盖判定的深度 pass 是彻底的空气着色器**：没有 varying、没有采样、没有任何 include。
   它存在的唯一理由是 depth pass 仍需要一个可编译的 fragment 阶段。不要为了「整齐」往里加东西。
2. **StandardOpaque 的覆盖判定只需要 UV + 材质索引**，与同样有覆盖判定的 `Checker3D`
   不同（后者用 `worldPos` + `worldNormal`）⇒ 覆盖判定的 varying 集合是 **domain 专有**的。
3. **`fragDirection` 落在 location 6**，不是 0。Sky 的 varying 槽位沿用
   `MaterialVertexVaryingConfig` 的位序，改这个数字必须同步改顶点阶段输出布局。

### 1.6 等价性验证方法（做任何 ShaderGen 删除前必须先跑）

判据是 **SPIR-V reflection 文本全等 + SPIR-V 字节数全等**，不是 SPIR-V 逐字节比较：

- 编译**完全相同**的源码两次，产物仍相差约 50 字节（内嵌内容哈希 / 标识符名）⇒ 逐字节比较无意义。
- 内容不同但语义等价的源码，字节差异会有数千处，而 reflection 文本完全相同 ⇒ reflection 才是稳定判据。
- 字节数全等是一个额外的强信号（能抓住「多一个未使用 varying」这类 reflection 看不出的偏差）。

复现方式：直接跑 `scripts/ci/verify-domain-master-shaders.ps1`。它做的事是——用
`ShaderDocumentProductionRegression --dump-source <dir>` 导出**旧路径的真实编译输入**
（`GetFinalGLSL()`，即 glslang 实际吃到的文本），为每个 pass 形状写一份 `generated/options.glsl`，
再用 `spvfc` 分别编译旧文本与新主着色器，比较 reflection 与字节数。脚本退出码 0 = 全等价。
（注意 `--dump-source` 的目录必须已存在——它不创建目录。）

⚠ `--dump-source` 导出的文本**未必能直接编译**：实测同一 fixture 的两次导出，
天空盒那份漏掉了采样器宏（见 1.1 的非确定性说明）。遇到「参考文本编不过」时，
先判断是不是参考文本本身有病，而不是急着改主着色器。

已测通过（5 例，`failures=0`，由 `scripts/ci/verify-domain-master-shaders.ps1` 保证）：

| 主着色器 | options 变体 | reflection 行数 | SPIR-V 字节 |
|---|---|---|---|
| `standard_opaque/forward_lit.frag` | fwd | 24 | 65544 |
| `standard_opaque/forward_lit.frag` | fwd-alpha | 24 | 67496 |
| `standard_opaque/shadow_caster.frag` | shadow | 16 | 1416 |
| `standard_opaque/shadow_caster_masked.frag` | shadow-masked | 22 | 9260 |
| `sky/sky.frag` | sky | 22 | 7132 |

**第二条等价性证据链（进程内，与上表互为交叉验证）**：门禁用例
`DH.domain-backend-equivalence`（`ShaderResourceSchemaRegressionGate all` 的 `pipeline` 分组）
在**同一进程**里把同一个 `MaterialRecipe` 走两条路各建一次构建上下文：

1. 旧路：`GenericMaterialBuilder` → `GetFinalGLSL()` → `ULRE.ShaderCompiler`；
2. 新路：`DomainShaderBuildBackend` → 固定主着色器 + `generated/options.glsl`。

然后两侧都用 `ReflectSPVData()` 取 reflection，逐行格式化（入口点、各阶段 I/O 的
loc/name/base/vec/col、全部 11 类 descriptor 的 set/binding/array/name、push constant 的
offset/size/name）后做**文本全等**比较，再比较 `GetSPVSize()`。用例还带两条**反自证**断言：

- 旧路 GLSL 里**不许**出现 `generated/options.glsl`（否则说明新路代码串进了旧路）；
- 新路 GLSL 里**必须**出现 `generated/options.glsl`（否则说明根本没走固定主着色器）。

两条路都**不挂** `shader_artifact_store`，因此不读也不写磁盘缓存，比较的是纯编译产物。
当前 5/5 通过：fwd 65072 B、fwd-alpha 67024 B、shadow 1232 B、shadow-alpha 8896 B、sky 6732 B。
（字节数与上表不同属正常：上表是 `spvfc` 用脚本写死的 options 编出来的，本节是两条进程内
路径各用自己的 options 生成逻辑，**比的正是这两条路彼此等价**。）

模板路由必须**走引擎路由器**而不是门禁自己按 purpose/alpha 拼：先
`SelectSceneRenderTemplateRequest()` 定模板与 profile，再
`AppendMaterialRenderTemplateRoots()` 把材质能力槽拷进请求。只调前者会因槽缺失被
`ValidateRenderTemplateRequest` 拒；只调后者会给 Sky 选错模板。

### 1.7 磁盘缓存烘焙链（Phase 4.1 固化）

固定主着色器要真正替代生成器，还必须回答一个问题：**SPV 磁盘缓存怎么复用**。
生成器时代每次冷启动都要跑一遍 AST 组装 + glslang；固定主着色器省掉了前者，
但如果缓存语义不对，后者仍会白烧一遍，甚至悄悄写出错误的产物。

缓存键是 `ShaderStageKey`（`stage / definition_hash / glsl_module_graph_hash /
interface_hash / resource_hash / compiler_hash`，六项 FNV1a），**按阶段存**，
和最终 GLSL 文本无关；`generated_source_digest` **不参与**命中判定，只作事后校验。
由此推出两条必须遵守的实现约束：

1. **摘要必须覆盖 domain options。** 主着色器文本对「同领域 + 同 pass」是逐字节相同的，
   options 的全部差异都在 `generated/options.glsl` 里。若 stage source digest 只覆盖
   主着色器文本，fwd 与 fwd-alpha 会算出**同一个摘要**（实测：主着色器文本完全一致，
   SPV 却是 65072 vs 67024）。摘要既是缓存命中判据也是事后校验判据，撞了就只剩
   缓存文件名这一道保险。因此 `CompileDomainMasterFragment` 把 options 的**内容键**
   （`MakeOptionsKey` = FNV1a(options 文本, domain, purpose)）作为溯源注释
   `// domain-options-key <hex>` 钉在片段源尾部：摘要自然覆盖 options，落盘的 `.frag`
   也能自述「这份 SPV 是哪组宏编出来的」。
2. **元数据必须在换源之后重建。** `BuildGenericMaterial` 在领域后端换掉片段源**之前**
   就构造好了 `ShaderProgramArtifactMetadata`，其中的 `generated_source_digest` 描述的
   是被丢弃的生成文本。`FinalizeShaderBuildContext` 会把它原样落盘，所以
   `CreateBuildContext` 必须在 `ApplyDomainMasterShader` 之后、`FinalizeBuildContext`
   之前调 `RefreshProgramArtifactMetadata()`。去掉这一步，门禁立刻报
   `generated_source_digest=… does not match the compiled sources (…)`（已反自证）。

**为什么后端要自己做早期缓存探针**：`FinalizeShaderBuildContext` 的命中路径只是
「不覆盖」已设好的 SPV，并不会跳过已经发生的编译。所以 `CompileDomainMasterFragment`
在 `SetFinalGLSL` 之后立刻用固定主着色器的源算摘要、`LoadStageSPV` 试读、要求
`stored_digest == expected_digest`，命中就直接 `SetCachedSPVData` 返回，完全不碰 glslang。
这对固定领域是合法的：SPIR-V 只取决于（主着色器源, options），而 options 已被
stage key + 源尾注释双重点住。

门禁用例 `DH.domain-backend-cache` 把上述链条钉成四段断言（每个形状独立缓存目录，
因为四种 Lit 形状共享同一个 `ShaderStageKey`）：

1. 冷构建必须**未命中**，并把 SPV 落盘；
2. 同进程再构建必须**命中**，且 SPV 与冷构建**逐字节相同**；
3. 盘上 stage digest 与 program metadata 的 `generated_source_digest` 都必须等于
   按实际编译源重算的值（第 2 条约束的反自证）；
4. 缓存里的 `.frag` 必须含 `generated/options.glsl`（证明是固定主着色器编出来的）。

外加一条跨形状断言：四个形状的片段 stage source digest **两两互不相同**
（第 1 条约束的反自证）。当前 4/4 形状 `cold miss -> warm hit`，
spv = 65072 / 67024 / 1232 / 8896 字节。

⚠ `SaveStageSPV` / `GetSPVSize()` 是**字节**数，不是 32 位字数——照字数 `memcmp` 会越界读，
表现为「明明相同却报不等」。`ShaderArtifactStore` 写盘还要求缓存根目录**事先存在**
（`MakePath` 逐段创建，父目录不存在时整体失败，且一个目录都不建）。

### 1.8 无光族领域的固化经验（Phase 4.2 固化）

Phase 4.2 补齐了 `UnlitTexture` / `VertexColor` / `VertexPalette` / `VertexLuminance`
四个领域。它们都属于「前向 + `flat_lighting`」形状，但**不是** `forward_lit.frag`
少几行——照抄 `standard_opaque` 一定编不过。下面五条都是踩出来的，不写下来下一次
还会再踩。

**① `ntb/ntb_none.glsl` 必须显式 include，且必须排在 `surface/material_surface.glsl` 之前**

`EvalSurface` 在**函数体内**调用 `GetNTB(...)`，而 GLSL 在**定义处**就要解析符号，
所以 `ntb_none.glsl` 不能等到后面再进来。生成器那条路没有这个顺序问题：它把
`ntb_none.glsl` 的内容**文本内联**进最终 GLSL（`#ifndef NTB_NONE_GLSL` 整块就贴在
材质源 include 的正前方）。

漏掉它会看到两条报错，第二条极具误导性：

```
'material_surface.glsl:35': error: 'GetNTB' : no matching overloaded function found
error: '#' : preprocessor directive cannot be preceded by another token
```

第二条只是 `EvalSurface` 那一行被整体判死之后，紧随其后的 `#if` 变成了「前导 token 之后的 `#`」。

**② 面法线兜底块不是可选的——不要「推理掉」看着是死代码的东西**

无光族领域不做任何法线光照，很容易判断「`#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK`
那一块对我没用」。这个判断是**错的**：生成器对**任何前向 pass** 都无条件发射该块
（只有 `shadow_pass` 才跳过），它决定 `si.worldNormal` 是否被填值，进而影响
`EvalSurface` / alpha 覆盖链的产物。删掉它，三个领域的 SPIR-V 会分别少
**64 / 92 / 64 字节**——尺寸差是唯一能看见的信号，而尺寸差又只在逐字节比对里
才被当成失败。

> 通用规则：**照抄生成器，不要讲道理**。等价性验收的对象是「生成器实际产出的字节」，
> 不是「我认为它应该产出的字节」。

**③ 只有两种材质 SSBO 布局，bindings 文件不能互借**

| 布局 | 结构 | stride | 表基址 | 使用者 |
|---|---|---|---|---|
| `PBRSurface` | `{ vec4 base_color; float metallic; float roughness; float normal_scale; float fresnel; }` | 32 | `global_addresses.addr_pbr_surface` | StandardOpaque / **VertexColor** / **VertexPalette** / **UnlitTexture** |
| `EmissiveSurface` | `{ vec4 color; }` | 16 | `global_addresses.addr_emissive_surface` | UnlitSolid / **VertexLuminance** / ErrorChecker3D / DebugNormal |

注意反直觉的两处：`UnlitTexture` 走的是 **PBR** 行（它需要 `base_color` 之外的
`metallic` 等常量来保证 alpha 链的形状一致），而 `VertexLuminance` 走的是
**Emissive** 行。生成器在 Emissive 情况下会同时发射
`struct EmissiveSurfaceData {...}` 与 `layout(buffer_reference…) buffer EmissiveSurfaceRow {...}`，
两份都要照抄。

**④ 纹理表的存在性只由领域契约的槽位表决定**

`UnlitTexture` 有 `base_color` 槽 ⇒ 单元素
`MaterialTextureReferencesRef { uvec2 tex_base_color; }`（stride 16）+ `#define TrilinearSampler 2u`；
`VertexColor` / `VertexPalette` / `VertexLuminance` 的槽位表是空的 ⇒
**整个纹理表都不出现**——没有 `MaterialTextureReferencesRef`，没有任何采样器 `#define`。
所以 `vertex_color_bindings.glsl` 内容是空的（只留守卫宏），这是正确状态，不是漏写。

**⑤ varying location 是顶点/片段两阶段的硬契约，只能查不能猜**

权威来源是生成器 dump 出来的最终 GLSL。当前实测表：

| varying | location | 备注 |
|---|---|---|
| `fragDataIndexID` | 0 | `flat perprimitiveEXT in uint`；**无私有数据行的领域整条不声明** |
| `fragWorldPos` | 1 | |
| `fragWorldNormal` | 2 | |
| `fragUV0` | 3 | |
| `fragStyleID` | 4 | `flat perprimitiveEXT in uint`；**只有 `Text2D` 用**（见 1.9 ④） |
| `fragVertexColor` | 5 | |
| `fragDirection` | 6 | |
| `fragLuminance` | 7 | `float`，不是向量 |

location 之间**留空是正常现象**，不要自作聪明改成紧凑排布。
`VertexColor` / `VertexPalette` 没有私有数据行，因此 `fragDataIndexID` 整条不声明，
`materialDataIndex` 直接是字面量 `0u`。

> 勘误（Phase 4.3）：4 号并不是「永久空位」——`Text2D` 的 `fragStyleID` 就在 4 号。

**⑥ 逐领域目录可以跨域复用，但只能在片段阶段逐字节相同时复用**

`VertexPalette` 与 `VertexColor` 的片段阶段**完全一致**（差异只体现在顶点语义：
`TransformID` / `Size`，而那属于 `DomainContracts` 的事），因此
`DomainShaderTemplate` 允许 `VertexPalette → vertex_color/vertex_color.frag`。
`kDomainDirs[]` 的 `domain_dir` / `forward_file` 因此是多对一的映射，不是 1:1。

**等价矩阵（Phase 4.2 后的 9 形状，`DH.domain-backend-equivalence` 全绿）**

| 固定主着色器 | pass | SPIR-V 字节 |
|---|---|---|
| `standard_opaque/forward_lit.frag` | ForwardColor | 65072 |
| `standard_opaque/forward_lit.frag`（alpha） | ForwardColor | 67024 |
| `standard_opaque/shadow_caster_masked.frag` | ShadowDepth + coverage | 8896 |
| `standard_opaque/shadow_caster.frag` | ShadowDepth 裸 | 1232 |
| `sky/sky.frag` | ForwardColor | 6732 |
| `unlit_texture/unlit_texture.frag` | ForwardColor | 13360 |
| `vertex_color/vertex_color.frag` | ForwardColor | 10248 |
| `vertex_luminance/vertex_luminance.frag` | ForwardColor | 12152 |
| `vertex_color/vertex_color.frag`（作 VertexPalette） | ForwardColor | 10248 |

**无光族的 `options.glsl` 是空文件**（4 个形状实测均为 0 字节）。这**不是**发射器漏发：
`ResolveDomainOptionsProfile` 对「无光照 + 有材质源」给出 `ForwardUnlit`，而该 profile
不产生任何 `#define`——它们的主着色器全部走 `flat_lighting.glsl`，没有场景光照、
没有 PCF、没有 ambient provider 需要注入。空文件依然必须存在且必须被第二 include 根
解析到，否则 `#include "generated/options.glsl"` 直接失败。

**`verify-domain-master-shaders.ps1` 的参考源也换成了门禁**：脚本过去依赖
`ShaderDocumentProductionRegression --dump-source` 的生产夹具，而那套夹具里
**根本没有**无光族领域，且夹具名与形状名不同名，扩表必然要手抄映射。
现在统一走 `ShaderResourceSchemaRegressionGate dump-domain-glsl <dir>`：它按
`kDomainBackendEquivShapes` 一次性导出 9 份 `<slug>.frag` **和**
`<slug>/generated/options.glsl`（后者由真实后端的 `EmitDomainOptionsForDefinition`
产出），脚本不再持有任何宏表，也就不可能与发射器漂移。

### 1.9 内建定义族领域与「骨架」事实（Phase 4.3 固化）

Phase 4.3 一次性补齐了剩下六个**定义驱动**的领域：`UnlitSolid`（`builtin/pure_color`）、
`Checker2D`、`Checker3D`（`builtin/checkerboard_3d`）、`ErrorChecker3D`、
`Text2D`（`builtin/text_gpu`）、`DebugNormal`。等价矩阵由 9 形状扩到 **15 形状**。
这一轮暴露的问题与前几轮完全不同：不是「漏了哪个模块」，而是**每一行的模板归属**。

**① 主着色器属于哪条「骨架」，决定尾段有没有 alpha 分支**

生成器只有四份片元骨架（`ShaderLibrary/fragment/*.glsl.tmpl`）：

| 骨架 | 使用它的领域 | `main()` 尾段 |
|---|---|---|
| `forward_lit` | StandardOpaque、Foliage、Terrain、HumanSkin | 有 `HGLApplyAlpha(EvalAlpha(...))` 块 **和** `#if defined(HGL_ALPHA_OUTPUT)` 分支 |
| `forward_unlit` | UnlitTexture、UnlitSolid、VertexColor/Palette/Luminance、Checker2D/3D、ErrorChecker3D、DebugNormal、**Text2D** | 只有 `WriteMaterialOutput(HGLComposeColor(finalColor));` |
| `sky` | Sky | 同上（且 `si` 完全由 `fragDirection` 填） |
| `shadow_caster` | StandardOpaque 深度 pass | `main()` 可为空实现 |

`Text2D` 是**唯一**「看起来该有 alpha 分支、实际属于 unlit 骨架」的领域：
它 `alpha_blend = true`，`options.glsl` 里确实有 `#define HGL_ALPHA_OUTPUT 1`，
但骨架里**没有**任何消费者。照着 `unlit_texture.frag` 补上那两段 → SPIR-V 从
18200 涨到 19560 字节，等价性直接失败。

> 通用规则：**骨架归属看 `RenderTemplateID`，不靠宏推断**。
> `alpha_output` 宏存在 ≠ 骨架里有对应分支。真正的 alpha 通路是
> `HGLComposeColor` → 内部 `HGLApplyAlpha(color.a)`，且 `flat_lighting` 已把
> `surface.alpha` 带进 `finalColor.a`。

**② 两族宏原先在 `options.glsl` 之外，Phase 4.3 才收进来**

到此为止 `options.glsl` 只发模板固定宏 + 覆盖率宏，于是有两个真实构建请求
**无处表达**，只能继续走 `compile_defines`（散落在编译命令行）：

| 宏 | 语义来源 | 消费者 |
|---|---|---|
| `HGL_MATERIAL_HAS_VERTEX_NORMAL` | 几何顶点格式含 Normal × 材质要法线光照 | `material/checkerboard_3d_source.glsl`、`material/error_checkerboard_3d_source.glsl` |
| 定义自带 `defines`（如 `TEXT_SDF_ENABLED`） | `.mi.toml` 的 `defines` 表 | `material/text_source_gpu.glsl`（SDF / 位图双路） |

现在两者都由 `DomainOptionsInput` 承载：

```cpp
bool         vertex_normal_present   = false;  // 与 face_normal_fallback 严格互补
const char *const *definition_defines = nullptr;
uint32       definition_define_count  = 0;     // 逐条发射 `#define <name> 1`
```

`HGL_MATERIAL_HAS_VERTEX_NORMAL` 与 `HGL_MATERIAL_FACE_NORMAL_FALLBACK` 是**同一个谓词
的两种结果**（`GenericMaterialBuilder::PrepareResolvedDefinition` 只推其中一个），
实现上互为镜像，别把它们当成两个独立开关。

**③ 四个 Emissive 行领域共享一份 row bindings**

`UnlitSolid` / `VertexLuminance` / `ErrorChecker3D` / `DebugNormal` 的
`material_private_data` 全是 `EmissiveSurface`（跨度 16）。原先每个领域各有一份
`*_bindings.glsl`，四份内容逐字节相同 ⇒ 抽成
`ShaderLibrary/domain/emissive_row_bindings.glsl`（`vertex_luminance_bindings.glsl`
已删除）。抽的依据是「这一层只依赖 `DomainContracts[].material_private_data`，
不依赖领域语义」——所以它可以跨领域复用，而**领域专属的贴图表不能**（②里的
`text_2d_bindings.glsl` 就必须独立）。

**④ `Text2D` 的 `fragStyleID` 在 location 4，且纹理表只有一张图**

```glsl
layout(location=0) flat perprimitiveEXT in  uint fragDataIndexID;
layout(location=3) in vec2 fragUV0;
layout(location=4) flat perprimitiveEXT in  uint fragStyleID;   // ← 只有 Text2D
layout(location=5) in vec4 fragVertexColor;
```

文字颜色来自顶点色、样式来自 `sbo_char_style` 样式表，**都不在材质行里**；
但贴图表仍然存在（`base_color` 槽 = 字体图集），所以 `fragDataIndexID` 不能省。
`LinearSampler` / `NearestSampler` 两个宏**都要发**：SDF 走 Linear、位图走 Nearest，
而选哪条由定义自带 `TEXT_SDF_ENABLED` 决定 —— 过滤方式属于**定义**而非领域，
物理文件里必须同时给出两个索引（多余的那个没有消费者，零代码）。

**⑤ `Checker2D` 是最简形状：没有 varying、没有数据行、没有面法线兜底**

`materialDataIndex` 是字面量 `0u`，`main()` 只有一行 `EvalSurface` 调用；
`options.glsl` 为空。它的 `checker_2d_bindings.glsl` **刻意留成空文件**（只守卫宏），
与 `vertex_color_bindings.glsl` 同构：把「本领域无数据行、无贴图」写成可见的物理事实。

**⑥ 空 bindings 文件是有意义的，不要合并删除**

`checker_2d_bindings.glsl` / `checker_3d_bindings.glsl` 除了 `#ifndef` 守卫什么都没有。
保留它们的理由：主着色器的 `#include` 行是**结构**，不是条件；将来契约新增槽位时，
改动点只有一个文件，而不是回来重新判断「这个领域当初为什么没有 bindings」。

**等价矩阵（Phase 4.3 后的 15 形状，`DH.domain-backend-equivalence` 全绿）**

| slug | 定义 | 领域 | `options.glsl` | SPIR-V 字节 |
|---|---|---|---|---|
| `fwd` | Lit | StandardOpaque | 4 宏 + ambient include | 65072 |
| `fwd-alpha` | Lit | StandardOpaque | 6 宏 + ambient include | 67024 |
| `shadow` | Lit | StandardOpaque | 覆盖率宏 | 1232 |
| `shadow-alpha` | Lit | StandardOpaque | 覆盖率宏 | 8896 |
| `sky` | SkyCube | Sky | 空 | 6732 |
| `unlit-tex` | UnlitTexture | UnlitTexture | 空 | 13360 |
| `vert-color` | VertexColor | VertexColor | 空 | 10248 |
| `vert-pal` | VertexPaletteColor | VertexPalette | 空 | 10248 |
| `vert-lum` | VertexLuminance | VertexLuminance | 空 | 12152 |
| `unlit-solid` | `builtin/pure_color` | UnlitSolid | 空 | 12428 |
| `checker-2d` | `builtin/checkerboard_2d` | Checker2D | 空 | 10620 |
| `checker-3d` | `builtin/checkerboard_3d` | Checker3D | `HGL_MATERIAL_FACE_NORMAL_FALLBACK` | 13824 |
| `err-check-3d` | `builtin/error_checkerboard_3d` | ErrorChecker3D | `HGL_MATERIAL_FACE_NORMAL_FALLBACK` | 15900 |
| `text-gpu` | `builtin/text_gpu` | Text2D | `HGL_ALPHA_OUTPUT` + `TEXT_SDF_ENABLED` | 18200 |
| `debug-normal` | DebugNormalColor | DebugNormal | 空 | 13000 |

（表内是门禁 `spv=` 报告的 Debug 构建字节数；`verify-domain-master-shaders.ps1`
用 `spvfc` 走 Release 布局，同一形状数值不同，两边各自内部自洽即可。）

**`kDomainBackendEquivShapes[]` 是只追加的表**：`DH.domain-backend-cache`
按下标引用条目（`[13]` = text-gpu、`[14]` = debug-normal），重排会静默把缓存用例
指向别的形状。这条约束已写进数组注释。

**覆盖到的模板差异**：15 形状里 `forward_lit` 4 份、`forward_unlit` 9 份、
`sky` 1 份、`shadow_caster` 2 份 —— 四份骨架全部至少被一份主着色器复现。

---

## 2. 待清理的旧 ShaderGen 模块清单

实现该终极架构后，`src/ShaderGen` 内部庞大的历史包袱将全部彻底删除：

| 模块 / 类 | 现有代码行 | 历史职责 | 终极形态处理 |
|---|---|---|---|
| `GenericMaterialBuilder.cpp` | 929 | 统筹材质依赖、插槽选择、AST 装配 | **彻底删除** |
| `MaterialShaderEmitter.cpp` | 675 | 动态拼接 GLSL 文本与宏定义 | **彻底删除** |
| `ShaderCodeModuleFile.cpp` | 573 | 解析 `// @ulre` 12 种注记表 | **彻底删除** |
| `FragmentTemplateComposer.cpp` | 499 | 动态拼接片元模板插槽与输出声明 | **彻底删除** |
| `ShaderCodeModuleRegistry.cpp` | 418 | 模块图维护、DFS 依赖遍历、剔除环 | **彻底删除** |
| `ShaderDocument.cpp` / AST | 173 | 维护代码块顺序、自述校验 | **彻底删除** |
| `RenderTemplateResolve.cpp` | 150 | 动态解析模板根与插槽角色 | **彻底删除** |
| **合计** | **3400+ 行**（核心逻辑占 ~38%） | 全部属于 C++ 人肉代码预处理器 | **替换为一个不到 200 行的极简 OptionsEmitter** |

---

## 3. 演进路线图（含实际进度）

1. **第一阶段：固化各 MaterialDomain 的主着色器** ✅ `709eb58ce`
   - 已固化：`StandardOpaque` 的 ForwardColor / ShadowDepth(有覆盖) / ShadowDepth(无覆盖)，`Sky` 的 ForwardColor。
   - 已定义 `options.glsl` 注入协议与 `<domain>_bindings.glsl` 分层。
   - `4.2` 补入无光族四域，累计 9 形状（见 1.8）。
   - 待补：`UI2D` / `Foliage` / `Terrain` / `HumanSkin` / `Custom`（**当前没有任何材质
     定义指向它们**，属阶段五之后的按需补齐，不再是阻塞项）。
2. **第二阶段：实现极简 `DomainOptionsEmitter`** ✅ `fc70e0847`
   - 输入 `MaterialDomain` + Pass 类型，输出 `options.glsl` 文本（8 步发射顺序、4 路 profile 表）。
   - 不做任何依赖推导、不做任何模块排序——模块顺序已经物理写在主着色器里。
   - `DomainOptionsRegression.exe` 26 项纯度断言（只有 `#define`/`#include`、无空行、LF 结尾、
     宏集合与 profile 精确匹配、不再发射已死的 `HGL_USE_*`）。
3. **第三阶段：直通编译后端 `DomainShaderBuildBackend`** ✅
   - `3.0` ✅ `b26f738ea`：把 glslang 桥从 `src/ShaderGen` 提成独立模块 `ULRE.ShaderCompiler`，
     用 `SetShaderCompilerLifecycleHooks()` 反转生命周期，消除「运行时反向依赖生成器」。
     现在的模块边界是
     `SceneGraph/Work/Tools → ULRE.ShaderCompiler → GLSLCompiler.dll`，
     而 `ULRE.ShaderGen` 只是从上方注册后端钩子。
   - `3.1` ✅ `77efd2ede`：`DomainShaderTemplate` 查找表 + `DomainShaderBuildBackend`，
     多 include 根支持（`includes[0]=ShaderLibrary`，`includes[1]=generated`，上限 2 个），
     并由环境变量选择后端（默认仍是生成器，故是**可选项**而非切替）。
   - `3.2` ✅：`DomainShaderTemplate` 增加**逐领域**前向文件名与 `supports_depth`
     （`standard_opaque/forward_lit.frag`、`sky/sky.frag`；Sky 不参与深度 pass），
     修掉「把 Sky 的前向 pass 也拼成 `forward_lit.frag`」的隐患；新增门禁用例
     `DH.domain-backend-equivalence`（见 1.6），并给领域后端补
     `GetLastDomainShaderBuildBackendError()`（`GLogError` 在门禁里不可见，诊断必须走字符串出口）；
     顺带刷新了仓库根的 `GLSLCompiler.dll`——它一直是**旧的单 include 根构建**，与插件源码脱节。
4. **第四阶段：业务域全面接入新管道并打通全 Pass**
   - `4.1` ✅ `b0110c8be`：修掉 `generated_source_digest` 描述「被丢弃的生成文本」的真 bug，
     后端加早期缓存探针，`DH.domain-backend-cache` 落地（见 1.7）。
   - `4.2` ✅：无光族四域（`UnlitTexture` / `VertexColor` / `VertexPalette` /
     `VertexLuminance`）主着色器固化，等价矩阵扩到 9 形状（见 1.8）。
     `kDomainDirs[]` 由 2 行扩到 6 行，并支持跨域复用同一份片段源。
   - `4.3` ✅：内建定义族六域（`UnlitSolid` / `Checker2D` / `Checker3D` /
     `ErrorChecker3D` / `Text2D` / `DebugNormal`）固化，等价矩阵扩到 15 形状；
     `kDomainDirs[]` 12 行，四份片元骨架全部被复现；`options.glsl` 收编最后两族
     外挂宏（`HGL_MATERIAL_HAS_VERTEX_NORMAL`、定义自带 `defines`）。
     详见 1.9。
   - 待办（`4.4`）：跑 `SimpleSphere` / `PBRSpheres` / `LineRender` / `DrawText`
     实机验证 + `verify-release-form.ps1`。
5. **第五阶段：整体切替与旧代码物理删除**
   - 将渲染系统底层对接至直通管道。
   - 一次性物理删除 `src/ShaderGen` 下冗余的生成器、注记解析与 AST 模块。

