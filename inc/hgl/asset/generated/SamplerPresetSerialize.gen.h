// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/asset/generated/SamplerPresetMeta.gen.h>

namespace hgl::serialize
{
    template<> struct StructSerializer<hgl::graph::mtl::SamplerPresetSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "name", v.name);
            ar.Field(2, "mag_filter", v.mag_filter);
            ar.Field(3, "min_filter", v.min_filter);
            ar.Field(4, "mipmap_mode", v.mipmap_mode);
            ar.Field(5, "address_mode", v.address_mode);
            ar.Field(6, "anisotropy", v.anisotropy);
            ar.Field(7, "max_anisotropy", v.max_anisotropy);
            ar.Field(8, "max_lod", v.max_lod);
            ar.Field(9, "compare_op", v.compare_op);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_SamplerPresetSpec; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::SamplerPresetFileSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "samplers", v.samplers);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_SamplerPresetFileSpec; }
    };

}//namespace hgl::serialize
