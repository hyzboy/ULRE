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

协议内容只有两类，**没有任何算法逻辑、没有任何 `const` 变量、没有任何非 `#define` 之外的声明**：

```glsl
// options.glsl —— StandardOpaque / ForwardColor 的一个具体实例
#define HGL_MATERIAL_HAS_VERTEX_NORMAL 1
#define HGL_USE_MATERIAL_SOURCE_PROVIDER 1
#define HGL_USE_SCENE_LIGHTING 1
#define MTL_TEX_NORMAL_CHANNELS 2
#define HGL_SHADOW_PCF_POISSON_TAPS 16
#define HGL_SHADOW_NORMAL_OFFSET 1
#define HGL_ALPHA_TEST 1
#define HGL_ALPHA_CUTOFF 0.500000
#include "lighting/indirect_sky_ambient.glsl"     // ← 环境光 provider（多态 include）
```

硬约束（实测得出，不可再犯）：

1. **多态 include 必须是字面路径写死在 `options.glsl` 文本里**。
   `#define AMB "lighting/indirect_sky_ambient.glsl"` 配合 `#include AMB` **编译失败**：
   `ERROR: '#include' : must be followed by a header name`。GLSL 预处理器不支持宏展开成头文件名。
2. 它只允许出现 `#define` 与 `#include`。任何需要计算的逻辑都不许进来，否则它又会退化成
   一个 C++ 端的代码生成器。
3. 它只承载**按 program 变化**的量（覆盖判定开关、阴影采样参数、法线贴图通道数、环境光
   provider）。材质业务参数（颜色、粗糙度乘数、贴图索引）一律走运行时 BDA，**绝不进这里**。
4. `HGL_ALPHA_CUTOFF` 用 `#define`（`alpha_compositor.glsl` 在家守卫内直接比 `alpha < HGL_ALPHA_CUTOFF`）。

### 1.3 绝大部分分支走“多态 include”与“空对象（Null Object）”，彻底消灭 `#if`
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

1. **第一阶段：固化各 MaterialDomain 的主着色器** ✅（StandardOpaque 4 档 + Sky 已完成）
   - 已固化：`StandardOpaque` 的 ForwardColor / ShadowDepth(有覆盖) / ShadowDepth(无覆盖)，`Sky` 的 ForwardColor。
   - 已定义 `options.glsl` 注入协议与 `<domain>_bindings.glsl` 分层。
   - 待补：`UnlitSolid` / `VertexColor` / `VertexPalette` / `VertexLuminance` /
     `UnlitTexture` / `UI2D` / `Text2D` / `Checker2D` / `Checker3D` 等（这些走
     `flat_lighting` 形状，留待阶段四按域迁移时逐个固化）。
2. **第二阶段：实现极简 `DomainOptionsEmitter`（< 200 行）**
   - 输入 `MaterialDomain` + Pass 类型，输出上述 `options.glsl` 文本。
   - 不做任何依赖推导、不做任何模块排序——模块顺序已经物理写在主着色器里。
3. **第三阶段：直通编译后端 `DomainShaderBuildBackend`**
   - 直接调 `GLSLCompiler.dll` 编译固定主着色器，用 SPIR-V 反射提取 binding/varying 信息。
   - 把 1.6 的等价性判据固化为回归用例。
4. **第四阶段：业务域全面接入新管道并打通全 Pass**
   - 逐域迁移，跑 `SimpleSphere` / `PBRSpheres` / `LineRender` / `DrawText` 实机验证。
5. **第五阶段：整体切替与旧代码物理删除**
   - 将渲染系统底层对接至直通管道。
   - 一次性物理删除 `src/ShaderGen` 下冗余的生成器、注记解析与 AST 模块。
