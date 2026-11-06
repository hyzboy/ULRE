// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   属性元数据表：供检查器 GUI、文档导出、严格键校验与通用格式后端消费。
//   它是「结构长什么样」的**唯一运行期真源**，与格式无关。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <cstddef>

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/LevelSpec.h>

namespace hgl::asset::generated
{
    inline const char *AssetEnumName_LevelMaterialType(const hgl::graph::LevelMaterialType v)
    {
        switch (v)
        {
        case hgl::graph::LevelMaterialType::PBRSurface: return "PBRSurface";
        case hgl::graph::LevelMaterialType::EmissiveSurface: return "EmissiveSurface";
        case hgl::graph::LevelMaterialType::TransmissionSurface: return "TransmissionSurface";
        case hgl::graph::LevelMaterialType::PBRAdvancedSurface: return "PBRAdvancedSurface";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_LevelMaterialType(const char *s, hgl::graph::LevelMaterialType &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "PBRSurface") == 0) { out = hgl::graph::LevelMaterialType::PBRSurface; return true; }
        if (hgl::stricmp(s, "EmissiveSurface") == 0) { out = hgl::graph::LevelMaterialType::EmissiveSurface; return true; }
        if (hgl::stricmp(s, "TransmissionSurface") == 0) { out = hgl::graph::LevelMaterialType::TransmissionSurface; return true; }
        if (hgl::stricmp(s, "PBRAdvancedSurface") == 0) { out = hgl::graph::LevelMaterialType::PBRAdvancedSurface; return true; }
        return false;
    }

    inline const char *AssetEnumName_LevelCameraMode(const hgl::graph::LevelCameraMode v)
    {
        switch (v)
        {
        case hgl::graph::LevelCameraMode::ViewModel: return "view_model";
        case hgl::graph::LevelCameraMode::Free: return "free";
        case hgl::graph::LevelCameraMode::LookAt: return "look_at";
        case hgl::graph::LevelCameraMode::FirstPerson: return "first_person";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_LevelCameraMode(const char *s, hgl::graph::LevelCameraMode &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "view_model") == 0) { out = hgl::graph::LevelCameraMode::ViewModel; return true; }
        if (hgl::stricmp(s, "ViewModel") == 0) { out = hgl::graph::LevelCameraMode::ViewModel; return true; }
        if (hgl::stricmp(s, "free") == 0) { out = hgl::graph::LevelCameraMode::Free; return true; }
        if (hgl::stricmp(s, "Free") == 0) { out = hgl::graph::LevelCameraMode::Free; return true; }
        if (hgl::stricmp(s, "look_at") == 0) { out = hgl::graph::LevelCameraMode::LookAt; return true; }
        if (hgl::stricmp(s, "LookAt") == 0) { out = hgl::graph::LevelCameraMode::LookAt; return true; }
        if (hgl::stricmp(s, "first_person") == 0) { out = hgl::graph::LevelCameraMode::FirstPerson; return true; }
        if (hgl::stricmp(s, "FirstPerson") == 0) { out = hgl::graph::LevelCameraMode::FirstPerson; return true; }
        return false;
    }

    inline const char *AssetEnumName64_LevelMaterialType(const int64_t v)
    { return AssetEnumName_LevelMaterialType(static_cast<hgl::graph::LevelMaterialType>(v)); }

    inline bool AssetEnumParse64_LevelMaterialType(const char *s, int64_t &out)
    {
        hgl::graph::LevelMaterialType tmp{};
        if (!AssetEnumParse_LevelMaterialType(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_LevelCameraMode(const int64_t v)
    { return AssetEnumName_LevelCameraMode(static_cast<hgl::graph::LevelCameraMode>(v)); }

    inline bool AssetEnumParse64_LevelCameraMode(const char *s, int64_t &out)
    {
        hgl::graph::LevelCameraMode tmp{};
        if (!AssetEnumParse_LevelCameraMode(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    extern const ClassMetaInfo Class_LevelTransformSpec;
    extern const ClassMetaInfo Class_LevelTextureSpec;
    extern const ClassMetaInfo Class_LevelMaterialSpec;
    extern const ClassMetaInfo Class_LevelNodeSpec;
    extern const ClassMetaInfo Class_LevelCameraSpec;
    extern const ClassMetaInfo Class_LevelClearColorSpec;
    extern const ClassMetaInfo Class_LevelSpec;

    inline const PropertyMetaInfo Meta_LevelTransformSpec[] =
    {
        { "position", "position", "position", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelTransformSpec, position), (uint32_t)sizeof(decltype(hgl::graph::LevelTransformSpec::position)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "rotation_axis", "rotation_axis", "rotation_axis", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelTransformSpec, rotation_axis), (uint32_t)sizeof(decltype(hgl::graph::LevelTransformSpec::rotation_axis)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "rotation_angle_deg", "rotation_angle_deg", "rotation_angle_deg", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelTransformSpec, rotation_angle_deg), (uint32_t)sizeof(decltype(hgl::graph::LevelTransformSpec::rotation_angle_deg)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "scale", "scale", "scale", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelTransformSpec, scale), (uint32_t)sizeof(decltype(hgl::graph::LevelTransformSpec::scale)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_LevelTransformSpec =
    {
        "LevelTransformSpec", "", "",
        Meta_LevelTransformSpec, (uint32_t)(sizeof(Meta_LevelTransformSpec) / sizeof(Meta_LevelTransformSpec[0])),
        (uint32_t)sizeof(hgl::graph::LevelTransformSpec), (uint32_t)alignof(hgl::graph::LevelTransformSpec)
    };

    inline const PropertyMetaInfo Meta_LevelTextureSpec[] =
    {
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelTextureSpec, name), (uint32_t)sizeof(decltype(hgl::graph::LevelTextureSpec::name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "uri", "uri", "uri", "Default", "", "OSString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelTextureSpec, uri), (uint32_t)sizeof(decltype(hgl::graph::LevelTextureSpec::uri)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "required", "required", "required", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelTextureSpec, required), (uint32_t)sizeof(decltype(hgl::graph::LevelTextureSpec::required)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_LevelTextureSpec =
    {
        "LevelTextureSpec", "", "",
        Meta_LevelTextureSpec, (uint32_t)(sizeof(Meta_LevelTextureSpec) / sizeof(Meta_LevelTextureSpec[0])),
        (uint32_t)sizeof(hgl::graph::LevelTextureSpec), (uint32_t)alignof(hgl::graph::LevelTextureSpec)
    };

    inline const PropertyMetaInfo Meta_LevelMaterialSpec[] =
    {
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, name), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "type", "type", "type", "Default", "", "LevelMaterialType", "hgl::graph::LevelMaterialType", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, type), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::type)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_LevelMaterialType, &AssetEnumName64_LevelMaterialType },
        { "base_color", "base_color", "base_color", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, base_color), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::base_color)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "metallic", "metallic", "metallic", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, metallic), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::metallic)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "roughness", "roughness", "roughness", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, roughness), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::roughness)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "normal_scale", "normal_scale", "normal_scale", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, normal_scale), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::normal_scale)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "fresnel", "fresnel", "fresnel", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, fresnel), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::fresnel)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "color", "color", "color", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, color), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::color)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "trans_color", "trans_color", "trans_color", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, trans_color), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::trans_color)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "clearcoat", "clearcoat", "clearcoat", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, clearcoat), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::clearcoat)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "clearcoat_roughness", "clearcoat_roughness", "clearcoat_roughness", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, clearcoat_roughness), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::clearcoat_roughness)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "sheen_color", "sheen_color", "sheen_color", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, sheen_color), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::sheen_color)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "sheen_roughness", "sheen_roughness", "sheen_roughness", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, sheen_roughness), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::sheen_roughness)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "anisotropy", "anisotropy", "anisotropy", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, anisotropy), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::anisotropy)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "subsurface_power", "subsurface_power", "subsurface_power", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, subsurface_power), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::subsurface_power)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "thickness", "thickness", "thickness", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, thickness), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::thickness)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "textures", "textures", "textures", "Default", "", "ManagedArray<LevelTextureSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelMaterialSpec, textures), (uint32_t)sizeof(decltype(hgl::graph::LevelMaterialSpec::textures)), 0u, 0.0, 0.0, 0.0, &Class_LevelTextureSpec, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_LevelMaterialSpec =
    {
        "LevelMaterialSpec", "", "",
        Meta_LevelMaterialSpec, (uint32_t)(sizeof(Meta_LevelMaterialSpec) / sizeof(Meta_LevelMaterialSpec[0])),
        (uint32_t)sizeof(hgl::graph::LevelMaterialSpec), (uint32_t)alignof(hgl::graph::LevelMaterialSpec)
    };

    inline const PropertyMetaInfo Meta_LevelNodeSpec[] =
    {
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelNodeSpec, name), (uint32_t)sizeof(decltype(hgl::graph::LevelNodeSpec::name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "parent", "parent", "parent", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelNodeSpec, parent), (uint32_t)sizeof(decltype(hgl::graph::LevelNodeSpec::parent)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "geometry", "geometry", "geometry", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelNodeSpec, geometry), (uint32_t)sizeof(decltype(hgl::graph::LevelNodeSpec::geometry)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "model", "model", "model", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelNodeSpec, model), (uint32_t)sizeof(decltype(hgl::graph::LevelNodeSpec::model)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "recipe", "recipe", "recipe", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelNodeSpec, recipe), (uint32_t)sizeof(decltype(hgl::graph::LevelNodeSpec::recipe)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "material", "material", "material", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelNodeSpec, material), (uint32_t)sizeof(decltype(hgl::graph::LevelNodeSpec::material)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "mobility", "mobility", "mobility", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelNodeSpec, mobility), (uint32_t)sizeof(decltype(hgl::graph::LevelNodeSpec::mobility)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "visible", "visible", "visible", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelNodeSpec, visible), (uint32_t)sizeof(decltype(hgl::graph::LevelNodeSpec::visible)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "transform", "transform", "transform", "Default", "", "LevelTransformSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelNodeSpec, transform), (uint32_t)sizeof(decltype(hgl::graph::LevelNodeSpec::transform)), 0u, 0.0, 0.0, 0.0, &Class_LevelTransformSpec, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_LevelNodeSpec =
    {
        "LevelNodeSpec", "", "",
        Meta_LevelNodeSpec, (uint32_t)(sizeof(Meta_LevelNodeSpec) / sizeof(Meta_LevelNodeSpec[0])),
        (uint32_t)sizeof(hgl::graph::LevelNodeSpec), (uint32_t)alignof(hgl::graph::LevelNodeSpec)
    };

    inline const PropertyMetaInfo Meta_LevelCameraSpec[] =
    {
        { "enabled", "enabled", "enabled", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, enabled), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::enabled)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "mode", "mode", "mode", "Default", "", "LevelCameraMode", "hgl::graph::LevelCameraMode", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, mode), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::mode)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_LevelCameraMode, &AssetEnumName64_LevelCameraMode },
        { "position", "position", "position", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, position), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::position)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "target", "target", "target", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, target), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::target)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "distance", "distance", "distance", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, distance), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::distance)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "yaw", "yaw", "yaw", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, yaw), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::yaw)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "pitch", "pitch", "pitch", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, pitch), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::pitch)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "fov_deg", "fov_deg", "fov_deg", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, fov_deg), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::fov_deg)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "near_plane", "near_plane", "near_plane", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, near_plane), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::near_plane)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "far_plane", "far_plane", "far_plane", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, far_plane), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::far_plane)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "main", "main", "main", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelCameraSpec, main), (uint32_t)sizeof(decltype(hgl::graph::LevelCameraSpec::main)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_LevelCameraSpec =
    {
        "LevelCameraSpec", "", "",
        Meta_LevelCameraSpec, (uint32_t)(sizeof(Meta_LevelCameraSpec) / sizeof(Meta_LevelCameraSpec[0])),
        (uint32_t)sizeof(hgl::graph::LevelCameraSpec), (uint32_t)alignof(hgl::graph::LevelCameraSpec)
    };

    inline const PropertyMetaInfo Meta_LevelClearColorSpec[] =
    {
        { "enabled", "enabled", "enabled", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelClearColorSpec, enabled), (uint32_t)sizeof(decltype(hgl::graph::LevelClearColorSpec::enabled)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "value", "value", "value", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelClearColorSpec, value), (uint32_t)sizeof(decltype(hgl::graph::LevelClearColorSpec::value)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_LevelClearColorSpec =
    {
        "LevelClearColorSpec", "", "",
        Meta_LevelClearColorSpec, (uint32_t)(sizeof(Meta_LevelClearColorSpec) / sizeof(Meta_LevelClearColorSpec[0])),
        (uint32_t)sizeof(hgl::graph::LevelClearColorSpec), (uint32_t)alignof(hgl::graph::LevelClearColorSpec)
    };

    inline const PropertyMetaInfo Meta_LevelSpec[] =
    {
        { "schema", "schema", "schema", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelSpec, schema), (uint32_t)sizeof(decltype(hgl::graph::LevelSpec::schema)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelSpec, name), (uint32_t)sizeof(decltype(hgl::graph::LevelSpec::name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "description", "description", "description", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelSpec, description), (uint32_t)sizeof(decltype(hgl::graph::LevelSpec::description)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "camera", "camera", "camera", "Default", "", "LevelCameraSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelSpec, camera), (uint32_t)sizeof(decltype(hgl::graph::LevelSpec::camera)), 0u, 0.0, 0.0, 0.0, &Class_LevelCameraSpec, PropertyKind::None, nullptr, nullptr },
        { "clear_color", "clear_color", "clear_color", "Default", "", "LevelClearColorSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelSpec, clear_color), (uint32_t)sizeof(decltype(hgl::graph::LevelSpec::clear_color)), 0u, 0.0, 0.0, 0.0, &Class_LevelClearColorSpec, PropertyKind::None, nullptr, nullptr },
        { "materials", "materials", "materials", "Default", "", "ManagedArray<LevelMaterialSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelSpec, materials), (uint32_t)sizeof(decltype(hgl::graph::LevelSpec::materials)), 0u, 0.0, 0.0, 0.0, &Class_LevelMaterialSpec, PropertyKind::Struct, nullptr, nullptr },
        { "nodes", "nodes", "nodes", "Default", "", "ManagedArray<LevelNodeSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::LevelSpec, nodes), (uint32_t)sizeof(decltype(hgl::graph::LevelSpec::nodes)), 0u, 0.0, 0.0, 0.0, &Class_LevelNodeSpec, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_LevelSpec =
    {
        "LevelSpec", "", "",
        Meta_LevelSpec, (uint32_t)(sizeof(Meta_LevelSpec) / sizeof(Meta_LevelSpec[0])),
        (uint32_t)sizeof(hgl::graph::LevelSpec), (uint32_t)alignof(hgl::graph::LevelSpec)
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::LevelTransformSpec *) { return &Class_LevelTransformSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::LevelTextureSpec *) { return &Class_LevelTextureSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::LevelMaterialSpec *) { return &Class_LevelMaterialSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::LevelNodeSpec *) { return &Class_LevelNodeSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::LevelCameraSpec *) { return &Class_LevelCameraSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::LevelClearColorSpec *) { return &Class_LevelClearColorSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::LevelSpec *) { return &Class_LevelSpec; }
} // namespace hgl::asset::generated
