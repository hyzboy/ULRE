// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/graph/asset/MaterialRecipeSpec.h>
#include <hgl/asset/generated/MaterialRecipeMeta.gen.h>

namespace hgl::serialize
{
    template<> struct EnumTraits<::VkCullModeFlagBits>
    {
        static constexpr bool defined = true;
        static const char *ToName(::VkCullModeFlagBits v)
        { return hgl::asset::generated::AssetEnumName_VkCullModeFlagBits(v); }
        static bool FromName(const char *s, ::VkCullModeFlagBits &out)
        { return hgl::asset::generated::AssetEnumParse_VkCullModeFlagBits(s, out); }
    };

    template<> struct EnumTraits<::VkCompareOp>
    {
        static constexpr bool defined = true;
        static const char *ToName(::VkCompareOp v)
        { return hgl::asset::generated::AssetEnumName_VkCompareOp(v); }
        static bool FromName(const char *s, ::VkCompareOp &out)
        { return hgl::asset::generated::AssetEnumParse_VkCompareOp(s, out); }
    };

    template<> struct EnumTraits<::VkBlendFactor>
    {
        static constexpr bool defined = true;
        static const char *ToName(::VkBlendFactor v)
        { return hgl::asset::generated::AssetEnumName_VkBlendFactor(v); }
        static bool FromName(const char *s, ::VkBlendFactor &out)
        { return hgl::asset::generated::AssetEnumParse_VkBlendFactor(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::mtl::VertexInputMode>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::mtl::VertexInputMode v)
        { return hgl::asset::generated::AssetEnumName_VertexInputMode(v); }
        static bool FromName(const char *s, hgl::graph::mtl::VertexInputMode &out)
        { return hgl::asset::generated::AssetEnumParse_VertexInputMode(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::mtl::PositionMappingMode>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::mtl::PositionMappingMode v)
        { return hgl::asset::generated::AssetEnumName_PositionMappingMode(v); }
        static bool FromName(const char *s, hgl::graph::mtl::PositionMappingMode &out)
        { return hgl::asset::generated::AssetEnumParse_PositionMappingMode(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::mtl::OrientationMode>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::mtl::OrientationMode v)
        { return hgl::asset::generated::AssetEnumName_OrientationMode(v); }
        static bool FromName(const char *s, hgl::graph::mtl::OrientationMode &out)
        { return hgl::asset::generated::AssetEnumParse_OrientationMode(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::mtl::ScaleMode>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::mtl::ScaleMode v)
        { return hgl::asset::generated::AssetEnumName_ScaleMode(v); }
        static bool FromName(const char *s, hgl::graph::mtl::ScaleMode &out)
        { return hgl::asset::generated::AssetEnumParse_ScaleMode(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::mtl::ProjectionMode>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::mtl::ProjectionMode v)
        { return hgl::asset::generated::AssetEnumName_ProjectionMode(v); }
        static bool FromName(const char *s, hgl::graph::mtl::ProjectionMode &out)
        { return hgl::asset::generated::AssetEnumParse_ProjectionMode(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::mtl::MaterialDomain>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::mtl::MaterialDomain v)
        { return hgl::asset::generated::AssetEnumName_MaterialDomain(v); }
        static bool FromName(const char *s, hgl::graph::mtl::MaterialDomain &out)
        { return hgl::asset::generated::AssetEnumParse_MaterialDomain(s, out); }
    };

    template<> struct StructSerializer<hgl::graph::mtl::RecipeTextureSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "name", v.texture_name);
            ar.Field(2, "asset", v.resource_id);
            ar.Field(3, "required", v.required);
            ar.Field(4, "layer", v.layer);
            ar.Field(5, "auto_mipmaps", v.auto_mipmaps);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_RecipeTextureSpec; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::RecipePipelineSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "cull_mode", v.cull_mode);
            ar.Field(2, "depth_test", v.depth_test);
            ar.Field(3, "depth_write", v.depth_write);
            ar.Field(4, "depth_compare_op", v.depth_compare_op);
            ar.Field(5, "alpha_blend", v.alpha_blend);
            ar.Field(6, "blend_src", v.blend_src);
            ar.Field(7, "blend_dst", v.blend_dst);
            ar.Field(8, "dynamic_line_width", v.dynamic_line_width);
            ar.Field(9, "overlay", v.overlay);
            ar.Field(10, "wireframe", v.wireframe);
            ar.Field(11, "line_width", v.line_width);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_RecipePipelineSpec; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::RecipeRenderStateSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "alpha_test", v.alpha_test);
            ar.Field(2, "alpha_cutoff", v.alpha_cutoff);
            ar.Field(3, "dither", v.dither);
            ar.Field(4, "pipeline", v.pipeline);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_RecipeRenderStateSpec; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::RecipeFragmentSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "surface_module_name", v.surface_module_name);
            ar.Field(2, "samplers", v.samplers);
            ar.Field(3, "defines", v.defines);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_RecipeFragmentSpec; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::RecipeTransformSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "source", v.input);
            ar.Field(2, "mapping", v.position_mapping);
            ar.Field(3, "orientation", v.orientation);
            ar.Field(4, "scale", v.scale);
            ar.Field(5, "projection", v.projection);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_RecipeTransformSpec; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::MaterialRecipeSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "name", v.name);
            ar.Field(3, "parent", v.parent);
            ar.Field(4, "domain", v.domain);
            ar.Field(5, "fragment", v.fragment);
            ar.Field(6, "transform", v.transform);
            ar.Field(7, "render_state", v.render_state);
            ar.Field(8, "textures", v.textures);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_MaterialRecipeSpec; }
    };

}//namespace hgl::serialize
