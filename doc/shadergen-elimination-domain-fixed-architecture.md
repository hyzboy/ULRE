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
门禁 `DH.domain-backend-equivalence`（`golden/domain-backend/*.equiv.txt` 是这 15 个形状的
反射文本 + SPV 体积冻结基线，任何发射漂移都会在那里现形）：

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

复现方式：门禁 `all` 里的 `DH.domain-backend-equivalence`（退出码 0 = 15 个形状全部
与冻结基线一致）。历史上的做法是——用 `ShaderDocumentProductionRegression --dump-source <dir>`
导出**旧路径的真实编译输入**（`GetFinalGLSL()`，即 glslang 实际吃到的文本），为每个 pass
形状写一份 `generated/options.glsl`，再用 `spvfc` 分别编译旧文本与新主着色器，比较 reflection
与字节数。旧生成器路径在阶段五-2-g 被物理删除后，**该参考侧已不可再生**：等价性证据已在
删除之前冻结进 `src/Tools/ShaderGen/golden/domain-backend/*.equiv.txt`（门禁拥有并逐次比对），
`scripts/ci/verify-domain-master-shaders.ps1` 与门禁的 `dump-domain-glsl` 子命令一并退役。

⚠ `--dump-source` 导出的文本**未必能直接编译**：实测同一 fixture 的两次导出，
天空盒那份漏掉了采样器宏（见 1.1 的非确定性说明）。遇到「参考文本编不过」时，
先判断是不是参考文本本身有病，而不是急着改主着色器。

冻结时的实测结果（5 例，`failures=0`）：

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

**（已退役）`verify-domain-master-shaders.ps1` 与门禁 `dump-domain-glsl`**：脚本过去依赖
`ShaderDocumentProductionRegression --dump-source` 的生产夹具，而那套夹具里
**根本没有**无光族领域，且夹具名与形状名不同名，扩表必然要手抄映射。
后来统一走 `ShaderResourceSchemaRegressionGate dump-domain-glsl <dir>`：它按
`kDomainBackendEquivShapes` 一次性导出 9 份 `<slug>.frag` **和**
`<slug>/generated/options.glsl`（后者由真实后端的 `EmitDomainOptionsForDefinition`
产出），脚本不再持有任何宏表，也就不可能与发射器漂移。
阶段五-2-g 物理删除生成器后，`dump-domain-glsl` 赖以生产的「旧路径最终 GLSL」已不存在
（该子命令会以 `fixed_fragment_source is required` 硬失败），故**二者一并删除**：
等价性证据已冻结在 `golden/domain-backend/*.equiv.txt`，由
`DH.domain-backend-equivalence` 逐次比对 —— 一份更持久、且不依赖已删代码的保证。

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

（表内是门禁 `spv=` 报告的 Debug 构建字节数。已退役的 `verify-domain-master-shaders.ps1`
当年用 `spvfc` 走 Release 布局，同一形状数值不同，两边各自内部自洽 —— 那也是它作为
独立证据的一层价值；退役后这层「第二编译器交叉验证」不再存在，字节数只作漂移探测器。）

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
| `GenericMaterialBuilder.cpp` | 929 | 统筹材质依赖、插槽选择、AST 装配 | **彻底删除**（阶段六；5.2 起已不再拼装片段） |
| `MaterialShaderEmitter.cpp` | 675 | 动态拼接 GLSL 文本与宏定义 | ✅ 已删；活的一半（`DumpShaderGenGLSL` / `BuildSamplerMacros` / `BuildMaterialStageDocument`）并入 `MaterialShaderCompiler.cpp`，等阶段六随网格阶段一起消失 |
| `ShaderCodeModuleFile.cpp` | 573 | 解析 `// @ulre` 12 种注记表 | 保留（阶段六）：`VertexABIBuilder` 仍靠它读 provider 模块元数据 |
| `FragmentTemplateComposer.cpp` | 499 | 动态拼接片元模板插槽与输出声明 | ✅ 已删 |
| `ShaderCodeModuleRegistry.cpp` | 418 | 模块图维护、DFS 依赖遍历、剔除环 | 保留（阶段六）：同上 |
| `ShaderDocument.cpp` / AST | 173 | 维护代码块顺序、自述校验 | 保留：已退化为纯"文本块容器 + 出处"，网格阶段仍在用 |
| `RenderTemplateResolve.cpp` | 150 | 动态解析模板根与插槽角色 | 保留（阶段六）：`resolved_template_hash` 是缓存键的一维 |
| **合计** | **3400+ 行**（核心逻辑占 ~38%） | 全部属于 C++ 人肉代码预处理器 | 片元侧已清零；剩余全部只在网格/顶点阶段服务，阶段六收尾 |

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
   - `4.4` ✅：示例实机验证 + 端到端回归，抓出两个真 bug：
     - **后端注册 TU 被静态库链接丢弃**（自 `3.0` 起潜伏）。`ULRE::Engine` 对
       `ULRE.ShaderGen` 走普通静态库链接，而 `ShaderGenProgramBuildBackend.cpp`
       唯一的入口是「无导出符号、无人引用」的注册对象，于是整块 TU 被链接器剔除，
       `SetShaderCompilerLifecycleHooks()` 从未被调用、钩子恒空、没有任何后端被安装，
       运行时静默降级到 manifest 后端（`EnsureShaderProgramManifestBackend` 是
       先到先得），所有示例的每个图元都 `unresolved runtime pipeline` 却不报错。
       根 `CMakeLists.txt` 改用 `$<LINK_LIBRARY:WHOLE_ARCHIVE,ULRE.ShaderGen>`。
       发行形态依旧正确：`ULRE_ENABLE_SHADERGEN=OFF` 时该目标不存在，钩子空是**设计**，
       manifest 后端读烘焙缓存即可（`verify-release-form.ps1` 硬断言 prod 树无生成器产物）。
     - **顶点 ABI 用了错误的 `vertex_node_config`**。`ResolveVertexABI` 把
       `ResolveMaterialVertexNodeConfig()` 的结果只写进 `plan.vertex_node_config`
       （mesh 发射器用），喂给 `BuildResolvedMaterialVertexABI` 的却是
       `plan.vertex_definition`——里面存的是定义层最弱那一层（默认 `Passthrough3D`）。
       recipe 显式设置 `PixelToLocal` 时就出现「发射器按 2 分量、对账按 3 分量」，
       `VertexNodeConfigResolver::CheckPositionComponentCount` 硬失败
       （`DrawTriangle` 的 `DrawTriangle.VertexColor` 即此）。现在求解结果回写进
       `plan.vertex_definition.vertex_node_config`，两端同源。
     - 结果：五个示例（`SimpleSphere` / `PBRSpheres` / `LineRender` / `TextDraw` /
       `DrawTriangle`）零着色器错误；`verify-release-form.ps1` 全 5 步
       `[PASS] release form verified`；门禁 `all` 61/0、`DomainOptionsRegression` 26/0、
       `ShaderDocumentProductionRegression --full` 通过。
       （当时还跑过 `verify-domain-master-shaders.ps1` 15/15，该脚本随生成器删除在
       5.4 退役，见 §1.8。）
5. **第五阶段：整体切替与旧代码物理删除**
   - `5.1` ✅：**默认后端反转为领域后端**。选择器改名为
     `IsShaderGenBuildBackendRequested()`：未设 `ULRE_SHADER_BUILD_BACKEND` 即返回 `false`，
     `InstallSelectedShaderProgramBuildBackend()` 走 `InstallDomainShaderBuildBackend()`；
     生成器后端只在显式 `ULRE_SHADER_BUILD_BACKEND=shadergen` 时可达。
     保留反向选择器只为 `5.2` 删除前留一条原地回切的对照路径，删除落地后一并消失。
     实证（避免"没报错即通过"的假验证）：默认运行的日志里会出现
     `domain-options/<hash>/generated/options.glsl` 与 `domain/vertex_color/vertex_color.frag`，
     显式选 `shadergen` 时这两行消失、改读 `vertex/s3_clip_passthrough.glsl`——两条路径互斥可见。
     五个示例如常零着色器错误，门禁 61/0、`DomainOptionsRegression` 26/0、
     领域主着色器等价 15/15、`verify-release-form.ps1` 全 5 步 `[PASS]`。
     注意：本次未动 `ShaderGenProgramBuildBackend.cpp` 里的生命周期注册对象，
     `5.2` 删除它时必须把注册与选择器迁到存活的 `DomainShaderBuildBackend.cpp`。
   - `5.2`（进行中）：将渲染系统底层对接至直通管道后，分批物理删除 `src/ShaderGen`
     下冗余的生成器、注记解析与 AST 模块。实际落地分四步（原始清单曾假定"整体可删"，
     核对代码后收窄为片段侧——网格/顶点阶段要到阶段六才物理固定）：
     - `5.2-a` ✅ `b6be01478`：`fixed_fragment_source` 在**构建前**传入，片段 stage key
       的 `definition_hash` 直接吃这份真正会被编译的文本（事后换源会让键描述一份从未
       被编译的代码）。
     - `5.2-b` ✅ `c91e687c9`：生命周期接线搬出待删的生成器 TU，独立成
       `compile/ShaderBuildBackendLifecycle.cpp`——它无对外符号、靠 `WHOLE_ARCHIVE`
       存活，跟着生成器一起消失的症状是"发行形态没有后端"，事后才暴露。
     - `5.2-c` ✅ `a60ef252d`：`DH.domain-backend-equivalence` 冻结为
       `golden/domain-backend/<shape>.equiv.txt`（reflection 文本 + `spv_bytes`），
       15 形状全录；录制时两侧现场互证，再由字节落盘，默认路径完全不碰生成器。
     - `5.2-d` ✅ `6449b850e`：门禁 8 处 `CreateMaterialFromDefinition` →
       `BuildGenericMaterial`（前者本就是后者的薄包装），把门禁从待删 TU 上摘下来。
     - `5.2-e` ✅：删「生成器自述」通道（`ShaderStructureDump` + 30 份 golden），
       `SPV.generated-interface`（真编译 + 反射）守同一批形状；顺带把
       `StructureDumpVariant` 正名为 `ReflectionCoverageVariant`，避免旧名字诱导误删。
     - `5.2-f` ✅ `d5772b486`：`fixed_fragment_source` 升为硬前置，五处残余入口
       全部先 `ResolveFixedFragmentSource` 再建；`GenerateStageSources` 的片段拼装
       分支成为静态死代码。该变化当场证出真问题：`sky.frag` 声明 location 0 的
       `fragDataIndexID`，而 `sky_minimal.mi.toml` 未覆盖它 → 链接期 "no matching"。
       **固定片段定义了领域的最低输入集，配方必须覆盖它。**
     - `5.2-g` ✅：物理删除 `FragmentTemplateComposer`、`DocumentFragmentBuilder`、
       `ShaderGenProgramBuildBackend`（含两个 `CreateMaterialFromDefinition` 重载与
       `ULRE_SHADER_BUILD_BACKEND` 选择器）。`MaterialShaderEmitter` **未整份删除，
       而是并入 `MaterialShaderCompiler.cpp`**：只有 `BuildCodeModuleDocument` 真死，
       `DumpShaderGenGLSL` / `BuildSamplerMacros` / `MaterialDefinitionRequiresPayloadRow`
       / `BuildMaterialStageDocument`（网格阶段仍需要 SSBO 声明与 FS 索引表）全活。
       `RenderTemplateResolve.cpp` 实测不可删（`resolved_template_hash` 是缓存键的一维，
       且门禁仍有模板解析用例），留到阶段六随构建器一起处理。
       `glsl_module/*` 与 `MaterialShaderCompiler.cpp` 保留（前者是网格阶段的 provider
       元数据查询，后者是"最终 GLSL → SPV + 反射"的唯一实现）。
       验收：门禁 58/0、`DomainOptionsRegression` 26/0、`TestCSMIncrementalPass` 51 条全过、
       四个示例零着色器错误。

## 阶段六：顶点 / 网格阶段盘点（6.1）

本节记录 6.1 对当前代码和仓库内定义/调用点的盘点。**源码尚未切换**；后续 6.2
必须用实际编译输入验证候选分组，不能把本节的维度表误当作已经完成的 shader 文件集。

### 6.1.1 先纠正两个边界

- 当前 Vulkan 图形路径实际编译的是 `ShaderStage::Mesh`。`MeshShaderMode::VertexPassthrough`
  仍然是 mesh shader（模拟传统 VS 的每线程顶点搬运），并不存在供该路径选择的独立
  `ShaderStage::Vertex` GLSL。故 6.2 的直接目标应是固定 `.mesh`；只有未来确有传统 vertex
  stage 消费者时，才增加 `.vert`。原计划的 `.vert (+ .mesh)` 不能作为当前实现既定事实。
- `vertex_node_config` 不是“14 种节点配置”。代码里有 **7 种 Stage 2 映射模块 + 7 种
  Stage 3 投影/朝向模块**；配置是 5 个枚举字段组成的有效组合，并且运行时 recipe
  可以覆盖定义的缺省值。不能把 14 个模块数当成 14 个完整配置或 14 个固定 shader。

### 6.1.2 变异维度与当前取值

| 维度 | 当前源码选择规则 / 已知取值 | 对固定源码的影响 |
|---|---|---|
| `MeshShaderMode` | 恰好三种：`VertexPassthrough`、`LineQuad`、`CharQuad`。显式 `CharQuad` 定义优先；否则 `PrimitiveType::Lines` 选 `LineQuad`；其余选 `VertexPassthrough`。 | 三种不同主体和拓扑，必须成为不同结构形状；不能只按领域选择。 |
| `VertexInputMode` / Position 格式 | ABI 当前只闭环支持 `R32G32B32_SFLOAT → Vec3`、`R32G32_SFLOAT → Vec2`、`R16G16_SINT → Vec2Int` 三种位置解码模块，三对闭环登记在 `VertexABIBuilder.cpp` 的 `POSITION_MODULE_TABLE[]`（`{format, component_count, module_name}`），精确匹配、无别名无回退。`CharQuad` 自带 SSBO，不读外部位置（`VertexInputMode::None` 放行）。 | Stage 1 输入模块与 ABI 绑定，文件选择必须以已解析的几何格式为准，而非只看材质配置。 |
| Stage 1 其他属性解码 | UV：`s1_uv` / `s1_uv_rg16f`；NTB：`s1_ntb` / `s1_ntb_rg8` / `s1_ntb_rg16f` / `s1_ntb_a2bgr10`；Color：属性色 / palette index；另有 Luminance、TransformID、Size。它们由语义需求及属性格式共同选择。 | 引用模块集合不同即为不同源码依赖闭包；不能靠一个领域名推导。 |
| Stage 2 `position_mapping` | 7 个固定模块：`Passthrough3D`、`LiftXY_XY0`、`LiftXY_X0Y`、`LiftXY_0XY`、`NDCLift`、`ZeroOneToNDC`、`PixelToLocal`。只有 `Passthrough3D` 要 3 个输入分量，其余要 2 个；与 Position 解码格式不符时必须失败。 | 固定模块路径可查表，但不能把不同 mapping 当作同一输入 ABI。 |
| Stage 3 | 7 个固定模块：world camera VP、L2W only、ortho viewport、ortho then L2W、clip passthrough、camera-facing world、camera-facing fixed pixels。朝向为 camera-facing 时按 scale 选后两者，普通朝向按 projection 选前五者。 | 有世界空间输出能力差异；也决定 WorldPosition / WorldNormal 是否可写。 |
| `MaterialDomain` 规范节点配置 | 16 个非 `Custom` 契约中，13 个使用默认 3D；`UnlitTexture` 与 `Checker2D` 共用 `Vec2 + NDCLift + World + ScaleMode::World + LocalToWorldOnly`；`Text2D` 为 `None + PixelToLocal + World + ScaleMode::World + OrthoViewport`。即当前契约只有 3 种规范 tuple。 | 规范值可作为各领域默认值，**不能**据此忽略 recipe override。 |
| 仓库内 recipe 节点覆盖 | 已发现的有效覆盖形状：默认 3D；2D NDC→L2W；2D `[0,1]`→NDC→L2W；2D PixelToLocal→OrthoViewport（`DrawTriangle`）；Vec2 LiftXY→WorldCameraVP（网格/线）；CameraFacingFree→world-facing。`CameraFacingAxisY`、fixed-pixel、ortho-then-L2W 也在 API 枚举中可达，但当前搜索未发现仓库调用点。 | 至少需覆盖上述真实调用形状；公开 API 可达组合若保留，则需有稳定查表拒绝/选择策略，而不是生成时临时拼 GLSL。 |
| varying 接口 | mesh 可输出 8 种语义：DataIndexID、Color、UV0、WorldPosition、WorldNormal、Luminance、FragDirection、StyleID。PerVertex 可写集合与 LineQuad（DataIndexID / Color / Luminance / WorldPosition）及 CharQuad（UV0 / Color / DataIndexID / StyleID）不同；Color 还分属性色 / palette 路径。 | 声明 location、插值类别、数组容量与写入语句都属于链接接口。固定文件必须覆盖真实的语义集合与写入来源，不能只保证各 stage 单独编译。 |
| Pass / coverage | DepthOnly 与 ShadowDepth 走 depth 路径；有效 varying 按 alpha coverage 需求裁剪，无 alpha 求值时可能为空；alpha-test / dither 需要保留对应覆盖输入。 | 文件候选需以**解析后的 stage interface** 分组；两种 depth purpose 只有在归一化接口相同时才可共用。 |
| `max_invocations` / 拓扑容量 | VP、LineQuad 理想值均为 64，可被设备的 workgroup、输出顶点、输出图元上限折小；CharQuad 固定为 `TEXT_CHARQUAD_MAX_INVOCATIONS`（当前 64），设备不足必须失败。输出容量分别为 VP `(3×N, N)`、LineQuad/CharQuad `(4×N, 2×N)`。 | `local_size_x`、`max_vertices`、`max_primitives` 可由 options 常量承载；CharQuad 的 64 是 CPU dispatch 契约，不能被钳低。若保留普通模式钳制，最终 N 仍必须纳入 shader program identity。 |

### 6.1.3 固定文件集的归一化键

初步最小分组应按**实际消费的 mesh 源形状**合并，而不是复制一份给每个 domain。
候选身份至少包含：

```text
mode
+ position decoder + Stage 1 attribute decoder/include closure
+ effective Stage 2 / Stage 3 module identity
+ resolved stage-interface entries (semantic, location, interpolation, per-vertex/per-primitive)
+ color source
+ pass/coverage interface
+ final workgroup/topology constants (unless safely represented as options constants)
```

`MaterialDomain` 提供默认契约和 fragment 侧最低输入约束，但并非所有 mesh 源差异的唯一拥有者；
recipe 覆盖、几何 ABI、图元类型、coverage 与设备 profile 都会改变实际输入。只有当两个形状
在上述消费契约上相同，且最终 mesh GLSL / include 闭包相同，才可共用一个物理文件。

这不要求引入运行时 shader generator：运行时可用已解析的 program shape key 查找一份**预先存在**
的 `.mesh` 文件，再用 `generated/options.glsl` 提供合法的数值常量；不得在 C++ 中重排块、
枚举 varying、拼接 include 或发射 body。若仍要求结构差异经宏切换，就会重新引入条件编译；
阶段六的“无 `#if`”约束下，结构差异应落在独立固定源码文件中。

**生产 fixture 实测（非全量）**：`ShaderDocumentProductionRegression --full --dump-source`
导出 15 个可构建 fixture 的真实 Mesh `GetFinalGLSL()`；按完整文本 SHA-256 聚类为 9 组：

| 固定源码候选组 | 完全相同的 fixture 输出 |
|---|---|
| Checker2D forward | `checkerboard-2d-forward` / `checkerboard-2d-alpha` |
| Checker3D face-normal forward | `checkerboard-3d-face-forward` / `checkerboard-3d-face-alpha` |
| Checker3D vertex-normal forward | `checkerboard-3d-normal-forward` / `checkerboard-3d-normal-alpha` |
| UnlitSolid forward | `pure-color-forward` |
| StandardOpaque forward | `lit-forward` / `lit-forward-alpha` / `lit-ibl` |
| StandardOpaque shadow | `lit-shadow` |
| StandardOpaque masked shadow | `lit-shadow-alpha` |
| Sky forward | `sky-cube` / `sky-minimal` |
| Text2D CharQuad | `text-gpu-charquad` |

这些组只证明上述 fixture **当前输出可合并**，不是最终文件数量：本组没有 `LineQuad`、
`R16G16_SINT + PixelToLocal` 的 `DrawTriangle/VertexColor`、全部注册定义和全部几何格式。
同一次 `--full` 有 9 个 depth/shadow fixture 因领域固定片段主着色器覆盖缺口被显式跳过
（UnlitSolid 2、Checker2D 2、Checker3D 5）；不能把跳过数计作覆盖或从表中推断这些 mesh
源码已等价。6.2/6.4 必须补齐矩阵后重新聚类。

### 6.1.4 对 6.2 的实现约束

1. 用生产路径捕获 `.mesh` 最终编译文本及 include 闭包，覆盖所有已注册 material definition、
   实际 Forward / Depth / Shadow 覆盖形状、三种 mesh mode、三种合法 Position 解码和仓库内
   recipe override；以最终 GLSL + 链接接口相同为条件聚类，得到可复核的文件数量与清单。
2. 明确设备 profile 对普通 VP / LineQuad 的 invocation clamp 是否保留。若保留，需要为实际 N
   选择固定源码/常量组合；若固定为 64，则先证明所有支持设备满足输出能力且行为/性能变化可接受。
   CharQuad 恒为 64 且其 CPU dispatch 守卫不得删除。
3. 修正或拒绝 `MeshModeDescriptor::ResolveStandardStage1Input` 对
   `VK_FORMAT_R32G32B32A32_SFLOAT` 返回 Vec3、而 `VertexABIBuilder` 当前不支持该格式的分歧；
   不得将此格式误列入固定源覆盖面。
4. 把 6.4 的 `DrawTriangle` / `VertexColor`（`R16G16_SINT` + `Make2DNodeConfigOrtho(false)`）
   放入形状矩阵，确保节点配置、ABI 与 varying 链接在源选择时仍同一份解析结果。
5. ~~当前 `ShaderLibrary/mesh/line_quad.glsl.tmpl` 仍用 `#ifdef S1_TRANSFORM_ID_GLSL` /
   `S1_SIZE_GLSL` 改变执行语句，`vertex/s1_position_*.glsl` 仍按 `HGL_*_LOADER`
   宏条件装载属性~~（该快照已完成使命：`.glsl.tmpl` 已全部改写为 `.mesh`，`HGL_*_LOADER`
   已在网格阶段收口 Layer 2 删除，见 §8），`orient_world.glsl` 仍按
   `HGL_L2W_FROM_VERTEX_ATTR` 条件编译。
   这些不是 options 中允许保留的 dither 条件；若阶段六继续要求无 `#if`，固定形状必须
   静态封闭所需属性和变换路径，并删除这些依赖的分支，而不能只把当前模板改名为 `.mesh`。

## 阶段六 6.2：网格阶段固定源目录（已落地）

6.2 把 6.1 的九组真实输出固化成**仓库内的物理源码**。生产路径不再从模板拼装网格阶段：

```text
ShaderLibrary/mesh/
  stage_common_head.glsl          公共前导上半段（扩展 / RootAddresses / ResolveTransformID）
  stage_common.glsl               公共前导下半段（descriptor 宏 / scene UBO / l2w / 结构 / 属性 buffer_reference）
  fixed/<形状键>.mesh × 9         每个形状一份固定主着色器
```

`GenericMaterialBuilder::TryLoadFixedMeshSource()` 以 `mesh/fixed/<十进制形状键>.mesh` 查表；
文件不存在即视为目录未覆盖（合法空目录），只有不可读/空/超大才报错。形状键由
`ComputeMeshSourceShapeKey()` 只对**已解析的构建计划**取值哈希（mode、位置格式、调用数、
Stage2/Stage3 模块路径、`needs_l2w`、八种 varying 标志、stage interface 尺寸），不依赖
最终文本——否则键会自指。

### 为什么拆成 head + body 两个 include

生成器的发射顺序是：扩展 → push constant → `ResolveTransformID` → **topology 行** →
`descriptor_macros`/`scene_ubo`/`l2w_ssbo` → 结构体 → 属性 `buffer_reference` → 形状相关部分。
`local_size_x` 与 `max_vertices`/`max_primitives` 是形状差异，必须插在中间；把公共前导切成
两段，固定源码的全局声明顺序才与生成器逐行一致，SPIR-V 才能逐字节比对。若把 topology 挪到
整段前导之后，普通形状的 SPIR-V 侥幸不变（workgroup/输出布局是 `OpExecutionMode`，不进
指令流），但 `CharQuad`（`256/128`）会真的改变二进制——这类"看起来没事"的顺序漂移必须在
结构上排除，而不是靠运气。

### 属性装载：结构性 `#if` 已经消失

`s1_position_vec3.glsl` / `s1_position_vec2.glsl` 的 `LoadVertexData()` 原本靠 8 个
`#ifdef HGL_*_LOADER` 钩子按需展开属性装载。固定源把这 8 个分支**静态展开成显式语句**
（下方片段是当时的中间形态；Layer 2 之后 `LoadVertexData()` 改为无条件调用
`LoadUV(); LoadNTB(); LoadColor(); LoadLuminance(); LoadTransformID(); LoadSize();`，
未在场的流由 `vertex/s1_stream_defaults.glsl` 的空实现补齐，语义等价）：

```glsl
void LoadVertexData()
{
    Position = sbo_vertex_position.data[draw_params.vertex_base + VertexIndexID];
    TexCoord = sbo_vertex_uv.data[draw_params.vertex_base + VertexIndexID];
    Normal   = sbo_vertex_ntb.data[draw_params.vertex_base + VertexIndexID];
}
```

固定源码及其 include 闭包不再含结构性 `#if`；`s1_uv` / `s1_ntb` 等属性模块只保留"声明
全局变量 + 定义 `sbo_vertex_*` 基址宏"，其装载钩子已在网格阶段收口 Layer 2 中
**全部删除**（改为 `#define S1_STREAM_*` + `void LoadXxx()`，见 §8）。

### topology 数值为什么是字面量而不是 options 常量

6.1.4 曾预留「`local_size_x` / topology 由 options 常量承载」。落地时改为**字面量**，理由：

- 普通模式的 invocation 数若被设备上限钳制，最终 N 已进入形状键（`mesh_max_invocations`），
  不同 N 对应不同固定文件；再让 options 二次覆盖只会让"同一文件在不同设备上编译出不同
  拓扑"，与固定源的不变性目标冲突。
- `CharQuad` 的 64 是 CPU dispatch 契约，设备不足必须失败而不是钳低；把它放进可生成的
  options 等于把契约变成可漂移数据。6.3 的守卫按字面量 + 反射出的 `local_size_x` 取证。

### 等价性验证（逐字节）

`glslangValidator` 拒绝 `#include`，除非源码声明 `GL_GOOGLE_include_directive`；仓库编译器
（`src/Tools/GLSLCompiler/glsl2spv.cpp` 的 `PrepareShaderSource`）的做法是在 `#version` 行
**紧后面**插入该扩展声明。用同一手法处理两份文本后编译，即可直接比二进制：

```text
把 #extension GL_GOOGLE_include_directive : require 插到 #version 460 之后
glslangValidator -V --target-env vulkan1.2 -S mesh -IShaderLibrary -o out.spv <file>.mesh
```

对 6.1 的九组代表 fixture，生成器导出的 `GetFinalGLSL()` 与对应 `fixed/<键>.mesh` 编译出的
SPIR-V **9/9 SHA-256 完全相同**（12976 / 9920 / 12932 / 14944 / 15800 / 12696 / 13552 /
13172 / 19800 字节）。这比"文本相似"强得多：它同时证明模块选择、varying location 与
interpolation、拓扑常量、属性装载顺序、`main` 主体全部等价。

生产侧验收：`ShaderDocumentProductionRegression --full` 全部 15 个可构建 fixture 报
`source=fixed` 且退出码 0（`VerifyFixedMeshStage` 断言
`stage_key.definition_hash == HashFinalShaderSource(final GLSL)`）；`ShaderResourceSchemaRegressionGate all`
仍 58 PASS / 0 FAIL。9 个 depth/shadow fixture 仍因领域固定片段主着色器覆盖缺口被跳过，
与本阶段无关。

### 仍未覆盖（不在 6.2 范围内）

- `LineQuad` 模式尚无捕获形状（`ShaderLibrary/mesh/line_quad.glsl.tmpl` 仍带 `#ifdef`）。
- `DrawTriangle` / `VertexColor`（`R16G16_SINT` + `Make2DNodeConfigOrtho`）尚无形状，属 6.4。
- `VK_FORMAT_R32G32B32A32_SFLOAT` 在 `MeshModeDescriptor::ResolveStandardStage1Input` 与
  `VertexABIBuilder` 之间的分歧仍未修正，不得列入固定源覆盖面。


---

## 6.3 网格拓扑契约守卫迁移

### 问题

`L0.charquad-workgroup-contract` 原先守的是「设备上限 < 契约值必须构建失败、不得静默钳制」。
这条断言依赖生成器里的两件事：`ResolveCharQuadTopology` 算出的 `max_vertices` /
`max_primitives`，以及生成侧写下的 `local_size_x`。生成器一删，公式与守卫同时消失。

更糟的是固定源把问题搬进了数据：形状键（`ComputeMeshSourceShapeKey`）只哈希
**模式 + invocation 数**，不哈希 topology 三行。于是手改 `fixed/<键>.mesh` 里的
`local_size_x` 不会让键失配 —— 文件照样被找到、照样编译，然后 CPU 按
`TEXT_CHARQUAD_MAX_INVOCATIONS` 派发组数、GPU 按被改小的组处理，尾部整段漏画且无报错。
这正是该用例登记的历史缺陷，只是从代码搬到了数据。

### 三处改动

1. **公式上移**到公开头 `inc/hgl/mtl/MeshShaderLimits.h`：
   `ResolveMeshModeTopology(MeshShaderMode, uint32_t) -> MeshModeCapacity`。
   它只有算术、没有发射逻辑，因此能活过 `meshgen/` 的删除，并同时服务发射侧与校验侧。
   `meshgen/MeshModeDescriptor.h` 的三个 topology resolver 改为一行转发，公式只存一份。
2. **装配期解析回数值**：`GenericMaterialBuilder::ValidateMeshSourceTopology()` 在三条
   源路径（request 覆盖 / 目录固定源 / 生成器）汇合之后，把最终文本里的
   `local_size_x` / `max_vertices` / `max_primitives` 解析成整数，与构建计划逐位比对，
   任一不等即 `GLogError` + 构建失败。生成器路径按构造必然一致，多跑一次只多三次
   字符串查找，换来三条路径同一把锁。
3. **编译产物取证**：用例 ⑧ 对最终 mesh GLSL 真编译一次，反射出 SPIR-V 里的
   `OpExecutionMode`，读 `LocalSize` / `OutputVertices` / `OutputPrimitivesEXT`。
   前两条（文本、装配期解析）只能证明"我们写/读的数字对"，这一条才证明"驱动真正拿到的
   模块里数字对"。

### 反射的两个坑

- **`LocalSize` 可能不是字面量**。较新 glslang 把 mesh 局部尺寸降级为
  `OpExecutionModeId %main LocalSizeId %cx %cy %cz`（opcode **331** / 模式 **38**），
  实参是常量 id 而不是字面量。实测仓库 `GLSLCompiler.dll` 就产出这一形态：模块里
  只有 `OpExecutionMode` 的 `OutputVertices(26)=256`、`OutputPrimitivesEXT(5270)=128`
  与 `5298`，没有 `LocalSize(17)`。因此 `FindSPVLocalSizeX()` 先试字面量形态，再退回
  `OpExecutionModeId` 查 `OpConstant(43)` 解出值 —— 两种形态都要认。
- **`OutputVertices` / `OutputPrimitivesEXT` 仍是字面量**，走
  `FindSPVExecutionModeLiteral()` 直接读 `words[i+3]`。

两个 helper 都按 `word0 = (word_count << 16) | opcode` 从索引 5 起线性走指令流，
不依赖任何反汇编库。

### 反证（确认守卫真的在守）

把 `GenericMaterialBuilder.cpp` 的 `ValidateMeshSourceTopology` 调用临时短路后重跑，
用例立刻报三条 `CharQuad 接受了被改写的 local_size_x / max_vertices / max_primitives`。
恢复后回到 PASS —— 证明 ⑤⑥⑦ 三条把「数据被改动」变成硬失败，不是被别的机制顺带挡住的。

### 验收

- `ShaderResourceSchemaRegressionGate all`：58 PASS / 0 FAIL（`L0.charquad-workgroup-contract`
  仍是单个具名用例，内部覆盖 ① 无 profile 契约值、② 声明更小必须失败、③ 设备上限更低
  必须失败、④ 宽裕设备不得放大、⑤⑥⑦ 三个 topology 篡改必须失败、⑧ 反射取证）。
- `ShaderDocumentProductionRegression --full`：退出码 0，15/15 `source=fixed`，
  9 个 depth/shadow `[SKIP]` 与本阶段无关。

---

## 6.4 `DrawTriangle` 真实形状的门禁与固定源

### 失败现场

用户运行 `DrawTriangle` 示例时构建直接失败：

```
[VertexNodeConfigResolver.h:30] 位置映射与顶点位置输入不匹配：
  position_mapping=Passthrough3D 需要 Position 分量数 3，
  而 Stage 1 输入模块提供 2 个分量（VertexColor）
[GenericMaterialBuilder.cpp] Resolved vertex ABI build failed: name=VertexColor
```

`DrawTriangle` 的几何是 2 分量整数像素坐标（`VF_V2I16`），并显式设了
`Make2DNodeConfigOrtho()`（`Vec2Position` + `PixelToLocal` + `OrthoViewport`）。
但 `vertex_node_config` 有**四层**来源（`ResolveMaterialVertexNodeConfig`）：
request 覆盖 → recipe（非默认时）→ **definition（非默认时）** → recipe 兜底。
`VertexColor` 的 definition 是默认 3D（`Passthrough3D`，3 分量），recipe 是 2D，
于是 ABI 求解读到了 definition 那一层，与 `s1` 的 2 分量对不上。

修法早在阶段 4.4 就落地，是 `ResolveVertexABI` 里的一行回写：

```cpp
plan.vertex_definition.vertex_node_config = plan.vertex_node_config;
```

本阶段的任务是把这行变成一个**有看守的契约** —— 6.5 删掉 ABI 求解链时，
若有人顺手删掉它，必须立刻红灯。

### 固定源的第 10 个形状

`ComputeMeshSourceShapeKey` 对 `VertexColor` + `VF_V2I16` + `Make2DNodeConfigOrtho`
算出 `13020131598835485143`。捕获流程与 6.2 相同，逐个字节核对：

```
generator=044D60D08A04B63C59A506640D0FC8CC6F204B85D017B16B80DE4934C7A411C5
fixed    =044D60D08A04B63C59A506640D0FC8CC6F204B85D017B16B80DE4934C7A411C5
```

`mesh/fixed/` 目录现为 **10 份**。内容是：`s1_color` + `s1_position_vec2i`
（4B packed int16x2）+ `s2_pixel_to_local` + `s3_ortho_viewport`，
`local_size_x = 64`、`triangles`、`max_vertices = 192`、`max_primitives = 64`。

`vec2i` 与 `vec2` 这两份解码模块**都能编译通过**：选错不会报错，只会让整批顶点
按 8B 跨距去读 4B 数据，静默错位。所以下面的断言是双向的（必须命中 vec2i、
且不得命中浮点 vec2/vec3），而不是"看起来没问题"。

### 门禁用例 `L0.draw-triangle-vertex-color-2d`

登记在 `interface` 组，用一次 `defer_finalize = true` 的构建（只要 GLSL 文本，
不编译 SPV），断言：

1. 构建必须成功 —— 回写失效时 ABI 对账会先失败，这里直接拿到 `nullptr`。
2. 网格阶段必须 `#include "vertex/s1_position_vec2i.glsl"`，且**不得**出现
   `s1_position_vec2.glsl` / `s1_position_vec3.glsl`。
3. 映射必须 `#include "vertex/s2_pixel_to_local.glsl"`，且**不得**出现
   `s2_passthrough3d.glsl`（证明 recipe 覆盖没有丢）。
4. 该形状在 `ShaderLibrary/mesh/fixed/` 里必须存在一份**内容与这份 GLSL 完全
   一致**的 `.mesh`。按内容比对而非按文件名硬编码形状键：6.5 之后没有生成器兜底，
   目录里缺这一份该域就没有 mesh 阶段可编译；而重新捕获时只需替换文件，门禁依然有效。
5. 反方向反证：`Passthrough3D` + 2 分量几何、`PixelToLocal` + 3 分量几何，
   **都必须构建失败** —— 证明分量数对账不是单向放行。

### 反证（确认守卫真的在守）

把 `GenericMaterialBuilder.cpp:266` 的回写注释掉重跑，用例立刻报两条：

```
- DrawTriangle 形状构建失败（recipe 的 2D 节点配置未回写到 ABI 求解用的定义）
- PixelToLocal（2 分量）+ 3 分量位置几何被接受（分量数对账失效）
```

第二条尤其说明问题：没有回写时，ABI 读的是 definition 的 `Passthrough3D`，
它与 3 分量几何**对得上**，于是本该失败的组合被接受了。恢复后回到 PASS。

踩坑记录：`Copy-Item` 恢复备份会保留原文件的 `LastWriteTime`，MSBuild 认为源文件
没变而跳过重编，导致"恢复后仍然红灯"。改文件后需要显式 touch 时间戳。

### 计划原文的 `R32G32B32A32_SFLOAT` 分歧：不是活缺陷

6.4 原文要求"修复或显式拒绝 `MeshModeDescriptor::ResolveStandardStage1Input`
（声称 RGBA32F→`Vec3Position`）与 `VertexABIBuilder` 的分歧"。查证结论是**两者不可能
同时生效**：

- `VertexABIBuilder::BuildResolvedVertexABI` 是标准模式的唯一选择器，只接受
  `R32G32B32_SFLOAT` / `R32G32_SFLOAT` / `R16G16_SINT`（分别对应 12B / 8B / 4B
  三份解码模块），其余格式一律硬失败。
- `MeshModeDescriptor::resolve_stage1_input` 只在 `MeshTemplateEmitter.h:273` 的
  **else 分支**被读；而 `GenericMaterialBuilder::ResolveVertexABI` 只要
  `request.geometry_vertex_format` 存在就一定会先填
  `plan.resolved_vertex_input_document`，让 `MeshTemplateEmitter.h:269` 的
  `if (resolved_input_document && GetBlockCount() > 0)` 恒为真。

所以 `resolve_stage1_input` 对标准模式**不可达**，那条 RGBA32F 的说法是注释层面的
虚构。处理方式不是"修"，而是 6.5 直接把 `ResolveStandardStage1Input` 和
`resolve_stage1_input` / `resolve_stage2_mapping` / `resolve_stage3_projection`
三个成员一起删掉 —— 死代码留着只会继续误导。

### 6.4 验收

- `ShaderResourceSchemaRegressionGate all`：59 PASS / 0 FAIL（+1 即本用例）。
- `ShaderDocumentProductionRegression --full`：退出码 0，**16/16** `source=fixed`
  （+1 即 `vert-color-2d-int16`），9 个 depth/shadow `[SKIP]` 与本阶段无关。

## 6.4b `LineQuad` 固定源与「目录覆盖」门禁

### 为什么先做这一步

6.5 要删 `meshgen/*`，而 `mesh/line_quad.glsl.tmpl` 里还有三个活分支：
`S1_PALETTE_INDEX_GLSL`、`S1_TRANSFORM_ID_GLSL`（else 分支才走
`ResolveTransformID(gl_InstanceIndex)`）、`S1_SIZE_GLSL`（else 分支把
`width = min_width = 1.0`）。有分支就意味着生成器还在替这个形状做选择，
所以它必须先有一份固定 `.mesh`，否则删掉生成器后线框就画不出来。

### 捕获入口放在门禁里，而不是生产工具里

形状矩阵只存在于一处：门禁的 `kReflectionCoverageVariants`（16 个变体，
含两个 `PrimitiveType::Lines` 的 `LineQuad` 变体）。生产工具的 `Fixture`
没有 `primitive_type` 字段，要在那里生成线框就得复制一份形状定义 —— 两份
形状表必然漂移，漂移的表现是"少覆盖"而不是报错。

所以捕获入口放进同一个可执行文件：

```
ShaderResourceSchemaRegressionGate.exe emit-fixed-mesh
```

`EmitFixedMeshCatalog()` 遍历 `kReflectionCoverageVariants`，对每个变体用
`BuildReflectionCoverageVariant(..., MaterialShaderDocumentCapture *out_capture)`
拿到 `mesh_source_shape_key` 与 `mesh_source_document`，若该 GLSL 尚未出现在
目录里就写 `ShaderLibrary/mesh/fixed/<shape_key>.mesh`，打印 `[written]` /
`[unchanged]`。形状表只有一份，捕获与验证不可能互相漂移。

### 覆盖断言：新增形状必须有人捕获

`RunGeneratedInterfaceReflectionCase` 对每个变体的 mesh 阶段增加一条检查：
其 `GetFinalGLSL()` 必须在目录里有一份内容**完全一致**的 `.mesh`，否则

```
vertex-color-lines-quad: mesh stage has no source in ShaderLibrary/mesh/fixed（用 emit-fixed-mesh 捕获）
```

这条断言把"矩阵新增了形状但没人捕获固定源"从运行时的"某个域画不出来"
提前成门禁红灯。按内容比对而不是按文件名比对，重新捕获时只需替换文件，
不需要同步改断言。

### 捕获结果

16 个变体去重后共 17 份源（`lit-depth-only` 与 `lit-shadow-depth` 同形
（`16010825705453514407`），`lit-depth-masked` 与 `lit-shadow-masked` 同形
（`14117761268391746788`），`sky-cube-forward` 与 `sky-minimal-forward`
同形（`5523130418547404603`））。本次新增 7 份：

| 形状键 | 变体 | 字节（LF 文本长度） |
|---|---|---|
| `1383689879301672014` | `vertex-color-lines-quad` | 8620 |
| `13907659216699454923` | `checkerboard-2d-lines-quad-2d` | 8309 |
| `10556500708867411382` | `vertex-palette-color-forward` | 5664 |
| `16714434726803612563` | `vertex-color-forward` | 5511 |
| `6057340463783612614` | `vertex-luminance-forward` | 5631 |
| `5130637854479680447` | `unlit-texture-forward` | 5598 |
| `8102413982750205303` | `debug-normal-forward` | 5740 |

后五份不是新形状语义，而是这 5 个定义根本不在 `--full` 夹具表里：
生产工具从未捕获过它们，只有反射覆盖矩阵会走到。这也说明覆盖断言
放在门禁里是对的 —— 生产工具的表补不补都不影响覆盖完整性。

二次运行 `emit-fixed-mesh` 得 `written=0 unchanged=16`，即幂等。

关于换行：`.mesh` 在版本库里统一是 LF，Windows 检出时由 `core.autocrlf`
变成 CRLF，和目录里既有的 10 份一致。固定源路径按字节原样读文件、原样
交给编译器，`glslang` 对 CRLF 无要求，形状键哈希的是请求而不是文本，
所以换行不构成接口差异。

这条断言证明的是**覆盖**（每个变体都有固定源），不是**新鲜度**。形状键落盘
之后，固定路径就不再问生成器，因此"文件比模板旧"这类问题它看不出来 ——
模板变更导致接口不兼容时由同组的 mesh→fragment 链接检查兜住；
6.5 删掉生成器之后，"新鲜度"这个概念本身也就不存在了（文件即真源）。

### 反证

把 `ShaderLibrary/mesh/fixed/1383689879301672014.mesh` 移走后跑 `reflection` 组：

```
[FAIL] SPV.generated-interface
- vertex-color-lines-quad: mesh stage has no source in ShaderLibrary/mesh/fixed（用 emit-fixed-mesh 捕获）
```

exit 1，且**只有**这一条诊断 —— 说明失败确实由覆盖检查负责，没有别的检查
在掩盖它。恢复文件后 3 PASS / 0 FAIL。

### 6.4b 验收

- `emit-fixed-mesh`：`written=7 unchanged=9 failed=0`；再跑一次
  `written=0 unchanged=16 failed=0`。目录 17 份 `.mesh`。
- `ShaderResourceSchemaRegressionGate all`：59 PASS / 0 FAIL。
- `ShaderDocumentProductionRegression --full`：退出码 0，16/16 `source=fixed`，
  0 `source=generator`，9 个 `[SKIP]` 不变。

至此 `kReflectionCoverageVariants` 的 16 个变体全部有固定 mesh 源，
`meshgen/` 删除的前置条件已满足。

## 6.5a 网格阶段缺固定源即失败

### 改的是什么

`GenericMaterialBuilder::GenerateStageSources` 里原本在"形状键在
`ShaderLibrary/mesh/fixed/` 找不到文件"时有一个 `else` 分支，回头调
`EmitMeshTemplateDocument(...)` 现场生成网格阶段文本、再 `ShaderDocument`
序列化。这一分支删除，改成直接失败：

```
no fixed mesh source for shape key <键> (mesh stage is physically fixed;
the source must exist in ShaderLibrary/mesh/fixed)
```

片段侧（`fixed_fragment_source` 缺失）从 5.3 起就已经是硬失败，网格侧一直是
"有固定源就用、没有就生成"。两边不对称的后果不是"多留一条路"，而是生成器
永远删不掉：只要还有形状键没有固定源，删掉 `meshgen/` 就会让那批形状静默
变成空网格阶段。所以必须先让缺失变成不可忽略的错误。

这是 `EmitMeshTemplateDocument` 在**生产路径**上的最后一个调用点。剩下的调用
只在门禁的 `GenerateMeshShaderDocumentText`（6.5b 随文档层一起删）。

### 硬失败暴露出的三个形状矩阵

把生成器关掉之后，`all` 立刻从 59 PASS 变成 57 PASS / 2 FAIL：

| 用例 | 缺失的形状键 | 来源矩阵 |
|---|---|---|
| `DH.domain-backend-equivalence` | `329204832091855392` | `kDomainBackendEquivShapes`（15 个） |
| `Q0.authoritative-material-cache-identity` | `6106896277463206806` | Q0 自己的 A/B 几何 |

`kReflectionCoverageVariants`（16 个）盖不住这两个形状，原因不是"漏了几个"，
而是三张矩阵的**几何来源互不相同**：

- `kReflectionCoverageVariants` — 手写几何常量（`kReflectionCoverageGeometry`
  / `...2D`），mesh 模式由 `primitive_type` 推出。
- `kDomainBackendEquivShapes` — 几何**由领域契约推导**：
  位置格式取 `GetMaterialDomainCanonicalVertexNodeConfig`，其余语义取
  `GetMaterialDomainVertexSemanticRequirements` + 本表补充项。同一个定义在
  两张表里键也不同。
- Q0 — 缓存身份比对用的 A/B 几何，其中 `TexCoord = VF_V2HF` 这一档在别处
  完全不存在。

`builtin/error_checkerboard_3d` 只出现在 DH 表里，这就是它一直没被发现的原因：
它连 `--full` 夹具都不在，只有 `all` 跑到 DH 用例时才需要网格源。

因此 `emit-fixed-mesh` 从"遍历一张表"扩成"遍历三张矩阵"，每张表各跑一遍：

1. `kReflectionCoverageVariants` — 复用 `BuildReflectionCoverageVariant`。
2. `kDomainBackendEquivShapes` — 复用 `BuildDomainEquivRequest`（它已经做了
   `SelectSceneRenderTemplateRequest` + `AppendMaterialRenderTemplateRoots`）。
3. `kCacheIdentityEmitShapes` — 新的小表，几何常量
   `kCacheIdentityLitGeometryA/B`、`kCacheIdentityColorGeometry` 从 Q0 用例里
   **上提到命名空间作用域**，让捕获与验证共用同一份（复制一份必然漂移）。

捕获路径要能走到网格阶段，必须满足两个前提，缺一个就报别的错：

- **`request.defer_finalize = true`**。领域后端本来就是延后 finalize 的；
  finalize 阶段才编译固定片段，而编译需要按域就位的 options include 根。
  捕获只要网格文本，提前 finalize 会在片段上失败。
- **先 `ResolveFixedFragmentSource` 并把结果放进请求**。否则
  `BuildGenericMaterial` 会在片段检查处直接拒绝
  （`fixed_fragment_source is required`），根本到不了网格阶段。

### 捕获做法：最后一次机会

`emit-fixed-mesh` 本身要走 `BuildGenericMaterial`，而 6.5a 之后这个函数遇到
缺键就返回 null —— 工具**再也不可能写出新文件**。要补最后两份源，只能在
生成器还活着的时候捕获：

```powershell
git stash push -q -m "reopen-generator" -- src/ShaderGen/builder/GenericMaterialBuilder.cpp
# 构建 + emit-fixed-mesh
git stash pop -q
```

本次补的两份：

| 形状键 | 来源 | 字节 |
|---|---|---|
| `329204832091855392` | `DH` 的 `err-check-3d`（`builtin/error_checkerboard_3d`） | 5609 |
| `6106896277463206806` | Q0 的 `cache-lit-abi-b`（`TexCoord = VF_V2HF`） | 5846 |

**这是最后一次。** 6.5b 删掉 `meshgen/` 之后没有生成器可捕获，真正需要的
新形状只能手写 `.mesh`。也正因如此，`emit-fixed-mesh` 后续退化成一个纯粹的
**覆盖检查**：它能一次跑完三张矩阵并点名谁缺源，但 `[written]` 不会再出现。

### 反证踩到的坑：`GetLastBuildGenericMaterialError()` 只反映最后一次构建

移走 `6106896277463206806.mesh` 跑 `cache` 组，得到的是
`exit=0xC0000005`、**输出 0 行**。`cdb` 定位到
`RunAuthoritativeMaterialCacheIdentityCase` 里的诊断拼接：

```
std::string("... (last error: ") + GetLastBuildGenericMaterialError().c_str() + ")"
```

崩因有两层：

1. `s_last_build_generic_material_error` 在**每次** `BuildGenericMaterial`
   入口被 `Clear()`。Q0 依次构建 `lit_a` / `lit_b` / `color` / `lit_targeted`，
   失败的是 `lit_b`，而它后面的 `color`、`lit_targeted` 成功了，于是静态
   错误又被清空。此前所有构建都失败，才碰巧能打出诊断。
2. `hgl::String::c_str()` 在字符串为空时返回 **`nullptr`**（见
   `CMCore/inc/hgl/type/String.h`），`std::string + const char*` 里的
   `strlen(nullptr)` 直接踩空。

修法是把失败原因**逐次记录**在 `build` lambda 里（`build_error`，并区分
"模板请求失败 / 固定片段解析失败 / 构建无 ProgramLink"三种来源），诊断只读
这个局部变量，不再读那个"最后一次"的静态。

> 通用教训：`GetLastXxxError()` 这类"最后一次"访问器在**多轮构建的用例**里
> 是不可靠的；只要有一轮成功就会把前面真正的原因清掉。要在失败点就地取值。

### 6.5a 验收

- 负向对照：移走 `6106896277463206806.mesh` 跑 `cache` 组，exit 1，单条诊断

  ```
  [FAIL] Q0.authoritative-material-cache-identity
    - material builds must produce ProgramLink (last error: no fixed mesh source
      for shape key 6106896277463206806 (mesh stage is physically fixed;
      the source must exist in ShaderLibrary/mesh/fixed))
  ```

  键值与目录名都在诊断里，等于直接告诉人该补哪个文件；不再是崩溃、也不再是
  下游"某个域画不出来"。
- `ShaderResourceSchemaRegressionGate all`：59 PASS / 0 FAIL。
- `ShaderDocumentProductionRegression --full`：退出码 0，16/16 `source=fixed`，
  0 `source=generator`，9 个 `[SKIP]` 不变。
- `emit-fixed-mesh`：`written=0 unchanged=34 failed=0`（三张矩阵合起来 34 个
  形状键，去重后即目录里的 19 份 `.mesh`）。

顺带把捕获工具的失败输出从 `GLogError` 换成 `std::fprintf(stdout, ...)`：
`GLogError` 在这个工具里不落到重定向的日志文件，改造期间 DH/Q0 的失败原因
一直是不可见的。门禁文件本来就有大量 `fprintf`，与既有风格一致。

## 阶段六-5b：删除文档层与 meshgen（物理移除 ShaderDocument）

6.5a 保证了生产与测试全面使用物理固定源之后，`BuildMaterialStageDocument`
（模板合并 → 序列化）及整套 `ShaderDocument` 基础设施已不再有任何生产消费者。
本阶段将其物理删除：

### 1. 物理删除的文件

- `src/ShaderGen/meshgen/`（10 个文件全部删除）：
  - `MeshTemplateEmitter.h`
  - `MeshModeDescriptor.h`
  - `MeshShaderHeaderGen.h`
  - `MeshShaderVaryingGen.h`
  - `MeshShaderVertexAdapter.h`
  - `MeshShaderModeVertexPassthrough.h`
  - `MeshShaderModeLineQuad.h`
  - `MeshShaderModeCharQuad.h`
  - `MeshShaderTemplate.h`
  - `MeshShaderTemplate.cpp`
- 文档层实现与接口：
  - `src/ShaderGen/document/ShaderDocument.cpp`
  - `inc/hgl/mtl/ShaderDocument.h`
- 专属测试工具：
  - `src/Tools/ShaderGen/ShaderDocumentRegression.cpp`（及 CMake 目标与 ctest 声明）

### 2. 生产侧改造

- `inc/hgl/mtl/MaterialShaderCompiler.h`：
  - `MaterialShaderDocumentCapture` 简化为 `MaterialShaderBuildCapture`（仅包含 `mesh_source_shape_key` 与 `mesh_source_was_fixed`）。
  - `BuildMaterialStageDocuments` 统一收敛为三参数版本 `(profile, input, config)`，原 5 参数文档版本彻底废除。
  - 公开 `MaterialDefinitionRequiresPayloadRow` 声明（编译期直判信号）。
- `src/ShaderGen/builder/GenericMaterialBuilder.h/.cpp`：
  - 移除 `mesh_source_document` 成员与相关组装逻辑；顶点输入声明直接保留为 `resolved_vertex_input_glsl` 纯文本。
  - 形状键哈希精确保持历史 byte sequence（以字面量 `5` 代表原 `ShaderDocumentBlockKind::Module`），保证 19 份已落盘的 `.mesh` 文件名 100% 幂等。
- `src/ShaderGen/compile/MaterialShaderCompiler.cpp`：
  - 精简为单一编译入口：硬要求 `fixed_mesh_source` 与 `fixed_fragment_source` 两份物理固定源，直接走 `SetFinalGLSL` 编译，不经过任何文档合并与注入。

### 3. 测试工具改造与不变量转移

- `ShaderDocumentProductionRegression.cpp`：
  - 移除所有旧 `ShaderDocument` 断言与块序列化比对，转为直接验证固定阶段与 `MaterialDefinitionRequiresPayloadRow`。
- `ShaderResourceSchemaRegressionGate.cpp`：
  - 原 `MV.mesh-varying-writability`（生成器可写性表测试）改造为 `MV.fixed-mesh-varying-invariants`：使用 `AuditFixedMeshMasters` 对 `ShaderLibrary/mesh/fixed/*.mesh` 全量物理文件进行静态语法与结构审查（验证 stable location、GLSL 类型、`perprimitiveEXT` 约束、调度契约维数、真实写入性、Stage2/3 组合约束）。
  - `X.transform-graph-composition` 改为直接遍历 `PositionMappingMode` 与 `ProjectionMode` 枚举，断言 `VertexNodeConfigResolver` 映射的物理模块文件存在并保持阶段键区分度。
  - `Z.material-bda-emitter` 与 `AE.binding-macro-single-source` 迁移至三参数 `BuildMaterialStageDocuments`，直接使用物理领域头文件与固定源。
  - 捕获类型统一改名为 `MaterialShaderBuildCapture`。

### 4. 验收结果

- `ShaderResourceSchemaRegressionGate all`：59 PASS / 0 FAIL（全量通过，含 L0.draw-triangle-vertex-color-2d 对 AutoInstance 形状的守护断言）。
- `ShaderResourceSchemaRegressionGate emit-fixed-mesh`：`written=0 unchanged=35 failed=0`（补齐 SimpleSphere 的 `11473310428698951072.mesh`、AutoInstance 2D 顶点色的 `4442253396988711992.mesh` 以及 AutoMergeMaterialInstance 2D 纯色实例行的 `6503166534270626483.mesh` 后，固定库扩充至 22 份，全量通过 `AuditFixedMeshMasters` 静态语法与契约审查）。
- `ShaderDocumentProductionRegression --full`：16/16 `source=fixed`，0 `source=generator`，退出码 0。
---

## 阶段七-2：网格阶段按形状物理固定，顶点格式下沉到 mesh_options.glsl

### 改的是什么

阶段六-5b 之后网格阶段已「物理固定」，但固定文件的**命名键**仍是 FNV1a 哈希
（`ShaderLibrary/mesh/fixed/11473310428698951072.mesh` 之类），键里混入了顶点格式、位置映射、
投影模式与 invocation 数。后果是每出现一种新的顶点格式组合，就得补一份新的哈希文件——
本阶段开始时库里有 22 份，且仍在按示例逐个追加。

七-2 把命名键换成**形状**：

```
ShaderLibrary/mesh/shape/tri_<顶点数>_<varying 按 location 升序>.mesh
```

顶点格式、位置映射（s2）、投影（s3）、invocation 数全部移入生成的
`generated/mesh_options.glsl`。11 份 master 覆盖原来的 22 个哈希键：
`plain` / `dataid` / `dataid_pos` / `dataid_pos_nrm` / `dataid_pos_nrm_uv` / `dataid_uv` /
`dataid_dir` / `dataid_lum` / `pos` / `pos_nrm` / `color`。

「新增顶点格式要加新文件」这件事从此不存在：`DrawTriangle` 用 `R16G16_SINT`
（`s1_position_vec2i`）、`AutoInstance` 用 `R32G32_SFLOAT`（`s1_position_vec2`），两者现在同用
`tri_192_color.mesh`，差别只在各自的 options。

### 为什么不能按 MaterialDomain 命名

原计划写成 `ShaderLibrary/domain/<domain>/<domain>.mesh`。实测不成立：网格阶段的可变部分是
**varying 集合**，既不是 `domain` 也不是 `purpose`。

- 多个域共用同一套 varying：`VertexColor` 与 `Unlit` 都只输出 `loc0`。
- 同一域可能有两套 varying：`Lit` 带 UV 与不带 UV（`tri_192_dataid_pos_nrm_uv` vs `tri_192_dataid_pos_nrm`）。

按域命名会让「同一域不同顶点语义抢一份文件」，而这正是被 22 份哈希文件掩盖掉的那层维度。

### 每份 master 的结构

```glsl
#version 460
#include "mesh/stage_common_head.glsl"

layout(local_size_x = 64) in;
layout(triangles, max_vertices = 192, max_primitives = 64) out;
#include "mesh/stage_common.glsl"

// varying 声明，须与 domain/*.frag 逐 location 一致
layout(location = 0) perprimitiveEXT out uint fragDataIndexID[64];
// ...

// 唯一可变输入：顶点解码 + 位置映射 + 变换
#include "generated/mesh_options.glsl"

void WriteMeshVertexOutputs(uint vid) { /* ... */ }

#define ULRE_MESH_MAX_VERTS   192u
#define ULRE_MESH_INVOCATIONS 64u
#include "mesh/stage_body.glsl"
```

`ShaderLibrary/mesh/stage_body.glsl` 是 11 份 master 唯一的公共 `main()`：算 `total_vertices`、
`group_base_vertex`、`prims_this_group`、`verts_this_group`，`SetMeshOutputsEXT`，然后两个按
`gl_LocalInvocationIndex` 步进的循环（写顶点输出 + `gl_Position`，写三角形索引）。放在一份文件里
是为了避免 11 份 dispatch 逻辑各自漂移。

include 顺序是硬约束，不能调换：

1. `stage_common_head.glsl`（6 个 `#extension`、push constant、`gl_InstanceIndex` 宏）
2. topology 三个 `layout(...)`，取值必须是**字面量**——`ValidateMeshSourceTopology` 用文本解析
   `local_size_x = ` / `, max_vertices = ` / `, max_primitives = `，吃不了宏或常量
3. `stage_common.glsl`（descriptor 宏、scene UBO、l2w SSBO、13 个 `Vertex*Ref`）
4. varying 声明
5. `generated/mesh_options.glsl`
6. `WriteMeshVertexOutputs`

varying 必须在 options 之前：`s1_*` 数据模块要声明属性变量（`Position` / `Normal` / `TexCoord` /
`Color` / …），`s1_position_*` 的 `LoadVertexData()` 再按固定顺序调用各流的 `LoadXxx()`
把它们装进属性变量（未在场的流由 `vertex/s1_stream_defaults.glsl` 的空实现补齐，见 §8）。
options 整体排在 varying 之后、写出函数之前，才能既不破坏声明可见性、又保持「顶点解码由 options 决定」。

### `GetVertexColor()`：两种顶点色来源共用一个形状

`tri_192_color.mesh` 的写出函数必须有一句 `gl_MeshVerticesEXT[vid].fragVertexColor = GetVertexColor();`。
但顶点色有两种来源：直接读顶点属性（`s1_color`），或读调色板索引（`s1_palette_index`，
`VertexColor` 域在 `AutoMergeMaterialInstance` 之类的实例合并场景走后者）。

master 不允许结构性条件编译，所以让两个模块各自提供同名同签名的 `vec4 GetVertexColor()`：

- `vertex/s1_color.glsl` → `return Color;`
- `vertex/s1_palette_index.glsl` → `return unpackUnorm4x8(color_palette.color[ColorIndex]);`

一份形状覆盖两种来源，选择权仍在 options。

### 两道必须做的防护

**一、`// mesh-options-digest <FNV1a64>` 溯源注释。**
同形状的主着色器文本逐字节相同，两种不同顶点格式会算出同一个 stage source digest，于是磁盘上
旧的 SPV 被永久命中复用——症状是「换了顶点格式，几何还是老的」。options 文件的 include 闭包
（`ShaderLibrary/vertex/s1_*.glsl` 之外的 `generated/`）不在 ShaderLibrary 下，闭包哈希抓不到它，
所以把 options 文本的 FNV1a64 摘要作为一行注释钉在主着色器文本尾部，让 stage digest 覆盖 options。
门禁侧 `FixedMeshCatalogHas` 比对前剥掉这一行并 trim 尾随空白。

**二、options 缓存目录名必须含网格 options 哈希。**
原 `WriteOptionsToCache` 的目录名只由**片段** options 决定。两个顶点格式完全不同、但片段 options
逐字节相同的材质（例如同为 `VertexColor` 域的 `DrawTriangle` 与 `AutoInstance`）会共用同一个目录，
后写的 `mesh_options.glsl` 覆盖先写的，先编译的那份顶点数据就按错格式解码——几何静默错位，不报错。
现在目录名是 `<片段options键>-m<网格options哈希>`，`options.glsl` 与 `mesh_options.glsl` 永远成对。

### 网格阶段必须提前编译

`FinalizeShaderBuildContext` 只能在**唯一** ShaderLibrary 根下编译，而 options 根是编译那一刻的
进程级 `compile_info.includes`。因此新增的 `CompileFixedMesh` 与既有的 `CompileFixedFragment`
完全对称：设两个根 → 编译 → 收回第二个根。编好后 `CreateShaderDirect()` 因为该阶段已有 SPV 而跳过。

### 旧哈希路径的收尾

`ComputeMeshSourceShapeKey` / `TryLoadFixedMeshSource` 改名为
`ComputeLegacyMeshSourceKey` / `TryLoadLegacyMeshSource`，**FNV1a 字节序列一字未动**（已落盘的
文件名按它算出，多一字节全体失配）。它现在只剩 CharQuad / LineQuad 两个分支在用，由阶段八替换。

同时确认：`TextDraw`（CharQuad）与 `LineRender`（LineQuad）在本阶段仍报
`no fixed mesh source for legacy shape key ...`。`mesh/fixed/` 里那 3 份文件是门禁反射矩阵变体捕获的，
键与实际示例的配置不同——属于「矩阵没覆盖到实例配置」的老缺口，不是七-2 引入的（本阶段对 old path
只做了重命名）。阶段八-1 用 `mesh/char_quad.mesh` / `mesh/line_quad.mesh` 一次消掉。

### 验收结果

| 项目 | 结果 |
| --- | --- |
| `DomainOptionsRegression` | 47 PASS / 0 FAIL |
| `ShaderResourceSchemaRegressionGate all` | exit=0，0 个 FAIL |
| `ShaderDocumentProductionRegression --full` | exit=0，`full production fixtures passed` |
| `DrawTriangle` | 加载 `mesh/shape/tri_192_color.mesh`，零着色器错误 |
| `SimpleSphere` | 加载 `mesh/shape/tri_192_dataid_pos_nrm_uv.mesh`，零着色器错误 |
| `AutoInstance` | 加载 `mesh/shape/tri_192_color.mesh`，零着色器错误 |
| `AutoMergeMaterialInstance` | 加载 `mesh/shape/tri_192_dataid.mesh`，零着色器错误 |
| `SimpleCube` / `SkyCubeSphere` | 零着色器错误 |

四个此前报告「no fixed mesh source for shape key …」的示例全部恢复，且不再需要为任何单个示例补文件。

---

## 阶段七-4：门禁改审形状 master，退役过渡态哈希目录

### 为什么必须做这一步

七-2 让网格阶段「按形状物理固定」，但磁盘上仍躺着 22 份 `<形状键>.mesh`——它们是七-2 之前
`emit-fixed-mesh` 探针把生成结果按 FNV1a 键落盘的产物。其中的键吃的是**请求的完整字节序列**
（位置格式、invocation 数、位置映射、投影、`needs_l2w`、10 个 vary 布尔、stage 接口列表），
所以只要示例的顶点配置稍有不同，键就不同，就要再补一份文件。这正是「打地鼠」的物化形态。

留着这批文件的害处不是占空间，而是**它还在被当作真源**：

* 门禁 `AuditFixedMeshMasters` 只审 `mesh/fixed/`，而运行时实际加载的是 `mesh/shape/`。
  审计覆盖的是已经死掉的目录，形状 master 无人审查。
* `emit-fixed-mesh` 探针仍只扫 `mesh/fixed/`，报 `unchanged=35`，看上去一切正常，
  实际上它统计的是一批不会再被读取的文件。

### 做了什么

**1. 审计范围改为双目录，形状 master 走新规则**

`FixedMeshMasterRoot()` 换成 `LoadFixedMeshMasters()`：同时读 `ShaderLibrary/mesh/shape` 与
`ShaderLibrary/mesh/fixed`，每项名字前加 `shape/` 或 `fixed/` 前缀后统一排序。
`AuditFixedMeshMasters` 因此对 11 份形状 master + 3 份拓扑专用源一并审查。

对**含 `#include "generated/mesh_options.glsl"`** 的形状 master 新增三条断言：

| 断言 | 为什么 |
| --- | --- |
| `generated/mesh_options.glsl` 只能 include 一次 | 两次 include 会重复定义顶点解码相关实体 |
| 不得直接 include `vertex/s2_*` / `vertex/s3_*` | 位置映射与投影已整体下沉到 options，主着色器再直引一份就会与 options 选择的那份冲突 |
| 不得出现 `#ifdef` / `#if` / `#ifndef` / `#elif` | 顶点格式差异必须由 options 承载。主着色器里出现条件编译，等于把「按顶点格式选择」偷偷还给编译期分支 |

第三条是这次最关键的一条：七-2 之前，LineQuad 那份 master 里就有 4 个活 `#ifdef`
（`S1_PALETTE_INDEX_GLSL` / `S1_TRANSFORM_ID_GLSL` / `S1_SIZE_GLSL` / `S1_COLOR_GLSL`），
它们正是生成器时代的残留，也是阶段八 8.1 要清掉的东西。门禁现在会把它们点出来。

其余 master（`char_quad` / `line_quad` 那批）继续走原有规则：不得引入 Stage2、
Stage3 必须存在且排在 Stage2 之后、varying 维度必须等于 `max_vertices` / `max_primitives`、
`perprimitiveEXT` 与 `Flat` 必须一一对应、声明了就必须写。

**2. `emit-fixed-mesh` 探针退役写盘职责**

`EmitOneFixedMeshSource` → `CheckOneFixedMeshSource`：删掉 `dir` / `capture` / `written` /
`unchanged` 参数，删掉 `std::ofstream` 写盘与 `catalog.push_back`，只报两种结果：

```
[covered] lit-with-uv
[missing] some-shape: 网格阶段文本在 ShaderLibrary/mesh/shape 下没有对应 master（形状 master 需人工按形状新增）
```

汇总行从 `written=0 unchanged=35 failed=0` 改为 `covered=35 missing=0 failed=0`，
且 `missing > 0` 时退出码非零——原来只剩 `failed` 参与退出码，缺源会被当成正常。

探针本身保留，因为它的价值不在写盘：形状由**请求**决定，三张矩阵（反射覆盖、领域后端等价、
缓存身份）里哪些形状需要覆盖只有它一处维护。在别处复现同一张矩阵等于抄两份，抄歪了不会报错，
只会静默少覆盖一个形状——`builtin/error_checkerboard_3d` 就是这么漏过一次。

**3. 删除 19 份过渡态哈希文件**

`ShaderLibrary/mesh/fixed/` 从 22 份降到 3 份，保留：

| 文件 | 用途 | 去向 |
| --- | --- | --- |
| `3748810927694461494.mesh` | CharQuad（`max_vertices=256` / `max_primitives=128`） | 阶段八 8.1 |
| `1383689879301672014.mesh` | LineQuad，带顶点色 | 阶段八 8.1 |
| `13907659216699454923.mesh` | LineQuad，无 varying | 阶段八 8.1 |

删掉的 19 份是各 `tri_192_*` 形状 master 已完整覆盖的旧键（其中 1 份 debug-normal 只出现在
探针日志里，从未出现在示例配置中）。删后 `emit-fixed-mesh` 仍是 `covered=35 missing=0`——
覆盖率没有下降，说明这些文件确实已无独占内容。

### 验收结果

| 项目 | 结果 |
| --- | --- |
| `ShaderResourceSchemaRegressionGate all` | exit=0，0 个 FAIL |
| `ShaderResourceSchemaRegressionGate emit-fixed-mesh` | `covered=35 missing=0 failed=0`，exit=0 |
| `ShaderDocumentProductionRegression --full` | exit=0，`full production fixtures passed` |
| `DrawTriangle` / `SimpleSphere` / `AutoInstance` / `AutoMergeMaterialInstance` / `SimpleCube` | 5/5 存活，零 `no fixed mesh source` / `AcquireShaderProgram failed` / `Skip primitive` |

### 剩余

`TextDraw`（CharQuad）与 `LineRender`（LineQuad）仍然失败，属阶段八 8.1：这两类拓扑的
master 还没搬到 `mesh/shape` 的形状命名体系，仍走 `GenerateStageSources` 的 legacy 哈希分支，
而该分支的键计算从未改动，磁盘里也一直没有匹配的键。

---

## 阶段八-1：CharQuad / LineQuad 形状 Master 化

> **状态**：✅ 完成。`TextDraw`（CharQuad）与 `LineRender`（LineQuad）从「零成功」变为零着色器错误，
> 三份 `ShaderLibrary/mesh/*.glsl.tmpl` 与最后 3 份哈希 `.mesh` 全部删除，
> `GenerateStageSources` 的 legacy 分支不复存在。

### 1. 形状 master 命名：模式前缀是必需的

七-2 只覆盖了 `VertexPassthrough`，形状名形如 `tri_192_<varyings>`。八-1 把同一套命名推广到三种模式：

```
<模式前缀>_<容量>_<varying 令牌，按固定顺序用 _ 连接>.mesh
  tri_192_color.mesh                         VertexPassthrough，192 顶点，color
  charquad_256_dataid_uv_color_style.mesh     CharQuad，256 顶点，dataid+uv+color+style
  charquad_256_dataid_uv_color.mesh           CharQuad，256 顶点，dataid+uv+color
  linequad_256_color.mesh                     LineQuad，256 顶点，color
  linequad_256_plain.mesh                     LineQuad，256 顶点，无 varying
```

> **已废弃（阶段十-1 删除）**：`charquad_256_dataid_uv_color.mesh`（无 `_style` 后缀）在
> `s1_color` 统一为 `s1_color_default`、且两份 builtin 文本材质都声明 `emit_style_id` 之后
> 再无任何输入能拼出，属孤岛残骸，已物理删除。此处保留仅作命名演进的记录。

**模式前缀不是装饰，是唯一性的一部分。** 三种模式的几何主体是**三段完全不同的算法**：
`VertexPassthrough` 是「连续三点一组」，`CharQuad` 是「每工作项一个字符四边形 + 笔位推进」，
`LineQuad` 是「每条线扩成两三角」。若共用一个前缀，`<容量>_<varyings>` 相同的
`CharQuad` 与 `LineQuad` 会撞名——而两者的顶点输出语义毫不相干。撞名的后果不是编译失败，
是**静默用错几何**。

`BuildMeshShapeName` 因此改为：

```cpp
AnsiString BuildMeshShapeName(const MeshShaderMode mode,
                              const MaterialVertexVaryingConfig & varying,
                              const uint32 max_vertices);
```

另一处改动同样关键：**容量来源由 `invocations * 3` 改为 `ResolveMeshModeTopology(...).max_vertices`**。
旧写法把「工作项数」当成「顶点数」再乘 3，只对 `VertexPassthrough` 恰好成立；
`CharQuad`（`inv*4` 顶点 / `inv*2` 图元）与 `LineQuad`（同）会算错。容量是 master 文件名的组成部分，
算错就等于查不到文件——这一次修的是**根因**，不是补一个文件名。

### 2. 两份共享几何主体

形状之间的差异只在 **varying 集合**，几何算法是同一份。所以先抽主体，再让形状提供钩子：

| 文件 | 内容 |
| --- | --- |
| `ShaderLibrary/mesh/char_quad_body.glsl` | 笔位推进、字符度量、缩放/旋转/斜切、`viewport.ortho_matrix` 投影；调 `WriteCharQuadVertexOutputs(vid, uv, color)` ×4 与 `WriteCharQuadPrimitiveOutputs(prim, data_id, style_id)` ×2 |
| `ShaderLibrary/mesh/line_quad_body.glsl` | 端点解析、屏幕空间垂直扩张、4 顶点 2 图元；调 `GetClipPos(GetLocalPos())` 与 `WriteLineQuadVertexOutputs(vid)` |

主体里**没有再出现任何 varying 数组名**——只出现钩子名。少一个 varying 的形状只需重写两个钩子函数体，
不必复制整套几何（这正是「打地鼠」式复制哈希文件的反面）。

### 3. CharQuad 与 LineQuad 的不同：一个用 options，一个不用

| | 是否 include `generated/mesh_options.glsl` | 原因 |
| --- | --- | --- |
| `tri_*` / `linequad_*` | ✅ | 主体调 `GetClipPos(GetLocalPos())`，Stage 2 映射层与 Stage 3 变换层必须注入 |
| `charquad_*` | ❌ | 它不读任何外部顶点流；三层文本数据（字符度量 / 样式 / 实例）与整条 2D 变换链由 `vertex/s1_text_char_quad.glsl` + `viewport.ortho_matrix` 自持 |

对应地在 `GenerateStageSources` 里 `CharQuad` 直接跳过 `EmitMeshOptions`——不是特判偷懒，
而是 `EmitMeshOptions` 的契约本来就把「有 Stage 1 顶点解码」当硬前提（`ResolveVertexABI` 对
`VertexInputMode::None` 早退，`resolved_vertex_input_glsl` 为空，函数必然返回 false）。
写盘侧 `DomainShaderBuildBackend` 也只在 `mesh_options_glsl.Length() > 0` 时才写文件，两端一致。

### 4. `GenerateStageSources` 归一：legacy 分支彻底删除

八-1 之前该函数有两条路：`CharQuad` / `LineQuad` 走 `ComputeLegacyMeshSourceKey` +
`TryLoadLegacyMeshSource`（哈希键 + `mesh/fixed/<key>.mesh`），`VertexPassthrough` 走形状命名。
八-1 删掉整条 `else` 分支与这两个函数，三种模式共用一条路径：

```
mode → ResolveMeshModeTopology(mode, invocations) → capacity
     → BuildMeshShapeName(mode, varying, capacity.max_vertices) → shape_name
     → 读 ShaderLibrary/mesh/shape/<shape_name>.mesh
     → if (!IsCharQuadMode) { EmitMeshOptions(...) ; 追加 // mesh-options-digest <FNV1a> }
```

`// mesh-options-digest` 仍然是必需的：master 文本对同一形状**逐字节相同**，
不同顶点格式的差异全在 options 里，摘要进 `plan.ms` 才能让缓存键分开。

随之删除的磁盘文件：

| 文件 | 原用途 |
| --- | --- |
| `ShaderLibrary/mesh/fixed/3748810927694461494.mesh` | CharQuad |
| `ShaderLibrary/mesh/fixed/1383689879301672014.mesh` | LineQuad（带顶点色） |
| `ShaderLibrary/mesh/fixed/13907659216699454923.mesh` | LineQuad（无 varying） |

`ShaderLibrary/mesh/fixed/` 目录至此为空并移除：**形状由「模式 + 容量 + varying 集合」直接命名，
不再由哈希键间接寻址**，哈希文件这一过渡形态没有存在理由。

### 5. 两处符号归位

**(a) `GetLineSize()`**：`LineQuad` 需要「线宽 + 最小宽度」两个数，来自 Size 顶点流。
提供者是 `s1_size.glsl`：

```glsl
vec2 GetLineSize() { return sbo_vertex_size.data[draw_params.vertex_base + VertexIndexID]; }
```

但 Size 流是**可选**的——没有 Size 的形状不应因此编不过。所以 fallback 写在
`EmitMeshOptions` 生成的 options 文件尾部：

```glsl
#ifndef S1_SIZE_GLSL
vec2 GetLineSize() { return vec2(1.0, 1.0); }
#endif
```

位置不能挪：master 的顺序是 `mesh/stage_common.glsl` → `generated/mesh_options.glsl`，
只有生成文件的**尾部**才能看见 Stage 1 是否定义了 `S1_SIZE_GLSL`。
换句话说，「按需提供缺失符号」这件事只有生成器知道该不该做，所以它必须留在生成文件里，
而不是散落到各形状 master。

**(b) `HGL_L2W_FROM_VERTEX_ATTR`**：从 C++ 发射器搬进 `s1_transform_id.glsl` 自身。
理由是这个宏的语义就是「TransformID 模块在场」，模块自己定义它，两者不可能漂移。
`vertex/helpers/orient_world.glsl` 里保留一个真正的 `#ifdef`：两个分支是**同名函数的两种语义**，
不存在能同时服务两者的中间层，这是允许保留的条件编译。

### 6. 门禁收敛

| 函数 | 改动 |
| --- | --- |
| `LoadFixedMeshMasters()` | 扫描根由 `{mesh/fixed, mesh/shape}` 收敛为 `{mesh/shape}` |
| `RunShaderSemanticRegistryCase` 断言 ④ | `dirs[1]` 改为仅 `mesh/shape` |
| `LoadFixedMeshCatalog` | 同上 |
| `CheckOneFixedMeshSource` | 失败文案改为 `ShaderLibrary/mesh/shape 下缺形状 master（需人工按形状新增）` |
| `MaterialShaderCompiler.cpp` 错误文案 | `mesh/fixed/<shape_key>.mesh` → `mesh/shape/<shape_name>.mesh` |

**踩到的一个坑（值得记住）**：断言 ④ 对「含 `generated/mesh_options.glsl` 的 master」执行严格规则，
其中一条是**禁止 `#ifdef` / `#if` / `#elif` / `#ifndef` 出现**。实现是**对整份文件文本做裸子串扫描**，
于是 `linequad_256_color.mesh` 的**注释里**写到「那批 `#ifdef` 已随生成器一起删除」时，
门禁判定该文件含条件编译分支——**散文触发了规则**。

本次的处理是改写注释（避免字面量），因为改门禁语义属于 8.2 的收敛范围。
长期看门禁应当先剥掉 `//` 行注释再扫描：文档需要能讨论这些构造，
而「讨论」与「使用」在文本层必须区分开。

### 7. 删除三份 `mesh/*.glsl.tmpl` 并重指向契约

`char_quad.glsl.tmpl` / `line_quad.glsl.tmpl` / `vertex_passthrough.glsl.tmpl` 是阶段三外置的
固定骨架，全部内容已由 `mesh/*_body.glsl` + `mesh/stage_body.glsl` 承载，且 6.5 删除 `meshgen/*` 后
已无生产代码引用它们。删除后契约的**归属人**改为新文件：

| 测试 | 原契约文件 | 新契约文件 |
| --- | --- | --- |
| Test 29 ④ | `vertex_passthrough.glsl.tmpl`（`draw_params.is_indexed != 0u`） | `mesh/stage_body.glsl` |
| Test 29 ⑤ 禁复活 | `vertex_passthrough.glsl.tmpl`（`draw_params.total_vertices`） | `mesh/stage_body.glsl` |
| Test 34 | `line_quad.glsl.tmpl` ×2 | `mesh/line_quad_body.glsl` ×2 |
| Test 34 | `char_quad.glsl.tmpl` ×2 | `mesh/char_quad_body.glsl` ×2 |
| Test 34 | `vertex_passthrough.glsl.tmpl` ×1 | `mesh/stage_body.glsl` |

契约**语义一字未改**：`const uint total_elements = (draw_params.is_indexed != 0u)` 在
`line_quad_body.glsl:33` 与 `char_quad_body.glsl:27` 逐字保留，禁复活项
（`draw_params.vertex_count >> 1u` / `const uint total_chars = draw_params.vertex_count;` /
`draw_params.total_vertices`）在新文件里依然不存在。

### 8. 决策记录：`#ifdef HGL_*_LOADER` → 空对象 stub（已执行，宏已全删）

历史状态：`s1_position_vec3/vec2/vec2i.glsl` 的 `LoadVertexData()` 里有 7 段
`#ifdef HGL_<STREAM>_LOADER` / `<宏>` / `#endif`，各 `s1_*.glsl` 流模块自述
`#define HGL_<STREAM>_LOADER { ... }` 并展开。当时的备选是：

- **甲**：改成 `#include` 代码片段 → 3 位置变体 × 7 流 = **21 份片段文件**换 14 行 `#ifdef`，负收益；
- **乙**：保留 `#ifdef` 宏（当时的暂定结论）；
- **丙（最终采纳）**：**空对象 stub + 函数调用**。

采纳丙的形态：

1. 每个流模块自述 `#define S1_STREAM_<NAME> 1` 并提供 `void Load<Name>()`；
2. `vertex/s1_stream_defaults.glsl` 用 `#ifndef S1_STREAM_<NAME>` 为**未在场的流**补
   同名空函数（6 个：UV / NTB / Color / Luminance / TransformID / Size）；
3. 三个位置模块先 `#include "vertex/s1_stream_defaults.glsl"`，再在 `LoadVertexData()`
   里**无条件按固定顺序**调用 6 个入口。

为什么丙优于乙：`#ifdef` 方案的失效模式是「漏写一对 `#ifdef` = 该属性静默为 0」
（AutoInstance 全黑的成因就是 `s1_position_vec2` 漏了顶点色装载），而这恰恰是
**最难在测试里发现**的一类错误——几何正确、只有一个属性错。丙把这个事故类别
**物理消除**：位置模块里不再有分支，「忘记 `#ifdef`」这个动作不再存在；替换后的
新失效模式（漏写 `LoadXxx();` 调用）由 `VertexLoaderConsistency` 工具做三方对账
（守卫 ↔ stub ↔ 三个位置模块的调用序列），规则全部从 GLSL 文本推导。
另外丙不再依赖「位置模块必须最后 include」这一隐式约定，文件顺序耦合随之消失。

`HGL_L2W_FROM_VERTEX_ATTR` **不在此列、保留**：它是「属性在位 → `GetL2W()` 走
`l2w.mats[TransformID]`，否则按实例号查 `l2w_index`」的属性选择宏，不是装载宏。

### 验收结果

| 项目 | 结果 |
| --- | --- |
| `ShaderResourceSchemaRegressionGate all` | exit=0，**59 个 PASS，0 个 FAIL** |
| `ShaderResourceSchemaRegressionGate emit-fixed-mesh` | `covered=35 missing=0 failed=0`，exit=0 |
| `ShaderDocumentProductionRegression --full` | exit=0，`full production fixtures passed`（9 个 depth/shadow 已知跳过） |
| `TextDraw`（CharQuad） | 存活，**零** `no fixed mesh source` / `AcquireShaderProgram failed` / `Skip primitive` |
| `LineRender`（LineQuad） | 存活，零上述错误 |
| `DrawTriangle` / `SimpleSphere` / `AutoInstance` / `AutoMergeMaterialInstance` / `SimpleCube` | 5/5 存活，零上述错误 |

合计 **7/7 示例通过**。

### 冒烟测试发现的一处遗漏

`TextDraw` 首轮仍报 `cannot load fixed mesh master: shape=charquad_256_dataid_uv_color`。
根因不是命名算错，而是 **`builtin/text_gpu_bitmap` 的 varying 集合本身就没有 `emit_style_id`**
（`BuiltinMaterialDefinitions.cpp:90`），于是形状名自然少了 `style` 令牌。
补 `charquad_256_dataid_uv_color.mesh`（钩子里丢弃 `style_id`）后通过。

这恰好演示了形状命名的**正确行为**：varying 集合不同 ⇒ 形状名不同 ⇒ 必须是另一份 master 文件。
这正是「fragment 输入集合必须逐 location 对上」的物理约束在 mesh 侧的镜像，
不能用同一份源里的开关冒充。

### 剩余

8.1 的验收项（`TextDraw` + `LineRender` 零着色器错误）已达成，下一阶段是 8.2 门禁收敛。

顺带发现一处**与本次改动无关的既有失败**，归 8.2：`TestCSMIncrementalPass` 的 Test 14 断言
`src/ShaderGen/compile/MaterialShaderCompiler.cpp` 里存在 `kMaterialInstanceAddressesFieldNames[field_index]`，
但该发射代码在域固定架构落地时已整体移走（`git show HEAD` 版本中同样不存在），
于是该用例在 HEAD 上就已经失败。因为它在 Test 14 就中止，Test 29 / Test 34 本轮无法通过运行验证；
契约改写的正确性用逐条字符串核对代替（见 §7 的两列对照），文件存在性与匹配文本均已确认。
## 阶段八-2：门禁测试全面收敛到终态事实

8.2 的目标不是"再删一批代码"，而是**把门禁从"生成器打算做什么"改判为"终态架构里什么是真的"**。
生成器删除后有一类契约会自动失效却不报错：它断言的对象（发射器）没了，
或者它自己拼一份期望字符串喂进去再查输出里有没有——那种断言连整段内容丢失都能过。
本阶段逐条处理。

### 1. Test 14：从"发射器遍历列表"改为跨文件逐字段比对

**失效点**：Test 14 ② 原本断言 `src/ShaderGen/compile/MaterialShaderCompiler.cpp` 里存在
`kMaterialInstanceAddressesFieldNames[field_index]`，即"GLSL 行结构由发射器遍历 X 列表生成"。
域固定架构落地时那段发射代码整体移走，GLSL 侧改成 `ShaderLibrary/domain/root_bindings.glsl`
里**物理固定的手写文本**——于是这条断言在 `HEAD` 上就已经失败，整个
`TestCSMIncrementalPass` 在 Test 14 中止（Test 29 / Test 34 因此长期没有真正跑过）。

**为什么不能简单删掉**：契约的**意图**（CPU 与 GLSL 两侧行结构不漂移）依然活着，只是**位置**变了。
手写文本必然有漂移面：加一个字段只改一侧，编译照样通过，运行时整条 BDA 寻址**静默错位**——
比读错值更糟，没有任何报错，只有画面不对。

**做法**：新增 `CheckMaterialInstanceAddressesRowMirror()`（`TestCSMIncrementalPass.cpp`），
两个解析器 + 一次**按顺序**逐字段比对：

| 侧 | 抽取方式 |
| --- | --- |
| C++ | 解析 `inc/hgl/graph/ShaderBufferSources.h` 的 `HGL_MATERIAL_INSTANCE_ADDRESSES_FIELD_LIST(M)` 宏体（按行尾 `\` 续行），每行取 `M(name, "glsl_type", cpu_type)` 的前两段 |
| GLSL | 解析 `ShaderLibrary/domain/root_bindings.glsl` 的 `struct MaterialInstanceAddresses { … }` 体，取每条 `type name;` |

比对**字段数与每一项的 (名字, GLSL 类型)**，顺序不同即失败——顺序决定行内偏移，错位与缺字段等价。
失败信息直接打印第几个字段两侧各是什么。

> 解析器必须容忍 CRLF：初次实现用 `line.back() == '\\'` 判续行，但工作区是 CRLF，
> 行尾是 `\r` 而不是 `\`，续行判定当场断掉、宏体解析为空。教训：**判行尾先 trim**。

**非空验证**（这是本阶段最重要的一步——守卫本身也要被证明不是摆设）：

| 探针 | 结果 |
| --- | --- |
| 把 GLSL 的 `shadow_flags` 改成 `shadow_flagz` 后直接跑（无需重编译，运行期读文件） | `exit=14`，`行结构第 3 个字段不一致——C++ 是 (shadow_flags, uint)，GLSL 是 (shadow_flagz, uint)` |
| 还原后 | `exit=0`，Test 14 `Passed (23 checks)` |

同一轮还顺手修正了两处会误导后来者的**过时注释**：
`root_bindings.glsl` 前导仍写着"字段顺序 = C++ 侧 `kRootAddressesFieldNames` 的顺序（MaterialShaderEmitter）"，
`ShaderBufferSources.h` 仍写着"GLSL struct 发射全部从这一份生成（发射端见 `MaterialShaderCompiler.cpp`
的 `BuildFSIndexTableDecls`）"。两处都改成指向真实现状 + 指向守住它的那条测试。

**连带收获**：`TestCSMIncrementalPass` 首次**完整跑通**——50 条用例全绿，
Test 29（22 条契约）与 Test 34（5 条契约）第一次真正执行而不是靠字符串核对。

### 2. 门禁 ④ 的 `#ifdef` 扫描改为只看代码

形状 master 的 ④ 判据是"不得含预处理分支"，实现为对整份文件文本做
`"#ifdef" / "#if " / "#elif" / "#ifndef"` 子串扫描——**注释也算**。
于是"本文件刻意不留 `#ifdef`"这种正当文档会被判死，8.1 只能靠改措辞绕开。

本阶段新增 `StripGlslComments()`（去 `//` 行注释与 `/* */` 块注释，块注释按行保留换行以便偏移可用），
`AuditFixedMeshMasters` 内的 `text` 改为**剥注释后的代码文本**，判据对象回到"代码"。

同一份文本还被 ①②③④ 的 include / `layout(` / 写入探针共用，改动后偏移全部在同一份剥后文本上取值，
不会出现"一半在原文本取偏移、一半在剥后文本查子串"的错配。

**非空验证（两个方向都要证，否则等于把门关掉）**：

| 探针（改 `tri_192_color.mesh`，无需重编译门禁） | 结果 |
| --- | --- |
| 在文件头加一行注释 `// 探针：形状 master 不得含 #ifdef / #if 分支` | `MV.fixed-mesh-varying-invariants` **PASS**（改动前会被判死） |
| 再加真正的 `#ifdef PROBE_FLAG` / `#endif` 两行代码 | `MV.fixed-mesh-varying-invariants` **FAIL**：`形状 master 不得含预处理分支 #ifdef` |
| 还原 | gate `all` `exit=0`，59 PASS |

### 3. `Z.material-bda-emitter`：去掉"自己拼期望值再回声"的空转

该用例原本把 `{ "base_color", Sampler2D, true }, { "normal", Sampler2DArray, false }` 等
自建文本作为 `fixed_fragment_source` 喂给编译器，然后断言输出里有
`#define MTL_ROW(i) EmissiveSurfaceRow(...payload_index...)`——期望值**硬编码在测试里**，
与库文件毫无关系：库里的 `MTL_ROW` 改了、删了、甚至整段丢了，只要测试自己那份字符串没变，照样绿。

改为从库文件取真值：

- 新增 `ExtractDefineLine(src, prefix)`：取以 `#define MTL_ROW(i)` 开头的整行（去行尾 CR/空白），
  作为**期望值**；库文件不再定义该宏 ⇒ 直接失败（`emissive_row_bindings.glsl 不再定义 MTL_ROW 行别名`）。
- 断言该行在最终 stage 文档里**恰好出现一次**（防重复发射）。
- 断言三要素 `struct EmissiveSurfaceData` / `buffer EmissiveSurfaceRow` / `MaterialInstanceAddressesRef`
  的声明点在**库文件**（`emissive_row_bindings.glsl` 或 `root_bindings.glsl`）里，而不是测试字符串里。
- 保留真正有语义的管线断言：`RequiresRecipePayloadRow()` 对有 payload 的材质为真、
  对纯纹理材质为假；`RequiresRecipeRuntimeRows()` 反之；扩展声明 → 值结构 → 行别名的顺序不变。

**为什么纯纹理子用例仍保留合成源**：真实库 `standard_opaque_bindings.glsl` 同时声明
`MTL_ROW` 与 `MTL_TEX`（行跨度 32 / 贴图引用跨度 48），无法用来证明"只有贴图行时不会多发 payload 行"。
那一段要证明的是**编译器的判定语义**而非库文件内容，故刻意自建最小源，并在代码里写明这一点。

**非空验证**：把库里的 `#define MTL_ROW(i)` 改成 `#define MTL_ROWX(i)` ⇒ `Z.material-bda-emitter` **FAIL**
并打印上面那句诊断；同时 `SPV.generated-interface` / `DH.domain-backend-equivalence` / `DH.domain-backend-cache`
也一起 FAIL——这三条会真编 shader，反过来证明它们不是空转。

### 4. `X.transform-graph-composition`：复核后无需改动

该用例在 7.x 已经改判过："这个图会拼出哪些 Stage2/Stage3 模块"不再靠生成一段 GLSL 去观察，
而是直接问 `VertexNodeConfigResolver` 要模块路径，再核对**该模块文件真的在库里**。
复核确认已是终态形态（少一个文件、改名、删枚举分支都硬失败），本阶段不动。

### 验收结果

| 命令 | 结果 |
| --- | --- |
| `ShaderResourceSchemaRegressionGate.exe all` | `exit=0`，**59 PASS / 0 FAIL** |
| `ShaderResourceSchemaRegressionGate.exe emit-fixed-mesh` | `covered=35 missing=0 failed=0`，`exit=0` |
| `ShaderDocumentProductionRegression.exe --full` | `exit=0`，`full production fixtures passed` |
| `TestCSMIncrementalPass.exe` | `exit=0`，**50 条用例全绿**（Test 14 / 29 / 34 首次真正执行） |

### 剩余

8.2 的判定性收敛已完成。下一阶段 8.3 只做**改名**（`MaterialShaderCompiler` → `StageSPVCompiler`、
`GenericMaterialBuilder` → `MaterialProgramAssembler`、抹掉 `src/ShaderGen` 模块名），
不改语义；改名之后，门禁用例名里的 `emitter` / `emit` 一类措辞一并复核。

## 阶段八-3：程序构建模块命名与数据层归属收敛

> **状态**：✅ 完成。程序构建目标不再使用 `ShaderGen` 目录/target 名称；阶段编译与材质程序组装的文件和 API 改为职责命名，GLSL 模块数据及资源清单实现归入 `ShaderMaterialData`。

### 1. 构建模块重命名

`src/ShaderGen` 与 `ULRE.ShaderGen` 分别改为 `src/ShaderProgramBuild` 与 `ULRE.ShaderProgramBuild`。根聚合目标通过 `ULRE_PROGRAM_BUILD` 链接新 target，回归工具、发行形态检查脚本及运行时注释同步使用新名称。

`ULRE_ENABLE_SHADERGEN` / `ULRE_ENABLE_SHADERGEN=0` 保留为既有配置接口，避免仅重命名内部模块时改变开发/发行构建行为；它现在控制 `ULRE.ShaderProgramBuild` 是否构建。

### 2. 按实际职责重命名接口

仓库中原名对应的是自由函数与输入结构，而非同名 C++ class；因此这次重命名覆盖了文件、公开 API、计划结构、诊断函数与全部调用点：

| 旧名称 | 新名称 |
| --- | --- |
| `MaterialShaderCompiler.h/.cpp` | `StageSPVCompiler.h/.cpp` |
| `MaterialShaderCompilerInput` / `MaterialCompileConfig` | `StageSPVCompilerInput` / `StageSPVCompileConfig` |
| `BuildMaterialStageDocuments` | `CompileMaterialStages` |
| `GenericMaterialBuilder.h/.cpp` | `MaterialProgramAssembler.h/.cpp` |
| `BuildGenericMaterial` / `GenericMaterialBuildPlan` | `AssembleMaterialProgram` / `MaterialProgramAssemblyPlan` |
| `MaterialShaderBuildCapture` | `MaterialProgramBuildCapture` |

`MaterialProgramBuildCapture` 移到 assembler 头中，由写入它的组装侧拥有；阶段编译仍通过 `FinalizeShaderBuildContext` 保持原有编译/缓存流程。固定网格源注释同步改为 `mesh/shape/<shape_name>.mesh`。

### 3. GLSL 模块元数据归属下沉

`ShaderCodeModule`、文件元数据解析、模块注册表与 `ShaderCodeResourceManifest` 的实现从程序构建 target 移入 `src/ShaderMaterialData`，对应公共头由数据层 target 统一登记。`MaterialRecipe` / `RenderTemplate` 使用这些描述符时不再跨层引用程序构建私有源。

资源清单实现此前通过 `builder/MaterialResourceManifestBuilder.h` 间接取得字符串比较函数；下沉后改为本文件已有的 `std::strcmp`，完整的「材质定义 → code-module 资源清单」辅助入口仍留在组装模块。两层依赖方向保持为 `ShaderProgramBuild → ShaderMaterialData`。

### 4. 终态措辞与门禁名

- `Z.material-bda-emitter` 改名为 `Z.material-bda-fixed-bindings`，对应它现在验证的固定库绑定真值与最终文档，而不是旧发射器假设。
- 描述符验证生成物随 `DescriptorMacroGen` 更新并通过 `--verify`；`stage_common_head.glsl` 中依赖已删除 mesh-header generator 的说明替换为固定形状 master 的共享前缀说明。
- 发行脚本继续接受旧的 `ULRE_ENABLE_SHADERGEN` 配置名，但检查的实际产物是 `ULRE.ShaderProgramBuild.*`。

### 验收结果

| 命令 | 结果 |
| --- | --- |
| Debug 构建 `ShaderResourceSchemaRegressionGate` / `ShaderDocumentProductionRegression` / `TestCSMIncrementalPass` | 全部构建成功 |
| `ShaderResourceSchemaRegressionGate.exe all` | `exit=0`，**59 PASS / 0 FAIL** |
| `ShaderResourceSchemaRegressionGate.exe emit-fixed-mesh` | `covered=35 missing=0 failed=0` |
| `ShaderDocumentProductionRegression.exe --full` | `exit=0`，`full production fixtures passed`；已知 9 个 depth/shadow master 缺口仍为 skip |
| `TestCSMIncrementalPass.exe` | `exit=0`，Test 51 `All CSM Incremental Pass Contract Tests PASSED` |
| `git diff --check` | 无空白错误；活动源码/构建配置中不再引用 `src/ShaderGen`、`ULRE.ShaderGen` 或旧的编译器/组装器 API |

**剩余**：8.3 完成；下一步进入阶段九，全量示例与发行形态验收。

## 阶段九-1：全量示例运行与缺失形状补齐

> **状态**：✅ 示例清单通过。8 个目标均以当前源码重建并启动；每个进程都初始化 GPU cull、退出码为 0，日志中没有 shader 构建失败、缺失 master 或 unresolved pipeline。

### 1. LoadGeometry 揭示的形状缺口

`LoadGeometry` 的包围盒线框使用 `builtin/pure_color` + `PrimitiveType::Lines`。它同时要求 `dataid` varying，因而实际 shape 是 `linequad_256_dataid`；已有 corpus 只有 `linequad_256_color` 与 `linequad_256_plain`。首次运行稳定报：

```
fixed mesh master source is missing or unreadable: mesh/shape/linequad_256_dataid.mesh
AcquireShaderProgram failed for BBox_0 ... unresolved runtime pipeline
```

新增 `ShaderLibrary/mesh/shape/linequad_256_dataid.mesh`：位置/变换仍由 mesh options 提供，几何复用 `line_quad_body.glsl`，两个 per-primitive 三角形都写入 `gl_InstanceIndex`。`emit-fixed-mesh` 的形状矩阵追加了 `load-geometry-wire`，以后缺失该 shape 会直接失败。

### 2. 示例析构时 Geometry 所有权冲突

首次关闭窗口时，`SimpleSphere`、`BasicLitMeshes`、`LoadGeometry` 均出现应用退出期 access violation。Windows dump 符号栈指向的根因是 Geometry 的双重/逆序释放，而不是 shader 或驱动编译：

- `SimpleSphere` 已将球体交给 `GeometryManager`，应用析构又 `SAFE_CLEAR(sphere_geometry)`；移除重复释放。
- `BasicLitMeshes` 的 `MeshEntry` 同样不拥有 Geometry；移除其析构删除，并在释放 `VertexDataManager` 前让 `GeometryManager` 先清理 Geometry，避免 `GeometryDataVDM` 析构时回调已释放的 VDM。
- `LoadGeometry` 的 bbox Geometry 已由 `GeometryManager` 管理，移除应用析构中的重复 `delete`；文件载入 Geometry 仍由对应 mesh entry 持有。

### 3. 示例验收

| 示例 | 运行方式 / 结果 |
| --- | --- |
| `DrawTriangle` | Debug 重建；启动并正常关闭，rc=0 |
| `AutoInstance` | Debug 重建；启动并正常关闭，rc=0 |
| `AutoMergeMaterialInstance` | Debug 重建；启动并正常关闭，rc=0 |
| `SimpleSphere` | Debug 重建；启动并正常关闭，rc=0（原退出期 AV 已消除） |
| `BasicLitMeshes` | Debug 重建；启动并正常关闭，rc=0（VDM/Geometry 销毁顺序已修正） |
| `PBRSpheres` | `PBR_FRAMES=8`；rc=0，GPU cull 已初始化 |
| `LoadGeometry` | Debug 重建；线框 master 被加载并编译，启动/关闭 rc=0 |
| `LoadScene` | `LS_FRAMES=8`；帧数完成并 rc=0，GPU cull 已初始化 |

8 个运行日志均为 **0** shader/pipeline 错误；每个都出现 `GPUCullSystem Initialized`。固定 mesh 覆盖门 `emit-fixed-mesh` → `covered=36 missing=0 failed=0`；gate `all` → `59 PASS / 0 FAIL`。

**非门禁观察**：单独运行 Debug `PBRSpheres` 的 `PBR_SELFCHECK=1` 时，画面非空（`distinct_byte_values=256`），但当前设备得到 FNV `0x5EE2B4F88E551284`，与源码冻结值 `0x3DD88A0999CDC486` 不同。此阶段的图形 smoke 用定帧 `PBR_FRAMES=8` 验证管线与 GPU cull，不把跨配置/设备像素摘要当作 shader 构建判据；阶段九-2 仍需按发布形态脚本单独验收。

**剩余**：9.1 完成；继续执行阶段九-2 的全门禁与发行形态验证。

## 阶段九-2：全门禁与发行形态封口

> **状态**：✅ 完成。开发形态回归、固定 mesh 覆盖、全量 production fixtures 与发行形态跨构建树探针全部通过。

### 1. 最终开发形态验收

| 命令 | 结果 |
| --- | --- |
| `ShaderResourceSchemaRegressionGate.exe all` | `exit=0`，**59 PASS / 0 FAIL** |
| `ShaderResourceSchemaRegressionGate.exe emit-fixed-mesh` | `covered=37 missing=0 failed=0` |
| `ShaderDocumentProductionRegression.exe --full` | `exit=0`，`full production fixtures passed` |
| `TestCSMIncrementalPass.exe` | `exit=0`，Test 51 全部通过 |
| 图形示例 smoke | 9.1 列出的 8 个程序均重建、退出码 0；GPU cull 已初始化，shader 构建错误为 0 |

`--full` 仍将 **9 个**缺少 depth/shadow 固定 master 的 fixture 报为 skip；这是阶段六已登记的覆盖缺口，不被记作通过或失败，也不影响当前示例与当前 master 的验收。

### 2. `verify-release-form.ps1` 五步验收

脚本使用独立 `build_probe_nosg` 发行树，实际运行结果：

| 步骤 | 判据 | 结果 |
| --- | --- | --- |
| 1/5 | 配置开发形态与 `ULRE_ENABLE_SHADERGEN=OFF` 发行形态构建树 | PASS |
| 2/5 | 开发树构建 `ShaderResourceSchemaRegressionGate` | PASS |
| 3/5 | 烘焙材质 program 与 compute manifest / SPV 夹具 | PASS |
| 4/5 | 发行树构建仅链接 `ULRE::Runtime` 的 `RuntimeFormProbe`；无 `ULRE.ShaderProgramBuild` 产物 | PASS |
| 5/5 | 发行探针只读加载材质与 compute 夹具 | PASS |

终端汇总：`[PASS] release form verified: baked cache resolved with zero generator symbols`。

### 3. 项目结项

- 开发构建的物理固定阶段源由 MaterialDomain fragment masters、shape-named mesh masters 与最小 options 驱动；运行时程序构建层命名为 `ShaderProgramBuild`，材质/模块元数据归 `ShaderMaterialData`。
- 发行形态不构建 `ULRE.ShaderProgramBuild`，仍可通过预烘焙 program/compute manifest 与 SPV 在独立进程内解析运行。
- 各阶段按独立提交收敛：8.1 `43eb3c0e1`、8.2 `10d671213`、8.3 `9a625738e`、9.1 `d283af3a1`；9.2 为本次文档结项提交。
- `ULRE_ENABLE_SHADERGEN` 是兼容保留的 CMake 选项名；其控制的内部 target 已重命名为 `ULRE.ShaderProgramBuild`。

**未结项阻塞**：无。保留的边界事项是 9 个 depth/shadow fixture 尚无固定 master，以及 Debug `PBRSpheres` 的冻结像素摘要与当前设备实测摘要不同；后者不影响本阶段通过的定帧运行、shader 构建、GPU cull 和发行形态探针。

## 阶段九后续回归：PlaneGrid3D 的 LineQuad + Luminance

阶段九结项后，`PlaneGrid3D` 暴露了另一种可达 varying 组合：`LineQuad + dataid + lum`，请求形状名为 `linequad_256_dataid_lum`。新增 `ShaderLibrary/mesh/shape/linequad_256_dataid_lum.mesh`，用 `line_quad_body.glsl` 复用线扩张几何，并为两枚 per-primitive 输出写入同一 `gl_InstanceIndex`，四个 quad 顶点复制首端亮度值。

`emit-fixed-mesh` 的 cache-identity 形状矩阵追加了 `plane-grid-luminance-wire`（`VertexLuminance` + `Lines` + Position/Luminance 格式），避免该组合再次漏出覆盖清单。PlaneGrid3D 的析构也移除了对已注册至 `GeometryManager` 的 `geom_plane_grid` 的重复释放。

**最终复验**：PlaneGrid3D Debug 重建运行并正常退出（rc=0），确认新 master 成功打开、GPU cull 已初始化、shader/pipeline 错误为 0；gate `all` 仍为 **59 PASS / 0 FAIL**；`emit-fixed-mesh` 更新为 `covered=37 missing=0 failed=0`；`verify-release-form.ps1 -SkipConfigure` 五步再次全绿。此补充将阶段九最终覆盖数从阶段 9.1 首次验收时的 36 提升为 37。

## 阶段九后续回归：AtmosphereSky 固定表面选择与双面绘制

AtmosphereSky 的日志中，`SkyMinimal` 配方没有贴图行（`planned_texture=0`），但片元 stage key 实际加载 `domain/sky/sky.frag`，该 master 固定 include `sky_cube_surface.glsl` 并通过 `MTL_TEX` 读取 cubemap 行。GPU cull 的 `culldump` 证明实体已可见、批计数为 1、间接命令 `groupCount=(20,1,1)` 已生成；随后该无贴图 shader 路径触发 `VK_ERROR_DEVICE_LOST`，Fence/Swapchain 报错是下游现象，不是“没有绘图命令”。

修复：

- `ResolveDomainShaderTemplate` 对 Sky 的固定表面变体显式分流：默认/`sky_minimal_surface` 选 `sky_minimal.frag`，`sky_cube_surface` 保留 `sky.frag`，其他未知 Sky 表面提供者硬失败。`D.material-domain-contract` 加入三种路径选择断言。
- 新增 `domain/sky/sky_minimal.frag`，直接 include 无贴图 `surface/sky_minimal_surface.glsl`；Cubemap 变体继续使用原 `sky.frag`。
- `MakeSkyConfig` 改为 `VK_CULL_MODE_NONE`：天空球从球内观察，不能用单面绕序剔除。
- 同一 AtmosphereSky 复跑发现 `AtmosphereSkyAmbient::MeshEntry` 也重复删除由 `GeometryManager` 持有的网格，移除其析构删除。

**验证**：AtmosphereSkyMinimal/SunGizmo/Ambient 与 SkyCubeSphere 均 Debug 重建并正常退出；device-lost、GPUCull submit 与 unresolved-pipeline 错误均为 0。`SKYMIN_SELFCHECK=1` 恢复为 PASS：1280×720、219 个不同字节值、FNV=`0xC914C338EFC7790E`。RegressionGate 保持 59 PASS / 0 FAIL，固定 mesh 覆盖 37/0/0；production fixtures、Test 51 及 `verify-release-form.ps1 -SkipConfigure` 五步均复验通过。

## 阶段十-1：网格顶点解码矩阵白名单盘点（网格收口 Layer 1）

网格阶段收口的目标是把「顶点解码（s1）/局部映射（s2）/投影朝向（s3）」这条组合链从**运行时自由拼装**收敛为**可枚举的白名单**，从而获得三件事：组合数可被门禁穷举、模块顺序耦合可被消除、`#ifdef HGL_*_LOADER` 这类隐式协议可被删除。收口之前必须先有**唯一可信的组合清单**，否则后续层的任何"简化"都只能靠猜。本节就是这份清单。

### 一、模块现状全集

`generated/mesh_options.glsl` 由 `DomainOptionsEmitter::EmitMeshOptions` 拼装，顺序固定为：`s1`（流解码）→ `s2`（局部映射）→ `s3`（投影与朝向）。磁盘上的模块文件全集：

| 层 | 模块 | 数量 | 说明 |
|----|------|------|------|
| s1 位置 | `s1_position_vec3` / `s1_position_vec2` / `s1_position_vec2i` | 3 | 位置格式硬门决定，`LoadVertexData()` 在此展开 |
| s1 流 | `s1_uv` / `s1_uv_rg16f` / `s1_ntb` / `s1_ntb_rg8` / `s1_ntb_rg16f` / `s1_ntb_a2bgr10` / `s1_color` / `s1_color_default` / `s1_palette_index` / `s1_luminance` / `s1_transform_id` / `s1_size` | 12 | 每份文件以 `#define HGL_<STREAM>_LOADER {...}` 交付自己的装载片段 |
| s2 映射 | `s2_passthrough3d` / `s2_ndc_lift` / `s2_zeroone_to_ndc` / `s2_lift_xy0` / `s2_lift_x0y` / `s2_lift_0xy` / `s2_pixel_to_local` | 7 | 由 `VertexNodeConfigResolver::GetMappingModulePath` 决定 |
| s3 朝向/投影 | `s3_world_camera_vp` / `s3_l2w_only` / `s3_ortho_viewport` / `s3_ortho_then_l2w` / `s3_clip_passthrough` / `s3_camera_facing_world` / `s3_camera_facing_fixed_pixels` | 7 | 由 `GetStage3ModulePath` 的 (投影, 朝向, 缩放) 三元组决定 |

讨论口径：`s1_color_default` 是**空对象实现**（顶点色恒白），`line_quad_body.glsl` 自持的 `GetLineSize()` 默认实现是同类先例——两者共同构成本仓库"缺流即缺省"的既有范式，后续层应沿用而非另造。

### 二、示例实测可达组合

扫描 `build/` 下全部 46 份缓存 `mesh_options.glsl`（覆盖 stage9 全示例验证运行与后续若干次定点复跑），去掉 include 顺序噪声后得到 **27 种 distinct 组合**。按「s1 流集合」归并后得到可达性结论：

| 层 | 示例实测可达 | API 可达但示例未覆盖 |
|----|--------------|----------------------|
| s1 位置 | `vec3`、`vec2`、`vec2i`（全 3 种） | — |
| s1 流 | `uv`、`uv_rg16f`、`ntb`、`ntb_rg8`、`color`、`color_default`、`palette_index`、`luminance`、`transform_id`、`size`（10 种） | `ntb_rg16f`、`ntb_a2bgr10`（格式门支持，无示例使用） |
| s2 映射 | `passthrough3d`、`ndc_lift`、`zeroone_to_ndc`、`lift_xy0`、`pixel_to_local`（5 种） | `lift_x0y`、`lift_0xy`（`Make2DNodeConfig` 工厂支持，无领域/示例选中） |
| s3 朝向 | `world_camera_vp`、`l2w_only`、`ortho_viewport`（3 种） | `ortho_then_l2w`、`clip_passthrough`（仅 `Make2DNodeConfigNDC(false)` / `Make2DNodeConfigOrtho(true)` 可达）；`camera_facing_world`、`camera_facing_fixed_pixels`（**无任何领域或示例选择 CameraFacing 朝向，当前为纯死分支**） |

实测组合的矩阵形状高度集中，落点只有 4 个"家族"：

1. **默认 3D**：`s1_position_vec3` + `s2_passthrough3d` + `s3_world_camera_vp`（占绝大多数；可选叠加 `uv`/`uv_rg16f`、`ntb`/`ntb_rg8`、`color`/`color_default`/`luminance`）
2. **2D NDC**：`s1_position_vec2` + `s2_ndc_lift` + `s3_l2w_only`（`UnlitTexture` / `Checker2D` 领域契约）
3. **2D 纹理 0..1**：`s1_position_vec2` + `s2_zeroone_to_ndc` + `s3_l2w_only`（仅 `s1_uv` + `s1_color_default` 一例）
4. **文本像素空间**：`s1_position_vec2i` + `s2_pixel_to_local` + `s3_ortho_viewport`（`Text2D` 契约）
5. **顶点调色板**：`s1_palette_index` + `transform_id` + `size` + `s1_position_vec3` + `passthrough3d` + `world_camera_vp`（`VertexPalette` 领域）

`lift_xy0` 只与 `s1_luminance` 一同出现（`PlaneGrid3D` 的线框亮度路径），是家族外的唯一特例。

### 三、缓存快照的可信度边界（重要教训）

`build/` 下的缓存**不是同一代码版本的快照**：其中有若干份文件在 `mesh_options.glsl` 尾部内联了 `#ifndef S1_SIZE_GLSL` 的线宽默认实现，而当前实现已把该默认实现挪进 `mesh/line_quad_body.glsl`（源码中 `DomainOptionsEmitter.cpp` 已无该文本）。同时，部分旧快照在 LineQuad 路径上根本没有 `s1_color*` 模块，而当时 `s1_color_default` 尚未引入。

**结论：不要把缓存并集当成"当前可达集"，只能当成"证据下界"。** 后续层若需要精确的可达性判定，必须以当次代码重新生成的缓存为准，或由门禁 `emit-fixed-mesh` 直接裁决。本次盘点的实测列因此取并集（上界），API 可达列取静态推导——两层夹逼出的差异集中在上表「API 可达但示例未覆盖」一栏，恰好就是收口后可以判定去留的候选。

### 四、Layer 1 的清理动作

1. **删除孤岛形状 master**：`ShaderLibrary/mesh/shape/charquad_256_dataid_uv_color.mesh` 是 `s1_color` 统一为 `s1_color_default` 之后的残骸——两份 builtin 文本材质（`kTextGpuSource` / `kTextGpuBitmapSource`）都声明了 `emit_style_id`，因此 `BuildMeshShapeName` 只会算出 `charquad_256_dataid_uv_color_style`，无 style 的名字再无任何输入能拼出。删除前已核对引用面：`src/` 内仅 `MaterialProgramAssembler.cpp` 的一条历史注释提及，门禁无硬编码路径，无资产引用。
2. **本节即白名单基线**：Layer 2（消灭 `HGL_*_LOADER`，**已完成**，见 §8）、Layer 3（`VertexABIBuilder` 表驱动化，**已完成**：`VERTEX_STREAM_MODULES[]` + `POSITION_MODULE_TABLE[]`）、Layer 4（options 预设化）均以本节的模块全集与家族划分为对照表。
3. **门禁基线锁定**：`ShaderResourceSchemaRegressionGate all` = **57 PASS / 0 FAIL**，`emit-fixed-mesh` = **covered=37 missing=0 failed=0**。删除孤岛文件后两项计数必须不变（该形状从未进入覆盖清单）。

---

## 阶段十-2：网格 Options 预设化评估（网格收口 Layer 4）

### 一、评估对象

计划项原本的设想是：把最高频的三类组合（标准 3D PBR、2D 正交、2D 像素）做成静态预设，
从 `EmitMeshOptions` 里"查表返回常量串"，以减少**临时文件创建与磁盘 I/O**。

实测数据（本机 `build/out/Windows_64_Debug/domain-options/`，覆盖 57 项门禁 + 全部示例
变体累计下来的缓存）：

| 指标 | 实测值 |
| --- | --- |
| 成对 options 目录（`<片段键>-m<网格哈希>`） | 38 |
| 仅片段 options 目录（CharQuad 不写网格 options） | 15 |
| `generated/mesh_options.glsl` 文件数 | 38（与成对目录数一致，无遗漏） |
| 内容去重后份数 | **20** |
| 单文件大小 | 平均 183 B，最大 232 B，全部 38 份合计 **6 961 B** |
| 最高频的一份 | 出现 5 次：`s1_color_default` + `s1_position_vec3` + `s2_passthrough3d` + `s3_world_camera_vp` |

### 二、为什么"预设化"收益为零

1. **文本本身就极小**。整个 options 是 4～7 行 `#include`，最长 232 字节。`EmitMeshOptions`
   的全部工作就是查一次表 + 3～7 次 `AnsiString` 追加（≈1 次分配），量级子微秒；而它前后
   紧邻的是 FNV 遍历整棵 ShaderLibrary 闭包与 glslang 编译，量级毫秒。比值 ~1e-4。
   预设只能省掉这子微秒，属于测不出来的优化。
2. **磁盘 I/O 不可消除**。`generated/mesh_options.glsl` 必须**落在 options 根下**——glslang
   的 `#include` 是按 root-relative 从磁盘解析的，没有"内存 include"通道。预设能省的是
   拼串，不是写盘。
3. **天然去重已被内容寻址目录吃掉**。38 份文件里只有 20 份内容不同，重复的 18 份分散在
   不同目录里（目录名把片段 options + 网格 options 一起编进去，成对才安全）。而"同一目录
   重复写同一份内容"在重复构建时才发生，且写入幂等——预设在语义上无法比这更省。

**结论：不做预设化。** 记录理由而不是留一条"待办"，避免后续又被当成优化项重开。

### 三、"已存在就跳过写盘"为什么也不做

看似可行的替代是：目录名已经是内容的哈希，若 `<root>/generated/mesh_options.glsl` 已存在
就跳过写。但这会把"文件被删一半/被手工改过"从**必然被覆盖纠正**变成**静默读旧内容**——
而后果正是本方案最想消灭的那类故障（顶点按错格式解码、几何静默错位、不报错）。
用 183 字节的写换掉这个事故类别不划算。

### 四、真正要固化的是 digest 互锁

评估的产出不是预设表，而是把既有的两道防护（`// mesh-options-digest` 溯源注释、目录名含
网格 options 哈希）从"写了但没人验"提升为"门禁可证伪"：

1. **摘要计算收敛到单点**：写入目录名用的哈希、写进主着色器尾部的溯源注释、门禁校验的三方
   必须调同一个函数，否则三者可以各自漂移而都不报错。
2. **门禁断言摘要存在且正确**：原门禁在比对形状 master 前会把 `// mesh-options-digest <n>`
   剥掉，于是"摘要漏写"与"摘要算错"都能静默通过——而这两件事的后果都是磁盘上旧 SPV 被
   永久命中复用。新断言要求非 CharQuad 的网格文本**恰好一条**摘要行，且其值等于当次
   `mesh_options_glsl_out` 文本的摘要（即与真正落盘的那份字节严格互锁）。

见 `doc/ultra-thin-implementation-status.md` §12 的 Layer 4 条目。
## 阶段十-3：资源所有权单点化（网格收口 Layer 5）

Layer 1-4 都在"发射侧"（模块全集、loader 宏、ABI 选择表、options 摘要）。Layer 5 做的是
全量示例实跑，暴露出来的是**另一条轴**：资源的销毁路径。两者相关——网格到底"谁才是主人"
决定了示例能不能安全地持有它。

### 一、三类资源的唯一所有者

| 资源 | 唯一所有者 | 正确释放 | 错误写法 |
| --- | --- | --- | --- |
| `Geometry` | `GeometryManager` | `Release(g)` → `rm_geometry.Clear()` | `delete g` / `SAFE_CLEAR(g)` |
| `Texture` | `TextureManager` | `Release(tex)` | `delete tex` / `SAFE_CLEAR(tex)` |
| `Sampler` | `SamplerManager` | `Release(smp)` | `delete smp` / `SAFE_CLEAR(smp)` |
| `ECSContext` | 框架（`GetECSContext()` 返回借用） | 只 null 化 | `SAFE_CLEAR(ecs_world)` |

判据在 `CMCore/inc/hgl/type/ObjectManager.h` L72-84：`AutoIdObjectManager::Clear()` 里是
`delete obj.value`——管理器是强持有者，`Release()` 只是从表里摘掉并把所有权交回调用方，
`Clear()` 会在管理器收尾时自己 delete 表里剩下的每个对象。所以凡是通过
`Add` / `Load` / 构造进过管理器的对象，示例侧再 `delete` 一次就是**必崩的双重释放**。

### 二、示例侧出现的两类双重释放形态

1. **自建结构体里的析构**：示例常把 `Geometry*` 包进 `struct MeshEntry { ~MeshEntry(){ delete geometry; } }`
   放进 `ArrayList<MeshEntry>`。这是最隐蔽的一类——语义上像"RAII"，实际是把观察者写成了主人。
2. **App 析构里的批量回收**：`SAFE_CLEAR(geoms[i])` / `SAFE_CLEAR(sampler)` 之类，看起来是
   "收尾清理"，实际全部与管理器撞车。

**症状的一致性**：两者都表现为进程退出时的 AV，地址 `0xFEEEFEEE`——MSVC Debug 堆对已释放
块填的标记。反汇编若是 `mov rax,qword ptr [rax] ds:feeefeee'feeefeee`，基本可以直接判定
"访问了已经 delete 过的对象"，不必再怀疑逻辑。且崩溃点在**退出路径**，日志里帧循环早已正常
跑完，`AppFramework Destructor End` 缺失是唯一的文本线索。

### 三、引擎侧缺陷：`OffscreenWorld` 析构三定律

这一条不是示例的锅，是本节最有价值的产出。

`OffscreenWorld` 持有 `camera_system_` / `ubo_system_` / `collect_system_` / `rt_system_`
四个 `shared_ptr`，而 `ecs::System::context` 是**裸指针**。`ECSContext::Shutdown()` 并**不**
销毁系统（系统由 `SystemManager` 持 `shared_ptr`），于是系统的析构时机取决于**还有谁持有
最后一个引用**——如果 `OffscreenWorld` 的成员 `shared_ptr` 是最后一个，系统就会在
`ECSContext` 被 delete **之后**才析构，析构里任何走 `context->...` 的语句都是悬垂访问。

首个触雷点是 `RenderSceneUBOSystem::ReleaseViewportUBO()`：它内部
`GetBufferManager(context)` → `context->GetRenderContext()`。

实测栈：

```
~OffscreenWorld -> ~shared_ptr<RenderSceneUBOSystem>
                -> ~RenderSceneUBOSystem -> ReleaseViewportUBO
                -> GetBufferManager(context) -> GetRenderContext()  [AV @ 0xFEEEFEEE]
```

修正后的析构顺序（三段，缺一段就复现）：

1. 先 `reset()` 四个系统 `shared_ptr`——此时 `ECSContext` 仍存活，系统即便在此析构也是安全的；
   放掉后系统仍由 `ECSContext` 自身的 `shared_ptr` 持有，最终在 `Shutdown()` 内销毁；
2. 再 `world_->Shutdown()` + `world_.reset()`；
3. 最后 `rt_.reset()`。

一条可推广的规则：**任何在析构里持有 ECS 系统 `shared_ptr` 的模块对象，都必须先于
`ECSContext` 的销毁放掉它们**，否则就等于让 `ECSContext` 的生命周期被系统延迟析构"越过"。

### 四、为什么 `RenderBoundBox` / `LoadGeometry` 是例外

这两个示例把 `Geometry` 直接 `new` 出来自己持有、**没有**进 `GeometryManager`（不参与
"按名字取网格"的流程），因此它们的 `delete` 是合法的、也是必须的。判断某个示例是否属于
例外，看它是否走 `GeometryManager::Add` / `LoadGeometry`——走了就必须交还管理器，没走就
自己管。这条边界必须在改动示例时逐一看，不能一律套用一种写法。

### 五、本层修复清单（14 个示例文件 + 1 个引擎文件，17 个示例崩溃）

`Geometry` 双重释放——`MeshEntry` 形态（4 个）：
`Environment/BasicLitSunDirection`、`Environment/IBLEnvironment`、
`Geometry/MixedBufferMeshes`、`Material/FullPrecisionLitMeshes`。

`Geometry` 双重释放——App 析构 `SAFE_CLEAR` 形态（9 个）：
`Cull/GPUDrivenCull`、`Debug/SimplestAxis`、`GettingStarted/SimpleCube`、
`Interaction/RayPicking`、`Material/AlphaModes`、`Material/CoverageVariants`、
`Material/MaterialErrorFallback`、`Material/MaterialRecipeEntry`、`Transform/RecursiveCube`。

引擎侧（1 个）：`src/SceneGraph/module/OffscreenWorld.cpp`。

`Texture` / `Sampler` 双重释放（1 个）：`Texture/TextureMipLevels`
（三张 mip 纹理 + `sampler` + `ecs_world` 全改 null 化；另 `bc7_probe` 改为 `Release`）。

**关键结论：17 个崩溃全部是既存缺陷，与 Layer 1-4 的改动无关**——它们只是此前从未被"全量实跑"
照到（Layer 1-4 的验证只到门禁 / ctest 一级，示例因缺 `GLSLCompiler.dll` 而根本没跑起来）。
这也说明 Layer 5 这类"实跑验收"在门禁之外仍有独立价值：门禁用例覆盖的是文本与选项，覆盖不到
进程退出路径。
