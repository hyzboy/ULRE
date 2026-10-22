# Ambient 种类现状清单（Ambient Provider Inventory）

> 结论快照：2026-10-18，分支 `DynamicAmbient`。
> 目的：回答「整个工程里 Ambient 到底有几种、分别是什么、边界在哪」，
> 并记录当前实现的缺口。**本文只描述现状，不含改造方案。**
> 前置背景：Ambient 的**算法选择权已从材质上移到引擎/环境层**
> （`3569db6a3`、`e406b003b`、`f76abee7f`、`c5c5291a0`），材质 TOML 写
> `ambient_module_name` 会被显式拒绝并引导改用 `EnvironmentSystem::SetAmbientProvider()`
> （`src/ShaderMaterialData/MaterialDefinitionFile.cpp:837-840`）。

---

## 一、先厘清：Ambient 在模板里是 4 条独立槽，不是 1 条

`ForwardLitSlots[]`（`src/ShaderGen/template/RenderTemplate.cpp:16-32`）中与环境光相关的槽位：

| 槽位（`ShaderModuleSlotRole`） | 角色名（`GetShaderModuleSlotRoleName`） | 现役模块 | 种类数 |
|---|---|---|---|
| `AmbientLightProvider` | `ambient_light_provider` | `indirect_sky_ambient` / `indirect_ibl` | **2** |
| `AmbientOcclusionProvider` | `ambient_occlusion_provider` | `identity_ao` | 1（恒等占位） |
| `SkyProvider` | `sky_provider` | `sky_atmosphere` | 1 |
| `OutputPolicy` | `output_policy` | `forward_lighting` / `flat_lighting` | 2（后者**无 ambient**） |

因此「Ambient 种类」的准确口径是：
**2 种间接光算法** + 1 种 AO（恒 1.0）+ 1 条 flat 路径（完全不算环境光）。

全库仅这两个模块声明 `@ulre slot ambient_light_provider`：

```
ShaderLibrary/lighting/indirect_ibl.glsl
ShaderLibrary/lighting/indirect_sky_ambient.glsl
```

`@ulre slot ambient_occlusion_provider` 仅 `ShaderLibrary/ao/identity.glsl`；
`@ulre slot sky_provider` 仅 `ShaderLibrary/sky/sky_atmosphere.glsl`。

---

## 二、两种 `ambient_light_provider` 的实质差异

| 维度 | `indirect_sky_ambient` | `indirect_ibl` |
|---|---|---|
| **算法** | 程序化天空按世界法线采样（单次 `EvalSkyAtmosphere`） | split-sum IBL：irradiance cube + prefiltered cube + BRDF LUT |
| **输出公式** | `skyColor × baseColor × (1-metallic) × ao` | `(diffuse + specular) × ao`，含 Fresnel `F0 = mix(fresnel, baseColor, metallic)` |
| **镜面** | 无独立镜面项 | `prefiltered(R) × (F0·brdf.x + brdf.y)` |
| **数据源** | `scene_ubo`（大气参数 + 主光方向 `main_light` 行 0） | `ambient_env.env_tex`（`uvec4`：x=irradiance cube、y=prefilter cube、z=BRDF LUT、w=预留）+ `scene_ubo` |
| **include 闭包** | `lighting_interface`、`scene_ubo`、`sky_atmosphere`（整份大气模型） | `lighting_interface`、`scene_ubo`、`bindless_textures` |
| **sampler 依赖** | 无 | `Trilinear`（两个 cube）+ `Linear`（BRDF LUT） |
| **未就绪行为** | 总有颜色（天空是程序化的） | `env_tex.x==0 && env_tex.y==0` → **返回 `vec3(0)`** |
| **是否模板默认** | ✅ 是（`src/ShaderMaterialData/SceneRenderTemplateResolver.cpp:21-24`） | ❌ 需显式设置 |
| **示例** | `example/Environment/AtmosphereSkyAmbient.cpp`（不调 API，走默认） | `example/Environment/IBLEnvironment.cpp:296`（`SetAmbientProvider("indirect_ibl")`） |
| **GPU 参数块** | 无专属块，复用 `scene_ubo` | 专属块 `AmbientEnvInfo`（`inc/hgl/graph/ubo/AmbientEnvInfo.h`，16 字节、单份 buffer、`WorldAddresses::addr_ambient_env`） |

### 关键边界：两者是并列替代，不是降级链

`indirect_ibl` 在贴图未绑定时**直接变黑**（只剩直接光），**不会**回落到
`indirect_sky_ambient`。因此环境层切换算法轴时，必须同时保证数据轴
（`Profile::cpu.ambient_env`）已填有效句柄 —— 这是
`inc/hgl/ecs/systems/render/EnvironmentSystem.h:67` 那条 ⚠ 注释的由来：
「换模块**必须**同时确认 `ambient_env` 已给有效句柄（IBL 模块采样它）」。

### 两者可互换的结构原因

两个模块实现**逐字相同**的函数签名：

```glsl
vec3 EvalIndirectLighting(LightingInput lighting);
```

`#include` 是唯一实质差异 —— 这正是「换 ambient = 换一个 include」结论的根基，
也是 `AmbientLightProvider` 能成为纯 `@ulre` 模块级选择的前提。

---

## 三、能力轴现状（`provides/requires_capability`）

```
provides ambient_diffuse|ambient_specular   ← indirect_sky_ambient, indirect_ibl
provides ambient_diffuse                    ← sky_atmosphere（但它占 sky_provider 槽）
provides ambient_occlusion                  ← identity_ao
requires ...|direct_light|ambient_diffuse   ← forward_pbr（全库唯一 requires 点）
```

### 观察点 1：`ambient_specular` 无任何消费者

全库 `ambient_specular` 只有 `provides` 两处，`forward_pbr` 的
`requires_capability`（`ShaderLibrary/lighting/forward_pbr.glsl:6`）只到
`ambient_diffuse`。能力闭合只做 `required ⊆ provided`，因此这条能力轴
目前是「声明但无人索取」，实际闭合只走 diffuse。

### 观察点 2（待验证疑点）：`sky_atmosphere` 也 provides `ambient_diffuse`

`sky_atmosphere` 占的是 `sky_provider` 槽，却声明了 `ambient_diffuse`。
若能力闭合只判「谁提供了该能力」而不区分槽位，理论上会让缺 ambient provider
的模板误判为已满足。**此点尚未验证**，需对照
`ValidateRenderTemplateRequest`（`src/ShaderGen/template/RenderTemplate.cpp:363-420`）
的实际闭合口径确认。

---

## 四、AO 轴现状

`ShaderLibrary/ao/identity.glsl`：

```glsl
// @ulre slot ambient_occlusion_provider
// @ulre provides_capability ambient_occlusion

float GetAmbientOcclusion(SurfaceInput surface)
{
    return 1.0;
}
```

- 唯一 AO provider，**恒返回 1.0**，槽位是纯契约占位，**没有任何真实 AO 算法**。
- 消费点在 `compositor/forward_lighting.glsl:31` 附近（AO 契约注释）→
  `LightingInput.ao` → 两个 ambient provider 都乘 `lighting.ao`。
- 历史上 `LightingInput.ambientColor` 死字段与 `GetSkyAmbientColor()` 死函数
  已在 `5852ba7eb` 删除，AO 是间接光调制的**唯一**入口。

---

## 五、已消失的种类

| 名称 | 状态 | 证据 |
|---|---|---|
| `indirect_simple_ambient`（常量环境光） | **代码已删** | 全库只剩 `ShaderLibrary/lighting/indirect_sky_ambient.glsl:14` 一行注释「替换 indirect_simple_ambient」 |

即目前**没有「常量 ambient」这一档**：最低画质档就是程序化天空光，
仍要跑 `EvalSkyAtmosphere`。

另需区分：`ShaderLibrary/material/sky_cube_surface.glsl` 是 `surface_provider`
（天空盒 cubemap 外观），**不参与环境光计算**。

---

## 六、flat 路径：完全不参与 Ambient

`MakeForwardUnlitProfile()`（`SceneRenderTemplateResolver.cpp:37-47`）只有
`SurfaceProvider` + `OutputPolicy(flat_lighting)`，**没有** `AmbientLightProvider` 槽。

`ShaderLibrary/lighting/forward_flat.glsl` 的 `EvalLighting` 直接返回
`baseColor + emissive`；`compositor/flat_lighting.glsl` 的
`BuildForwardLightingInput` 把 `mainLightColor`、`reflectionColor` 置 0、
`shadowFactor` 置 1，`lighting.ao` 只从 `surface.ao` 透传，不做间接光合成。

因此 flat 是「第 3 种环境光策略」—— 即**无环境光**。

---

## 七、引擎层控制链路（算法轴 + 数据轴）

```
EnvironmentSystem::SetAmbientProvider(module_name)          // 算法轴（GLSL 模块名）
        ↓
EnvironmentManager::Profile::ambient_module_name            // 空串 = 模板默认
EnvironmentManager::Profile::cpu.ambient_env                // 数据轴（IBL 句柄 → UBO）
        ↓
ECSContext::GetAmbientProviderName()
        ↓
RenderPrimitiveCollectSystem（src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:483-489, :1151）
        ↓
MaterialDefinitionBuildRequest.ambient_module_name
        ↓
SelectSceneRenderTemplateRequest(..., ambient_module_name)   // 唯一实现，运行时与离线工具共用
        ↓
RenderTemplate 的 AmbientLightProvider 槽 → 变体键 / program 身份
```

设计要点：**算法轴与数据轴必须同属一个所有者**（环境层）。旧实现把算法轴放在材质
（`ambient_module_name` TOML 键）⇒ 同一套 PBR 表面被迫在材质里绑定环境光算法，
无法跨环境复用。

---

## 八、缺口清单

1. **`SetAmbientProvider()` 不校验模块名** ——
   `src/SceneGraph/module/EnvironmentManager.cpp:219-237` 只做 profile 句柄检查与
   等值短路，然后把字符串存进 `p->ambient_module_name`。非法模块名要拖到模板解析
   （`ValidateRenderTemplateRequest`）才报错，错误发现点偏远。
2. **AO 只有 `identity_ao`**，无任何真实 AO 算法，槽位是契约占位。
3. **缺少「无 ambient / 常量 ambient」廉价档** —— 低画质只能退到天空光，
   仍要承担 `sky_atmosphere` 的完整大气模型成本。
4. **`ambient_specular` 能力轴空转** —— 有 `provides` 无 `requires`。
5. **`sky_atmosphere` 的 `ambient_diffuse` 声明**是否会让能力闭合掩盖「缺 ambient
   provider」的模板，待验证（见 §三 观察点 2）。
6. **`indirect_ibl` 未就绪即变黑**，无回落策略；切换算法轴时的数据轴前置条件
   只靠注释约束，没有代码级校验。

---

## 附：相关历史结论（Ambient 在 Shader 层的四个衍生问题）

由本文档的同一轮分析衍生并已全部修复，供追溯：

| # | 问题 | 落点 |
|---|---|---|
| P1 | sampler 预设依赖无声明 → 编译期才炸 | `@ulre sampler_reference` 契约（`455e1da67`、`933370ef0`、`691d36b87`） |
| P2 | `LightingInput.ambientColor` 死字段 | `5852ba7eb` |
| P3 | `GetSkyAmbientColor()` 死函数 | `5852ba7eb` |
| P4 | 正文 `#include` 对 manifest 模块图不可见 | include↔`@ulre uses` 硬契约（`c2ea0bd5d`、`7653e45be`、`d8fe774c6`） |

P4 的契约细节见 `doc/ShaderGen_MaterialGLSL_DesignAnalysis.md`
「原理 2：GLSL 代码模块自描述」中 `@ulre uses` 一致性段落。
