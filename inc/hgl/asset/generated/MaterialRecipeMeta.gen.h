// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   属性元数据表：供检查器 GUI、文档导出、严格键校验与通用格式后端消费。
//   它是「结构长什么样」的**唯一运行期真源**，与格式无关。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

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
        { "texture_name", "name", "texture_name", "Texture", "TOML/GLSL 纹理名（snake_case，如 \"base_color\"）", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "resource_id", "asset", "resource_id", "Texture", "资源标识（路径 / 资产 ID / 逻辑名）", "AnsiString", nullptr, "tex", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "required", "required", "required", "Texture", "true 时缺失应触发显式错误", "bool", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 80u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "layer", "layer", "layer", "Texture", "Texture2DArray 层号；非数组纹理必须为 0", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_EditAnywhere | PF_HasRange | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 84u, 4u, 0u, 0.0, 31.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "auto_mipmaps", "auto_mipmaps", "auto_mipmaps", "Texture", "源无 mip 链时是否自动生成", "bool", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 88u, 8u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_RecipeTextureSpec =
    {
        "RecipeTextureSpec", "", "一条纹理解析声明（`[[textures]]` 表）。纯输入，不含任何运行时句柄。",
        Meta_RecipeTextureSpec, (uint32_t)(sizeof(Meta_RecipeTextureSpec) / sizeof(Meta_RecipeTextureSpec[0])),
        96u, 8u
    };

    inline const PropertyMetaInfo Meta_RecipePipelineSpec[] =
    {
        { "cull_mode", "cull_mode", "cull_mode", "Pipeline", "", "hgl::serialize::Presence<VkCullModeFlagBits>", "::VkCullModeFlagBits", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 0u, (uint32_t)sizeof(::VkCullModeFlagBits), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VkCullModeFlagBits, &AssetEnumName64_VkCullModeFlagBits },
        { "depth_test", "depth_test", "depth_test", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 8u, (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "depth_write", "depth_write", "depth_write", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 10u, (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "depth_compare_op", "depth_compare_op", "depth_compare_op", "Pipeline", "", "hgl::serialize::Presence<VkCompareOp>", "::VkCompareOp", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 12u, (uint32_t)sizeof(::VkCompareOp), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VkCompareOp, &AssetEnumName64_VkCompareOp },
        { "alpha_blend", "alpha_blend", "alpha_blend", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 20u, (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "blend_src", "blend_src", "blend_src", "Pipeline", "", "hgl::serialize::Presence<VkBlendFactor>", "::VkBlendFactor", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 24u, (uint32_t)sizeof(::VkBlendFactor), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VkBlendFactor, &AssetEnumName64_VkBlendFactor },
        { "blend_dst", "blend_dst", "blend_dst", "Pipeline", "", "hgl::serialize::Presence<VkBlendFactor>", "::VkBlendFactor", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 32u, (uint32_t)sizeof(::VkBlendFactor), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VkBlendFactor, &AssetEnumName64_VkBlendFactor },
        { "dynamic_line_width", "dynamic_line_width", "dynamic_line_width", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 40u, (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "overlay", "overlay", "overlay", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 42u, (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "wireframe", "wireframe", "wireframe", "Pipeline", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 44u, (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "line_width", "line_width", "line_width", "Pipeline", "", "hgl::serialize::Presence<float>", nullptr, nullptr, PropertyKind::Float, PF_EditAnywhere | PF_HasClamp | PF_HasStep | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 48u, (uint32_t)sizeof(float), 0u, 0.0, 64.0, 0.25, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_RecipePipelineSpec =
    {
        "RecipePipelineSpec", "", "`[render_state.pipeline]` 的**逐字段**覆盖规约。 11 个字段全部是 `Presence<T>`：只写与契约默认不同的键，未写的键保留默认值。 这是 `MaterialPipelineField` 位掩码的数据来源（`present` ⇒ 掩码对应位置 1）。",
        Meta_RecipePipelineSpec, (uint32_t)(sizeof(Meta_RecipePipelineSpec) / sizeof(Meta_RecipePipelineSpec[0])),
        56u, 4u
    };

    inline const PropertyMetaInfo Meta_RecipeRenderStateSpec[] =
    {
        { "alpha_test", "alpha_test", "alpha_test", "RenderState", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 0u, (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "alpha_cutoff", "alpha_cutoff", "alpha_cutoff", "RenderState", "", "hgl::serialize::Presence<float>", nullptr, nullptr, PropertyKind::Float, PF_EditAnywhere | PF_HasClamp | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 4u, (uint32_t)sizeof(float), 0u, 0.0, 1.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "dither", "dither", "dither", "RenderState", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 12u, (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "pipeline", "pipeline", "pipeline", "RenderState", "", "hgl::serialize::Presence<RecipePipelineSpec>", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 16u, (uint32_t)sizeof(hgl::graph::mtl::RecipePipelineSpec), 0u, 0.0, 0.0, 0.0, &Class_RecipePipelineSpec, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_RecipeRenderStateSpec =
    {
        "RecipeRenderStateSpec", "", "`[render_state]` 表：三个开关 + 一份可选的逐字段管线覆盖。",
        Meta_RecipeRenderStateSpec, (uint32_t)(sizeof(Meta_RecipeRenderStateSpec) / sizeof(Meta_RecipeRenderStateSpec[0])),
        76u, 4u
    };

    inline const PropertyMetaInfo Meta_RecipeFragmentSpec[] =
    {
        { "surface_module_name", "surface_module_name", "surface_module_name", "Fragment", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "samplers", "samplers", "samplers", "Fragment", "", "hgl::serialize::Presence<hgl::ManagedArray<AnsiString>>", nullptr, nullptr, PropertyKind::PtrArray, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 40u, (uint32_t)sizeof(hgl::ManagedArray<AnsiString>), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::String, nullptr, nullptr },
        { "defines", "defines", "defines", "Fragment", "", "hgl::serialize::Presence<hgl::ManagedArray<AnsiString>>", nullptr, nullptr, PropertyKind::PtrArray, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 72u, (uint32_t)sizeof(hgl::ManagedArray<AnsiString>), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::String, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_RecipeFragmentSpec =
    {
        "RecipeFragmentSpec", "", "`[fragment]` 表：表面模块名 + 采样器/宏的**整表替换**。",
        Meta_RecipeFragmentSpec, (uint32_t)(sizeof(Meta_RecipeFragmentSpec) / sizeof(Meta_RecipeFragmentSpec[0])),
        104u, 8u
    };

    inline const PropertyMetaInfo Meta_RecipeTransformSpec[] =
    {
        { "input", "source", "input", "Transform", "", "hgl::serialize::Presence<VertexInputMode>", "hgl::graph::mtl::VertexInputMode", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 0u, (uint32_t)sizeof(hgl::graph::mtl::VertexInputMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VertexInputMode, &AssetEnumName64_VertexInputMode },
        { "position_mapping", "mapping", "position_mapping", "Transform", "", "hgl::serialize::Presence<PositionMappingMode>", "hgl::graph::mtl::PositionMappingMode", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 2u, (uint32_t)sizeof(hgl::graph::mtl::PositionMappingMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_PositionMappingMode, &AssetEnumName64_PositionMappingMode },
        { "orientation", "orientation", "orientation", "Transform", "", "hgl::serialize::Presence<OrientationMode>", "hgl::graph::mtl::OrientationMode", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 4u, (uint32_t)sizeof(hgl::graph::mtl::OrientationMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_OrientationMode, &AssetEnumName64_OrientationMode },
        { "scale", "scale", "scale", "Transform", "", "hgl::serialize::Presence<ScaleMode>", "hgl::graph::mtl::ScaleMode", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 6u, (uint32_t)sizeof(hgl::graph::mtl::ScaleMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_ScaleMode, &AssetEnumName64_ScaleMode },
        { "projection", "projection", "projection", "Transform", "", "hgl::serialize::Presence<ProjectionMode>", "hgl::graph::mtl::ProjectionMode", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, 8u, (uint32_t)sizeof(hgl::graph::mtl::ProjectionMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_ProjectionMode, &AssetEnumName64_ProjectionMode },
    };

    inline const ClassMetaInfo Class_RecipeTransformSpec =
    {
        "RecipeTransformSpec", "", "`[transform]` 表：顶点着色器 PCG 节点配置的逐字段覆盖。",
        Meta_RecipeTransformSpec, (uint32_t)(sizeof(Meta_RecipeTransformSpec) / sizeof(Meta_RecipeTransformSpec[0])),
        10u, 1u
    };

    inline const PropertyMetaInfo Meta_MaterialRecipeSpec[] =
    {
        { "schema", "schema", "schema", "Identity", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_EditAnywhere | PF_HasRange | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 8u, 0u, 1.0, 1.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "name", "name", "name", "Identity", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "parent", "parent", "parent", "Identity", "", "AnsiString", nullptr, "rcp", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 48u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "domain", "domain", "domain", "Identity", "", "MaterialDomain", "hgl::graph::mtl::MaterialDomain", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 88u, 8u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_MaterialDomain, &AssetEnumName64_MaterialDomain },
        { "fragment", "fragment", "fragment", "Fragment", "", "RecipeFragmentSpec", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 96u, 104u, 0u, 0.0, 0.0, 0.0, &Class_RecipeFragmentSpec, PropertyKind::None, nullptr, nullptr },
        { "transform", "transform", "transform", "Transform", "", "RecipeTransformSpec", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 200u, 12u, 0u, 0.0, 0.0, 0.0, &Class_RecipeTransformSpec, PropertyKind::None, nullptr, nullptr },
        { "render_state", "render_state", "render_state", "RenderState", "", "RecipeRenderStateSpec", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 212u, 76u, 0u, 0.0, 0.0, 0.0, &Class_RecipeRenderStateSpec, PropertyKind::None, nullptr, nullptr },
        { "textures", "textures", "textures", "Texture", "", "hgl::ManagedArray<RecipeTextureSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 288u, 24u, 0u, 0.0, 0.0, 0.0, &Class_RecipeTextureSpec, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_MaterialRecipeSpec =
    {
        "MaterialRecipeSpec", "", "一份材质配方文件的完整字段规约。 与运行时的 `MaterialRecipe`（`inc/hgl/mtl/MaterialRecipe.h`）刻意分开： `MaterialRecipe` 带运行时句柄（`TextureAsset*`、SSBO 绑定）与已解析的 `VertexShaderNodeConfig`，不是纯数据也算不出稳定哈希；本结构是纯 POD， 只回答「文件里写了什么」。",
        Meta_MaterialRecipeSpec, (uint32_t)(sizeof(Meta_MaterialRecipeSpec) / sizeof(Meta_MaterialRecipeSpec[0])),
        312u, 8u
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::RecipeTextureSpec *) { return &Class_RecipeTextureSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::RecipePipelineSpec *) { return &Class_RecipePipelineSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::RecipeRenderStateSpec *) { return &Class_RecipeRenderStateSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::RecipeFragmentSpec *) { return &Class_RecipeFragmentSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::RecipeTransformSpec *) { return &Class_RecipeTransformSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::MaterialRecipeSpec *) { return &Class_MaterialRecipeSpec; }
} // namespace hgl::asset::generated
