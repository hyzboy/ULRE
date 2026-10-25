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
| `VertexInputMode` / Position 格式 | ABI 当前只闭环支持 `R32G32B32_SFLOAT → Vec3`、`R32G32_SFLOAT → Vec2`、`R16G16_SINT → Vec2Int` 三种位置解码模块。`CharQuad` 自带 SSBO，不读外部位置。 | Stage 1 输入模块与 ABI 绑定，文件选择必须以已解析的几何格式为准，而非只看材质配置。 |
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
5. 当前 `ShaderLibrary/mesh/line_quad.glsl.tmpl` 仍用 `#ifdef S1_TRANSFORM_ID_GLSL` /
   `S1_SIZE_GLSL` 改变执行语句，`vertex/s1_position_*.glsl` 仍按 `HGL_*_LOADER`
   宏条件装载属性，`orient_world.glsl` 仍按 `HGL_L2W_FROM_VERTEX_ATTR` 条件编译。
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
`#ifdef HGL_*_LOADER` 钩子按需展开属性装载。固定源把这 8 个分支**静态展开成显式语句**：

```glsl
void LoadVertexData()
{
    Position = sbo_vertex_position.data[draw_params.vertex_base + VertexIndexID];
    TexCoord = sbo_vertex_uv.data[draw_params.vertex_base + VertexIndexID];
    Normal   = sbo_vertex_ntb.data[draw_params.vertex_base + VertexIndexID];
}
```

固定源码及其 include 闭包不再含结构性 `#if`；`s1_uv` / `s1_ntb` 等属性模块只保留"声明
全局变量 + 定义 `sbo_vertex_*` 基址宏"，其 `HGL_*_LOADER` 钩子在固定形状里无人消费
（生成器删除后一并清理，见 6.5）。

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