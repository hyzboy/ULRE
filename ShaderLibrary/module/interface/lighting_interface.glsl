// @ulre begin
// @ulre name lighting_interface
// @ulre kind Shared
// @ulre priority 0
// @ulre end
// Lighting Interface — 定义统一直接光与间接光函数契约
#ifndef LIGHTING_INTERFACE_GLSL
#define LIGHTING_INTERFACE_GLSL

// All values required by a lighting algorithm are resolved before the
// algorithm is called. The source of baseColor, normal, or environment data
// is intentionally not part of this contract.
struct LightingInput
{
    vec3  baseColor;
    vec3  normal;
    vec3  viewDir;
    float metallic;
    float roughness;
    float fresnel;
    float ao;
    vec3  emissive;
    float alpha;

    vec3  mainLightDir;
    vec3  mainLightColor;
    vec3  reflectionColor;

    float shadowFactor;

    // ── 高级材质扩展（Filament 第一梯队 + 简易 Subsurface）──────────────────
    // 全部字段的**基线缺省值 = 0**（由 output_policy 装配处显式写常量 0）：
    // none 槽模块恒返回字面量 0，基线路径上这些字段从不被非零消费，
    // 常量折叠 + DCE 使整条高级链在 SPIR-V 里完全消失 ⇒ 零开销。
    // 数据面（64B 高级行）由阶段二接入 material_source_pbr_advanced。
    float clearCoat;           // 清漆强度 0..1（KHR_materials_clearcoat.clearcoatFactor）
    float clearCoatRoughness;  // 清漆层粗糙度（clearcoatRoughnessFactor）
    vec3  sheenColor;          // 绒面着色（KHR_materials_sheen.sheenColorFactor）
    float sheenRoughness;      // 绒面粗糙度（sheenRoughnessFactor）
    float anisotropy;          // 各向异性强度 0..1（KHR_materials_anisotropy.anisotropyStrength）
    float subsurfacePower;     // 简易 SSS wrap 权重 0..1（>0 时启用 wrap 漫反射）
    float thickness;           // 薄壳透射厚度 0..1（背散射强度）
};

// A lighting algorithm consumes only LightingInput and returns final RGBA.
//   vec4 EvalLighting(LightingInput lighting)

// ── 直接光可插槽契约（阶段一：diffuse / specular 拆分）───────────────────────
// 直接光被拆成两个**互相独立**的可插拔槽：
//   diffuse_light_provider  → vec3 EvalDiffuseLight (LightingInput lighting)
//   specular_light_provider → vec3 EvalSpecularLight(LightingInput lighting)
// 两者都只返回**未调制**的辐射项（不含 mainLightColor / shadowFactor）；
// 由 LightingModel 槽负责 `(diffuse + specular) * color * shadow` 的调制。
// 这样“换漫反射模型不影响高光模型”、"无高光"（specular_none 返回 0）都能
// 只换一个 include 实现，主着色器里不出现任何 #if。
vec3 EvalDiffuseLight(LightingInput lighting);
vec3 EvalSpecularLight(LightingInput lighting);

// ── 高级光可插槽契约（阶段一：ClearCoat / Sheen / Subsurface）───────────────
// 与 diffuse/specular 槽同构：每个槽返回**未调制**的辐射项（不含
// mainLightColor / shadowFactor），由 LightingModel 统一调制与叠加。
// 基线绑定的 *_none 模块恒返回字面量 vec3(0)，常量折叠 + DCE 后
// 主着色器与"没有这些槽"完全等价 —— 无 #ifdef、零运行时开销。
//   clearcoat_light_provider    → vec3 EvalClearCoatLight   (LightingInput lighting)
//   sheen_light_provider        → vec3 EvalSheenLight       (LightingInput lighting)
//   subsurface_light_provider   → vec3 EvalSubsurfaceLight  (LightingInput lighting)
vec3 EvalClearCoatLight(LightingInput lighting);
vec3 EvalSheenLight(LightingInput lighting);
vec3 EvalSubsurfaceLight(LightingInput lighting);

// 光照基础 Math 辅助库 —— 算法与 Google Filament 对齐
// （参考 J:\Filament\filament\shaders\src\surface_brdf.fs）

// 色度常数
#define ULRE_PI 3.141592653589793

// 基础 GGX 分布（alpha = perceptualRoughness^2）。
float ULRE_LIT_D_GGX(float NdotH, float alpha2)
{
    float d = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (ULRE_PI * d * d + 1e-7);
}

// D_GGX —— Filament surface_brdf.fs:
// "Walter et al. 2007, Microfacet Models for Refraction through Rough Surfaces"
// 用拉格朗日恒等式 ||N x H||^2 = 1 - NoH^2 直接求 oneMinusNoHSquared，
// 避免高光处 NoH^2≈1 的浮点消减；除零由分母 clamp + 结果上限防护。
// 入参 alpha = perceptualRoughness * perceptualRoughness。
float ULRE_LIT_D_GGX_Correlated(float NoH, float alpha, vec3 NxH)
{
    float oneMinusNoHSquared = max(dot(NxH, NxH), 0.0);
    float a = NoH * alpha;
    float k = min(alpha / (oneMinusNoHSquared + a * a), 453.5); // 453.5 防 fp16 溢出
    float d = k * (k * (1.0 / ULRE_PI));
    return d;
}

// V_SmithGGXCorrelated —— Filament surface_brdf.fs:
// "Heitz 2014, Understanding the Masking-Shadowing Function in
//  Microfacet-Based BRDFs"（height-correlated Smith-GGX）
// 直接返回 V = G / (4 NoV NoL)，替代旧的 G 分离 + 4NoVNoL 除法（消掠射角奇点）。
// 入参 alpha = roughness^2（roughness 为 perceptualRoughness）。
float ULRE_LIT_V_SmithGGXCorrelated(float alpha, float NoV, float NoL)
{
    float a2 = alpha * alpha;
    float lambdaV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
    float lambdaL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
    return 0.5 / max(lambdaV + lambdaL, 1e-7);
}

// 兼容保留：旧的 G_Smith（可分离近似）。新代码请用 ULRE_LIT_V_SmithGGXCorrelated。
float ULRE_LIT_G_Smith(float NdotV, float NdotL, float roughness)
{
    float k  = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k + 1e-7);
    float gl = NdotL / (NdotL * (1.0 - k) + k + 1e-7);
    return gv * gl;
}

// F_Schlick（F90 = 1，电介质掠射全反近似）。
vec3 ULRE_LIT_F_Schlick(float VdotH, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// F_Schlick 带 F90 —— Filament surface_brdf.fs:
// "Schlick 1994, An Inexpensive BRDF Model for Physically-Based Rendering"
// f0 + (f90 - f0) * (1 - VoH)^5；pow5 用三次平方（更快且与 Filament 同形）。
vec3 ULRE_LIT_F_Schlick_F90(vec3 F0, float F90, float VoH)
{
    float f = 1.0 - VoH;
    f = f * f;
    f = f * f * f;
    return F0 + (F90 - F0) * f;
}

// 电介质 F0 推导（Filament computeDielectricF0）：f0 = 0.16 * reflectance^2。
// reflectance = 0.5（IOR 1.5 空气界面）时 f0 = 0.04。
float ULRE_LIT_DielectricF0(float reflectance)
{
    return 0.16 * reflectance * reflectance;
}

// 解析 DFG 拟合（Karis 2013，UE4 Real-Shading 课程；免 LUT 版积分）。
// 返回 vec2(scale, offset) = (A+B, B)，满足 Filament 同构语义：
//   specularDFG = mix(dfg.xxx, dfg.yyy, F0) = F0*(A+B) + (1-F0)*B = F0*A + B
//   energyCompensation = 1 + F0 * (1/dfg.y - 1)
// 对应 f90 = 1 情形（glTF/Filament 默认）。
vec2 ULRE_LIT_EnvDFGApprox(float NoV, float perceptualRoughness)
{
    // [Karis 2013, "Real Shading in Unreal Engine 4", SIGGRAPH PBR course]
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    float r = clamp(perceptualRoughness, 0.045, 1.0);
    vec4 t = r * c0 + c1;
    float a004 = min(t.x * t.x, exp2(-9.28 * NoV)) * t.x + t.y;
    vec2 AB = vec2(-1.04, 1.04) * a004 + t.zw; // (A, B)
    // 转成 (scale=A+B, offset=B)，与 Filament dfg.xy 的 mix 约定对齐
    vec2 dfg = vec2(AB.x + AB.y, AB.y);
    dfg.y = max(dfg.y, 1e-4); // 能量补偿除零防护
    return dfg;
}

// 掠射角 F90 修正（Filament 约定 f90 = clamp(50 * reflectance * f0, 0, 1)；
// f0 = 0.16 reflectance^2 ⇒ f90 = 8 reflectance^3）。
float ULRE_LIT_F90(float reflectance, vec3 F0)
{
    return clamp(dot(F0, vec3(50.0 * reflectance)) / 3.0, 0.0, 1.0);
}

// ── 高级槽共用 Math（Filament 对齐）─────────────────────────────────────────

// F_Schlick 标量版（Filament surface_brdf.fs 同名函数）：清漆层用。
float ULRE_LIT_F_Schlick_S(float F0, float F90, float VoH)
{
    float f = 1.0 - VoH;
    f = f * f;
    f = f * f * f;
    return F0 + (F90 - F0) * f;
}

// SchlickGGX 可见度近似（Filament shading_clearcoat.fs 的 SchlickGGX 同形）：
// GGX1 = a / (NoX*(1-a) + a)；清漆层固定 a = 0.045。
float ULRE_LIT_SchlickGGX_S(float NdotX, float a)
{
    return a / (NdotX * (1.0 - a) + a);
}

// V_Kelemen —— "Kelemen and Szirmay-Kalos 2001,
// A Microfacet Based Coupled Specular-Matte BRDF Model with Importance Sampling"
// （Filament visibilityClearCoat 同款：0.25 / (LoH * LoH)）
float ULRE_LIT_V_Kelemen(float LoH)
{
    return 0.25 / max(LoH * LoH, 1e-6);
}

// D_Charlie —— "Estevez and Kulla 2017, Production Friendly Microfacet Sheen BRDF"
// （Filament shading_sheen / surface_brdf.fs 同款归一化 Charlie 分布）
float ULRE_LIT_D_Charlie(float NoH, float roughness)
{
    float r = clamp(roughness, 0.045, 1.0);
    float invAlpha = 1.0 / r;
    float cos2h = NoH * NoH;
    float sin2h = max(1.0 - cos2h, 0.0078125);
    return (2.0 + invAlpha) * pow(sin2h, invAlpha * 0.5) / (2.0 * ULRE_PI);
}

// V_Neubelt —— "Neubelt and Pettineo 2013, Crafting a Next-gen Material Pipeline for The Order: 1886"
// （Filament visibilityCloth 同款：1 / (4 * (NoL + NoV - NoL * NoV))）
float ULRE_LIT_V_Neubelt(float NoV, float NoL)
{
    return 1.0 / max(4.0 * (NoL + NoV - NoL * NoV), 1e-5);
}

// 绒面能量损失 sheenEnergyLoss —— Filament shading_sheen.fs 的
// energy-normalization 多项式拟合（Dupuy 2020 表项）。
float ULRE_LIT_SheenEnergyLoss(float NoV, float NoL, float alpha)
{
    float f0 = NoV * NoL * (NoV + NoL);
    float e = ((-83.1736 * alpha + 131.59) * alpha + 17.896) * alpha - 13.645;
    float f = exp2(max(e, -12.0)) * f0 + 1.0 - f0;
    return clamp(1.0 - f, 0.0, 1.0);
}

// 稳定正交切线（各向异性模块在缺少真实切线基时的缺省推导）：
// 取与 N 最不正交的轴做 Gram-Schmidt，得到处处连续（极点除外）的 T。
vec3 ULRE_LIT_StableTangent(vec3 N)
{
    vec3 axis = abs(N.z) < 0.99
        ? vec3(0.0, 0.0, 1.0)
        : vec3(1.0, 0.0, 0.0);
    return normalize(cross(axis, N));
}

#endif // LIGHTING_INTERFACE_GLSL
