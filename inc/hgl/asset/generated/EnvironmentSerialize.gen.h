// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/graph/asset/EnvironmentAssetSpec.h>
#include <hgl/asset/generated/EnvironmentMeta.gen.h>

namespace hgl::serialize
{
    template<> struct EnumTraits<hgl::graph::SkyMode>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::SkyMode v)
        { return hgl::asset::generated::AssetEnumName_SkyMode(v); }
        static bool FromName(const char *s, hgl::graph::SkyMode &out)
        { return hgl::asset::generated::AssetEnumParse_SkyMode(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::SkyAmbientLDR>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::SkyAmbientLDR v)
        { return hgl::asset::generated::AssetEnumName_SkyAmbientLDR(v); }
        static bool FromName(const char *s, hgl::graph::SkyAmbientLDR &out)
        { return hgl::asset::generated::AssetEnumParse_SkyAmbientLDR(s, out); }
    };

    template<> struct StructSerializer<hgl::graph::EnvironmentVisualSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "recipe", v.recipe);
            ar.Field(2, "geometry", v.geometry);
            ar.Field(3, "scale", v.scale);
            ar.Field(4, "track_camera", v.track_camera);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_EnvironmentVisualSpec; }
    };

    template<> struct StructSerializer<hgl::graph::EnvironmentAmbientSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "mode", v.mode);
            ar.Field(2, "provider", v.provider);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_EnvironmentAmbientSpec; }
    };

    template<> struct StructSerializer<hgl::graph::EnvironmentIBLSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "irradiance", v.irradiance);
            ar.Field(2, "prefilter", v.prefilter);
            ar.Field(3, "brdf_lut", v.brdf_lut);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_EnvironmentIBLSpec; }
    };

    template<> struct StructSerializer<hgl::graph::EnvironmentLDRSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "mode", v.mode);
            ar.Field(2, "exposure_base", v.exposure_base);
            ar.Field(3, "sun_scale", v.sun_scale);
            ar.Field(4, "min_ambient", v.min_ambient);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_EnvironmentLDRSpec; }
    };

    template<> struct StructSerializer<hgl::graph::EnvironmentSH9BandSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "r", v.r);
            ar.Field(2, "g", v.g);
            ar.Field(3, "b", v.b);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_EnvironmentSH9BandSpec; }
    };

    template<> struct StructSerializer<hgl::graph::EnvironmentSH9Spec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "auto_generate", v.auto_generate);
            ar.Field(2, "track_sun", v.track_sun);
            ar.Field(3, "sun_irradiance_scale", v.sun_irradiance_scale);
            ar.Field(4, "exposure", v.exposure);
            ar.Field(5, "bands", v.bands);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_EnvironmentSH9Spec; }
    };

    template<> struct StructSerializer<hgl::graph::EnvironmentSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "name", v.name);
            ar.Field(3, "description", v.description);
            ar.Field(4, "visual", v.visual);
            ar.Field(5, "ambient", v.ambient);
            ar.Field(6, "ibl", v.ibl);
            ar.Field(7, "ldr", v.ldr);
            ar.Field(8, "sh9", v.sh9);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_EnvironmentSpec; }
    };

}//namespace hgl::serialize
