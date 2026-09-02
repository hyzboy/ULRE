# ShaderGen 受控组合模板技术方案

## 1. 目标与边界

本方案面向固定 Vulkan 前向渲染器，不实现 Shader Graph，不支持任意材质代码改变 shader
控制流。ECS 根据实际渲染状态、材质、场景和质量配置选择完整模板及其 module roots；
ShaderGen 只递归展开依赖、汇总资源、验证并组合为可审计的 `ShaderDocument`，再离线
编译为 SPV。

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

### 4.2 最小表面模块：`surface/pbr_texture.glsl`

```glsl
layout(set = 1, binding = 0) uniform sampler2D base_color_texture;
layout(set = 1, binding = 1) uniform sampler2D material_texture;

SurfaceData GetSurfaceData(in FragmentInput input)
{
    vec4 base = texture(base_color_texture, input.uv0);
    vec4 material = texture(material_texture, input.uv0);

    SurfaceData surface;
    surface.base_color = base.rgb;
    surface.normal_ws = normalize(input.normal_ws);
    surface.metallic = material.b;
    surface.roughness = max(material.g, 0.045);
    surface.emissive = vec3(0.0);
    surface.opacity = base.a;
    return surface;
}
```

RG8/RGBA16F x2 NTB 的差异不应改变 `LightingModel`。它们是不同的
`surface_provider` 或 `normal_decode` module，最终都填写 `surface.normal_ws`。

### 4.3 最小直射光模块：`direct/sun.glsl`

```glsl
layout(set = 0, binding = 0) uniform SunLight
{
    vec4 direction_intensity;
    vec4 color;
} sun;

DirectLightData GetDirectLight(in FragmentInput input)
{
    DirectLightData result;
    result.direction_ws = normalize(-sun.direction_intensity.xyz);
    result.radiance = sun.color.rgb * sun.direction_intensity.w;
    return result;
}
```

### 4.4 最小环境光模块：`ambient/constant.glsl`

```glsl
layout(set = 0, binding = 1) uniform AmbientLight
{
    vec4 color;
} ambient;

AmbientLightData GetAmbientLight(in FragmentInput input)
{
    AmbientLightData result;
    result.diffuse_irradiance = ambient.color.rgb;
    result.gi_radiance = vec3(0.0);
    result.capabilities = 0u;
    return result;
}
```

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

### 4.6 Shadow 与 AO 模块

Shadow 与 AO 均由模板明确要求并提供实际实现。PCF shadow 和 SSAO 分别是独立 module；
SSAO 可先由 fullscreen pass 写入 AO texture，再由 Forward Lit 的 AO provider 采样。
不使用 shadow/AO 的路径应选择不同的版本化模板，例如 `forward_lit_unshadowed` 或
`forward_unlit`，禁止用 `return 1.0` 伪装为满足契约。

### 4.7 最小输出模块：`output/forward_hdr.glsl`

```glsl
layout(location = 0) out vec4 out_color;

void WriteFragmentOutput(in SurfaceData surface, in vec3 radiance)
{
    out_color = vec4(radiance + surface.emissive, surface.opacity);
}
```

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

骨骼动画是 `Skinning` slot 的替换实现；billboard 是 `LocalDeform` 或 `LocalToWorld` slot 的
替换实现；LineQuad/CharQuad 是独立 geometry template。它们不应向 Forward Lit 模板泄漏
特殊分支。

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
| `CompositorAssembler` | 过渡期 façade；长期替换为读取 recipe 的 `FragmentStageComposer` |
| `MeshShaderAssembler` | 保留受控 geometry strategy、设备限制和 ABI 验证 |
| `ShaderCodeModuleRegistry` | 校验 module capability、依赖、冲突与资源要求，不负责任意组合策略 |
| ECS/render preparation | 选择完整 template ID 与 module roots；不解释 module 内容 |
| `ShaderCooker` | 遍历 variant table，离线生成全部批准 SPV 和 artifact manifest |

## 8. 实施顺序

1. 定义 profile enums、`FixedShaderVariantKey` 和静态 variant table；先映射现有 Lit、
   Unlit、Sky、Shadow builtin。
2. 为模块 metadata 增加 slot role、输出 capability、输入 capability；复用现有 dependency、
   conflict、resource requirement 校验。
3. 将 `CompositorAssembler` 中 skeleton/default module 选择迁到 variant table，但保持
   现有 GLSL 文本和 Document block 顺序。
4. 将 Forward Lit 的 main 固化为本方案的模板；以 byte/key/SPV regression 验证。
5. 迁移 `GetSurfaceLightingConfig` 等 Builder 分支，删除重复模块路径真源。
6. 将 Shadow、Sky、Decal 和独立 PostProcess family 逐一接入。
7. 仅在表项和 cooker manifest 稳定后，再考虑将静态表外部化为 TOML metadata。

## 9. 验收标准

- 对同一 variant key，Document blocks、GLSL、stage key、SPV 和 metadata 可重复；
- 未登记组合、slot 缺失、capability 不匹配、descriptor/interface 不匹配均在生成期失败；
- PBR/IBL、Blinn-Phong/EnvMap、PBR/SH、Shadow Masked、Static/Skinned 等首批批准组合全部
  有 cooker fixture；
- 特例只能通过新增版本化模板或合法 table entry 实现；
- 无需 Shader Graph，也无需为特例修改通用生成器流程。
