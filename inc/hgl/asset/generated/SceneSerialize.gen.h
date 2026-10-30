// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
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

}//namespace hgl::asset::generated

namespace hgl::serialize
{
    template<> struct EnumTraits<hgl::graph::SceneMaterialType>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::SceneMaterialType v)
        { return hgl::asset::generated::AssetEnumName_SceneMaterialType(v); }
        static bool FromName(const char *s, hgl::graph::SceneMaterialType &out)
        { return hgl::asset::generated::AssetEnumParse_SceneMaterialType(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::SceneCameraMode>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::SceneCameraMode v)
        { return hgl::asset::generated::AssetEnumName_SceneCameraMode(v); }
        static bool FromName(const char *s, hgl::graph::SceneCameraMode &out)
        { return hgl::asset::generated::AssetEnumParse_SceneCameraMode(s, out); }
    };

    template<> struct StructSerializer<hgl::graph::SceneTransformSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "position", v.position);
            ar.Field(2, "rotation_axis", v.rotation_axis);
            ar.Field(3, "rotation_angle_deg", v.rotation_angle_deg);
            ar.Field(4, "scale", v.scale);
        }
    };

    template<> struct StructSerializer<hgl::graph::SceneTextureSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "name", v.name);
            ar.Field(2, "uri", v.uri);
            ar.Field(3, "required", v.required);
        }
    };

    template<> struct StructSerializer<hgl::graph::SceneMaterialSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "name", v.name);
            ar.Field(2, "type", v.type);
            ar.Field(3, "base_color", v.base_color);
            ar.Field(4, "metallic", v.metallic);
            ar.Field(5, "roughness", v.roughness);
            ar.Field(6, "normal_scale", v.normal_scale);
            ar.Field(7, "fresnel", v.fresnel);
            ar.Field(8, "color", v.color);
            ar.Field(9, "trans_color", v.trans_color);
            ar.Field(10, "textures", v.textures);
        }
    };

    template<> struct StructSerializer<hgl::graph::SceneNodeSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "name", v.name);
            ar.Field(2, "parent", v.parent);
            ar.Field(3, "geometry", v.geometry);
            ar.Field(4, "recipe", v.recipe);
            ar.Field(5, "material", v.material);
            ar.Field(6, "mobility", v.mobility);
            ar.Field(7, "visible", v.visible);
            ar.Field(8, "transform", v.transform);
        }
    };

    template<> struct StructSerializer<hgl::graph::SceneCameraSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "enabled", v.enabled);
            ar.Field(2, "mode", v.mode);
            ar.Field(3, "position", v.position);
            ar.Field(4, "target", v.target);
            ar.Field(5, "distance", v.distance);
            ar.Field(6, "yaw", v.yaw);
            ar.Field(7, "pitch", v.pitch);
            ar.Field(8, "fov_deg", v.fov_deg);
            ar.Field(9, "near_plane", v.near_plane);
            ar.Field(10, "far_plane", v.far_plane);
            ar.Field(11, "main", v.main);
        }
    };

    template<> struct StructSerializer<hgl::graph::SceneClearColorSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "enabled", v.enabled);
            ar.Field(2, "value", v.value);
        }
    };

    template<> struct StructSerializer<hgl::graph::SceneSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "name", v.name);
            ar.Field(3, "description", v.description);
            ar.Field(4, "camera", v.camera);
            ar.Field(5, "clear_color", v.clear_color);
            ar.Field(6, "materials", v.materials);
            ar.Field(7, "nodes", v.nodes);
        }
    };

}//namespace hgl::serialize
