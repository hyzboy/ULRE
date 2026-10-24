# ULRE 超薄极简着色器架构设计规范

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

开发者在项目中创建的材质资产统一为材质实例格式，**完全消除原 `*.material.toml` 内部的模块声明**：

### 5.1 业务 PBR 资产（例：`Wood_Floor.mi.toml`）
```toml
# 材质实例: 只选择 Domain，填数值与贴图
domain = "StandardOpaque"

[properties]
roughness_factor = 0.8
metallic_factor = 0.0

[textures]
base_color = "textures/wood_basecolor.png"
normal = "textures/wood_normal.png"
roughness = "textures/wood_roughness.png"
```

### 5.2 业务无光纯色资产（例：`Gizmo_Axis_X.mi.toml`）
```toml
domain = "UnlitSolid"

[properties]
color = [1.0, 0.0, 0.0, 1.0]
```

---

## 6. 内置默认与回退机制（Built-in Fallbacks）

### 6.1 内建保底材质的本质定位
1. **`builtin/pure_color`**：
   - 不是独立材质系统，而是基于 `MaterialDomain::UnlitSolid` 的**C++ 内置保底默认实例（Default Fallback Instance）**。
   - 供无材质指定的图元、简单 Gizmo 调试直接引用。
2. **`builtin/checkerboard` 系列**：
   - 基于 `MaterialDomain::ErrorChecker3D` 的**C++ 内置报错实例（Error Visualizer Instance）**。
   - 当检测到网格缺少必需顶点属性（如缺 Normal、缺 UV）或缺少必要贴图时，自动路由至该实例，并传递特定标记色（例如洋红表示无材质、黄色表示缺法线、蓝色表示缺 UV）。
   - **完全脱离磁盘文件依赖，全部在 C++ 内部常驻构造**。

---

## 7. C++ 运行时代际变更与重构规划

### 7.1 C++ 裁减清单（废除项）
- ❌ **废除 ShaderGen 代码拼接引擎**：删除复杂的模块依赖拓扑排序、代码字符串替换引擎。
- ❌ **废除磁盘 `*.material.toml` 母材质定义文件**：不再通过 TOML 配置模块引用和 NTB 策略。
- ❌ **废除 `MaterialDefinitionFileRegistry`**：将其简化为 `MaterialInstanceRegistry`。

### 7.2 C++ 保留与增强清单（保留项）
- ✅ **`MaterialDomain` 枚举与 `DomainContracts`**：作为引擎母材质唯一权威真源。
- ✅ **资产门卫校验**：在加载 `MaterialInstance` 时严格校验网格属性与贴图槽位。
- ✅ **超薄 `PipelineConfigEmitter`**：根据 Pass 环境拼装 5~10 行 include 注入头。
- ✅ **SPIR-V 编译缓存**：缓存 Key 精简为 `(DomainID, PipelineConfigHash)`。

---

## 8. 实施路线图（四阶段）

| 阶段 | 分支 | 核心任务 | 交付产物 |
| :--- | :--- | :--- | :--- |
| **阶段 1：契约头与算法函数库固化** ✅ | `feature/ultra-thin-stage1-lighting-slots` | 制定 5 大光照插槽的 GLSL 统一签名；实现对应的固定算法文件及“无/空”对象实现（`*_none.glsl`） | `ShaderLibrary/lighting/` 基础算法库 |
| **阶段 2：Domain 主着色器固定化** ✅ | `feature/ultra-thin-stage2-domain-shaders` | 主着色器固定为「固定骨架 + 纯 `#include` 拼接」，槽模块内零 `#if`；C++ 侧只生成配置头 | 固定主着色器 + `PipelineConfigEmitter` |
| **阶段 3：MaterialInstance 模型** | `feature/ultra-thin-stage3-material-instance` | Domain 即 Master Material；资产降维为「选 Domain + 填数值 + 配贴图」的 MaterialInstance；保底材质移入 C++ | 极简资产形态上线 |
| **阶段 4：资产迁移与旧架构清理** | `feature/ultra-thin-stage4-cleanup-and-migration` | 现有资产迁移到 `.mi.toml`；废弃 `material.toml` 与旧版 ShaderGen 拼接代码 | 全套自动化门禁验证通过 |

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
   `*.material.toml` 的 `[transform]` 块被**整块删除**（98 行）。
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
