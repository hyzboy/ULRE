// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   属性元数据表：供检查器 GUI、文档导出、严格键校验与通用格式后端消费。
//   它是「结构长什么样」的**唯一运行期真源**，与格式无关。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <cstddef>

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/MaterialRecipeSpec.h>
#include <hgl/mtl/MaterialDomain.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/VkEnumText.h>

namespace hgl::asset::generated
{
    inline const char *AssetEnumName_VkCullModeFlagBits(const ::VkCullModeFlagBits v)
    { return hgl::graph::mtl::GetVkCullModeName(static_cast<::VkCullModeFlags>(v)); }

    inline bool AssetEnumParse_VkCullModeFlagBits(const char *s, ::VkCullModeFlagBits &out)
    {
        ::VkCullModeFlags tmp = static_cast<::VkCullModeFlags>(out);
        if (!hgl::graph::mtl::ParseVkCullMode(s, tmp)) return false;
        out = static_cast<::VkCullModeFlagBits>(tmp);
        return true;
    }

    inline const char *AssetEnumName_VkCompareOp(const ::VkCompareOp v)
    { return hgl::graph::mtl::GetVkCompareOpName(v); }

    inline bool AssetEnumParse_VkCompareOp(const char *s, ::VkCompareOp &out)
    { return hgl::graph::mtl::ParseVkCompareOp(s, out); }

    inline const char *AssetEnumName_VkBlendFactor(const ::VkBlendFactor v)
    { return hgl::graph::mtl::GetVkBlendFactorName(v); }

    inline bool AssetEnumParse_VkBlendFactor(const char *s, ::VkBlendFactor &out)
    { return hgl::graph::mtl::ParseVkBlendFactor(s, out); }

    inline const char *AssetEnumName_VertexInputMode(const hgl::graph::mtl::VertexInputMode v)
    { return hgl::graph::mtl::GetVertexInputModeName(v); }

    inline bool AssetEnumParse_VertexInputMode(const char *s, hgl::graph::mtl::VertexInputMode &out)
    { return hgl::graph::mtl::ParseVertexInputMode(s, out); }

    inline const char *AssetEnumName_PositionMappingMode(const hgl::graph::mtl::PositionMappingMode v)
    { return hgl::graph::mtl::GetPositionMappingModeName(v); }

    inline bool AssetEnumParse_PositionMappingMode(const char *s, hgl::graph::mtl::PositionMappingMode &out)
    { return hgl::graph::mtl::ParsePositionMappingMode(s, out); }

    inline const char *AssetEnumName_OrientationMode(const hgl::graph::mtl::OrientationMode v)
    { return hgl::graph::mtl::GetOrientationModeName(v); }

    inline bool AssetEnumParse_OrientationMode(const char *s, hgl::graph::mtl::OrientationMode &out)
    { return hgl::graph::mtl::ParseOrientationMode(s, out); }

    inline const char *AssetEnumName_ScaleMode(const hgl::graph::mtl::ScaleMode v)
    { return hgl::graph::mtl::GetScaleModeName(v); }

    inline bool AssetEnumParse_ScaleMode(const char *s, hgl::graph::mtl::ScaleMode &out)
    { return hgl::graph::mtl::ParseScaleMode(s, out); }

    inline const char *AssetEnumName_ProjectionMode(const hgl::graph::mtl::ProjectionMode v)
    { return hgl::graph::mtl::GetProjectionModeName(v); }

    inline bool AssetEnumParse_ProjectionMode(const char *s, hgl::graph::mtl::ProjectionMode &out)
    { return hgl::graph::mtl::ParseProjectionMode(s, out); }

    inline const char *AssetEnumName_MaterialDomain(const hgl::graph::mtl::MaterialDomain v)
    { return hgl::graph::mtl::GetMaterialDomainName(v); }

    inline bool AssetEnumParse_MaterialDomain(const char *s, hgl::graph::mtl::MaterialDomain &out)
    { return hgl::graph::mtl::ParseMaterialDomain(s, out); }

    inline const char *AssetEnumName64_VkCullModeFlagBits(const int64_t v)
    { return AssetEnumName_VkCullModeFlagBits(static_cast<::VkCullModeFlagBits>(v)); }

    inline bool AssetEnumParse64_VkCullModeFlagBits(const char *s, int64_t &out)
    {
        ::VkCullModeFlagBits tmp{};
        if (!AssetEnumParse_VkCullModeFlagBits(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_VkCompareOp(const int64_t v)
    { return AssetEnumName_VkCompareOp(static_cast<::VkCompareOp>(v)); }

    inline bool AssetEnumParse64_VkCompareOp(const char *s, int64_t &out)
    {
        ::VkCompareOp tmp{};
        if (!AssetEnumParse_VkCompareOp(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_VkBlendFactor(const int64_t v)
    { return AssetEnumName_VkBlendFactor(static_cast<::VkBlendFactor>(v)); }

    inline bool AssetEnumParse64_VkBlendFactor(const char *s, int64_t &out)
    {
        ::VkBlendFactor tmp{};
        if (!AssetEnumParse_VkBlendFactor(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_VertexInputMode(const int64_t v)
    { return AssetEnumName_VertexInputMode(static_cast<hgl::graph::mtl::VertexInputMode>(v)); }

    inline bool AssetEnumParse64_VertexInputMode(const char *s, int64_t &out)
    {
        hgl::graph::mtl::VertexInputMode tmp{};
        if (!AssetEnumParse_VertexInputMode(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_PositionMappingMode(const int64_t v)
    { return AssetEnumName_PositionMappingMode(static_cast<hgl::graph::mtl::PositionMappingMode>(v)); }

    inline bool AssetEnumParse64_PositionMappingMode(const char *s, int64_t &out)
    {
        hgl::graph::mtl::PositionMappingMode tmp{};
        if (!AssetEnumParse_PositionMappingMode(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_OrientationMode(const int64_t v)
    { return AssetEnumName_OrientationMode(static_cast<hgl::graph::mtl::OrientationMode>(v)); }

    inline bool AssetEnumParse64_OrientationMode(const char *s, int64_t &out)
    {
        hgl::graph::mtl::OrientationMode tmp{};
        if (!AssetEnumParse_OrientationMode(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_ScaleMode(const int64_t v)
    { return AssetEnumName_ScaleMode(static_cast<hgl::graph::mtl::ScaleMode>(v)); }

    inline bool AssetEnumParse64_ScaleMode(const char *s, int64_t &out)
    {
        hgl::graph::mtl::ScaleMode tmp{};
        if (!AssetEnumParse_ScaleMode(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_ProjectionMode(const int64_t v)
    { return AssetEnumName_ProjectionMode(static_cast<hgl::graph::mtl::ProjectionMode>(v)); }

    inline bool AssetEnumParse64_ProjectionMode(const char *s, int64_t &out)
    {
        hgl::graph::mtl::ProjectionMode tmp{};
        if (!AssetEnumParse_ProjectionMode(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_MaterialDomain(const int64_t v)
    { return AssetEnumName_MaterialDomain(static_cast<hgl::graph::mtl::MaterialDomain>(v)); }

    inline bool AssetEnumParse64_MaterialDomain(const char *s, int64_t &out)
    {
        hgl::graph::mtl::MaterialDomain tmp{};
        if (!AssetEnumParse_MaterialDomain(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    extern const ClassMetaInfo Class_RecipeTextureSpec;
    extern const ClassMetaInfo Class_RecipePipelineSpec;
    extern const ClassMetaInfo Class_RecipeRenderStateSpec;
    extern const ClassMetaInfo Class_RecipeFragmentSpec;
    extern const ClassMetaInfo Class_RecipeTransformSpec;
    extern const ClassMetaInfo Class_MaterialRecipeSpec;

    inline const PropertyMetaInfo Meta_RecipeTextureSpec[] =
    {
        { "texture_name", "name", "texture_name", "Texture", "TOML/GLSL 纹理名（snake_case，如 \"base_color\"）", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::RecipeTextureSpec, texture_name), (uint32_t)sizeof(decltype(hgl::graph::mtl::RecipeTextureSpec::texture_name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "resource_id", "asset", "resource_id", "Texture", "资源标识（路径 / 资产 ID / 逻辑名）", "hgl::serialize::Presence<AnsiString>", nullptr, "tex", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeTextureSpec, resource_id), (uint32_t)sizeof(AnsiString), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "required", "required", "required", "Texture", "缺键取 `true`：缺失应触发显式错误", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeTextureSpec, required), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "layer", "layer", "layer", "Texture", "缺键取 0；非数组纹理必须为 0", "hgl::serialize::Presence<uint32_t>", nullptr, nullptr, PropertyKind::UInt32, PF_EditAnywhere | PF_HasRange | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeTextureSpec, layer), (uint32_t)sizeof(uint32_t), 0u, 0.0, 31.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "auto_mipmaps", "auto_mipmaps", "auto_mipmaps", "Texture", "缺键取 `false`", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeTextureSpec, auto_mipmaps), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_RecipeTextureSpec =
    {
        "RecipeTextureSpec", "", "一条纹理解析声明（`[[textures]]` 表）。纯输入，不含任何运行时句柄。 除 `texture_name`（合并键）外全部是 `Presence<T>`：这既是「配方只写与领域 默认不同的键」的落地方式，也是**继承时逐字段覆盖**的前提 —— 子配方只想换 `asset` 时，父配方给的 `layer` / `auto_mipmaps` 不会被静默丢掉。",
        Meta_RecipeTextureSpec, (uint32_t)(sizeof(Meta_RecipeTextureSpec) / sizeof(Meta_RecipeTextureSpec[0])),
        (uint32_t)sizeof(hgl::graph::mtl::RecipeTextureSpec), (uint32_t)alignof(hgl::graph::mtl::RecipeTextureSpec)
    };

    inline const PropertyMetaInfo Meta_RecipePipelineSpec[] =
    {
        { "cull_mode", "cull_mode", "cull_mode", "Pipeline", "", "hgl::serialize::Presence<VkCullModeFlagBits>", "::VkCullModeFlagBits", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, cull_mode), (uint32_t)sizeof(::VkCullModeFlagBits), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VkCullModeFlagBits, &AssetEnumName64_VkCullModeFlagBits },
        { "depth_test", "depth_test", "depth_test", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, depth_test), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "depth_write", "depth_write", "depth_write", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, depth_write), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "depth_compare_op", "depth_compare_op", "depth_compare_op", "Pipeline", "", "hgl::serialize::Presence<VkCompareOp>", "::VkCompareOp", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, depth_compare_op), (uint32_t)sizeof(::VkCompareOp), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VkCompareOp, &AssetEnumName64_VkCompareOp },
        { "alpha_blend", "alpha_blend", "alpha_blend", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, alpha_blend), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "blend_src", "blend_src", "blend_src", "Pipeline", "", "hgl::serialize::Presence<VkBlendFactor>", "::VkBlendFactor", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, blend_src), (uint32_t)sizeof(::VkBlendFactor), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VkBlendFactor, &AssetEnumName64_VkBlendFactor },
        { "blend_dst", "blend_dst", "blend_dst", "Pipeline", "", "hgl::serialize::Presence<VkBlendFactor>", "::VkBlendFactor", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, blend_dst), (uint32_t)sizeof(::VkBlendFactor), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VkBlendFactor, &AssetEnumName64_VkBlendFactor },
        { "dynamic_line_width", "dynamic_line_width", "dynamic_line_width", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, dynamic_line_width), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "overlay", "overlay", "overlay", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, overlay), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "wireframe", "wireframe", "wireframe", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, wireframe), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "line_width", "line_width", "line_width", "Pipeline", "", "hgl::serialize::Presence<float>", nullptr, nullptr, PropertyKind::Float, PF_EditAnywhere | PF_HasClamp | PF_HasStep | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipePipelineSpec, line_width), (uint32_t)sizeof(float), 0u, 0.0, 64.0, 0.25, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_RecipePipelineSpec =
    {
        "RecipePipelineSpec", "", "`[render_state.pipeline]` 的**逐字段**覆盖规约。 11 个字段全部是 `Presence<T>`：只写与契约默认不同的键，未写的键保留默认值。 这是 `MaterialPipelineField` 位掩码的数据来源（`present` ⇒ 掩码对应位置 1）。",
        Meta_RecipePipelineSpec, (uint32_t)(sizeof(Meta_RecipePipelineSpec) / sizeof(Meta_RecipePipelineSpec[0])),
        (uint32_t)sizeof(hgl::graph::mtl::RecipePipelineSpec), (uint32_t)alignof(hgl::graph::mtl::RecipePipelineSpec)
    };

    inline const PropertyMetaInfo Meta_RecipeRenderStateSpec[] =
    {
        { "alpha_test", "alpha_test", "alpha_test", "RenderState", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeRenderStateSpec, alpha_test), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "alpha_cutoff", "alpha_cutoff", "alpha_cutoff", "RenderState", "", "hgl::serialize::Presence<float>", nullptr, nullptr, PropertyKind::Float, PF_EditAnywhere | PF_HasClamp | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeRenderStateSpec, alpha_cutoff), (uint32_t)sizeof(float), 0u, 0.0, 1.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "dither", "dither", "dither", "RenderState", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeRenderStateSpec, dither), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "pipeline", "pipeline", "pipeline", "RenderState", "", "hgl::serialize::Presence<RecipePipelineSpec>", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeRenderStateSpec, pipeline), (uint32_t)sizeof(hgl::graph::mtl::RecipePipelineSpec), 0u, 0.0, 0.0, 0.0, &Class_RecipePipelineSpec, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_RecipeRenderStateSpec =
    {
        "RecipeRenderStateSpec", "", "`[render_state]` 表：三个开关 + 一份可选的逐字段管线覆盖。",
        Meta_RecipeRenderStateSpec, (uint32_t)(sizeof(Meta_RecipeRenderStateSpec) / sizeof(Meta_RecipeRenderStateSpec[0])),
        (uint32_t)sizeof(hgl::graph::mtl::RecipeRenderStateSpec), (uint32_t)alignof(hgl::graph::mtl::RecipeRenderStateSpec)
    };

    inline const PropertyMetaInfo Meta_RecipeFragmentSpec[] =
    {
        { "surface_module_name", "surface_module_name", "surface_module_name", "Fragment", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::RecipeFragmentSpec, surface_module_name), (uint32_t)sizeof(decltype(hgl::graph::mtl::RecipeFragmentSpec::surface_module_name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "samplers", "samplers", "samplers", "Fragment", "", "hgl::serialize::Presence<hgl::ManagedArray<AnsiString>>", nullptr, nullptr, PropertyKind::PtrArray, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeFragmentSpec, samplers), (uint32_t)sizeof(hgl::ManagedArray<AnsiString>), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::String, nullptr, nullptr },
        { "defines", "defines", "defines", "Fragment", "", "hgl::serialize::Presence<hgl::ManagedArray<AnsiString>>", nullptr, nullptr, PropertyKind::PtrArray, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeFragmentSpec, defines), (uint32_t)sizeof(hgl::ManagedArray<AnsiString>), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::String, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_RecipeFragmentSpec =
    {
        "RecipeFragmentSpec", "", "`[fragment]` 表：表面模块名 + 采样器/宏的**整表替换**。",
        Meta_RecipeFragmentSpec, (uint32_t)(sizeof(Meta_RecipeFragmentSpec) / sizeof(Meta_RecipeFragmentSpec[0])),
        (uint32_t)sizeof(hgl::graph::mtl::RecipeFragmentSpec), (uint32_t)alignof(hgl::graph::mtl::RecipeFragmentSpec)
    };

    inline const PropertyMetaInfo Meta_RecipeTransformSpec[] =
    {
        { "input", "source", "input", "Transform", "", "hgl::serialize::Presence<VertexInputMode>", "hgl::graph::mtl::VertexInputMode", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeTransformSpec, input), (uint32_t)sizeof(hgl::graph::mtl::VertexInputMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VertexInputMode, &AssetEnumName64_VertexInputMode },
        { "position_mapping", "mapping", "position_mapping", "Transform", "", "hgl::serialize::Presence<PositionMappingMode>", "hgl::graph::mtl::PositionMappingMode", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeTransformSpec, position_mapping), (uint32_t)sizeof(hgl::graph::mtl::PositionMappingMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_PositionMappingMode, &AssetEnumName64_PositionMappingMode },
        { "orientation", "orientation", "orientation", "Transform", "", "hgl::serialize::Presence<OrientationMode>", "hgl::graph::mtl::OrientationMode", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeTransformSpec, orientation), (uint32_t)sizeof(hgl::graph::mtl::OrientationMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_OrientationMode, &AssetEnumName64_OrientationMode },
        { "scale", "scale", "scale", "Transform", "", "hgl::serialize::Presence<ScaleMode>", "hgl::graph::mtl::ScaleMode", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeTransformSpec, scale), (uint32_t)sizeof(hgl::graph::mtl::ScaleMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_ScaleMode, &AssetEnumName64_ScaleMode },
        { "projection", "projection", "projection", "Transform", "", "hgl::serialize::Presence<ProjectionMode>", "hgl::graph::mtl::ProjectionMode", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::mtl::RecipeTransformSpec, projection), (uint32_t)sizeof(hgl::graph::mtl::ProjectionMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_ProjectionMode, &AssetEnumName64_ProjectionMode },
    };

    inline const ClassMetaInfo Class_RecipeTransformSpec =
    {
        "RecipeTransformSpec", "", "`[transform]` 表：顶点着色器 PCG 节点配置的逐字段覆盖。",
        Meta_RecipeTransformSpec, (uint32_t)(sizeof(Meta_RecipeTransformSpec) / sizeof(Meta_RecipeTransformSpec[0])),
        (uint32_t)sizeof(hgl::graph::mtl::RecipeTransformSpec), (uint32_t)alignof(hgl::graph::mtl::RecipeTransformSpec)
    };

    inline const PropertyMetaInfo Meta_MaterialRecipeSpec[] =
    {
        { "schema", "schema", "schema", "Identity", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_EditAnywhere | PF_HasRange | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::MaterialRecipeSpec, schema), (uint32_t)sizeof(decltype(hgl::graph::mtl::MaterialRecipeSpec::schema)), 0u, 1.0, 1.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "name", "name", "name", "Identity", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::MaterialRecipeSpec, name), (uint32_t)sizeof(decltype(hgl::graph::mtl::MaterialRecipeSpec::name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "parent", "parent", "parent", "Identity", "", "AnsiString", nullptr, "rcp", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::MaterialRecipeSpec, parent), (uint32_t)sizeof(decltype(hgl::graph::mtl::MaterialRecipeSpec::parent)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "domain", "domain", "domain", "Identity", "", "MaterialDomain", "hgl::graph::mtl::MaterialDomain", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::MaterialRecipeSpec, domain), (uint32_t)sizeof(decltype(hgl::graph::mtl::MaterialRecipeSpec::domain)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_MaterialDomain, &AssetEnumName64_MaterialDomain },
        { "fragment", "fragment", "fragment", "Fragment", "", "RecipeFragmentSpec", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::MaterialRecipeSpec, fragment), (uint32_t)sizeof(decltype(hgl::graph::mtl::MaterialRecipeSpec::fragment)), 0u, 0.0, 0.0, 0.0, &Class_RecipeFragmentSpec, PropertyKind::None, nullptr, nullptr },
        { "transform", "transform", "transform", "Transform", "", "RecipeTransformSpec", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::MaterialRecipeSpec, transform), (uint32_t)sizeof(decltype(hgl::graph::mtl::MaterialRecipeSpec::transform)), 0u, 0.0, 0.0, 0.0, &Class_RecipeTransformSpec, PropertyKind::None, nullptr, nullptr },
        { "render_state", "render_state", "render_state", "RenderState", "", "RecipeRenderStateSpec", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::MaterialRecipeSpec, render_state), (uint32_t)sizeof(decltype(hgl::graph::mtl::MaterialRecipeSpec::render_state)), 0u, 0.0, 0.0, 0.0, &Class_RecipeRenderStateSpec, PropertyKind::None, nullptr, nullptr },
        { "textures", "textures", "textures", "Texture", "", "hgl::ManagedArray<RecipeTextureSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::MaterialRecipeSpec, textures), (uint32_t)sizeof(decltype(hgl::graph::mtl::MaterialRecipeSpec::textures)), 0u, 0.0, 0.0, 0.0, &Class_RecipeTextureSpec, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_MaterialRecipeSpec =
    {
        "MaterialRecipeSpec", "", "一份材质配方文件的完整字段规约。 与运行时的 `MaterialRecipe`（`inc/hgl/mtl/MaterialRecipe.h`）刻意分开： `MaterialRecipe` 带运行时句柄（`TextureAsset*`、SSBO 绑定）与已解析的 `VertexShaderNodeConfig`，不是纯数据也算不出稳定哈希；本结构是纯 POD， 只回答「文件里写了什么」。",
        Meta_MaterialRecipeSpec, (uint32_t)(sizeof(Meta_MaterialRecipeSpec) / sizeof(Meta_MaterialRecipeSpec[0])),
        (uint32_t)sizeof(hgl::graph::mtl::MaterialRecipeSpec), (uint32_t)alignof(hgl::graph::mtl::MaterialRecipeSpec)
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::RecipeTextureSpec *) { return &Class_RecipeTextureSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::RecipePipelineSpec *) { return &Class_RecipePipelineSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::RecipeRenderStateSpec *) { return &Class_RecipeRenderStateSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::RecipeFragmentSpec *) { return &Class_RecipeFragmentSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::RecipeTransformSpec *) { return &Class_RecipeTransformSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::MaterialRecipeSpec *) { return &Class_MaterialRecipeSpec; }
} // namespace hgl::asset::generated
