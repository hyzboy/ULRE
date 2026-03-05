/// ShaderComposition_Examples.h — 合成着色器系统使用示例
///
/// 对比：
///   [旧方式] FixedMaterialDef（S_PureColor3D.h）
///     - 开发者手写完整 VS/FS GLSL
///     - 手工处理坐标变换、输出合成
///     - 每种光照模式都要手写一份
///
///   [新方式] ComposedMaterialDef（本文件）
///     - 开发者只写 VertexShaderBusiness + FragmentShaderBusiness
///     - 框架自动处理：L2W 变换、坐标投影、光照计算、输出合成
///     - 一套业务代码支持多种光照/输出 permutation

#pragma once

#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/vk/VKRenderAssign.h>

namespace hgl::graph::mtl {

// ═════════════════════════════════════════════════════════════════════════════
// 例 1：PureColor3D（单纯颜色，无光照）
// ═════════════════════════════════════════════════════════════════════════════

/**
 * PureColor3D 业务：
 *   - VS：输出顶点位置（自动处理 L2W + VP 变换）
 *   - FS：输出纯色（不计算光照）
 *   - 输出模式：单 RT + Alpha Blend
 */

// 业务顶点着色器：只关心 local 坐标，框架负责世界坐标和投影
constexpr const char EX_PURE_COLOR_3D_VS_BUSINESS[] = R"(
    vec4 VertexShaderBusiness(const VertexInput vi) {
        // 返回 local 坐标，框架会自动：
        //   1. 乘以 LocalToWorld 矩阵 → 世界坐标
        //   2. 乘以 Camera VP 矩阵 → clip space
        //   3. 插值到 FS
        return vec4(vi.Position, 1.0);
    }
)";

// 业务片元着色器：只关心业务逻辑，框架负责 Alpha 合成
constexpr const char EX_PURE_COLOR_3D_FS_BUSINESS[] = R"(
    vec4 FragmentShaderBusiness(const VS_Output vso) {
        // 从材质实例读取颜色
        return MaterialData.Color;  // vec4(R, G, B, A)
    }
)";

// 材质实例数据（只有颜色）
constexpr const char EX_PURE_COLOR_3D_MI_GLSL[] = R"(
    struct MaterialInstance {
        vec4 Color;
    };
)";

// 顶点输入（position + instance ID）
constexpr FixedVertexEntry EX_PURE_COLOR_3D_VERTEX[] = {
    {VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, "Position"},
};

// 描述符（视图矩阵、相机矩阵、L2W矩阵、材质数据）
constexpr FixedDescriptorEntry EX_PURE_COLOR_3D_DESCRIPTORS[] = {
    {DescriptorSetType::RenderTarget, DescriptorKind::UBO,  ShaderGenStageAllGraphics, "viewport", "ViewportInfo", nullptr},
    {DescriptorSetType::Camera,       DescriptorKind::UBO,  ShaderGenStageAllGraphics, "camera",   "CameraInfo",   nullptr},
    {DescriptorSetType::PerFrame,     DescriptorKind::UBO,  ShaderGenStageAllGraphics, "l2w",      "LocalToWorld", nullptr},
    {DescriptorSetType::PerMaterial,  DescriptorKind::SSBO, ShaderGenStageVertex,      "mtl",      "MaterialInstanceData", nullptr},
};

constexpr VertexShaderBusiness EX_PURE_COLOR_3D_VERTEX_BUSINESS { EX_PURE_COLOR_3D_VS_BUSINESS };
constexpr FragmentShaderBusiness EX_PURE_COLOR_3D_FRAGMENT_BUSINESS { EX_PURE_COLOR_3D_FS_BUSINESS };

// 合成定义（替代 FixedMaterialDef）
const ComposedMaterialDef EX_PURE_COLOR_3D_COMPOSED {
    .name = "PureColor3D",
    .primitive_type = PrimitiveType::Triangles,
    .vertex_entries = EX_PURE_COLOR_3D_VERTEX,
    .vertex_entry_count = 1,
    .descriptor_entries = EX_PURE_COLOR_3D_DESCRIPTORS,
    .descriptor_entry_count = 4,
    .vertex_business = &EX_PURE_COLOR_3D_VERTEX_BUSINESS,
    .fragment_business = &EX_PURE_COLOR_3D_FRAGMENT_BUSINESS,
    .output_mode = ShaderOutputMode::SingleRTAlphaBlend,
    .enable_lighting = false,  // 无光照
    .mi_glsl_codes = EX_PURE_COLOR_3D_MI_GLSL,
    .mi_struct_bytes = sizeof(float) * 4,  // Color
};

// ═════════════════════════════════════════════════════════════════════════════
// 例 2：BasicLit（带简单光照的材质）
// ═════════════════════════════════════════════════════════════════════════════

/**
 * BasicLit 业务：
 *   - VS：输出世界坐标法线（框架负责坐标和投影）
 *   - FS：调用 lighting = ComputeLighting(normal, albedo, view_dir)
 *          输出 luminance = albedo * (lighting.diffuse + lighting.specular)
 *   - 输出模式：单 RT + Alpha Blend
 *   - 光照模式：由 ShaderPermutationKey 决定（Lambertian / PBR / Cel Shading）
 */

constexpr const char EX_BASIC_LIT_VS_BUSINESS[] = R"(
    // VS 只处理顶点变换和法线计算
    // 框架会自动 pack 法线进 VS_Output
    vec4 VertexShaderBusiness(const VertexInput vi) {
        return vec4(vi.Position, 1.0);
    }
    
    vec3 GetWorldNormal(const VertexInput vi) {
        // 从 local normal → world normal (框架负责法线矩阵)
        return normalize((NormalMatrix * vi.Normal).xyz);
    }
)";

constexpr const char EX_BASIC_LIT_FS_BUSINESS[] = R"(
    // FS 处理贴图采样 + 光照合成
    vec4 FragmentShaderBusiness(const VS_Output vso) {
        // 获取表面属性
        vec3 albedo = texture(BaseColorMap, vso.TexCoord).rgb;
        vec3 normal = normalize(vso.WorldNormal);
        
        // 框架自动计算光照（根据 ShaderPermutationKey）
        LightingOutput lighting = ComputeLighting(
            normal,
            albedo,
            normalize(CameraPos - vso.WorldPos)
        );
        
        // 合成最终颜色
        vec3 finalColor = albedo * (lighting.diffuse + lighting.specular);
        return vec4(finalColor, MaterialData.Alpha);
    }
)";

constexpr const char EX_BASIC_LIT_MI_GLSL[] = R"(
    struct MaterialInstance {
        float Alpha;
        float Metallic;
        float Roughness;
    };
)";

constexpr FixedVertexEntry EX_BASIC_LIT_VERTEX[] = {
    {VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, "Position"},
    {VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, "Normal"},
    {VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex, "TexCoord"},
};

constexpr FixedDescriptorEntry EX_BASIC_LIT_DESCRIPTORS[] = {
    {DescriptorSetType::RenderTarget, DescriptorKind::UBO,            ShaderGenStageAllGraphics, "viewport", "ViewportInfo", nullptr},
    {DescriptorSetType::Camera,       DescriptorKind::UBO,            ShaderGenStageAllGraphics, "camera",   "CameraInfo",   nullptr},
    {DescriptorSetType::PerFrame,     DescriptorKind::UBO,            ShaderGenStageAllGraphics, "l2w",      "LocalToWorld", nullptr},
    {DescriptorSetType::PerFrame,     DescriptorKind::UBO,            ShaderGenStageAllGraphics, "light",    "LightData",    nullptr},
    {DescriptorSetType::PerMaterial,  DescriptorKind::Texture,        ShaderGenStageFragment, "BaseColorMap", nullptr, "sampler2D"},
    {DescriptorSetType::PerMaterial,  DescriptorKind::TextureSampler, ShaderGenStageFragment, "LinearSampler", nullptr, "sampler2D"},
    {DescriptorSetType::PerMaterial,  DescriptorKind::SSBO,           ShaderGenStageVertex,   "mtl",      "MaterialInstanceData", nullptr},
};

constexpr VertexShaderBusiness EX_BASIC_LIT_VERTEX_BUSINESS { EX_BASIC_LIT_VS_BUSINESS };
constexpr FragmentShaderBusiness EX_BASIC_LIT_FRAGMENT_BUSINESS { EX_BASIC_LIT_FS_BUSINESS };

const ComposedMaterialDef EX_BASIC_LIT_COMPOSED {
    .name = "BasicLit",
    .primitive_type = PrimitiveType::Triangles,
    .vertex_entries = EX_BASIC_LIT_VERTEX,
    .vertex_entry_count = 3,
    .descriptor_entries = EX_BASIC_LIT_DESCRIPTORS,
    .descriptor_entry_count = 7,
    .vertex_business = &EX_BASIC_LIT_VERTEX_BUSINESS,
    .fragment_business = &EX_BASIC_LIT_FRAGMENT_BUSINESS,
    .output_mode = ShaderOutputMode::SingleRTAlphaBlend,
    .enable_lighting = true,  // 启用光照（由 ShaderPermutationKey 决定具体算法）
    .mi_glsl_codes = EX_BASIC_LIT_MI_GLSL,
    .mi_struct_bytes = sizeof(float) * 3,  // Alpha, Metallic, Roughness
};

// ═════════════════════════════════════════════════════════════════════════════
// 例 3：前向渲染 vs. 延迟渲染自动切换
// ═════════════════════════════════════════════════════════════════════════════

/**
 * 场景：BasicLit 同时支持前向渲染（Forward）和延迟渲染（Deferred）
 *
 * 原理：
 *   - 开发者代码完全相同（VertexShaderBusiness + FragmentShaderBusiness）
 *   - 框架根据 RenderPass 信息生成不同的 FS output
 *
 * 前向渲染（输出到单 RT）：
 *   finalColor = albedo * (lighting.diffuse + lighting.specular)
 *   出力：RT[0] = vec4(finalColor, alpha)
 *
 * 延迟渲染（输出到 G-Buffer）：
 *   设置 output_mode = ShaderOutputMode::DualRTDeferred 后：
 *   出力：RT[0] = vec4(albedo, 1.0)         // Diffuse G-Buffer
 *        RT[1] = vec4(normal_encoded, mat_id)  // Normal + Material ID
 *        RT[2] = vec4(roughness, metallic, 0, 0)  // PBR 参数
 *
 * CompostionGenerator 会根据 output_mode 自动选择。
 */

// ═════════════════════════════════════════════════════════════════════════════
// 例 4：特效材质（Additive 混合）
// ═════════════════════════════════════════════════════════════════════════════

constexpr const char EX_FX_EMISSION_VS_BUSINESS[] = R"(
    vec4 VertexShaderBusiness(const VertexInput vi) {
        vec3 local_pos = vi.Position;
        local_pos += MaterialData.OffsetX * normalize(cross(vec3(0,1,0), vi.Normal));
        local_pos += MaterialData.OffsetY * vec3(0,1,0);
        return vec4(local_pos, 1.0);
    }
)";

constexpr const char EX_FX_EMISSION_FS_BUSINESS[] = R"(
    vec4 FragmentShaderBusiness(const VS_Output vso) {
        vec4 tex = texture(EmissionMap, vso.TexCoord);
        // 框架会自动转 Additive 输出：out = src + dst
        return tex * MaterialData.EmissionIntensity;
    }
)";

constexpr const char EX_FX_EMISSION_MI_GLSL[] = R"(
    struct MaterialInstance {
        float EmissionIntensity;
        float OffsetX;
        float OffsetY;
    };
)";

constexpr VertexShaderBusiness EX_FX_EMISSION_VERTEX_BUSINESS { EX_FX_EMISSION_VS_BUSINESS };
constexpr FragmentShaderBusiness EX_FX_EMISSION_FRAGMENT_BUSINESS { EX_FX_EMISSION_FS_BUSINESS };

const ComposedMaterialDef EX_FX_EMISSION_COMPOSED {
    .name = "FXEmission",
    .primitive_type = PrimitiveType::Triangles,
    .vertex_entries = EX_BASIC_LIT_VERTEX,
    .vertex_entry_count = 3,
    .descriptor_entries = EX_BASIC_LIT_DESCRIPTORS,
    .descriptor_entry_count = 7,
    .vertex_business = &EX_FX_EMISSION_VERTEX_BUSINESS,
    .fragment_business = &EX_FX_EMISSION_FRAGMENT_BUSINESS,
    .output_mode = ShaderOutputMode::SingleRTAdditive,  // 加式输出！
    .enable_lighting = false,  // 特效不需要光照
    .mi_glsl_codes = EX_FX_EMISSION_MI_GLSL,
    .mi_struct_bytes = sizeof(float) * 3,
};

}  // namespace hgl::graph::mtl

