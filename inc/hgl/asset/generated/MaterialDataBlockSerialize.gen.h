// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/graph/asset/MaterialDataBlockSpec.h>
#include <hgl/asset/generated/MaterialDataBlockMeta.gen.h>

namespace hgl::serialize
{
    template<> struct StructSerializer<hgl::graph::MaterialDataRowSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "base_color", v.base_color);
            ar.Field(2, "metallic", v.metallic);
            ar.Field(3, "roughness", v.roughness);
            ar.Field(4, "normal_scale", v.normal_scale);
            ar.Field(5, "fresnel", v.fresnel);
            ar.Field(6, "color", v.color);
            ar.Field(7, "trans_color", v.trans_color);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_MaterialDataRowSpec; }
    };

    template<> struct StructSerializer<hgl::graph::MaterialDataBlockSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "name", v.name);
            ar.Field(3, "target", v.target);
            ar.Field(4, "rows", v.rows);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_MaterialDataBlockSpec; }
    };

}//namespace hgl::serialize
