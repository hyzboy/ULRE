// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/graph/asset/MaterialTextureConfiguration.h>
#include <hgl/asset/generated/MaterialTextureConfigurationMeta.gen.h>

namespace hgl::serialize
{
    template<> struct StructSerializer<hgl::graph::MaterialTextureAssetReference>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "uri", v.uri);
            ar.Field(2, "array_layer", v.array_layer);
            ar.Field(3, "auto_mipmaps", v.auto_mipmaps);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_MaterialTextureAssetReference; }
    };

    template<> struct StructSerializer<hgl::graph::MaterialTextureConfiguration>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "textures", v.textures);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_MaterialTextureConfiguration; }
    };

}//namespace hgl::serialize
