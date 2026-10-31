// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这里是**布局描述**（FlatBuffers 的 voffset / 内联 struct / 向量元素类型）
//   与格式门面。字段遍历仍然只有 Serialize.gen.h 里那一份。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/asset/generated/MaterialRecipeSerialize.gen.h>
#include <hgl/asset/generated/material_recipe_schema_generated.h>
#include <hgl/graph/asset/MaterialRecipeSpec.h>

namespace hgl::asset::generated
{
    namespace fb = hgl::serialize::fb;

    inline bool VertexInputMode_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::mtl::VertexInputMode v{};
        if (!hgl::asset::generated::AssetEnumParse_VertexInputMode(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *VertexInputMode_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_VertexInputMode(static_cast<hgl::graph::mtl::VertexInputMode>(v));
    }

    inline bool PositionMappingMode_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::mtl::PositionMappingMode v{};
        if (!hgl::asset::generated::AssetEnumParse_PositionMappingMode(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *PositionMappingMode_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_PositionMappingMode(static_cast<hgl::graph::mtl::PositionMappingMode>(v));
    }

    inline bool OrientationMode_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::mtl::OrientationMode v{};
        if (!hgl::asset::generated::AssetEnumParse_OrientationMode(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *OrientationMode_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_OrientationMode(static_cast<hgl::graph::mtl::OrientationMode>(v));
    }

    inline bool ScaleMode_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::mtl::ScaleMode v{};
        if (!hgl::asset::generated::AssetEnumParse_ScaleMode(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *ScaleMode_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_ScaleMode(static_cast<hgl::graph::mtl::ScaleMode>(v));
    }

    inline bool ProjectionMode_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::mtl::ProjectionMode v{};
        if (!hgl::asset::generated::AssetEnumParse_ProjectionMode(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *ProjectionMode_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_ProjectionMode(static_cast<hgl::graph::mtl::ProjectionMode>(v));
    }

    inline bool MaterialDomain_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::mtl::MaterialDomain v{};
        if (!hgl::asset::generated::AssetEnumParse_MaterialDomain(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *MaterialDomain_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_MaterialDomain(static_cast<hgl::graph::mtl::MaterialDomain>(v));
    }

    inline bool VkCullModeFlagBits_FbsFromName(const char *s, int64_t &out)
    {
        ::VkCullModeFlagBits v{};
        if (!hgl::asset::generated::AssetEnumParse_VkCullModeFlagBits(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *VkCullModeFlagBits_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_VkCullModeFlagBits(static_cast<::VkCullModeFlagBits>(v));
    }

    inline bool VkCompareOp_FbsFromName(const char *s, int64_t &out)
    {
        ::VkCompareOp v{};
        if (!hgl::asset::generated::AssetEnumParse_VkCompareOp(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *VkCompareOp_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_VkCompareOp(static_cast<::VkCompareOp>(v));
    }

    inline bool VkBlendFactor_FbsFromName(const char *s, int64_t &out)
    {
        ::VkBlendFactor v{};
        if (!hgl::asset::generated::AssetEnumParse_VkBlendFactor(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *VkBlendFactor_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_VkBlendFactor(static_cast<::VkBlendFactor>(v));
    }

    inline const fb::FieldDesc RecipeTextureSpec_fbs_fields[] = {
        {1, "name", fb::FbType::String, 4},
        {2, "asset", fb::FbType::String, 6},
        {3, "required", fb::FbType::Bool, 8},
        {4, "layer", fb::FbType::UInt32, 10},
        {5, "auto_mipmaps", fb::FbType::Bool, 12},
    };
    inline const fb::TableDesc RecipeTextureSpec_fbs = {"RecipeTextureSpec", RecipeTextureSpec_fbs_fields, 5};

    inline const fb::FieldDesc RecipePipelineSpec_fbs_fields[] = {
        fb::OptScalar({1, "cull_mode", fb::FbType::Enum, 4, fb::FbType::None, 0, nullptr, &VkCullModeFlagBits_FbsFromName, &VkCullModeFlagBits_FbsToName}),
        fb::OptScalar({2, "depth_test", fb::FbType::Bool, 6}),
        fb::OptScalar({3, "depth_write", fb::FbType::Bool, 8}),
        fb::OptScalar({4, "depth_compare_op", fb::FbType::Enum, 10, fb::FbType::None, 0, nullptr, &VkCompareOp_FbsFromName, &VkCompareOp_FbsToName}),
        fb::OptScalar({5, "alpha_blend", fb::FbType::Bool, 12}),
        fb::OptScalar({6, "blend_src", fb::FbType::Enum, 14, fb::FbType::None, 0, nullptr, &VkBlendFactor_FbsFromName, &VkBlendFactor_FbsToName}),
        fb::OptScalar({7, "blend_dst", fb::FbType::Enum, 16, fb::FbType::None, 0, nullptr, &VkBlendFactor_FbsFromName, &VkBlendFactor_FbsToName}),
        fb::OptScalar({8, "dynamic_line_width", fb::FbType::Bool, 18}),
        fb::OptScalar({9, "overlay", fb::FbType::Bool, 20}),
        fb::OptScalar({10, "wireframe", fb::FbType::Bool, 22}),
        fb::OptScalar({11, "line_width", fb::FbType::Float, 24}),
    };
    inline const fb::TableDesc RecipePipelineSpec_fbs = {"RecipePipelineSpec", RecipePipelineSpec_fbs_fields, 11};

    inline const fb::FieldDesc RecipeRenderStateSpec_fbs_fields[] = {
        fb::OptScalar({1, "alpha_test", fb::FbType::Bool, 4}),
        fb::OptScalar({2, "alpha_cutoff", fb::FbType::Float, 6}),
        fb::OptScalar({3, "dither", fb::FbType::Bool, 8}),
        {4, "pipeline", fb::FbType::Table, 10, fb::FbType::None, 0, &RecipePipelineSpec_fbs},
    };
    inline const fb::TableDesc RecipeRenderStateSpec_fbs = {"RecipeRenderStateSpec", RecipeRenderStateSpec_fbs_fields, 4};

    inline const fb::FieldDesc RecipeFragmentSpec_fbs_fields[] = {
        {1, "surface_module_name", fb::FbType::String, 4},
        {2, "samplers", fb::FbType::VectorString, 6},
        {3, "defines", fb::FbType::VectorString, 8},
    };
    inline const fb::TableDesc RecipeFragmentSpec_fbs = {"RecipeFragmentSpec", RecipeFragmentSpec_fbs_fields, 3};

    inline const fb::FieldDesc RecipeTransformSpec_fbs_fields[] = {
        fb::OptScalar({1, "source", fb::FbType::Enum, 4, fb::FbType::None, 0, nullptr, &VertexInputMode_FbsFromName, &VertexInputMode_FbsToName}),
        fb::OptScalar({2, "mapping", fb::FbType::Enum, 6, fb::FbType::None, 0, nullptr, &PositionMappingMode_FbsFromName, &PositionMappingMode_FbsToName}),
        fb::OptScalar({3, "orientation", fb::FbType::Enum, 8, fb::FbType::None, 0, nullptr, &OrientationMode_FbsFromName, &OrientationMode_FbsToName}),
        fb::OptScalar({4, "scale", fb::FbType::Enum, 10, fb::FbType::None, 0, nullptr, &ScaleMode_FbsFromName, &ScaleMode_FbsToName}),
        fb::OptScalar({5, "projection", fb::FbType::Enum, 12, fb::FbType::None, 0, nullptr, &ProjectionMode_FbsFromName, &ProjectionMode_FbsToName}),
    };
    inline const fb::TableDesc RecipeTransformSpec_fbs = {"RecipeTransformSpec", RecipeTransformSpec_fbs_fields, 5};

    inline const fb::FieldDesc MaterialRecipeSpec_fbs_fields[] = {
        {1, "schema", fb::FbType::UInt32, 4},
        {2, "name", fb::FbType::String, 6},
        {3, "domain", fb::FbType::Enum, 8, fb::FbType::None, 0, nullptr, &MaterialDomain_FbsFromName, &MaterialDomain_FbsToName},
        {4, "fragment", fb::FbType::Table, 10, fb::FbType::None, 0, &RecipeFragmentSpec_fbs},
        {5, "transform", fb::FbType::Table, 12, fb::FbType::None, 0, &RecipeTransformSpec_fbs},
        {6, "render_state", fb::FbType::Table, 14, fb::FbType::None, 0, &RecipeRenderStateSpec_fbs},
        {7, "textures", fb::FbType::VectorTable, 16, fb::FbType::None, 0, &RecipeTextureSpec_fbs},
    };
    inline const fb::TableDesc MaterialRecipeSpec_fbs = {"MaterialRecipeSpec", MaterialRecipeSpec_fbs_fields, 7};

    inline bool ReadRecipeTextureSpec(const toml::value &table, hgl::graph::mtl::RecipeTextureSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTextureSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteRecipeTextureSpec(const hgl::graph::mtl::RecipeTextureSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTextureSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadRecipePipelineSpec(const toml::value &table, hgl::graph::mtl::RecipePipelineSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipePipelineSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteRecipePipelineSpec(const hgl::graph::mtl::RecipePipelineSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipePipelineSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadRecipeRenderStateSpec(const toml::value &table, hgl::graph::mtl::RecipeRenderStateSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeRenderStateSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteRecipeRenderStateSpec(const hgl::graph::mtl::RecipeRenderStateSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeRenderStateSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadRecipeFragmentSpec(const toml::value &table, hgl::graph::mtl::RecipeFragmentSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeFragmentSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteRecipeFragmentSpec(const hgl::graph::mtl::RecipeFragmentSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeFragmentSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadRecipeTransformSpec(const toml::value &table, hgl::graph::mtl::RecipeTransformSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTransformSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteRecipeTransformSpec(const hgl::graph::mtl::RecipeTransformSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTransformSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadMaterialRecipeSpec(const toml::value &table, hgl::graph::mtl::MaterialRecipeSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::MaterialRecipeSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteMaterialRecipeSpec(const hgl::graph::mtl::MaterialRecipeSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::MaterialRecipeSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::RecipeTextureSpec>
    EncodeRecipeTextureSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::mtl::RecipeTextureSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTextureSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::RecipeTextureSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, RecipeTextureSpec_fbs).o);
    }

    inline void DecodeRecipeTextureSpec(const ::hgl::asset::schema::RecipeTextureSpec *root, hgl::graph::mtl::RecipeTextureSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            RecipeTextureSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTextureSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::RecipePipelineSpec>
    EncodeRecipePipelineSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::mtl::RecipePipelineSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipePipelineSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::RecipePipelineSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, RecipePipelineSpec_fbs).o);
    }

    inline void DecodeRecipePipelineSpec(const ::hgl::asset::schema::RecipePipelineSpec *root, hgl::graph::mtl::RecipePipelineSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            RecipePipelineSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipePipelineSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::RecipeRenderStateSpec>
    EncodeRecipeRenderStateSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::mtl::RecipeRenderStateSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeRenderStateSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::RecipeRenderStateSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, RecipeRenderStateSpec_fbs).o);
    }

    inline void DecodeRecipeRenderStateSpec(const ::hgl::asset::schema::RecipeRenderStateSpec *root, hgl::graph::mtl::RecipeRenderStateSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            RecipeRenderStateSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeRenderStateSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::RecipeFragmentSpec>
    EncodeRecipeFragmentSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::mtl::RecipeFragmentSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeFragmentSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::RecipeFragmentSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, RecipeFragmentSpec_fbs).o);
    }

    inline void DecodeRecipeFragmentSpec(const ::hgl::asset::schema::RecipeFragmentSpec *root, hgl::graph::mtl::RecipeFragmentSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            RecipeFragmentSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeFragmentSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::RecipeTransformSpec>
    EncodeRecipeTransformSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::mtl::RecipeTransformSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTransformSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::RecipeTransformSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, RecipeTransformSpec_fbs).o);
    }

    inline void DecodeRecipeTransformSpec(const ::hgl::asset::schema::RecipeTransformSpec *root, hgl::graph::mtl::RecipeTransformSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            RecipeTransformSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTransformSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::MaterialRecipeSpec>
    EncodeMaterialRecipeSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::mtl::MaterialRecipeSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::MaterialRecipeSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::MaterialRecipeSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, MaterialRecipeSpec_fbs).o);
    }

    inline void DecodeMaterialRecipeSpec(const ::hgl::asset::schema::MaterialRecipeSpec *root, hgl::graph::mtl::MaterialRecipeSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            MaterialRecipeSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::MaterialRecipeSpec>::Do(ar, out);
    }

}//namespace hgl::asset::generated
