# ULRE 超薄极简着色器架构设计规范

> 本文是**设计正文**（为什么这样设计）。实施进度、提交链、验证状态、下一步动作与
> 环境坑请看配套的 **`doc/ultra-thin-implementation-status.md`**（交接文档）。

## 1. 架构愿景与核心理念

本设计旨在彻底终结传统引擎中复杂的 ShaderGen 代码拼接、AST 构造、宏开关地狱（Macro Hell）与中间层材质定义（`material.toml` 套娃），构建一套**物理级固定代码、零运行时分支、基于多态包含契约的超薄着色器架构**。

### 核心思想
1. **Domain ＝ Master Material（母材质）**：
   - 引擎内置固定十几种领域表面（如 `StandardOpaque`、`UnlitSolid`、`VertexColor`、`Sky` 等）。
   - **完全由引擎写死，上层开发者不可创建、不可修改**。
   - 每个 Domain 对应一个静态、固定的 `.frag.glsl` / `.vert.glsl` 文件，代码清晰直观、所见即所得。
2. **上层资产 ＝ MaterialInstance（材质实例）**：
   - 开发者仅能创建材质实例，只需声明：**使用哪个 Domain + 填什么数值参数 + 绑定什么贴图**。
   - 资产层绝不涉及任何着色器代码、光照算法、宏开关或模块插槽。
3. **多态接口化（Interface by Include）替代 `#if` 宏分支**：
   - 天光、阴影、漫反射、高光、光照合成等所有光照环节均定义为**统一签名的无分支契约函数**。
   - “关闭/无效果”通过**空对象实现（Null Object Implementation）**提供（例如无阴影直接 `return 1.0;`，无天光直接 `return vec3(0.0);`）。
   - 主 Shader 直线执行，内部绝无 `#if / #else`。GPU 编译器通过内联死代码消除（DCE）实现零运行时损耗。
4. **C++ 层极简职责（门卫 + 字符串发射器）**：
   - C++ 不拼接 Shader 代码，不做代码生成器。
   - 运行时仅负责：**资产合法性校验**（网格语义与贴图匹配） + **动态发射 5 行 `#include` 的配置头文件（`pipeline_config.h`）**。

---

## 2. 系统拓扑与数据流图

```mermaid
graph TD
    subgraph 离线/静态资产 (Static Assets)
        D1["Domain 着色器主干 (例: standard_opaque.frag.glsl)<br/>纯固定 GLSL，内无 #if 宏分支"]
        Lib["算法函数库 (统一函数签名)<br/>- shadow_none.glsl / shadow_csm.glsl<br/>- ambient_none.glsl / ambient_ibl.glsl<br/>- diffuse_lambert.glsl / diffuse_toon.glsl<br/>- composite_pbr.glsl / composite_toon.glsl"]
        MI["用户材质实例 (MaterialInstance.toml)<br/>domain = 'StandardOpaque'<br/>properties: roughness = 0.5<br/>textures: base_color = 'wood.png'"]
    end

    subgraph C++ 运行时 (Ultra-Thin Runtime)
        Validator["资产契约校验器 (Validator)<br/>只检查: 几何顶点格式与贴图槽是否齐备"]
        Emitter["管线配置发射器 (PipelineConfigEmitter)<br/>根据 Pass/环境状态输出纯文本 pipeline_config.h"]
    end

    subgraph 编译与执行 (GPU Pipeline)
        Includer["pipeline_config.h (纯包含头文件)<br/>#include 'shadow_csm.glsl'<br/>#include 'ambient_ibl.glsl'<br/>#include 'diffuse_burley.glsl'<br/>#include 'composite_pbr.glsl'"]
        Compiler["glslang / ShaderCompiler<br/>编译 Domain GLSL + pipeline_config.h<br/>- 自动内联函数<br/>- 死代码消除 (DCE)<br/>- 输出最优 SPIR-V"]
        Pipeline["VkPipeline (固定管线状态)"]
    end

    MI --> Validator
    Validator -->|校验通过| Emitter
    Emitter --> Includer
    Includer --> Compiler
    D1 --> Compiler
    Lib --> Compiler
    Compiler --> Pipeline
```

---

## 3. 算法插槽契约规范（统一接口签名）

所有光照相关的子算法统一采用以下 5 大插槽，每个插槽包含固定签名。**任何新增算法或“关闭”状态都必须实现该签名**：

### 3.1 阴影来源插槽 (Shadow Slot)
* **函数契约**：
  ```glsl
  float EvalShadow(in SurfaceData surface, in ShadowContext ctx);
  ```
* **实现规范**：
  - `shadow_none.glsl`：无阴影状态，直接 `return 1.0;`（全亮，编译期直接折叠乘法）。
  - `shadow_full.glsl`：强制全阴影，直接 `return 0.0;`。
  - `shadow_csm_pcf.glsl`：级联阴影 PCF 滤波计算。
  - `shadow_raytraced.glsl`：光线追踪软阴影。

### 3.2 天光/间接光插槽 (Ambient / GI Slot)
* **函数契约**：
  ```glsl
  vec3 EvalAmbient(in SurfaceData surface, in AmbientContext ctx);
  ```
* **实现规范**：
  - `ambient_none.glsl`：无间接光状态，直接 `return vec3(0.0);`。
  - `ambient_constant.glsl`：常数底色环境光。
  - `ambient_ibl.glsl`：基于拆分和近似（Split-Sum）的辐射度与环境反射探针。
  - `ambient_ddgi.glsl`：动态漫反射全局光照探针。

### 3.3 直接光漫反射插槽 (Diffuse BRDF Slot)
* **函数契约**：
  ```glsl
  vec3 EvalDiffuse(in DirectLight light, in SurfaceData surface);
  ```
* **落地校正（2026-10，阶段一）**：实际签名沿用既有枢纽结构 `LightingInput`
  （`ShaderLibrary/common/lighting_interface.glsl`），即
  `vec3 EvalDiffuseLight(LightingInput lighting)`；槽名落地为
  `diffuse_light_provider`，能力位 `direct_diffuse`。理由：`LightingInput`
  已含 `normal/viewDir/mainLightDir/baseColor/metallic/roughness/fresnel`
  全部输入，再造 `DirectLight`+`SurfaceData` 两个结构会与现有 surface 契约重复。
  两者都只返回**未调制**辐射项，光源色 × 阴影系数由合成槽乘一次。
* **实现规范**：
  - `diffuse_none.glsl`：无漫反射，直接 `return vec3(0.0);`。
  - `diffuse_lambert.glsl`：标准兰伯特漫反射（`NdotL / PI`）。
  - `diffuse_burley.glsl`：迪士尼粗糙度耦合漫反射。
  - `diffuse_toon.glsl`：NPR 卡通色阶分段阶梯漫反射。

### 3.4 高光反射插槽 (Specular BRDF Slot)
* **函数契约**：
  ```glsl
  vec3 EvalSpecular(in DirectLight light, in SurfaceData surface);
  ```
* **落地校正（2026-10，阶段一）**：同上，实际签名为
  `vec3 EvalSpecularLight(LightingInput lighting)`，槽名
  `specular_light_provider`，能力位 `direct_specular`。
* **实现规范**：
  - `specular_none.glsl`：无高光，直接 `return vec3(0.0);`。
  - `specular_cook_torrance.glsl`：微表面 GGX + 史密斯几何遮挡 + 菲涅尔高光。
  - `specular_blinn_phong.glsl`：简化 Blinn-Phong 高光。
  - `specular_toon_step.glsl`：硬边高光色斑。

### 3.5 光照合成插槽 (Compositor Slot)
* **函数契约**：
  ```glsl
  vec3 CompositeLighting(in LightingAccumulation accum, in SurfaceData surface);
  ```
* **实现规范**：
  - `composite_standard_pbr.glsl`：`accum.directDiffuse + accum.directSpecular + accum.ambient`。
  - `composite_fresnel_blend.glsl`：加入边缘菲涅尔加权与视线遮挡过渡。
  - `composite_toon_cell.glsl`：卡通分级合成与描边加权。

---

## 4. 着色器代码范例

### 4.1 动态生成的配置头（`pipeline_config.h`）
C++ 端在当前 Pass 构建或调度时，根据 Pass 配置在内存中生成的纯文本头文件：

```c
// pipeline_config.h (纯动态生成，内存注入)
#pragma once

// 1. 阴影算法选择
#include "lighting/shadow/shadow_csm_pcf.glsl"

// 2. 天光/间接光选择
#include "lighting/ambient/ambient_ibl.glsl"

// 3. 漫反射 BRDF
#include "lighting/diffuse/diffuse_burley.glsl"

// 4. 高光 BRDF
#include "lighting/specular/specular_cook_torrance.glsl"

// 5. 光照合成器
#include "lighting/composite/composite_standard_pbr.glsl"
```

若当前 Pass 为低配或无阴影模式，C++ 仅需替换第一行：
```c
#include "lighting/shadow/shadow_none.glsl"
```

> **落地校正（2026-10，阶段二）**
> 上面是概念示意，实际路径不带 `lighting/` 分组子目录以外的层级。这个头由
> `hgl/mtl/PipelineConfigEmitter.h` 的 `EmitPipelineConfig()` + `EmitPipelineConfigText()`
> 生成：前者是**唯一判定点**（必需槽缺失=硬失败 / 可选槽缺失=跳过 / 未声明槽=忽略 /
> 路径超长=失败），后者是**唯一排版出口**（一行一个 `#include "…"`，行尾 `\n`）。
> 顺序恒等于模板 `slots[]` 顺序，`SurfaceProvider` 因调用关系恒排末位。
>
> 真实发射结果（`MakeIdentityForwardLitProfile()`，实测 11 行）：
> ```c
> #include "sky/sky_atmosphere.glsl"
> #include "lighting/diffuse_lambert.glsl"
> #include "lighting/specular_cook_torrance.glsl"
> #include "lighting/indirect_sky_ambient.glsl"
> #include "shadow/pcf_shadow.glsl"
> #include "ao/identity.glsl"
> #include "lighting/forward_pbr.glsl"
> #include "material/pbr_surface_source.glsl"     // 来自材质定义
> #include "ntb/ntb_tangent_vbo_normalmap.glsl"    // 来自材质定义
> #include "compositor/forward_lighting.glsl"
> #include "surface/material_surface.glsl"        // 恒末位
> ```
> 换算法 = 换掉其中一行，其余 10 行逐字节不变（门禁 `PC.pipeline-config-emit`
> 钉住了「只换一行则只变一行、摘要必变、缓存键不串味」）。
>
> **阶段二之二（2026-10，消灭最后三处条件编译）**：`#include` 面收敛完成后，主着色器
> 里剩下的条件编译只有三处，全部消除，办法统一是**「槽位升必需 + 空对象补位」**：
>
> | 模板 | 新增必需槽 | 空对象 |
> |---|---|---|
> | `ForwardLitUnshadowedSlots`（10 槽） | `AmbientOcclusionProvider` | `ao/identity.glsl` |
> | `ForwardUnlitSlots`（4 槽） | `NTBProvider` | `ntb/ntb_none.glsl`（新增） |
> | `ShadowCasterOpaqueSlots` / `ShadowCasterMaskedSlots`（各 4 槽） | `NTBProvider` | 同上 |
>
> 结果：`ShaderLibrary/surface/material_surface.glsl` 与
> `ShaderLibrary/compositor/forward_lighting.glsl` 里**零 `#if`**；宏区从 3 个宏降到 2 个
> （`HGL_USE_MATERIAL_SOURCE_PROVIDER` + `HGL_USE_SCENE_LIGHTING`）。
> `ntb_none` 返回 `BuildOrthoNTB(normalize(surface.worldNormal))`，其 `N` 与旧路径
> `normalize(si.worldNormal)` 完全一致 ⇒ `ForwardUnlit` 输出逐字节不变。
> shadow caster 走的是手写合成（不进 `.tmpl`），NTB include 显式排在
> `material_surface` 之前，否则 `GetNTB` 未声明。

### 4.2 物理级固定代码主着色器（`standard_opaque.frag.glsl`）
代码逻辑直线向下，无任何 `#if` 分支，具备最强可读性与可维护性：

```glsl
#version 460
#extension GL_GOOGLE_include_directive : require

// 1. 包含管线动态注入的接缝头
#include "pipeline_config.h"

// 2. 包含公共输入与表面解析契约
#include "common/surface_common.glsl"
#include "surface/standard_opaque_eval.glsl"

// 3. 输出目标
layout(location = 0) out vec4 out_FragColor;

void main()
{
    // 解析材质表面属性 (albedo, roughness, metallic, normal 等)
    SurfaceData surface = EvaluateStandardOpaqueSurface();

    // 1. 间接光 / 天光 (若配置为 ambient_none 则直接返回 vec3(0))
    vec3 ambientColor = EvalAmbient(surface, g_AmbientContext);

    // 2. 阴影遮挡计算 (若配置为 shadow_none 则直接返回 1.0)
    float shadowFactor = EvalShadow(surface, g_ShadowContext);

    // 3. 直接光照计算 (漫反射 + 高光)
    vec3 directDiffuse  = EvalDiffuse(g_MainLight, surface) * shadowFactor;
    vec3 directSpecular = EvalSpecular(g_MainLight, surface) * shadowFactor;

    // 4. 光照汇总并进入合成器
    LightingAccumulation accum;
    accum.directDiffuse  = directDiffuse;
    accum.directSpecular = directSpecular;
    accum.ambient        = ambientColor;

    vec3 finalRGB = CompositeLighting(accum, surface);

    out_FragColor = vec4(finalRGB, surface.alpha);
}
```

---

## 5. 材质实例（MaterialInstance）资产规范

开发者在项目中创建的材质资产统一为**材质实例（`.mi.toml`）**格式。一份实例只回答三件事：**选哪个 Domain**、**发哪些顶点插值**、**声明哪些资源槽**。`name` / `provider_policy` / `[fragment]` 模块名 / `[vertex] requirements` / `[transform]` / 槽形态（`sampler` / `required` / `channels`）/ `material_data.type` **全部由领域契约给出**，资产里写不出来。

> **`.material.toml` 已废除**（阶段四）：解析器只识别 `.mi.toml` 后缀，门禁 `S.material-library-contract` 对任何非该后缀的资产文件直接报错。

### 5.1 业务 PBR 资产（例：`ShaderLibrary/material/lit.mi.toml`，略去行内注释）
```toml
schema = 4
id = "Lit"
domain = "StandardOpaque"

[vertex]
varyings = ["emit_data_index_id", "emit_world_pos", "emit_world_normal", "emit_uv0"]

[resources]
textures = [
    { name = "base_color" }, { name = "roughness" }, { name = "metallic" },
    { name = "occlusion" }, { name = "opacity_mask" }, { name = "normal" }
]
samplers = ["Trilinear", "Linear", "ShadowPCF"]
```

**资产没写、由 `StandardOpaque` 契约给出的部分**：`name`（缺省 = `id`）、
`provider_policy`（= `GeometryOnly`）、`[fragment].material_source_module_name`
（= `pbr_surface_source`）、`[fragment].ntb_module_name`、
`[vertex].requirements`（= Position + Normal + UV0）、`[transform]`（3D 规范变换）、
每个纹理槽的 `sampler` / `required` / `channels`（`normal` 固定 `channels = 2`）、
`[resources].material_data.type`（= `PBRSurface`）。

**`[vertex].requirements` 是顶点 ABI，不是插值需求**：它由领域契约的
`required_vertex_semantic_mask` 推导，且**不与 `varyings` 取交集** —— 同一领域
必须同顶点布局才能共享 layout / 合批。不发 `emit_world_normal` 的材质照样要提供
`Normal` 顶点属性。`varyings` 只决定着色器发什么插值。

### 5.2 业务无光纯色资产（例：`builtin/pure_color`，已代码化）
```toml
schema = 4
id = "builtin/pure_color"
domain = "UnlitSolid"

[vertex]
varyings = ["emit_data_index_id"]

[resources]
```
`UnlitSolid` 契约给出顶点需求（Position）、表面来源模块与 `EmissiveSurface`
数据类型，所以 `[vertex].requirements` / `[fragment]` / `provider_policy` 全部省略；
`[resources]` 留空是因为该领域没有纹理槽。**领域没有任何槽位时 `[resources]`
可整块省略**，写空表只是把「本材质确实不用贴图」说清楚。

---

## 6. 内置默认与回退机制（Built-in Fallbacks）

### 6.1 内建保底材质的本质定位
1. **`builtin/pure_color`**：
   - 不是独立材质系统，而是基于 `MaterialDomain::UnlitSolid` 的**C++ 内置保底默认实例（Default Fallback Instance）**。
   - 供无材质指定的图元、简单 Gizmo 调试直接引用。
2. **`builtin/checkerboard_3d` / `builtin/error_checkerboard_3d`**：
   - 前者基于 `MaterialDomain::Checker3D`（无报错标记的中性棋盘），后者基于
     `MaterialDomain::ErrorChecker3D` 的**C++ 内置报错实例（Error Visualizer Instance）**。
   - 当检测到网格缺少必需顶点属性（如缺 Normal、缺 UV）或缺少必要贴图时，自动路由至该实例，并传递特定标记色（例如洋红表示无材质、黄色表示缺法线、蓝色表示缺 UV）。
   - **完全脱离磁盘文件依赖，全部在 C++ 内部常驻构造**。
3. **`builtin/text_gpu` / `builtin/text_gpu_bitmap`**：`Text2D` 领域的 SDF / 位图两条
   字形路径，由 `TextRenderPipeline` 按字体数据源二选一。

> 现状（阶段三之三 + 阶段四）：上述 5 份定义已**全部代码化**，内嵌在
> `src/ShaderMaterialData/BuiltinMaterialDefinitions.cpp`，不再有对应的
> `.mi.toml`，也不再依赖磁盘。内嵌的仍是**完整 schema-4 源码文本**，走与资产
> **完全同一条**解析 + 领域校验管线——代码化消灭的是「对磁盘文件的依赖」，不是
> 「对校验管线的依赖」。
> `builtin/checkerboard_2d`（`Checker2D` 领域）**不在**保底链上（只是调试用降级
> 材质），因此仍以 `ShaderLibrary/material/checkerboard_2d.mi.toml` 资产文件形式存在。

---

## 7. C++ 运行时代际变更与重构规划

### 7.1 C++ 裁减清单（废除项）
- ❌ **废除 ShaderGen 代码拼接引擎**：删除复杂的模块依赖拓扑排序、代码字符串替换引擎。
- ❌ **废除磁盘 `*.mi.toml` 母材质定义文件**：不再通过 TOML 配置模块引用和 NTB 策略。
- ❌ **废除 `MaterialDefinitionFileRegistry`**：将其简化为 `MaterialInstanceRegistry`。

### 7.2 C++ 保留与增强清单（保留项）
- ✅ **`MaterialDomain` 枚举与 `DomainContracts`**：作为引擎母材质唯一权威真源。
- ✅ **资产门卫校验**：在加载 `MaterialInstance` 时严格校验网格属性与贴图槽位。
- ✅ **超薄 `PipelineConfigEmitter`**：根据 Pass 环境拼装 5~10 行 include 注入头。
- ✅ **SPIR-V 编译缓存**：缓存 Key 精简为 `(DomainID, PipelineConfigHash)`。

---

## 8. 实施路线图（六阶段）

| 阶段 | 分支 | 核心任务 | 交付产物 |
| :--- | :--- | :--- | :--- |
| **阶段 1：契约头与算法函数库固化** ✅ | `feature/ultra-thin-stage1-lighting-slots` | 制定 5 大光照插槽的 GLSL 统一签名；实现对应的固定算法文件及“无/空”对象实现（`*_none.glsl`） | `ShaderLibrary/lighting/` 基础算法库 |
| **阶段 2：Domain 主着色器固定化** ✅ | `feature/ultra-thin-stage2-domain-shaders` | 主着色器固定为「固定骨架 + 纯 `#include` 拼接」，槽模块内零 `#if`；C++ 侧只生成配置头 | 固定主着色器 + `PipelineConfigEmitter` |
| **阶段 3：MaterialInstance 模型** ✅ | `feature/ultra-thin-stage3-material-instance` | Domain 即 Master Material；资产降维为「选 Domain + 填数值 + 配贴图」的 MaterialInstance；保底材质移入 C++ | 极简资产形态上线 |
| **阶段 4：资产迁移** ✅ | `feature/ultra-thin-stage4-cleanup-and-migration` | 现有资产迁移到 `.mi.toml`；废弃 `material.toml` | 全套自动化门禁验证通过 |
| **阶段 5：拼接清理前置安全网** ✅ | `feature/ultra-thin-stage5-shadergen-emission-cleanup` | 为旧版 ShaderGen 拼接代码的清理造好 golden 安全网（emission 顺序快照），并清掉小债 | `SD.emission-order-golden`（12 变体）+ 小债清零 |
| **阶段 6：骨架外置** ✅ | 同上 | 把 `FragmentTemplateComposer` 里最后一段内联 GLSL 骨架 `ComposeShadow()` 外化为 `fragment/shadow_caster.glsl.tmpl`；补 masked 分支快照覆盖 | 四条 emission 通路全部走 `.tmpl`；矩阵 16 变体（含 3 种 mesh 发射模式） |

阶段 6 的进展：

1. ✅ **先补覆盖再动刀**：9 份 `.mi.toml` 与 5 份 builtin 定义都没有设 `alpha_test`，
   所以 `ComposeShadow()` 的 masked 分支（`requires_alpha == true`）快照覆盖为 **0**。
   先给 `StructureDumpVariant` 加 `alpha_test`、矩阵 12 → 14（`lit-depth-masked` /
   `lit-shadow-masked`），并确认新 golden 里确实出现了 alpha 路径，然后才删代码。
   `alpha_test` 必须写在 `ResolveSceneRenderTemplateRequest()` **之前**
   （`MaterialCoverageContract.cpp:112` 在那一刻读渲染状态）。
2. ✅ 新增 `ShaderConfigProfile::ShadowCaster`，让 `EmitShaderConfigDefines` 成为
   `#define HGL_COVERAGE_ONLY 1` 的单一真源；该 profile 下跳过 provider 旗标、
   纹理通道宏与 PCF 宏。门禁 `D2.shader-config-emit` 增子用例 ⑥ 钉死这份规范文本
   （首跑就抓到宏泄漏）。
3. ✅ 新增 `ShaderLibrary/fragment/shadow_caster.glsl.tmpl`（7 个占位符），
   `GetRenderTemplateFragmentSourceFile()` 补齐 `ShadowCasterOpaque` /
   `ShadowCasterMasked`，`ComposeShadow()` 及 4 个随之失效的 helper 全部删除
   （852 → 646 行）。
4. ✅ **唯一的行为变化**：`HGL_COVERAGE_ONLY` / `HGL_ALPHA_*` 从「材质注入块之前」
   移到「之后」，与 forward 模板一致；宏集合与文本逐字节相同，4 份 shadow emission
   golden 位移而字节数不变，`SHADERGEN_CODEGEN_GENERATION` 13 → 14 并记入 changelog。
   保留旧顺序需要在材质注入路径上加 `pending_prefix` 钩子，收益不抵代价。
5. ✅ **再补一次覆盖**：矩阵只从 fragment 维度枚举过，mesh 侧 `LineQuad` 发射模式
   （只能由 `primitive_type == Lines` 推出）**零覆盖**。收尾补 `vertex-color-lines-quad` /
   `vertex-color-lines-quad-2d`，矩阵 14 → 16，三种 mesh 发射模式全部有网。
   详见 `doc/ultra-thin-implementation-status.md` §5.3。
6. ⬜ **mesh 侧骨架外置与 STL 残留清理仍未做** —— 安全网已就位、路径已解锁，
   但两者都是独立、可评审的改动。STL 清理经实测**不是纯机械替换**：最小目标
   `MeshShaderTemplate.cpp` 的第一个依赖 `GetShaderLibraryPath()` 是返回
   `std::string` 的 inline 公共头（26 个调用点），且 `std::vector<PendingBlock>` /
   `std::sort` 在 HGL 里没有等价物。局部改只会让仓里一半 HGL 一半 STL。
   详见 `doc/ultra-thin-implementation-status.md` §10.2。

阶段 5 的进展：

1. ✅ 新增 `DumpShaderEmissionOrder()`（`inc/hgl/mtl/ShaderStructureDump.h` +
   `src/ShaderGen/compile/ShaderStructureDump.cpp`）与门禁 case
   `SD.emission-order-golden`，12 份 `<slug>.emission.txt` golden 覆盖
   `ForwardLit` / `ForwardUnlit` / `ShadowCaster` / `Sky` 四条 emission 通路
   （阶段 6 补 masked 分支后扩为 14 份，见下）。
   粒度刻意只到**骨架行**（预处理指令 / 顶层 `layout` / 顶层函数签名），使
   「token 顺序不变」的重构无需刷新 golden。
2. ✅ 修掉一个会让 golden 自身不可信的坑：`SamplerPresetLibrary` 是进程级单例，
   只跑 `pipeline` 组时预设未装载，采样器 `#define` 被**静默省略**；两个快照
   case 现均先调用 `EnsureSamplerPresetsLoaded()`。
3. ✅ 小债清理：删除 legacy `ShaderModuleSlotRole::DirectLightProvider`；模块总数
   `82` 的两处硬编码收敛为 `kShaderCodeModuleCount` 单一常量。
4. ⬜ **旧 ShaderGen 字符串拼接代码的实际迁移** —— fragment 侧已于阶段 6 收口
   （见下方「阶段 6 的进展」），剩余 mesh 侧骨架与多段 `+=` 合并仍待处理，详见
   `doc/ultra-thin-implementation-status.md` §10.2。

阶段 4 的进展：

1. ✅ 9 份 `ShaderLibrary/material/*.material.toml` 全部 `git mv` 为 `.mi.toml`
   并重写为降维形态（只剩 `domain` + `[vertex] varyings` + `[resources]`）；
   `LoadDirectory()` 的扩展名过滤改为 `.mi.toml`，门禁
   `S.material-library-contract` 从「静默跳过非 `.material.toml`」改为**正向断言**
   ——任何非 `.mi.toml` 的资产文件立刻报错。
2. ✅ `name` / `provider_policy` / `[fragment]` / `[vertex] requirements` 四项对
   **领域材质**改为可缺省，走 `GetMaterialDomainVertexSemanticRequirements()` 等
   契约兜底；非领域材质（Custom）缺任何一项都带**键级诊断**报错（历史行为是静默
   `return false`）。
3. ✅ 5 份内嵌 builtin 源码同步降维。
4. ✅ 门禁新增 4 条探针：正向推导（`DerivedVertexColor`，断言四项全部由契约填对）、
   ABI 不变式（`DerivedKeepAbi`）、以及 Custom 缺 `provider_policy` /
   `requirements` 的两条反向探针。
5. ⬜ **旧 ShaderGen 字符串拼接代码清理**：它与资产迁移无耦合，已改由阶段 5 承接 ——
   阶段 5 先造好 emission 顺序 golden 安全网（见上），实际迁移仍未做。
   详见 `doc/ultra-thin-implementation-status.md` §10.2。

阶段 2 的进展（按交付顺序）：

1. ✅ `PipelineConfigEmitter`（`inc/hgl/mtl/PipelineConfigEmitter.h` +
   `src/ShaderGen/template/PipelineConfigEmitter.cpp`）：`#include` 这一半的
   单一真源与唯一排版出口；`FragmentTemplateComposer` 已改为委托它，不再自己
   遍历 `template.slots`。门禁 `PC.pipeline-config-emit` 覆盖顺序、必需/可选
   缺失、一行换算法、摘要敏感性。
2. ✅ 消灭主着色器里最后三处条件编译（`material_surface.glsl` 的两处
   `#if HGL_USE_NTB_PROVIDER`、`forward_lighting.glsl` 的
   `#if defined(HGL_USE_AMBIENT_OCCLUSION_PROVIDER`）。做法是**把槽位改成必需 +
   补空对象**，而不是加宏：`ForwardUnlitSlots`（4 槽）与两个 shadow caster
   槽表（各 4 槽）补 `NTBProvider`，`ForwardLitUnshadowedSlots`（10 槽）补
   `AmbientOcclusionProvider`；空对象是新增的 `ShaderLibrary/ntb/ntb_none.glsl`
   （复用已有 `ao/identity.glsl`）。`HGL_USE_NTB_PROVIDER` /
   `HGL_USE_AMBIENT_OCCLUSION_PROVIDER` 两个宏连同 `ShaderConfig` 里的两个字段
   一并整删，宏区只剩 `HGL_USE_MATERIAL_SOURCE_PROVIDER` +
   `HGL_USE_SCENE_LIGHTING`。
   > **为什么必须是「必需槽 + 空对象」而不是「可选槽 + 宏」**：GLSL 在**函数定义处**
   > 解析符号，`EvalSurface` 无条件调用 `GetNTB`、`forward_lighting` 无条件调用
   > `GetAmbientOcclusion`，因此凡 include 了这两个文件的模板都必须有实现；
   > 用宏只守卫调用点，缺陷仍然暴露在定义处。
3. ✅ 收敛 `BuildFragmentDefines` 里为兼容旧槽位而保留的过渡判定——NTB/AO
   两个字段删除后，该函数只剩 `material_source` / `scene_lighting` 两个布尔，
   并进一步委托 `ShaderConfig` 的 `EmitShaderConfigDefines` 发射。
   `TemplateHasSlot` 辅助函数随之成为死代码，已删。
4. ✅ 「固定主着色器」按**渲染模板**而非按 Domain 落文件：`ShaderLibrary/fragment/`
   下只有 `forward_lit.glsl.tmpl` / `forward_unlit.glsl.tmpl` / `sky.glsl.tmpl`
   三个骨架，内容 = `{{defines}}` + 固定 include + `{{module_includes}}` +
   main 体。之所以不按 Domain 各拆一个物理 `.frag.glsl`：Domain 之间的差异
   **全部**表达在模块集合与配置头里，拆成多份物理文件只会复制同一段 main 体，
   正是本架构要消灭的重复。主文件里刻意保留的宏只有 alpha 覆盖相关的三处
   （`HGL_ALPHA_TEST` / `HGL_ALPHA_DITHER` / `HGL_ALPHA_OUTPUT`）——它们必须与
   `discard` / 输出写在同一个函数体内，属于设计明确允许的例外。

阶段 3 的进展（按交付顺序）：

1. ✅ **顶点变换推入领域契约**。`MaterialDomainContract` 新增第 10 个字段
   `canonical_vertex_node_config`（`VertexShaderNodeConfig`，聚合缺省即默认 3D
   配置），`MaterialDomain.cpp` 只为真需要非默认变换的三个领域显式填写：
   `UnlitTexture` 与 `Checker2D` 用 2D NDC、`Text2D` 用
   `None + PixelToLocal + OrthoViewport`；其余 13 个领域靠聚合默认。
   校验器新增 `TransformMismatch`，在 NTB 段之后追加「3c. 顶点变换」一档：
   资产自选的 `[transform]` 与领域规范不符即报错在 `key == "domain"` 上。
   解析器 `ParseDefinition` 在资产缺 `[transform]` 时**从契约兜底**
   （`GetMaterialDomainCanonicalVertexNodeConfig()`），因此 14 个
   `*.mi.toml` 的 `[transform]` 块被**整块删除**（98 行）。
   配套新增 `IsSameVertexNodeConfig()`；门禁 `D.material-domain-contract`
   加 1 条负向探针（3D 领域自选 2D 变换必须被拒）+ 2 处正向断言
   （默认 3D 领域解析结果必须等于默认配置；`Text2D` 最小材质在无
   `[transform]` 时解析成功且投影为 `OrthoViewport`）。
   > **为什么变换属于 Domain 而不是资产**：5 个字段（source / mapping /
   > orientation / scale / projection）在现有 14 个资产里只有 4 种组合，且组合
   > **完全由领域决定**——2D 精灵/棋盘必然是 NDC 抬升，字形必然是
   > 像素→局部 + 正交。让资产自选变换既无表达力收益，又打开了「3D 材质用 2D
   > 变换」这类必然出错的组合。这正是「Domain 即 Master Material」的第一条
   > 落地点：凡是领域已确定的东西，资产就不该有旋钮。

2. ✅ **纹理槽形态与 `material_data` 类型推入领域契约**。
   `MaterialDomainTextureSlot` 从 3 字段扩到 6 字段，新增
   `sampler`（`SamplerType`）、`binding_required`、`channels`（0 = 由纹素格式
   决定，2 = 双通道法线 BC5）；`MaterialDomainContract` 新增第 11 个字段
   `material_data_type`（`GlobalSSBOType`）。配套新增
   `FindMaterialDomainTextureSlot()` 与 `GetMaterialDomainCanonicalMaterialDataType()`。
   校验器新增 `TextureSlotMismatch` / `MaterialDataTypeMismatch`：原「4. 纹理槽
   白名单」重写为**单遍扫描**（先查白名单，命中后比形态），并新增「5. 材质数据
   类型」一档，两者都报在 `key == "domain"` 上，错误文案给出领域规范值并附
   「建议直接删除 X 键」。解析器 `ParseDefinition` 在 `sampler` / `required` /
   `channels` 缺失时从契约兜底；槽名不在任何契约内则**显式报错**（无规范值可
   兜底）；`[resources].material_data` 整块缺失时从契约兜底，但**显式写了非规范
   值会被保留**交给校验器报错——与阶段三之一同一原则：兜底只填空，不改作者声明。
   9 个 `*.mi.toml` 随之瘦身：`lit` 的 6 个槽全部收成 `{ name = "..." }`，
   4 个无光材质的 `material_data = { type = "EmissiveSurface" }` 整行删除。
   门禁 `D.material-domain-contract` 新增 4 条负向探针（自选采样器种类 /
   自选绑定必要性 / 自选通道数 / 自选 `material_data` 类型），并把 `Text2D`
   最小材质探针从「无 `[transform]`」升级为「纹理只写 `name`」——一次钉住变换、
   槽形态、数据类型三者的契约兜底。门禁仍为 55 PASS / 0 FAIL。
   > **为什么槽形态属于 Domain**：同一领域内 `sampler` / `required` / `channels`
   > 必然相同；资产重复声明唯一可能的后果是「同领域不同布局」，进而分叉
   > `layout_hash`、破坏合批。`channels` 更危险——它直接决定 shader 是否注入
   > `MTL_TEX_<NAME>_CHANNELS` 与 Z 分量还原逻辑，写错运行期不报错、只是数值全错。
   > **为什么 `[resources].samplers`（过滤设置）留在资产**：这是真实的表达力差异，
   > `Text2D` 的 SDF 字形要 `Linear`、位图字形要 `Nearest`，领域无法统一。
   > 区分标准始终是「同领域是否必然相同」——采样器**种类**必然相同，采样器
   > **过滤**不必相同。
   > 契约表用聚合初始化，漏填 `material_data_type` 会被静默填成枚举 0
   > （`MeshDrawParams`），而 `IsGlobalSSBOType()` 会照样放行；因此加了一条
   > 编译期 `static_assert`，要求 16 个契约的该字段必须落在 surface 族
   > （`PBRSurface` / `EmissiveSurface` / `TransmissionSurface`）内。

3. ✅ **保底链代码化**。`builtin/pure_color`、`builtin/checkerboard_3d`、
   `builtin/error_checkerboard_3d`、`builtin/text_gpu`、`builtin/text_gpu_bitmap`
   五份定义从 `ShaderLibrary/material/*.mi.toml` 移入 C++：
   新增 `inc/hgl/mtl/BuiltinMaterialDefinitions.h` +
   `src/ShaderMaterialData/BuiltinMaterialDefinitions.cpp`（内嵌 schema-4 源码表），
   `MaterialDefinitionFileRegistry` 新增 `RegisterDefinitionSource()`
   （与 `LoadFile` 共用解析 + 领域校验 + 重复 ID 检测，`LoadFile` 改为委托它），
   `GetMaterialDefinitionFileRegistry()` 改为**先注册内嵌、再扫资产目录**，
   五个文件随之删除（目录内剩 9 份资产定义）。
   加载日志变为 `Loaded 9 material TOML definitions (0 errors) + 5 builtin definitions`；
   空库判据从 `file_count == 0 || registry.GetCount() == 0` 改为只看 `file_count`
   （内嵌定义已保证 registry 非空）。门禁 `S.material-definition-file-schema`
   改为复刻运行时的两段式注册（内嵌 5 + 目录 9），并新增一条硬断言：
   只喂内嵌源码的独立 registry 必须能解析五个保底 ID ——
   谁把它们挪回 TOML，这条立刻变红。
   > **为什么必须代码化**：这五个 ID 是**保底链本身**（`kFallbackMaterialRules`
   > 与 `TextRenderPipeline` 按字面量引用）。保底物一旦因为资产缺失 / 误删 / 写错
   > 而解析不出来，错误处理就会再失败一次——保底物的可依赖性必须高于它所要兜底
   > 的东西。它们也没有作者可调的自由度：纹理槽形态与 `material_data` 类型已在
   > 阶段三之二推入领域契约，剩下的只有「领域 + 顶点语义 + 变体开关」，属于引擎
   > 内部知识。
   > **代码化的边界**：内嵌的仍是完整 schema-4 源码，注册时走与 `.mi.toml`
   > **完全相同**的解析 + 领域校验管线。若改为在 C++ 里手工拼
   > `MaterialDefinition` 结构体，领域契约就得在第二处复述一遍
   > （`material_source_module_name` / `ntb_module_name` / `vertex_node_config` /
   > 纹理槽形态…），那正是本架构要消灭的重复。代码化消灭的是「对磁盘文件的
   > 依赖」，不是「对校验管线的依赖」。
