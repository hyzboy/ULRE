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

namespace hgl::asset::generated
{
    inline const char *AssetEnumName_VertexInputMode(const hgl::graph::mtl::VertexInputMode v)
    {
        switch (v)
        {
        case hgl::graph::mtl::VertexInputMode::Vec2Position: return "Vec2Position";
        case hgl::graph::mtl::VertexInputMode::Vec3Position: return "Vec3Position";
        case hgl::graph::mtl::VertexInputMode::Vec2IntPosition: return "Vec2IntPosition";
        case hgl::graph::mtl::VertexInputMode::None: return "None";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_VertexInputMode(const char *s, hgl::graph::mtl::VertexInputMode &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "Vec2Position") == 0) { out = hgl::graph::mtl::VertexInputMode::Vec2Position; return true; }
        if (hgl::stricmp(s, "Vec3Position") == 0) { out = hgl::graph::mtl::VertexInputMode::Vec3Position; return true; }
        if (hgl::stricmp(s, "Vec2IntPosition") == 0) { out = hgl::graph::mtl::VertexInputMode::Vec2IntPosition; return true; }
        if (hgl::stricmp(s, "None") == 0) { out = hgl::graph::mtl::VertexInputMode::None; return true; }
        return false;
    }

    inline const char *AssetEnumName_PositionMappingMode(const hgl::graph::mtl::PositionMappingMode v)
    {
        switch (v)
        {
        case hgl::graph::mtl::PositionMappingMode::Passthrough3D: return "Passthrough3D";
        case hgl::graph::mtl::PositionMappingMode::LiftXY_XY0: return "LiftXY_XY0";
        case hgl::graph::mtl::PositionMappingMode::LiftXY_X0Y: return "LiftXY_X0Y";
        case hgl::graph::mtl::PositionMappingMode::LiftXY_0XY: return "LiftXY_0XY";
        case hgl::graph::mtl::PositionMappingMode::NDCLift: return "NDCLift";
        case hgl::graph::mtl::PositionMappingMode::ZeroOneToNDC: return "ZeroOneToNDC";
        case hgl::graph::mtl::PositionMappingMode::PixelToLocal: return "PixelToLocal";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_PositionMappingMode(const char *s, hgl::graph::mtl::PositionMappingMode &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "Passthrough3D") == 0) { out = hgl::graph::mtl::PositionMappingMode::Passthrough3D; return true; }
        if (hgl::stricmp(s, "LiftXY_XY0") == 0) { out = hgl::graph::mtl::PositionMappingMode::LiftXY_XY0; return true; }
        if (hgl::stricmp(s, "LiftXY_X0Y") == 0) { out = hgl::graph::mtl::PositionMappingMode::LiftXY_X0Y; return true; }
        if (hgl::stricmp(s, "LiftXY_0XY") == 0) { out = hgl::graph::mtl::PositionMappingMode::LiftXY_0XY; return true; }
        if (hgl::stricmp(s, "NDCLift") == 0) { out = hgl::graph::mtl::PositionMappingMode::NDCLift; return true; }
        if (hgl::stricmp(s, "ZeroOneToNDC") == 0) { out = hgl::graph::mtl::PositionMappingMode::ZeroOneToNDC; return true; }
        if (hgl::stricmp(s, "PixelToLocal") == 0) { out = hgl::graph::mtl::PositionMappingMode::PixelToLocal; return true; }
        return false;
    }

    inline const char *AssetEnumName_OrientationMode(const hgl::graph::mtl::OrientationMode v)
    {
        switch (v)
        {
        case hgl::graph::mtl::OrientationMode::World: return "World";
        case hgl::graph::mtl::OrientationMode::CameraFacingFree: return "CameraFacingFree";
        case hgl::graph::mtl::OrientationMode::CameraFacingAxisY: return "CameraFacingAxisY";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_OrientationMode(const char *s, hgl::graph::mtl::OrientationMode &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "World") == 0) { out = hgl::graph::mtl::OrientationMode::World; return true; }
        if (hgl::stricmp(s, "CameraFacingFree") == 0) { out = hgl::graph::mtl::OrientationMode::CameraFacingFree; return true; }
        if (hgl::stricmp(s, "CameraFacingAxisY") == 0) { out = hgl::graph::mtl::OrientationMode::CameraFacingAxisY; return true; }
        return false;
    }

    inline const char *AssetEnumName_ScaleMode(const hgl::graph::mtl::ScaleMode v)
    {
        switch (v)
        {
        case hgl::graph::mtl::ScaleMode::World: return "World";
        case hgl::graph::mtl::ScaleMode::FixedPixelSize: return "FixedPixelSize";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_ScaleMode(const char *s, hgl::graph::mtl::ScaleMode &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "World") == 0) { out = hgl::graph::mtl::ScaleMode::World; return true; }
        if (hgl::stricmp(s, "FixedPixelSize") == 0) { out = hgl::graph::mtl::ScaleMode::FixedPixelSize; return true; }
        return false;
    }

    inline const char *AssetEnumName_ProjectionMode(const hgl::graph::mtl::ProjectionMode v)
    {
        switch (v)
        {
        case hgl::graph::mtl::ProjectionMode::WorldCameraVP: return "WorldCameraVP";
        case hgl::graph::mtl::ProjectionMode::LocalToWorldOnly: return "LocalToWorldOnly";
        case hgl::graph::mtl::ProjectionMode::OrthoViewport: return "OrthoViewport";
        case hgl::graph::mtl::ProjectionMode::OrthoThenLocalToWorld: return "OrthoThenLocalToWorld";
        case hgl::graph::mtl::ProjectionMode::ClipPassthrough: return "ClipPassthrough";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_ProjectionMode(const char *s, hgl::graph::mtl::ProjectionMode &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "WorldCameraVP") == 0) { out = hgl::graph::mtl::ProjectionMode::WorldCameraVP; return true; }
        if (hgl::stricmp(s, "LocalToWorldOnly") == 0) { out = hgl::graph::mtl::ProjectionMode::LocalToWorldOnly; return true; }
        if (hgl::stricmp(s, "OrthoViewport") == 0) { out = hgl::graph::mtl::ProjectionMode::OrthoViewport; return true; }
        if (hgl::stricmp(s, "OrthoThenLocalToWorld") == 0) { out = hgl::graph::mtl::ProjectionMode::OrthoThenLocalToWorld; return true; }
        if (hgl::stricmp(s, "ClipPassthrough") == 0) { out = hgl::graph::mtl::ProjectionMode::ClipPassthrough; return true; }
        return false;
    }

    inline const char *AssetEnumName_MaterialDomain(const hgl::graph::mtl::MaterialDomain v)
    {
        switch (v)
        {
        case hgl::graph::mtl::MaterialDomain::Custom: return "Custom";
        case hgl::graph::mtl::MaterialDomain::StandardOpaque: return "StandardOpaque";
        case hgl::graph::mtl::MaterialDomain::Foliage: return "Foliage";
        case hgl::graph::mtl::MaterialDomain::Terrain: return "Terrain";
        case hgl::graph::mtl::MaterialDomain::UI2D: return "UI2D";
        case hgl::graph::mtl::MaterialDomain::HumanSkin: return "HumanSkin";
        case hgl::graph::mtl::MaterialDomain::UnlitSolid: return "UnlitSolid";
        case hgl::graph::mtl::MaterialDomain::VertexColor: return "VertexColor";
        case hgl::graph::mtl::MaterialDomain::VertexPalette: return "VertexPalette";
        case hgl::graph::mtl::MaterialDomain::VertexLuminance: return "VertexLuminance";
        case hgl::graph::mtl::MaterialDomain::UnlitTexture: return "UnlitTexture";
        case hgl::graph::mtl::MaterialDomain::UnlitTexture3D: return "UnlitTexture3D";
        case hgl::graph::mtl::MaterialDomain::Text2D: return "Text2D";
        case hgl::graph::mtl::MaterialDomain::Sky: return "Sky";
        case hgl::graph::mtl::MaterialDomain::DebugNormal: return "DebugNormal";
        case hgl::graph::mtl::MaterialDomain::Checker2D: return "Checker2D";
        case hgl::graph::mtl::MaterialDomain::Checker3D: return "Checker3D";
        case hgl::graph::mtl::MaterialDomain::ErrorChecker3D: return "ErrorChecker3D";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_MaterialDomain(const char *s, hgl::graph::mtl::MaterialDomain &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "Custom") == 0) { out = hgl::graph::mtl::MaterialDomain::Custom; return true; }
        if (hgl::stricmp(s, "StandardOpaque") == 0) { out = hgl::graph::mtl::MaterialDomain::StandardOpaque; return true; }
        if (hgl::stricmp(s, "Foliage") == 0) { out = hgl::graph::mtl::MaterialDomain::Foliage; return true; }
        if (hgl::stricmp(s, "Terrain") == 0) { out = hgl::graph::mtl::MaterialDomain::Terrain; return true; }
        if (hgl::stricmp(s, "UI2D") == 0) { out = hgl::graph::mtl::MaterialDomain::UI2D; return true; }
        if (hgl::stricmp(s, "HumanSkin") == 0) { out = hgl::graph::mtl::MaterialDomain::HumanSkin; return true; }
        if (hgl::stricmp(s, "UnlitSolid") == 0) { out = hgl::graph::mtl::MaterialDomain::UnlitSolid; return true; }
        if (hgl::stricmp(s, "VertexColor") == 0) { out = hgl::graph::mtl::MaterialDomain::VertexColor; return true; }
        if (hgl::stricmp(s, "VertexPalette") == 0) { out = hgl::graph::mtl::MaterialDomain::VertexPalette; return true; }
        if (hgl::stricmp(s, "VertexLuminance") == 0) { out = hgl::graph::mtl::MaterialDomain::VertexLuminance; return true; }
        if (hgl::stricmp(s, "UnlitTexture") == 0) { out = hgl::graph::mtl::MaterialDomain::UnlitTexture; return true; }
        if (hgl::stricmp(s, "UnlitTexture3D") == 0) { out = hgl::graph::mtl::MaterialDomain::UnlitTexture3D; return true; }
        if (hgl::stricmp(s, "Text2D") == 0) { out = hgl::graph::mtl::MaterialDomain::Text2D; return true; }
        if (hgl::stricmp(s, "Sky") == 0) { out = hgl::graph::mtl::MaterialDomain::Sky; return true; }
        if (hgl::stricmp(s, "DebugNormal") == 0) { out = hgl::graph::mtl::MaterialDomain::DebugNormal; return true; }
        if (hgl::stricmp(s, "Checker2D") == 0) { out = hgl::graph::mtl::MaterialDomain::Checker2D; return true; }
        if (hgl::stricmp(s, "Checker3D") == 0) { out = hgl::graph::mtl::MaterialDomain::Checker3D; return true; }
        if (hgl::stricmp(s, "ErrorChecker3D") == 0) { out = hgl::graph::mtl::MaterialDomain::ErrorChecker3D; return true; }
        return false;
    }

    inline const char *AssetEnumName_VkCullModeFlags(const ::VkCullModeFlags v)
    { return hgl::graph::mtl::GetVkCullModeName(v); }

    inline bool AssetEnumParse_VkCullModeFlags(const char *s, ::VkCullModeFlags &out)
    { return hgl::graph::mtl::ParseVkCullMode(s, out); }

    inline const char *AssetEnumName_VkCompareOp(const ::VkCompareOp v)
    { return hgl::graph::mtl::GetVkCompareOpName(v); }

    inline bool AssetEnumParse_VkCompareOp(const char *s, ::VkCompareOp &out)
    { return hgl::graph::mtl::ParseVkCompareOp(s, out); }

    inline const char *AssetEnumName_VkBlendFactor(const ::VkBlendFactor v)
    { return hgl::graph::mtl::GetVkBlendFactorName(v); }

    inline bool AssetEnumParse_VkBlendFactor(const char *s, ::VkBlendFactor &out)
    { return hgl::graph::mtl::ParseVkBlendFactor(s, out); }

}//namespace hgl::asset::generated

namespace hgl::serialize
{
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

    template<> struct EnumTraits<::VkCullModeFlags>
    {
        static constexpr bool defined = true;
        static const char *ToName(::VkCullModeFlags v)
        { return hgl::asset::generated::AssetEnumName_VkCullModeFlags(v); }
        static bool FromName(const char *s, ::VkCullModeFlags &out)
        { return hgl::asset::generated::AssetEnumParse_VkCullModeFlags(s, out); }
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
    };

    template<> struct StructSerializer<hgl::graph::mtl::MaterialRecipeSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "name", v.name);
            ar.Field(3, "domain", v.domain);
            ar.Field(4, "fragment", v.fragment);
            ar.Field(5, "transform", v.transform);
            ar.Field(6, "render_state", v.render_state);
            ar.Field(7, "textures", v.textures);
        }
    };

}//namespace hgl::serialize
