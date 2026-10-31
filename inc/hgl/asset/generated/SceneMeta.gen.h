// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   属性元数据表：供检查器 GUI、文档导出、严格键校验与通用格式后端消费。
//   它是「结构长什么样」的**唯一运行期真源**，与格式无关。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/asset/AssetObject.h>
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

    inline const char *AssetEnumName64_SceneMaterialType(const int64_t v)
    { return AssetEnumName_SceneMaterialType(static_cast<hgl::graph::SceneMaterialType>(v)); }

    inline bool AssetEnumParse64_SceneMaterialType(const char *s, int64_t &out)
    {
        hgl::graph::SceneMaterialType tmp{};
        if (!AssetEnumParse_SceneMaterialType(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_SceneCameraMode(const int64_t v)
    { return AssetEnumName_SceneCameraMode(static_cast<hgl::graph::SceneCameraMode>(v)); }

    inline bool AssetEnumParse64_SceneCameraMode(const char *s, int64_t &out)
    {
        hgl::graph::SceneCameraMode tmp{};
        if (!AssetEnumParse_SceneCameraMode(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    extern const ClassMetaInfo Class_SceneTransformSpec;
    extern const ClassMetaInfo Class_SceneTextureSpec;
    extern const ClassMetaInfo Class_SceneMaterialSpec;
    extern const ClassMetaInfo Class_SceneNodeSpec;
    extern const ClassMetaInfo Class_SceneCameraSpec;
    extern const ClassMetaInfo Class_SceneClearColorSpec;
    extern const ClassMetaInfo Class_SceneSpec;

    inline const PropertyMetaInfo Meta_SceneTransformSpec[] =
    {
        { "position", "position", "position", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 12u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "rotation_axis", "rotation_axis", "rotation_axis", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 12u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "rotation_angle_deg", "rotation_angle_deg", "rotation_angle_deg", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 24u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "scale", "scale", "scale", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 28u, 12u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_SceneTransformSpec =
    {
        "SceneTransformSpec", "", "",
        Meta_SceneTransformSpec, (uint32_t)(sizeof(Meta_SceneTransformSpec) / sizeof(Meta_SceneTransformSpec[0])),
        40u, 4u
    };

    inline const PropertyMetaInfo Meta_SceneTextureSpec[] =
    {
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "uri", "uri", "uri", "Default", "", "OSString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "required", "required", "required", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 80u, 8u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_SceneTextureSpec =
    {
        "SceneTextureSpec", "", "",
        Meta_SceneTextureSpec, (uint32_t)(sizeof(Meta_SceneTextureSpec) / sizeof(Meta_SceneTextureSpec[0])),
        88u, 8u
    };

    inline const PropertyMetaInfo Meta_SceneMaterialSpec[] =
    {
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "type", "type", "type", "Default", "", "SceneMaterialType", "hgl::graph::SceneMaterialType", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_SceneMaterialType, &AssetEnumName64_SceneMaterialType },
        { "base_color", "base_color", "base_color", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 44u, 16u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "metallic", "metallic", "metallic", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 60u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "roughness", "roughness", "roughness", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 64u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "normal_scale", "normal_scale", "normal_scale", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 68u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "fresnel", "fresnel", "fresnel", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 72u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "color", "color", "color", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 76u, 16u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "trans_color", "trans_color", "trans_color", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 92u, 20u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "textures", "textures", "textures", "Default", "", "ManagedArray<SceneTextureSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 112u, 24u, 0u, 0.0, 0.0, 0.0, &Class_SceneTextureSpec, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_SceneMaterialSpec =
    {
        "SceneMaterialSpec", "", "",
        Meta_SceneMaterialSpec, (uint32_t)(sizeof(Meta_SceneMaterialSpec) / sizeof(Meta_SceneMaterialSpec[0])),
        136u, 8u
    };

    inline const PropertyMetaInfo Meta_SceneNodeSpec[] =
    {
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "parent", "parent", "parent", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "geometry", "geometry", "geometry", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 80u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "recipe", "recipe", "recipe", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 120u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "material", "material", "material", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 160u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "mobility", "mobility", "mobility", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 200u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "visible", "visible", "visible", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 240u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "transform", "transform", "transform", "Default", "", "SceneTransformSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 244u, 44u, 0u, 0.0, 0.0, 0.0, &Class_SceneTransformSpec, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_SceneNodeSpec =
    {
        "SceneNodeSpec", "", "",
        Meta_SceneNodeSpec, (uint32_t)(sizeof(Meta_SceneNodeSpec) / sizeof(Meta_SceneNodeSpec[0])),
        288u, 8u
    };

    inline const PropertyMetaInfo Meta_SceneCameraSpec[] =
    {
        { "enabled", "enabled", "enabled", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 1u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "mode", "mode", "mode", "Default", "", "SceneCameraMode", "hgl::graph::SceneCameraMode", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 1u, 3u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_SceneCameraMode, &AssetEnumName64_SceneCameraMode },
        { "position", "position", "position", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 12u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "target", "target", "target", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 16u, 12u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "distance", "distance", "distance", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 28u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "yaw", "yaw", "yaw", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 32u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "pitch", "pitch", "pitch", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 36u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "fov_deg", "fov_deg", "fov_deg", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "near_plane", "near_plane", "near_plane", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 44u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "far_plane", "far_plane", "far_plane", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 48u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "main", "main", "main", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 52u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_SceneCameraSpec =
    {
        "SceneCameraSpec", "", "",
        Meta_SceneCameraSpec, (uint32_t)(sizeof(Meta_SceneCameraSpec) / sizeof(Meta_SceneCameraSpec[0])),
        56u, 4u
    };

    inline const PropertyMetaInfo Meta_SceneClearColorSpec[] =
    {
        { "enabled", "enabled", "enabled", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "value", "value", "value", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 16u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_SceneClearColorSpec =
    {
        "SceneClearColorSpec", "", "",
        Meta_SceneClearColorSpec, (uint32_t)(sizeof(Meta_SceneClearColorSpec) / sizeof(Meta_SceneClearColorSpec[0])),
        20u, 4u
    };

    inline const PropertyMetaInfo Meta_SceneSpec[] =
    {
        { "schema", "schema", "schema", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 8u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "description", "description", "description", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 48u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "camera", "camera", "camera", "Default", "", "SceneCameraSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 88u, 56u, 0u, 0.0, 0.0, 0.0, &Class_SceneCameraSpec, PropertyKind::None, nullptr, nullptr },
        { "clear_color", "clear_color", "clear_color", "Default", "", "SceneClearColorSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 144u, 24u, 0u, 0.0, 0.0, 0.0, &Class_SceneClearColorSpec, PropertyKind::None, nullptr, nullptr },
        { "materials", "materials", "materials", "Default", "", "ManagedArray<SceneMaterialSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 168u, 24u, 0u, 0.0, 0.0, 0.0, &Class_SceneMaterialSpec, PropertyKind::Struct, nullptr, nullptr },
        { "nodes", "nodes", "nodes", "Default", "", "ManagedArray<SceneNodeSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 192u, 24u, 0u, 0.0, 0.0, 0.0, &Class_SceneNodeSpec, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_SceneSpec =
    {
        "SceneSpec", "", "",
        Meta_SceneSpec, (uint32_t)(sizeof(Meta_SceneSpec) / sizeof(Meta_SceneSpec[0])),
        216u, 8u
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::SceneTransformSpec *) { return &Class_SceneTransformSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::SceneTextureSpec *) { return &Class_SceneTextureSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::SceneMaterialSpec *) { return &Class_SceneMaterialSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::SceneNodeSpec *) { return &Class_SceneNodeSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::SceneCameraSpec *) { return &Class_SceneCameraSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::SceneClearColorSpec *) { return &Class_SceneClearColorSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::SceneSpec *) { return &Class_SceneSpec; }
} // namespace hgl::asset::generated
