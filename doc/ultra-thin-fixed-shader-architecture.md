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

## 8. 实施路线图（三阶段）

| 阶段 | 核心任务 | 交付产物 |
| :--- | :--- | :--- |
| **阶段 1：契约头与算法函数库固化** | 制定 5 大光照插槽的 GLSL 统一签名；实现对应的固定算法文件及“无/空”对象实现（`*_none.glsl`） | `ShaderLibrary/lighting/` 基础算法库 |
| **阶段 2：Domain 主着色器固定化** | 将 `StandardOpaque`、`UnlitSolid` 等核心 Domain 重构为无 `#if` 的物理 `.frag.glsl` 文件；编写 C++ `PipelineConfigEmitter` | 固定主着色器 + C++ 配置发射器 |
| **阶段 3：资产降维与旧架构清理** | 将现有材质转为纯粹的 `MaterialInstance`；将 `pure_color` 与 `checkerboard` 保底机制移入 C++；废弃旧版 ShaderGen 拼接代码与 `material.toml` | 极简架构上线，全套自动化门禁验证通过 |
