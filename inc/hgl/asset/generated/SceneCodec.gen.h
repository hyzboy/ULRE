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
#include <hgl/asset/generated/SceneSerialize.gen.h>
#include <hgl/asset/generated/scene_schema_generated.h>
#include <hgl/graph/asset/SceneAssetSpec.h>

namespace hgl::asset::generated
{
    namespace fb = hgl::serialize::fb;

    inline bool SceneMaterialType_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::SceneMaterialType v{};
        if (!hgl::asset::generated::AssetEnumParse_SceneMaterialType(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *SceneMaterialType_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_SceneMaterialType(static_cast<hgl::graph::SceneMaterialType>(v));
    }

    inline bool SceneCameraMode_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::SceneCameraMode v{};
        if (!hgl::asset::generated::AssetEnumParse_SceneCameraMode(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *SceneCameraMode_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_SceneCameraMode(static_cast<hgl::graph::SceneCameraMode>(v));
    }

    inline const fb::FieldDesc SceneTransformSpec_fbs_fields[] = {
        {1, "position", fb::FbType::Struct, 4, fb::FbType::None, 3},
        {2, "rotation_axis", fb::FbType::Struct, 6, fb::FbType::None, 3},
        {3, "rotation_angle_deg", fb::FbType::Float, 8},
        {4, "scale", fb::FbType::Struct, 10, fb::FbType::None, 3},
    };
    inline const fb::TableDesc SceneTransformSpec_fbs = {"SceneTransformSpec", SceneTransformSpec_fbs_fields, 4};

    inline const fb::FieldDesc SceneTextureSpec_fbs_fields[] = {
        {1, "name", fb::FbType::String, 4},
        {2, "uri", fb::FbType::String, 6},
        {3, "required", fb::FbType::Bool, 8},
    };
    inline const fb::TableDesc SceneTextureSpec_fbs = {"SceneTextureSpec", SceneTextureSpec_fbs_fields, 3};

    inline const fb::FieldDesc SceneMaterialSpec_fbs_fields[] = {
        {1, "name", fb::FbType::String, 4},
        {2, "type", fb::FbType::Enum, 6, fb::FbType::None, 0, nullptr, &SceneMaterialType_FbsFromName, &SceneMaterialType_FbsToName},
        {3, "base_color", fb::FbType::Struct, 8, fb::FbType::None, 4},
        {4, "metallic", fb::FbType::Float, 10},
        {5, "roughness", fb::FbType::Float, 12},
        {6, "normal_scale", fb::FbType::Float, 14},
        {7, "fresnel", fb::FbType::Float, 16},
        {8, "color", fb::FbType::Struct, 18, fb::FbType::None, 4},
        {9, "trans_color", fb::FbType::Struct, 20, fb::FbType::None, 4},
        {10, "textures", fb::FbType::VectorTable, 22, fb::FbType::None, 0, &SceneTextureSpec_fbs},
    };
    inline const fb::TableDesc SceneMaterialSpec_fbs = {"SceneMaterialSpec", SceneMaterialSpec_fbs_fields, 10};

    inline const fb::FieldDesc SceneNodeSpec_fbs_fields[] = {
        {1, "name", fb::FbType::String, 4},
        {2, "parent", fb::FbType::String, 6},
        {3, "geometry", fb::FbType::String, 8},
        {4, "recipe", fb::FbType::String, 10},
        {5, "material", fb::FbType::String, 12},
        {6, "mobility", fb::FbType::String, 14},
        {7, "visible", fb::FbType::Bool, 16},
        {8, "transform", fb::FbType::Table, 18, fb::FbType::None, 0, &SceneTransformSpec_fbs},
    };
    inline const fb::TableDesc SceneNodeSpec_fbs = {"SceneNodeSpec", SceneNodeSpec_fbs_fields, 8};

    inline const fb::FieldDesc SceneCameraSpec_fbs_fields[] = {
        {1, "enabled", fb::FbType::Bool, 4},
        {2, "mode", fb::FbType::Enum, 6, fb::FbType::None, 0, nullptr, &SceneCameraMode_FbsFromName, &SceneCameraMode_FbsToName},
        {3, "position", fb::FbType::Struct, 8, fb::FbType::None, 3},
        {4, "target", fb::FbType::Struct, 10, fb::FbType::None, 3},
        {5, "distance", fb::FbType::Float, 12},
        {6, "yaw", fb::FbType::Float, 14},
        {7, "pitch", fb::FbType::Float, 16},
        {8, "fov_deg", fb::FbType::Float, 18},
        {9, "near_plane", fb::FbType::Float, 20},
        {10, "far_plane", fb::FbType::Float, 22},
        {11, "main", fb::FbType::Bool, 24},
    };
    inline const fb::TableDesc SceneCameraSpec_fbs = {"SceneCameraSpec", SceneCameraSpec_fbs_fields, 11};

    inline const fb::FieldDesc SceneClearColorSpec_fbs_fields[] = {
        {1, "enabled", fb::FbType::Bool, 4},
        {2, "value", fb::FbType::Struct, 6, fb::FbType::None, 4},
    };
    inline const fb::TableDesc SceneClearColorSpec_fbs = {"SceneClearColorSpec", SceneClearColorSpec_fbs_fields, 2};

    inline const fb::FieldDesc SceneSpec_fbs_fields[] = {
        {1, "schema", fb::FbType::UInt32, 4},
        {2, "name", fb::FbType::String, 6},
        {3, "description", fb::FbType::String, 8},
        {4, "camera", fb::FbType::Table, 10, fb::FbType::None, 0, &SceneCameraSpec_fbs},
        {5, "clear_color", fb::FbType::Table, 12, fb::FbType::None, 0, &SceneClearColorSpec_fbs},
        {6, "materials", fb::FbType::VectorTable, 14, fb::FbType::None, 0, &SceneMaterialSpec_fbs},
        {7, "nodes", fb::FbType::VectorTable, 16, fb::FbType::None, 0, &SceneNodeSpec_fbs},
    };
    inline const fb::TableDesc SceneSpec_fbs = {"SceneSpec", SceneSpec_fbs_fields, 7};

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

    inline flatbuffers::Offset<::hgl::asset::schema::SceneTransformSpec>
    EncodeSceneTransformSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneTransformSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneTransformSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::SceneTransformSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, SceneTransformSpec_fbs).o);
    }

    inline void DecodeSceneTransformSpec(const ::hgl::asset::schema::SceneTransformSpec *root, hgl::graph::SceneTransformSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            SceneTransformSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneTransformSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::SceneTextureSpec>
    EncodeSceneTextureSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneTextureSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneTextureSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::SceneTextureSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, SceneTextureSpec_fbs).o);
    }

    inline void DecodeSceneTextureSpec(const ::hgl::asset::schema::SceneTextureSpec *root, hgl::graph::SceneTextureSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            SceneTextureSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneTextureSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::SceneMaterialSpec>
    EncodeSceneMaterialSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneMaterialSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneMaterialSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::SceneMaterialSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, SceneMaterialSpec_fbs).o);
    }

    inline void DecodeSceneMaterialSpec(const ::hgl::asset::schema::SceneMaterialSpec *root, hgl::graph::SceneMaterialSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            SceneMaterialSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneMaterialSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::SceneNodeSpec>
    EncodeSceneNodeSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneNodeSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneNodeSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::SceneNodeSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, SceneNodeSpec_fbs).o);
    }

    inline void DecodeSceneNodeSpec(const ::hgl::asset::schema::SceneNodeSpec *root, hgl::graph::SceneNodeSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            SceneNodeSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneNodeSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::SceneCameraSpec>
    EncodeSceneCameraSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneCameraSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneCameraSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::SceneCameraSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, SceneCameraSpec_fbs).o);
    }

    inline void DecodeSceneCameraSpec(const ::hgl::asset::schema::SceneCameraSpec *root, hgl::graph::SceneCameraSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            SceneCameraSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneCameraSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::SceneClearColorSpec>
    EncodeSceneClearColorSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneClearColorSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneClearColorSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::SceneClearColorSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, SceneClearColorSpec_fbs).o);
    }

    inline void DecodeSceneClearColorSpec(const ::hgl::asset::schema::SceneClearColorSpec *root, hgl::graph::SceneClearColorSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            SceneClearColorSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneClearColorSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::SceneSpec>
    EncodeSceneSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::SceneSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, SceneSpec_fbs).o);
    }

    inline void DecodeSceneSpec(const ::hgl::asset::schema::SceneSpec *root, hgl::graph::SceneSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            SceneSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::SceneSpec>::Do(ar, out);
    }

}//namespace hgl::asset::generated
