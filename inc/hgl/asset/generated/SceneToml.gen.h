// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   语义：键缺失 = 保留 C++ 类内初始值；键存在但类型/长度不符 = 解析失败。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <toml/toml.hpp>
#include <hgl/asset/AssetSerialize.h>
#include <hgl/graph/asset/SceneAssetSpec.h>

namespace hgl::asset::generated
{
    inline const char *AssetEnumName_SceneMaterialType(const hgl::graph::SceneMaterialType v)
    {
        switch (v)
        {
        case hgl::graph::SceneMaterialType::PBRSurface: return "PBRSurface";
        case hgl::graph::SceneMaterialType::EmissiveSurface: return "EmissiveSurface";
        case hgl::graph::SceneMaterialType::TransmissionSurface: return "TransmissionSurface";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_SceneMaterialType(const char *s, hgl::graph::SceneMaterialType &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "PBRSurface") == 0) { out = hgl::graph::SceneMaterialType::PBRSurface; return true; }
        if (hgl::stricmp(s, "EmissiveSurface") == 0) { out = hgl::graph::SceneMaterialType::EmissiveSurface; return true; }
        if (hgl::stricmp(s, "TransmissionSurface") == 0) { out = hgl::graph::SceneMaterialType::TransmissionSurface; return true; }
        return false;
    }

    inline const char *AssetEnumName_SceneCameraMode(const hgl::graph::SceneCameraMode v)
    {
        switch (v)
        {
        case hgl::graph::SceneCameraMode::ViewModel: return "view_model";
        case hgl::graph::SceneCameraMode::Free: return "free";
        case hgl::graph::SceneCameraMode::LookAt: return "look_at";
        case hgl::graph::SceneCameraMode::FirstPerson: return "first_person";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_SceneCameraMode(const char *s, hgl::graph::SceneCameraMode &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "view_model") == 0) { out = hgl::graph::SceneCameraMode::ViewModel; return true; }
        if (hgl::stricmp(s, "ViewModel") == 0) { out = hgl::graph::SceneCameraMode::ViewModel; return true; }
        if (hgl::stricmp(s, "free") == 0) { out = hgl::graph::SceneCameraMode::Free; return true; }
        if (hgl::stricmp(s, "Free") == 0) { out = hgl::graph::SceneCameraMode::Free; return true; }
        if (hgl::stricmp(s, "look_at") == 0) { out = hgl::graph::SceneCameraMode::LookAt; return true; }
        if (hgl::stricmp(s, "LookAt") == 0) { out = hgl::graph::SceneCameraMode::LookAt; return true; }
        if (hgl::stricmp(s, "first_person") == 0) { out = hgl::graph::SceneCameraMode::FirstPerson; return true; }
        if (hgl::stricmp(s, "FirstPerson") == 0) { out = hgl::graph::SceneCameraMode::FirstPerson; return true; }
        return false;
    }

    inline bool ReadSceneTransformSpec(const toml::value &table, hgl::graph::SceneTransformSpec &out);
    inline bool ReadSceneTextureSpec(const toml::value &table, hgl::graph::SceneTextureSpec &out);
    inline bool ReadSceneMaterialSpec(const toml::value &table, hgl::graph::SceneMaterialSpec &out);
    inline bool ReadSceneNodeSpec(const toml::value &table, hgl::graph::SceneNodeSpec &out);
    inline bool ReadSceneCameraSpec(const toml::value &table, hgl::graph::SceneCameraSpec &out);
    inline bool ReadSceneClearColorSpec(const toml::value &table, hgl::graph::SceneClearColorSpec &out);
    inline bool ReadSceneSpec(const toml::value &table, hgl::graph::SceneSpec &out);
    inline toml::value WriteSceneTransformSpec(const hgl::graph::SceneTransformSpec &in);
    inline toml::value WriteSceneTextureSpec(const hgl::graph::SceneTextureSpec &in);
    inline toml::value WriteSceneMaterialSpec(const hgl::graph::SceneMaterialSpec &in);
    inline toml::value WriteSceneNodeSpec(const hgl::graph::SceneNodeSpec &in);
    inline toml::value WriteSceneCameraSpec(const hgl::graph::SceneCameraSpec &in);
    inline toml::value WriteSceneClearColorSpec(const hgl::graph::SceneClearColorSpec &in);
    inline toml::value WriteSceneSpec(const hgl::graph::SceneSpec &in);

    inline bool ReadSceneTransformSpec(const toml::value &table, hgl::graph::SceneTransformSpec &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "position"))
        {
            if (!detail::AsFloatArray(*v, &out.position.x, 3)) return false;
        }
        if (const toml::value *v = detail::Find(table, "rotation_axis"))
        {
            if (!detail::AsFloatArray(*v, &out.rotation_axis.x, 3)) return false;
        }
        if (const toml::value *v = detail::Find(table, "rotation_angle_deg"))
        {
            if (!detail::AsFloat(*v, out.rotation_angle_deg)) return false;
        }
        if (const toml::value *v = detail::Find(table, "scale"))
        {
            if (!detail::AsFloatArray(*v, &out.scale.x, 3)) return false;
        }
        return true;
    }

    inline bool ReadSceneTextureSpec(const toml::value &table, hgl::graph::SceneTextureSpec &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "name"))
        {
            if (!detail::AsString(*v, out.name)) return false;
        }
        if (const toml::value *v = detail::Find(table, "uri"))
        {
            if (!detail::AsString(*v, out.uri)) return false;
        }
        if (const toml::value *v = detail::Find(table, "required"))
        {
            if (!detail::AsBool(*v, out.required)) return false;
        }
        return true;
    }

    inline bool ReadSceneMaterialSpec(const toml::value &table, hgl::graph::SceneMaterialSpec &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "name"))
        {
            if (!detail::AsString(*v, out.name)) return false;
        }
        if (const toml::value *v = detail::Find(table, "type"))
        {
            if ((*v).is_string())
            {
                if (!AssetEnumParse_SceneMaterialType((*v).as_string().c_str(), out.type)) return false;
            }
            else if ((*v).is_integer())
            {
                out.type = static_cast<hgl::graph::SceneMaterialType>((*v).as_integer());
            }
            else
                return false;
        }
        if (const toml::value *v = detail::Find(table, "base_color"))
        {
            if (!detail::AsFloatArray(*v, &out.base_color.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "metallic"))
        {
            if (!detail::AsFloat(*v, out.metallic)) return false;
        }
        if (const toml::value *v = detail::Find(table, "roughness"))
        {
            if (!detail::AsFloat(*v, out.roughness)) return false;
        }
        if (const toml::value *v = detail::Find(table, "normal_scale"))
        {
            if (!detail::AsFloat(*v, out.normal_scale)) return false;
        }
        if (const toml::value *v = detail::Find(table, "fresnel"))
        {
            if (!detail::AsFloat(*v, out.fresnel)) return false;
        }
        if (const toml::value *v = detail::Find(table, "color"))
        {
            if (!detail::AsFloatArray(*v, &out.color.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "trans_color"))
        {
            if (!detail::AsFloatArray(*v, &out.trans_color.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "textures"))
        {
            if (!(*v).is_array()) return false;
            {
                const auto &arr = (*v).as_array();
                out.textures.Clear();
                for (size_t i = 0; i < arr.size(); ++i)
                {
                    auto *item = out.textures.Create();
                    if (!ReadSceneTextureSpec(arr[i], *item)) return false;
                }
            }
        }
        return true;
    }

    inline bool ReadSceneNodeSpec(const toml::value &table, hgl::graph::SceneNodeSpec &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "name"))
        {
            if (!detail::AsString(*v, out.name)) return false;
        }
        if (const toml::value *v = detail::Find(table, "parent"))
        {
            if (!detail::AsString(*v, out.parent)) return false;
        }
        if (const toml::value *v = detail::Find(table, "geometry"))
        {
            if (!detail::AsString(*v, out.geometry)) return false;
        }
        if (const toml::value *v = detail::Find(table, "recipe"))
        {
            if (!detail::AsString(*v, out.recipe)) return false;
        }
        if (const toml::value *v = detail::Find(table, "material"))
        {
            if (!detail::AsString(*v, out.material)) return false;
        }
        if (const toml::value *v = detail::Find(table, "mobility"))
        {
            if (!detail::AsString(*v, out.mobility)) return false;
        }
        if (const toml::value *v = detail::Find(table, "visible"))
        {
            if (!detail::AsBool(*v, out.visible)) return false;
        }
        if (const toml::value *v = detail::Find(table, "transform"))
        {
            if (!ReadSceneTransformSpec(*v, out.transform)) return false;
        }
        return true;
    }

    inline bool ReadSceneCameraSpec(const toml::value &table, hgl::graph::SceneCameraSpec &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "enabled"))
        {
            if (!detail::AsBool(*v, out.enabled)) return false;
        }
        if (const toml::value *v = detail::Find(table, "mode"))
        {
            if ((*v).is_string())
            {
                if (!AssetEnumParse_SceneCameraMode((*v).as_string().c_str(), out.mode)) return false;
            }
            else if ((*v).is_integer())
            {
                out.mode = static_cast<hgl::graph::SceneCameraMode>((*v).as_integer());
            }
            else
                return false;
        }
        if (const toml::value *v = detail::Find(table, "position"))
        {
            if (!detail::AsFloatArray(*v, &out.position.x, 3)) return false;
        }
        if (const toml::value *v = detail::Find(table, "target"))
        {
            if (!detail::AsFloatArray(*v, &out.target.x, 3)) return false;
        }
        if (const toml::value *v = detail::Find(table, "distance"))
        {
            if (!detail::AsFloat(*v, out.distance)) return false;
        }
        if (const toml::value *v = detail::Find(table, "yaw"))
        {
            if (!detail::AsFloat(*v, out.yaw)) return false;
        }
        if (const toml::value *v = detail::Find(table, "pitch"))
        {
            if (!detail::AsFloat(*v, out.pitch)) return false;
        }
        if (const toml::value *v = detail::Find(table, "fov_deg"))
        {
            if (!detail::AsFloat(*v, out.fov_deg)) return false;
        }
        if (const toml::value *v = detail::Find(table, "near_plane"))
        {
            if (!detail::AsFloat(*v, out.near_plane)) return false;
        }
        if (const toml::value *v = detail::Find(table, "far_plane"))
        {
            if (!detail::AsFloat(*v, out.far_plane)) return false;
        }
        if (const toml::value *v = detail::Find(table, "main"))
        {
            if (!detail::AsBool(*v, out.main)) return false;
        }
        return true;
    }

    inline bool ReadSceneClearColorSpec(const toml::value &table, hgl::graph::SceneClearColorSpec &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "enabled"))
        {
            if (!detail::AsBool(*v, out.enabled)) return false;
        }
        if (const toml::value *v = detail::Find(table, "value"))
        {
            if (!detail::AsFloatArray(*v, &out.value.x, 4)) return false;
        }
        return true;
    }

    inline bool ReadSceneSpec(const toml::value &table, hgl::graph::SceneSpec &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "schema"))
        {
            if (!detail::AsUnsigned(*v, out.schema)) return false;
        }
        if (const toml::value *v = detail::Find(table, "name"))
        {
            if (!detail::AsString(*v, out.name)) return false;
        }
        if (const toml::value *v = detail::Find(table, "description"))
        {
            if (!detail::AsString(*v, out.description)) return false;
        }
        if (const toml::value *v = detail::Find(table, "camera"))
        {
            if (!ReadSceneCameraSpec(*v, out.camera)) return false;
        }
        if (const toml::value *v = detail::Find(table, "clear_color"))
        {
            if (!ReadSceneClearColorSpec(*v, out.clear_color)) return false;
        }
        if (const toml::value *v = detail::Find(table, "materials"))
        {
            if (!(*v).is_array()) return false;
            {
                const auto &arr = (*v).as_array();
                out.materials.Clear();
                for (size_t i = 0; i < arr.size(); ++i)
                {
                    auto *item = out.materials.Create();
                    if (!ReadSceneMaterialSpec(arr[i], *item)) return false;
                }
            }
        }
        if (const toml::value *v = detail::Find(table, "nodes"))
        {
            if (!(*v).is_array()) return false;
            {
                const auto &arr = (*v).as_array();
                out.nodes.Clear();
                for (size_t i = 0; i < arr.size(); ++i)
                {
                    auto *item = out.nodes.Create();
                    if (!ReadSceneNodeSpec(arr[i], *item)) return false;
                }
            }
        }
        return true;
    }

    inline toml::value WriteSceneTransformSpec(const hgl::graph::SceneTransformSpec &in)
    {
        toml::table t;
        t["position"] = detail::FromFloatArray(&in.position.x, 3);
        t["rotation_axis"] = detail::FromFloatArray(&in.rotation_axis.x, 3);
        t["rotation_angle_deg"] = detail::FromFloat(in.rotation_angle_deg);
        t["scale"] = detail::FromFloatArray(&in.scale.x, 3);
        return toml::value(std::move(t));
    }

    inline toml::value WriteSceneTextureSpec(const hgl::graph::SceneTextureSpec &in)
    {
        toml::table t;
        t["name"] = detail::FromString(in.name);
        t["uri"] = detail::FromString(in.uri);
        t["required"] = detail::FromBool(in.required);
        return toml::value(std::move(t));
    }

    inline toml::value WriteSceneMaterialSpec(const hgl::graph::SceneMaterialSpec &in)
    {
        toml::table t;
        t["name"] = detail::FromString(in.name);
        {
            const char *nm = AssetEnumName_SceneMaterialType(in.type);
            t["type"] = nm
                ? toml::value(toml::value::string_type(nm))
                : detail::FromInteger((int64_t)in.type);
        }
        t["base_color"] = detail::FromFloatArray(&in.base_color.x, 4);
        t["metallic"] = detail::FromFloat(in.metallic);
        t["roughness"] = detail::FromFloat(in.roughness);
        t["normal_scale"] = detail::FromFloat(in.normal_scale);
        t["fresnel"] = detail::FromFloat(in.fresnel);
        t["color"] = detail::FromFloatArray(&in.color.x, 4);
        t["trans_color"] = detail::FromFloatArray(&in.trans_color.x, 4);
        {
            toml::array a;
            a.reserve((size_t)in.textures.GetCount());
            for (int i = 0; i < in.textures.GetCount(); ++i)
                a.push_back(toml::value(WriteSceneTextureSpec(*in.textures[i])));
            t["textures"] = toml::value(std::move(a));
        }
        return toml::value(std::move(t));
    }

    inline toml::value WriteSceneNodeSpec(const hgl::graph::SceneNodeSpec &in)
    {
        toml::table t;
        t["name"] = detail::FromString(in.name);
        t["parent"] = detail::FromString(in.parent);
        t["geometry"] = detail::FromString(in.geometry);
        t["recipe"] = detail::FromString(in.recipe);
        t["material"] = detail::FromString(in.material);
        t["mobility"] = detail::FromString(in.mobility);
        t["visible"] = detail::FromBool(in.visible);
        t["transform"] = toml::value(WriteSceneTransformSpec(in.transform));
        return toml::value(std::move(t));
    }

    inline toml::value WriteSceneCameraSpec(const hgl::graph::SceneCameraSpec &in)
    {
        toml::table t;
        t["enabled"] = detail::FromBool(in.enabled);
        {
            const char *nm = AssetEnumName_SceneCameraMode(in.mode);
            t["mode"] = nm
                ? toml::value(toml::value::string_type(nm))
                : detail::FromInteger((int64_t)in.mode);
        }
        t["position"] = detail::FromFloatArray(&in.position.x, 3);
        t["target"] = detail::FromFloatArray(&in.target.x, 3);
        t["distance"] = detail::FromFloat(in.distance);
        t["yaw"] = detail::FromFloat(in.yaw);
        t["pitch"] = detail::FromFloat(in.pitch);
        t["fov_deg"] = detail::FromFloat(in.fov_deg);
        t["near_plane"] = detail::FromFloat(in.near_plane);
        t["far_plane"] = detail::FromFloat(in.far_plane);
        t["main"] = detail::FromBool(in.main);
        return toml::value(std::move(t));
    }

    inline toml::value WriteSceneClearColorSpec(const hgl::graph::SceneClearColorSpec &in)
    {
        toml::table t;
        t["enabled"] = detail::FromBool(in.enabled);
        t["value"] = detail::FromFloatArray(&in.value.x, 4);
        return toml::value(std::move(t));
    }

    inline toml::value WriteSceneSpec(const hgl::graph::SceneSpec &in)
    {
        toml::table t;
        t["schema"] = detail::FromInteger(in.schema);
        t["name"] = detail::FromString(in.name);
        t["description"] = detail::FromString(in.description);
        t["camera"] = toml::value(WriteSceneCameraSpec(in.camera));
        t["clear_color"] = toml::value(WriteSceneClearColorSpec(in.clear_color));
        {
            toml::array a;
            a.reserve((size_t)in.materials.GetCount());
            for (int i = 0; i < in.materials.GetCount(); ++i)
                a.push_back(toml::value(WriteSceneMaterialSpec(*in.materials[i])));
            t["materials"] = toml::value(std::move(a));
        }
        {
            toml::array a;
            a.reserve((size_t)in.nodes.GetCount());
            for (int i = 0; i < in.nodes.GetCount(); ++i)
                a.push_back(toml::value(WriteSceneNodeSpec(*in.nodes[i])));
            t["nodes"] = toml::value(std::move(a));
        }
        return toml::value(std::move(t));
    }

} // namespace hgl::asset::generated
