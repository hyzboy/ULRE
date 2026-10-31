// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   TOML 格式门面（Read* / Write*）。字段遍历仍然只有 Serialize.gen.h 里那一份，
//   本文件不含任何 FlatBuffers 依赖，数据面可以单独包含它。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/asset/generated/SceneSerialize.gen.h>

#include <hgl/graph/asset/SceneAssetSpec.h>

namespace hgl::asset::generated
{
    inline bool ReadSceneTransformSpec(const toml::value &table, hgl::graph::SceneTransformSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneTransformSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteSceneTransformSpec(const hgl::graph::SceneTransformSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneTransformSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadSceneTextureSpec(const toml::value &table, hgl::graph::SceneTextureSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneTextureSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteSceneTextureSpec(const hgl::graph::SceneTextureSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneTextureSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadSceneMaterialSpec(const toml::value &table, hgl::graph::SceneMaterialSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneMaterialSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteSceneMaterialSpec(const hgl::graph::SceneMaterialSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneMaterialSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadSceneNodeSpec(const toml::value &table, hgl::graph::SceneNodeSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneNodeSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteSceneNodeSpec(const hgl::graph::SceneNodeSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneNodeSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadSceneCameraSpec(const toml::value &table, hgl::graph::SceneCameraSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneCameraSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteSceneCameraSpec(const hgl::graph::SceneCameraSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneCameraSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadSceneClearColorSpec(const toml::value &table, hgl::graph::SceneClearColorSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneClearColorSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteSceneClearColorSpec(const hgl::graph::SceneClearColorSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneClearColorSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadSceneSpec(const toml::value &table, hgl::graph::SceneSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteSceneSpec(const hgl::graph::SceneSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

}//namespace hgl::asset::generated
