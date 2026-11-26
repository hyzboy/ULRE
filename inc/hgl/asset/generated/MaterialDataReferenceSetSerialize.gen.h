// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/graph/asset/MaterialDataReferenceSet.h>
#include <hgl/asset/generated/MaterialDataReferenceSetMeta.gen.h>

namespace hgl::serialize
{
    template<> struct StructSerializer<hgl::graph::MaterialDataReference>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "uri", v.uri);
            ar.Field(2, "row", v.row);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_MaterialDataReference; }
    };

    template<> struct StructSerializer<hgl::graph::MaterialDataReferenceSet>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "instances", v.instances);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_MaterialDataReferenceSet; }
    };

}//namespace hgl::serialize
