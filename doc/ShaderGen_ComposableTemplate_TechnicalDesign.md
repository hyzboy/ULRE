# ShaderGen 受控组合模板技术方案

> **校对说明(2026-09)**:本文是设计意图文档。为不破坏原有设计叙述,正文未删改,只在开头补一节「实现状态(以当前代码为准)」,并在与当前代码冲突的示例 GLSL 片段处**就地校正 + 标注**;所有未在代码树中命中的设计态符号集中列在 §0.2,不做改名,也不删设计意图。

## 0. 实现状态(以当前代码为准)

### 0.1 已落地符号(设计名 → 代码实名 / 依据)

| 本文设计名 | 代码现状 | 依据 |
|---|---|---|
| `ShaderDocument`(块 / 顺序 / hash) | **已落地**,同名 | `inc/hgl/mtl/ShaderDocument.h:9-19`(块种类)、`:90`(块序)、`:101-109`(序列化与 hash);块序实现 `src/ShaderGen/document/ShaderDocument.cpp:42-57` |
| `StageTemplate` / `StageTemplateResolver` | 落地为**两件东西**:`RenderTemplate`(模板定义 + slot 表)与 `SceneRenderTemplateResolver`(从显式选定的身份构造请求) | `inc/hgl/mtl/RenderTemplate.h:14-120`、`inc/hgl/mtl/SceneRenderTemplateResolver.h:9-37`、`src/ShaderGen/template/SceneRenderTemplateResolver.cpp` |
| "已批准组合表" / `FixedPipelineVariantTable` | 落地形态是 **`RenderTemplateID` + `RenderTemplateDefinition Templates[]` 静态表**(7 个模板:`forward_lit_shadowed_ao` / `forward_lit_shadowed_identity_ao` / `forward_lit_unshadowed_ao` / `forward_unlit` / `shadow_caster_opaque` / `shadow_caster_masked` / `sky`) | `src/ShaderGen/template/RenderTemplate.cpp:74-96` |
| "resolver 对未登记组合返回明确诊断" | **已落地**:`RenderTemplateValidationError`(含 `UnknownTemplate`/`StageMismatch`/`VersionMismatch`/`EmptyModuleRoot`/`UnknownSlotRole`/`UnexpectedSlotRole`/`DuplicateSlotRole`/`MissingRequiredSlot`/`ModuleNotFound`) | `inc/hgl/mtl/RenderTemplate.h:109-121` |
| slot 契约(`surface_provider`、`lighting_model`、`output_policy`…) | **已落地**为 `enum class ShaderModuleSlotRole`:`SurfaceProvider`、`DirectLightProvider`、`ShadowProvider`、`AmbientLightProvider`、`AmbientOcclusionProvider`、`LightingModel`、`OutputPolicy`、`MaterialSourceProvider`、`NTBProvider`、`SkyProvider` | `inc/hgl/mtl/RenderTemplate.h:26-47` |
| "输出 capability 满足 lighting model 输入 capability" | **已落地**:模块定义携带 `slot_role` / `provided_capabilities` / `required_capabilities` | `inc/hgl/mtl/ShaderCodeModule.h:260-262` |
| `Module` / `Function` block | **已落地**(是 `ShaderDocumentBlockKind::Module` / `::Function` 两个枚举值,不是独立类) | `inc/hgl/mtl/ShaderDocument.h:9-19` |
| `FragmentTemplateComposer` | **已落地**,同名(slot → `#include` 发射、MainBody 组装、输出声明内联生成) | `inc/hgl/mtl/FragmentTemplateComposer.h:18-51`、`src/ShaderGen/template/FragmentTemplateComposer.cpp:128`、`:251`、`:491-515` |
| `MeshTemplateEmitter` | **已落地**,同名(+ `MeshTemplateComposer`) | `src/ShaderGen/meshgen/MeshTemplateEmitter.h:63-80`、`inc/hgl/mtl/MeshTemplateComposer.h` |
| `ShaderCodeModuleRegistry` | **已落地**,同名(+ metadata/capability/dependency/conflict 校验) | `inc/hgl/mtl/ShaderCodeModuleRegistry.h`、`inc/hgl/mtl/ShaderCodeModuleMetadata.h`、`src/ShaderGen/glsl_module/` |
| 模块 capability/slot 元数据 | **已落地**:GLSL 源内 `@ulre slot` / `@ulre provides_capability` / `@ulre require` 注解由解析器读入 | `src/ShaderGen/glsl_module/ShaderCodeModuleFile.cpp:165`、`:482-495`;实例 `ShaderLibrary/lighting/indirect_sky_ambient.glsl:2-10` |
| `GenericMaterialBuilder` | **已落地**,同名 | `src/ShaderGen/builder/GenericMaterialBuilder.h/.cpp` |
| `MaterialShaderEmitter` | **已落地**,同名(生成 Material ABI / 纹理引用行 + `MTL_TEX(i)` 宏) | `src/ShaderGen/compile/MaterialShaderEmitter.h/.cpp`(`MTL_TEX` 展开见 `MaterialShaderEmitter.cpp:306-324`) |
| `ShaderCooker` | **已落地**,同名(枚举 definition × purpose × 图元,走与生产相同管线) | `src/Tools/ShaderGen/ShaderCooker.cpp`(模板解析见 `:146-175`) |
| `AppendMaterialRenderTemplateRoots` | **已落地**,同名 | `inc/hgl/mtl/MaterialDefinitionRegistry.h:61-63` |
| ECS/render preparation 只选 template ID + roots | **已落地**:`MaterialDefinitionRegistry` 提供 profile/roots,规则集中在 `SceneRenderTemplateResolver` 的 `Make*Profile()` | `inc/hgl/mtl/SceneRenderTemplateResolver.h:22-25` |

回归/验收夹具(对应 §9):`src/Tools/ShaderGen/ShaderDocumentRegression.cpp`、`ShaderDocumentProductionRegression.cpp`、`SceneRenderTemplateResolverRegression.cpp`、`ShaderResourceSchemaRegressionGate.cpp`。

### 0.2 「未能核实 / 设计态」清单(未改名、未删,仅记录)

- **全树 0 命中的符号(设计态命名)**:`FixedShaderVariantKey`、`FixedPipelineVariantTable`、`StageTemplate`、`StageTemplateResolver`、`MaterialShaderDocument`、`ModuleBlock`、`FunctionBlock`、`SlotRole`、`TemplateComposer`、`ShaderStageTemplate`、`SurfaceData`、`DirectLightData`、`AmbientLightData`、`FragmentInput`、`WorldVertex`、`GetSurfaceData`、`GetDirectLight`、`GetAmbientLight`、`EvaluateDirectLighting`、`EvaluateAmbientLighting`、`WriteFragmentOutput`、`BuildFragmentInput`、`GetSurfaceLightingConfig`、`ApplyLocalDeform`、`ApplyLocalToWorld`、`LoadVertexInput`、`WriteMeshVertex`、`BlinnPhong`(作为枚举/模块)、`EnvMap`、`MatrixPalette`、`SampleAmbientSpecular`、`PipelineFamily`、`AmbientProfile`、`NTBEncoding`、`GeometryProfile`、`SkinningMode`。落地等价物见 §0.1 与各节就地校正注。
- **部分落地/改名落地(不计入上面 0 命中)**:`LightingModel` → `ShaderModuleSlotRole::LightingModel`(`inc/hgl/mtl/RenderTemplate.h:34`);`ShadowFactor` → `ShaderModuleCapability::ShadowFactor`(`inc/hgl/mtl/ShaderCodeModule.h:142`,是 capability 位而非结构体);`GetShadowFactor` → `float GetShadowFactor(SurfaceInput)`(`ShaderLibrary/shadow/identity.glsl:12`、`pcf_shadow.glsl:133`);`GetAmbientOcclusion` → `float GetAmbientOcclusion(SurfaceInput)`(`ShaderLibrary/ao/identity.glsl:12`);`AlphaMode` → 引擎 shader-gen 侧无此轴,同名枚举只存在于 glTF 导入器(`src/Tools/GLTFConvert/gltf/GLTFMaterial.h:99`)。
- **§2 的"变体轴"模型**:现实里这些轴被压平进 `RenderTemplateID` + purpose(颜色/深度)+ mesh 模式枚举,没有笛卡尔轴表(见上条命名命中情况);**`AmbientProfile` 轴未落地**(ambient 由独立的 `AmbientLightProvider` 模块选择,模板 ID 不区分常量光/SH/IBL);`Sky + NTB`、`ShadowCaster + IBL`、`CharQuad + PBR` 这类"轴组合拒绝"没有对应的枚举组合校验,只有 §0.1 里那份 slot/角色级校验。
- **§4.6 的 SSAO / AO provider**:`ao/identity.glsl` 存在(`ShaderLibrary/ao/identity.glsl`),但没有 SSAO 模块与 fullscreen AO pass 的落地证据(0 命中)。
- **骨骼动画 / `SkinningMode` / `MatrixPalette`**:未实现(全树唯一 "skinning" 命中在第三方 meshoptimizer README)。billboard **已实现**,但形态是 Stage3 顶点模块(`ShaderLibrary/vertex/s3_camera_facing_world.glsl`、`s3_camera_facing_fixed_pixels.glsl`),不是"替换 `LocalDeform`/`LocalToWorld` slot"。
- **§7 `MaterialShaderEmitter` 一行的"binding/index-table Document fragments"**:说法已过期——BDA 终态后**没有 per-material 描述符/binding index table**,材质纹理引用经 `pc_root.addr_texture_references` + `MTL_TEX(i)` 解引用(见 §4.2 就地校正)。
- **§8 各条的实施状态**:第 1 条只部分落地(有 `RenderTemplateID`+静态模板表,无 variant key 轴);第 2 条已落地(见 0.1);第 3 条已落地(模板表即 skeleton/default 选择真源);第 4 条部分落地(Forward Lit 模板已固化,但"byte/key/SPV regression"只有 §0.1 列出的那几个 gate);第 5 条不成立(`GetSurfaceLightingConfig` 0 命中);第 6 条 Sky/ShadowCaster 已接入,**Decal 与独立 PostProcess family 未见模板登记**;第 7 条(TOML 外置)未做。
- **§9 的 fixture 清单**(PBR/IBL、Blinn-Phong/EnvMap、PBR/SH…):代码里只有 `ShaderLibrary/material/lit_ibl.material.toml` 等材质定义与上述 4 个 regression gate,"首批批准组合全部有 cooker fixture"这一条无法按本文的轴粒度核实。

(以上"0 命中"均由 `rg` 在 `inc/`、`src/`、`example/`、`ShaderLibrary/`、`res/` 全树检索确认。)

## 1. 目标与边界

本方案面向固定 Vulkan 前向渲染器，不实现 Shader Graph，不支持任意材质代码改变 shader
控制流。ECS/render preparation 根据实际渲染状态、场景和质量配置选择完整场景模板及其
scene module roots，再把 MaterialDefinition 提供的 material provider capability 通过
`AppendMaterialRenderTemplateRoots` 追加到该模板；ShaderGen 只递归展开依赖、汇总资源、
验证并组合为可审计的 `ShaderDocument`，再离线编译为 SPV。

首批覆盖范围：

- Forward Lit：PBR、Fake PBR、Blinn-Phong；
- 环境/天光：常量环境光、SH、EnvMap、IBL、大气和后续 GI；
- 表面输入：纯色、顶点色、纹理材质、RG8 与 RGBA16F x2 NTB；
- 几何：静态 Mesh、骨骼动画、Billboard、LineQuad、CharQuad；
- Pass：Opaque、Masked、Blend、Dither、A2C、ShadowCaster、Sky、Decal；
- 后处理：SSAO、DOF 等独立全屏固定管线。

不在范围内：

- 节点图、任意 DAG、用户注入 GLSL `main()`；
- 由模板隐式决定 descriptor、varying 或 output ABI；
- 为某个材质向通用生成器增加特例分支。

若开发者需要不同控制流或不同 contribution 合成，应增加或修改版本化模板，而不是扩展
生成器的自由度。

## 2. 固定组合模型

```text
FixedShaderVariantKey
  -> FixedPipelineVariantTable
       -> StageTemplate
       -> resolved module slots
       -> contract/layout/interface validation
       -> ShaderDocument
       -> GLSL serialization
       -> offline GLSLCompiler
       -> SPV artifact
```

`FixedShaderVariantKey`：

```text
family | pass | lighting_model | ambient_profile | ntb_encoding
       | skinning_mode | alpha_mode | geometry_profile | template_version
```

建议初始枚举：

```text
PipelineFamily: ForwardLit, ForwardUnlit, ShadowCaster, Sky, Decal, PostProcess
LightingModel: Unlit, BlinnPhong, FakePBR, PBR
AmbientProfile: None, Constant, SH, EnvMap, IBL, GIProbe
NTBEncoding: None, RG8, RGBA16F2
SkinningMode: Static, MatrixPalette
AlphaMode: Opaque, Masked, Blend, Dither, A2C
GeometryProfile: Mesh, SkinnedMesh, Billboard, LineQuad, CharQuad, Fullscreen
```

> **状态注(2026-09)**:以上枚举是设计态命名,代码里 0 命中(见 §0.2)。当前的对应真源是 `RenderTemplateID`(7 个已登记模板)与 `ShaderModuleSlotRole`(slot 角色),见 §0.1;不存在"key 的笛卡尔轴表",也没有对这些轴的组合拒绝校验。

组合不是全部轴的笛卡尔积。`FixedPipelineVariantTable` 仅登记已批准条目；resolver 对未登记
组合返回明确诊断。模板声明的每一个 slot 都必须有实际 module；没有该功能的渲染路径应
选择另一个模板，不能使用静默降级实现。

例如：

| 合法组合 | 原因 |
|---|---|
| `ForwardLit + PBR + IBL + RGBA16F2 + Static` | 主质量档 |
| `ForwardLit + BlinnPhong + EnvMap + RG8 + MatrixPalette` | 低配骨骼档 |
| `ForwardLit + FakePBR + SH + None` | 极低配档 |
| `ShadowCaster + Unlit + None + MatrixPalette + Masked` | 阴影裁切骨骼档 |
| `Sky + Unlit + IBL + Fullscreen` | 天空/环境显示 |

以下组合应在 resolver 阶段拒绝：`Sky + NTB`、`ShadowCaster + IBL`、
`CharQuad + PBR`、`BlinnPhong + IBL specular`（若未明确实现）。

## 3. Fragment 模板与模块契约

Forward Lit 只支持逐像素光照。Vertex/Mesh stage 负责几何处理和 varying 输出，不计算光照。

固定的 Forward Lit 模板控制流：

```glsl
SurfaceData surface = GetSurfaceData(input);

DirectLightData direct = GetDirectLight(input);
AmbientLightData ambient = GetAmbientLight(input);
ShadowFactor shadow = GetShadowFactor(input);
float ao = GetAmbientOcclusion(input, surface);

vec3 direct_radiance =
    EvaluateDirectLighting(surface, input, direct) * shadow.value;
vec3 ambient_radiance =
    EvaluateAmbientLighting(surface, input, ambient) * ao;

WriteFragmentOutput(surface, direct_radiance + ambient_radiance);
```

> **就地校正(2026-09)**:上面是设计伪码,下列符号在当前代码里 0 命中:`SurfaceData`、`DirectLightData`、`AmbientLightData`、`ShadowFactor`、`FragmentInput`、`GetSurfaceData`、`GetDirectLight`、`GetAmbientLight`、`EvaluateDirectLighting`、`EvaluateAmbientLighting`、`WriteFragmentOutput`。当前真实控制流(模板 + slot 模块拼接后的等价物)是:
>
> ```glsl
> // ShaderLibrary/fragment/forward_lit.glsl.tmpl:11-18 —— {{...}} 占位由 FragmentTemplateComposer 填充
> const SurfaceOutput surface = EvalSurface(si, materialDataIndex);        // slot: surface_provider
> const LightingInput lighting = BuildForwardLightingInput(surface, si);    // slot: output_policy
> const vec4 finalColor = EvalLighting(lighting);                           // slot: lighting_model
> WriteMaterialOutput(HGLComposeColor(finalColor));
> ```
>
> 对应关系:`FragmentInput → SurfaceInput`(`ShaderLibrary/common/surface_interface.glsl:13-24`)、`SurfaceData → SurfaceOutput`(`:29-38`)、`DirectLightData`+`AmbientLightData` → 单一 `LightingInput`(`ShaderLibrary/common/lighting_interface.glsl:13-42`,含 `mainLightDir/mainLightColor/ambientColor/reflectionColor/shadowFactor`;`shadowFactor` 是 `float`,不再有 `ShadowFactor` 结构体)、`GetSurfaceData → EvalSurface`、`EvaluateDirectLighting → EvalDirectLighting(LightingInput)`(`ShaderLibrary/lighting/direct_cook_torrance_pbr.glsl:15`)、`EvaluateAmbientLighting` → 独立 ambient 模块的 `EvalIndirectLighting(LightingInput)`(`ShaderLibrary/lighting/indirect_sky_ambient.glsl:20`)、`WriteFragmentOutput → WriteMaterialOutput(HGLComposeColor(...))`(输出声明由 `FragmentTemplateComposer::BuildOutputDeclarationText` 内联生成,见 §4.7)。

模块只可填充下列 slot：

| Slot | 输出或职责 | 典型实现 |
|---|---|---|
| `surface_provider` | `SurfaceData GetSurfaceData(FragmentInput)` | PBR texture、顶点色、decal |
| `direct_provider` | `DirectLightData GetDirectLight(FragmentInput)` | Sun、forward local-light list |
| `shadow_provider` | `ShadowFactor GetShadowFactor(FragmentInput)` | PCF ShadowMap 等 shadow 实现 |
| `ambient_provider` | `GetAmbientLight` 和可选 `SampleAmbientSpecular` | Constant、SH、EnvMap、IBL、GI |
| `ao_provider` | `float GetAmbientOcclusion(FragmentInput, SurfaceData)` | material AO、SSAO、GTAO |
| `lighting_model` | direct/ambient evaluation | PBR、FakePBR、Blinn-Phong |
| `output_policy` | MRT/HDR/alpha policy | Forward HDR、decal、depth-only |

`SurfaceData` 和 `AmbientLightData` 是稳定 ABI；可以随版本增加字段，但不得让模块经隐式全局
变量耦合：

```glsl
struct SurfaceData
{
    vec3 base_color;
    vec3 normal_ws;
    float metallic;
    float roughness;
    vec3 emissive;
    float opacity;
};

struct DirectLightData
{
    vec3 direction_ws;
    vec3 radiance;
};

struct AmbientLightData
{
    vec3 diffuse_irradiance;
    vec3 gi_radiance;
    uint capabilities;
};

struct ShadowFactor
{
    float value;     // [0, 1]
};
```

在实际 GLSL 中，不需要每个 profile 都声明全部资源。sampler/UBO/SSBO 必须由 provider
作为 `Resource` block 独立声明，不能作为 GLSL struct 成员。IBL provider 可额外实现
`SampleAmbientSpecular(surface, input, ambient)`；PBR lighting model 需要该能力时调用它，
而 Blinn-Phong 可只消费 `diffuse_irradiance`。`capabilities` 用于模块 metadata 和生成期
校验，运行时不应依赖动态分支。

## 4. 最简 GLSL 范例

以下是展示 slot 契约的最小示例；真实项目应使用现有 include 路径、descriptor 生成与
`ShaderDocument` source metadata。

### 4.1 `templates/forward_lit.frag.glsl`

```glsl
#version 460

// Generated resource and interface declarations are inserted here.

// surface_provider, direct_provider, shadow_provider, ambient_provider,
// ao_provider, lighting_model and output_policy are inserted here.

void main()
{
    FragmentInput input = BuildFragmentInput();
    SurfaceData surface = GetSurfaceData(input);

    DirectLightData direct = GetDirectLight(input);
    AmbientLightData ambient = GetAmbientLight(input);
    ShadowFactor shadow = GetShadowFactor(input);
    float ao = GetAmbientOcclusion(input, surface);

    vec3 direct_radiance =
        EvaluateDirectLighting(surface, input, direct) * shadow.value;
    vec3 ambient_radiance =
        EvaluateAmbientLighting(surface, input, ambient) * ao;

    WriteFragmentOutput(surface, direct_radiance + ambient_radiance);
}
```

该模板对应固定 `MainBody` block；所有 provider 和 lighting model 在此之前以 `Module` 或
`Function` block 写入。

> **就地校正(2026-09)**:真实模板**不是手写 GLSL 文件**,而是带占位符的 `.tmpl` + C++ 组装:
> - 文件:`ShaderLibrary/fragment/forward_lit.glsl.tmpl`(另有 `forward_unlit.glsl.tmpl`、`sky.glsl.tmpl`),占位符为 `{{defines}}`、`{{module_includes}}`、`{{fragment_inputs}}`、`{{output_declarations}}`、`{{code_modules}}`、`{{surface_input_wiring}}`、`{{sky_provider_include}}` 等;
> - 填充者:`src/ShaderGen/template/FragmentTemplateComposer.cpp:491-515`(`ApplyFragmentTemplateSlot` 逐槽替换,`MainBody` 块再入 Document);
> - 上面示例里的 `FragmentInput input = BuildFragmentInput();` 在现实中是 `{{surface_input_wiring}}` → `SurfaceInput si;` + 逐字段装配(`FragmentTemplateComposer.cpp:586`)。

### 4.2 最小表面模块：`surface/pbr_texture.glsl`

**就地校正(2026-09)**:原文的 `layout(set = 1, binding = 0) uniform sampler2D base_color_texture;` 这类**命名 sampler 绑定**在当前引擎中已不存在——per-material 描述符集与 `BindSSBO`/`SBS_*` 全删,全设备只剩 Scene(0)/Bindless(1) 两集,材质纹理引用经 **BDA**(`pc_root.addr_texture_references`)+ `MTL_TEX(i)` 宏解引用。下面片段已改成当前写法(可对照 `ShaderLibrary/material/texture_source.glsl`、`pbr_surface_source.glsl` 逐行核对):

```glsl
// MTL_TEX(i) 由 ShaderGen 生成 —— 展开见 src/ShaderGen/compile/MaterialShaderEmitter.cpp:322
//   MaterialTextureReferencesRef(pc_root.addr_texture_references
//     + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs)
//           .values[i].texture_reference_index) * uint64_t(row_stride))
// 返回 uvec2:.x = bindless 纹理句柄(1-based,0=未绑定),.y = Texture2DArray 层号

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)   // 原 GetSurfaceData(in FragmentInput)
{
    const uvec2 base_ref = MTL_TEX(dataIndex).tex_base_color;
    const vec4 base = Sample2DArray(base_ref.x, TrilinearSampler,
                                    si.uv0, float(base_ref.y));
    const uvec2 rough_ref = MTL_TEX(dataIndex).tex_roughness;
    const vec4 material = Sample2DArray(rough_ref.x, LinearSampler,
                                        si.uv0, float(rough_ref.y));

    SurfaceOutput surface;
    surface.baseColor = base.rgb;
    surface.normal = normalize(si.worldNormal);      // 原 surface.normal_ws
    surface.metallic = material.b;
    surface.roughness = max(material.g, 0.045);
    surface.emissive = vec3(0.0);
    surface.alpha = base.a;                          // 原 surface.opacity
    return surface;
}
```

证据:取样统一宏 `Sample2DArray`(`ShaderLibrary/common/bindless_textures.glsl:51-59`,展开为 `texture(sampler2DArray(bindless_tex[nonuniformEXT(handle - 1u)], bindless_samp[nonuniformEXT(samp_idx)]), vec3(uv, layer))`);bindless 数组声明 `:34-38`(`binding=0` `texture2DArray` / `binding=1` `sampler` / `binding=2` `textureCubeArray`,`BINDLESS_SET=1`);`MTL_TEX` 真实用法 `ShaderLibrary/material/pbr_surface_source.glsl:44-59`;材质纹理行结构 `ShaderLibrary/common/bindless_textures.glsl:24-25`。

RG8/RGBA16F x2 NTB 的差异不应改变 `LightingModel`。它们是不同的
`surface_provider` 或 `normal_decode` module，最终都填写 `surface.normal_ws`。

### 4.3 最小直射光模块：`direct/sun.glsl`

**就地校正(2026-09)**:`layout(set = 0, binding = 0) uniform SunLight` 不存在。Scene 集 binding 0 是 **camera**(`kSceneBindingCamera`,`inc/hgl/common/DescriptorSetTypeDef.h:15`),而且**没有任何 per-object / 每光源 UBO**——太阳方向与颜色来自环境 profile 的 sky 段(Scene binding 1),由 `sky/sky_atmosphere.glsl` 暴露为两个函数。当前真实写法:

```glsl
// 数据源:Scene 集 binding 1 的 SkyInfo sky(块声明 ShaderLibrary/ubo/scene_ubo.glsl:61-70)
#include "sky/sky_atmosphere.glsl"

// ShaderLibrary/lighting/direct_cook_torrance_pbr.glsl:15
vec3 EvalDirectLighting(LightingInput lighting)
{
    // lighting.mainLightDir / mainLightColor / shadowFactor 已由
    // BuildForwardLightingInput 填好(ShaderLibrary/compositor/forward_lighting.glsl:33-37)
    vec3 L = lighting.mainLightDir;
    ...
}
```

`GetSkyMainLightDir()` / `GetSkyMainLightColor()` 读的就是 `sky.sun_direction` / `sky.sun_color * sky.sun_intensity`(`ShaderLibrary/sky/sky_atmosphere.glsl:16-24`);阴影因子来自 Scene binding 5 的 `ShadowInfo`(`ubo/scene_ubo.glsl:112-121`),由 `float GetShadowFactor(SurfaceInput)` 取(`ShaderLibrary/shadow/pcf_shadow.glsl:133`)。

### 4.4 最小环境光模块：`ambient/constant.glsl`

**就地校正(2026-09)**:没有 `layout(set = 0, binding = 1) uniform AmbientLight`;Scene binding 1 已被 sky 占用,而且**不存在"常量环境光 UBO"**。当前 ambient 不是一个取数的 provider,而是一个**填 `LightingInput` 的模块**:slot `ambient_light_provider`,函数签名 `vec3 EvalIndirectLighting(LightingInput)`。现有实现两例:

```glsl
// ShaderLibrary/lighting/indirect_sky_ambient.glsl:20-30(方向相关天光环境光)
vec3 EvalIndirectLighting(LightingInput lighting)
{
    vec3 sky_light = EvalSkyAtmosphere(normalize(lighting.normal));
    return sky_light * lighting.baseColor
         * (1.0 - lighting.metallic) * lighting.ao;
}
```

即:`ambient.color.rgb` 的来源是 `LightingInput.ambientColor`(`ShaderLibrary/compositor/forward_lighting.glsl:35` 由 `GetSkyAmbientColor()` 填)+ 模块内自己采样天空;`diffuse_irradiance`/`gi_radiance`/`capabilities` 三个字段在当前 `LightingInput` 里不存在,模块间 capability 协商走的是 `@ulre provides_capability` / `ShaderModuleCapability`(`inc/hgl/mtl/ShaderCodeModule.h:142`、`src/ShaderGen/glsl_module/ShaderCodeModuleFile.cpp:198`)。

SH、IBL 或 GI 只需替换本模块。例如 IBL provider 填充 irradiance，并在其 `Resource` block
声明 irradiance cube、prefiltered cube 和 BRDF LUT，同时实现
`SampleAmbientSpecular`；PBR model 消费该能力，Blinn-Phong model 可只消费
diffuse irradiance。

### 4.5 最小 PBR 光照模块：`lighting/pbr.glsl`

```glsl
vec3 EvaluateDirectLighting(
    in SurfaceData surface,
    in FragmentInput input,
    in DirectLightData light)
{
    float ndotl = max(dot(surface.normal_ws, light.direction_ws), 0.0);
    return surface.base_color * light.radiance * ndotl;
}

vec3 EvaluateAmbientLighting(
    in SurfaceData surface,
    in FragmentInput input,
    in AmbientLightData ambient)
{
    return surface.base_color * ambient.diffuse_irradiance;
}
```

这是最小可运行示意，不是完整 Cook-Torrance。完整 PBR 只替换这两个函数；模板、surface、
direct、shadow 和 AO 模块都不变。

> **就地校正(2026-09)**:现实中的 lighting_model 模块只实现**一个**函数——`vec4 EvalLighting(LightingInput)`(`ShaderLibrary/lighting/forward_pbr.glsl:17`、`forward_flat.glsl:16`),不再有 `EvaluateDirectLighting`/`EvaluateAmbientLighting` 两个入口;直接光算法被拆进 `direct_light_provider` 模块的 `EvalDirectLighting(LightingInput)`(Cook-Torrance 实测见 `ShaderLibrary/lighting/direct_cook_torrance_pbr.glsl:15-39`)。

### 4.6 Shadow 与 AO 模块

Shadow 与 AO 均由模板明确要求并提供实际实现。PCF shadow 和 SSAO 分别是独立 module；
SSAO 可先由 fullscreen pass 写入 AO texture，再由 Forward Lit 的 AO provider 采样。
禁止用 `return 1.0` 伪装为满足契约。

> **状态注(2026-09)**:这条**已落地**——`RenderTemplateID` 里就有 `ForwardLitUnshadowedAO`(名 `forward_lit_unshadowed_ao`)与 `ForwardUnlit`,AO "恒等"也有独立模板 `ForwardLitShadowedIdentityAO` 与模块 `ShaderLibrary/ao/identity.glsl`(`float GetAmbientOcclusion(SurfaceInput)`,:12);PCF shadow 已有实现 `ShaderLibrary/shadow/pcf_shadow.glsl`(签名 `float GetShadowFactor(SurfaceInput)`)。**未落地**的是 SSAO:全树 0 命中,也没有 fullscreen AO pass 的证据。

### 4.7 最小输出模块：`output/forward_hdr.glsl`

**就地校正(2026-09)**:输出声明**不是手写的**——它由 ShaderGen 依据 `OutputContract` 生成,形态为 `layout(location=N) out <type> <name>; void WriteMaterialOutput(<type> value) { <name> = value; }`(`src/ShaderGen/template/FragmentTemplateComposer.cpp:271-289`,变量名由 `GetMaterialOutputName(write_semantic_id)` 决定,`inc/hgl/mtl/MaterialOutputContract.h:63-68`)。因此:

- 函数名是 `WriteMaterialOutput(<type> value)`,只有**一个值参数**(不是 `(surface, radiance)`);
- emissive 的合成发生在调用点:`WriteMaterialOutput(HGLComposeColor(finalColor))`(`ShaderLibrary/fragment/forward_lit.glsl.tmpl:17`,合成规则在 `ShaderLibrary/common/alpha_compositor.glsl`);
- 模块例:`ShaderLibrary/compositor/forward_lighting.glsl`(`@ulre slot output_policy`,负责 `LightingInput` 装配)。

设计意图(输出策略可由 `output_policy` slot 替换)成立,只是"`output/forward_hdr.glsl` 这类手写文件"不存在。

## 5. Mesh 模板

Mesh stage 同样采用固定 slot：

```text
VertexInput
  -> LocalDeform
  -> Skinning (optional)
  -> LocalToWorld
  -> VaryingPack
```

最简 static mesh main：

```glsl
void main()
{
    VertexInput vertex = LoadVertexInput(gl_LocalInvocationIndex);
    vec3 local_position = ApplyLocalDeform(vertex);
    WorldVertex world = ApplyLocalToWorld(vertex, local_position);
    WriteMeshVertex(world, vertex);
}
```

> **就地校正(2026-09)**:上面四个函数 + `VertexInput`/`WorldVertex` 全树 0 命中。当前 mesh stage 是 **mesh shader + 三段式顶点模块**:`MeshTemplateEmitter` 生成骨架(拓扑计算、`SetMeshOutputsEXT`、`for (uint vid = gl_LocalInvocationIndex; vid < verts_this_group; vid += {{local_size}}u)` 内展开),顶点数据读取被宏化为 `LoadVertexData()`(它按 Stage1 模块里的 `HGL_UV_LOADER`/`HGL_TRANSFORMID_LOADER` 等宏展开,源数据经 `MeshDrawParams` 行的 BDA 地址读,例:`ShaderLibrary/vertex/s1_uv.glsl:15`、`s1_transform_id.glsl:17`),LocalToWorld 走 `GetL2W()`(`ShaderLibrary/vertex/helpers/orient_world.glsl:13-27`,`return l2w.mats[transformID]`,`l2w` 是 `pc_root.addr_l2w` 的 buffer_reference,见 `ShaderLibrary/common/l2w_ssbo.glsl:15-26`)。真实骨架文本:`ShaderLibrary/mesh/vertex_passthrough.glsl.tmpl`;调度入口:`src/ShaderGen/meshgen/MeshTemplateEmitter.h:63-80`(`EmitMeshTemplateDocument`)。
>
> 骨骼动画(`SkinningMode`/`MatrixPalette`)**未实现**(全树唯一 "skinning" 命中在第三方 README);billboard **已实现**,但形态是 Stage3 顶点模块(`ShaderLibrary/vertex/s3_camera_facing_world.glsl`、`s3_camera_facing_fixed_pixels.glsl`)+ Stage1/2 组合,不是"替换 LocalDeform/LocalToWorld slot"。LineQuad/CharQuad 确实是独立 mesh 模式(`MeshShaderModeLineQuad.h`、`MeshShaderModeCharQuad.h`)。

## 6. ShaderDocument 组装规则

每个模板由 C++ 静态表或后续的受校验 metadata 定义其 slot 顺序：

```text
Version
Extension
Define
Resource
Interface
Module: surface_provider
Module: direct_provider
Module: shadow_provider
Module: ambient_provider
Module: ao_provider
Function: lighting_model
Function: output_policy
MainBody: template main
```

> **状态注(2026-09)**:块种类与顺序**已落地**且与上表一致——`ShaderDocumentBlockKind` = `Version`/`Extension`/`Define`/`Resource`/`Interface`/`Module`/`Function`/`MainBody`/`Raw`(`inc/hgl/mtl/ShaderDocument.h:9-19`),顺序常量 `ShaderDocument.cpp:42-57`(多一个 `Raw`),越序/未知块类型在 `Serialize()` 报 `block-order` 诊断(`ShaderDocument.cpp:115-127`),hash 走 `GetSerializedHash()`(`:182`)。`Module: xxx` 的实际来源是 slot → `#include` 发射(`FragmentTemplateComposer.cpp:128` `ApplyFragmentTemplateSlot` / `:491-515`),`MainBody` 是模板 `.tmpl` 文本块。

生成期必须验证：

1. variant 已在 `FixedPipelineVariantTable` 中注册；
2. 每一个必需 slot 恰好有一个实现；
3. provider 的 descriptor requirements 与 resolved descriptor contract 一致；
4. provider 输出 capability 满足 lighting model 输入 capability；
5. mesh varying contract 满足 fragment input contract；
6. template/module/profile version 和已解析 module ID 参与 stage key；
7. block 顺序、来源和序列化 hash 可复现。

## 7. C++ 职责调整

| 组件 | 调整后职责 |
|---|---|
| `GenericMaterialBuilder` | 解析 recipe/definition 并请求 `FixedPipelineVariantTable`；不再写 lighting/skeleton 路径分支 |
| `FixedPipelineVariantTable` | 声明合法 key、模板、各 slot module ID、pipeline/contract policy |
| `StageTemplateResolver` | 解析 key 并返回完整 recipe 或明确错误 |
| `MaterialShaderEmitter` | 生成已解析的 Material ABI、SSBO、binding/index-table Document fragments |

> **状态注(2026-09)**:
> - `GenericMaterialBuilder`、`MaterialShaderEmitter`、`FragmentTemplateComposer`、`MeshTemplateEmitter`、`ShaderCodeModuleRegistry`、`ShaderCooker` 均已落地(路径见 §0.1);`StageTemplateResolver` 的职责已落在 `SceneRenderTemplateResolver`(`inc/hgl/mtl/SceneRenderTemplateResolver.h:22-37`,含 `Make*Profile()` 与 `ResolveSceneRenderTemplateRequest`)。
> - `FixedPipelineVariantTable` **未落地**(0 命中),落地物是 `RenderTemplateDefinition Templates[]` 静态表。
> - `MaterialShaderEmitter` 那行的"binding/index-table Document fragments"**已过期**:BDA 终态后没有 binding/index table,材质纹理引用改由 `pc_root.addr_texture_references` + `MTL_TEX(i)` 表达(`MaterialShaderEmitter.cpp:306-324`)。
| `FragmentTemplateComposer` | 根据已解析的模板请求将 slots、contract 和 main 写入 Document |
| `MeshTemplateEmitter` | 保留受控 geometry strategy、设备限制和 ABI 验证 |
| `ShaderCodeModuleRegistry` | 校验 module capability、依赖、冲突与资源要求，不负责任意组合策略 |
| ECS/render preparation | 选择完整 template ID 与 module roots；不解释 module 内容 |
| `ShaderCooker` | 遍历 variant table，离线生成全部批准 SPV 和 artifact manifest |

## 8. 实施顺序

1. 定义 profile enums、`FixedShaderVariantKey` 和静态 variant table；先映射现有 Lit、
   Unlit、Sky、Shadow builtin。
2. 为模块 metadata 增加 slot role、输出 capability、输入 capability；复用现有 dependency、
   conflict、resource requirement 校验。
3. 已将 skeleton/default module 选择迁到 variant table，保持 GLSL 文本和
   Document block 顺序。
4. 将 Forward Lit 的 main 固化为本方案的模板；以 byte/key/SPV regression 验证。
5. 迁移 `GetSurfaceLightingConfig` 等 Builder 分支，删除重复模块路径真源。
6. 将 Shadow、Sky、Decal 和独立 PostProcess family 逐一接入。
7. 仅在表项和 cooker manifest 稳定后，再考虑将静态表外部化为 TOML metadata。

> **各条实施状态(2026-09,以当前代码为准)**:
> 1. **部分落地**——静态模板表已有(`RenderTemplate.cpp:74-96`,7 个模板覆盖 Lit/Unlit/Sky/ShadowCaster),但 `FixedShaderVariantKey` 与"profile enums 各轴"没有落地(见 §0.2)。
> 2. **已落地**——模块 metadata 带 `slot_role`/`provided_capabilities`/`required_capabilities`(`inc/hgl/mtl/ShaderCodeModule.h:260-262`),依赖/冲突/资源校验在 `ShaderCodeModuleMetadata.h` 与 `src/ShaderGen/glsl_module/`。
> 3. **已落地**——skeleton/default module 选择真源就是模板表 + `SceneRenderTemplateResolver` 的显式 profile,不再靠 Builder 分支推。
> 4. **部分落地**——Forward Lit 的 main 已固化为 `ShaderLibrary/fragment/forward_lit.glsl.tmpl`,回归 gate 见 §0.1;但"byte/key/SPV 全量 regression"尚未覆盖全部模板。
> 5. **未落地/不成立**——`GetSurfaceLightingConfig` 全树 0 命中,无对应迁移对象。
> 6. **部分落地**——ShadowCaster(`ShadowCasterOpaque`/`ShadowCasterMasked`)与 Sky 已接入;**Decal 与独立 PostProcess family 未见模板登记**(`RenderTemplateID` 里没有)。
> 7. **未落地**——静态表未外部化为 TOML。


## 9. 验收标准

- 对同一 variant key，Document blocks、GLSL、stage key、SPV 和 metadata 可重复；
- 未登记组合、slot 缺失、capability 不匹配、descriptor/interface 不匹配均在生成期失败；
- PBR/IBL、Blinn-Phong/EnvMap、PBR/SH、Shadow Masked、Static/Skinned 等首批批准组合全部
  有 cooker fixture；
- 特例只能通过新增版本化模板或合法 table entry 实现；
- 无需 Shader Graph，也无需为特例修改通用生成器流程。

> **状态注(2026-09)**:逐条对照——第 2 条主要部分**已落地**(slot 缺失 `MissingRequiredSlot`、未登记模板 `UnknownTemplate`、stage/version 不匹配、模块缺失 `ModuleNotFound` 都在 `RenderTemplateValidationError` 里,`inc/hgl/mtl/RenderTemplate.h:109-121`);第 1 条的"可重复"由 `ShaderDocument::GetSerializedHash` + 槽替换的确定性文本填充保证;第 3、4 条**未能按本文的轴粒度核实**(没有 variant key 轴,也没有 IBL/EnvMap/SH 这些轴对应的 cooker fixture 清单;现有材质定义清单见 `ShaderLibrary/material/*.material.toml`);第 5 条与实现一致(无 Shader Graph,特例只能加版本化模板)。
