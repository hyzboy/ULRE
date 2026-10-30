// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   属性元数据表：供检查器 GUI、文档导出与通用批量工具消费。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/SceneAssetSpec.h>

namespace hgl::asset::generated
{
    inline const PropertyMetaInfo Meta_SceneTransformSpec[] =
    {
        { "position", "position", "position", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 12u, 0u, 0.0, 0.0, 0.0 },
        { "rotation_axis", "rotation_axis", "rotation_axis", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 12u, 0u, 0.0, 0.0, 0.0 },
        { "rotation_angle_deg", "rotation_angle_deg", "rotation_angle_deg", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 24u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "scale", "scale", "scale", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 28u, 12u, 0u, 0.0, 0.0, 0.0 },
    };

    inline const ClassMetaInfo Class_SceneTransformSpec =
    {
        "SceneTransformSpec", "", "",
        Meta_SceneTransformSpec, (uint32_t)(sizeof(Meta_SceneTransformSpec) / sizeof(Meta_SceneTransformSpec[0])),
        40u, 4u
    };

    inline const PropertyMetaInfo Meta_SceneTextureSpec[] =
    {
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "uri", "uri", "uri", "Default", "", "OSString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "required", "required", "required", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 80u, 8u, 0u, 0.0, 0.0, 0.0 },
    };

    inline const ClassMetaInfo Class_SceneTextureSpec =
    {
        "SceneTextureSpec", "", "",
        Meta_SceneTextureSpec, (uint32_t)(sizeof(Meta_SceneTextureSpec) / sizeof(Meta_SceneTextureSpec[0])),
        88u, 8u
    };

    inline const PropertyMetaInfo Meta_SceneMaterialSpec[] =
    {
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "type", "type", "type", "Default", "", "SceneMaterialType", "hgl::graph::SceneMaterialType", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "base_color", "base_color", "base_color", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 44u, 16u, 0u, 0.0, 0.0, 0.0 },
        { "metallic", "metallic", "metallic", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 60u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "roughness", "roughness", "roughness", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 64u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "normal_scale", "normal_scale", "normal_scale", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 68u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "fresnel", "fresnel", "fresnel", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 72u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "color", "color", "color", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 76u, 16u, 0u, 0.0, 0.0, 0.0 },
        { "trans_color", "trans_color", "trans_color", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 92u, 20u, 0u, 0.0, 0.0, 0.0 },
        { "textures", "textures", "textures", "Default", "", "ManagedArray<SceneTextureSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 112u, 24u, 0u, 0.0, 0.0, 0.0 },
    };

    inline const ClassMetaInfo Class_SceneMaterialSpec =
    {
        "SceneMaterialSpec", "", "",
        Meta_SceneMaterialSpec, (uint32_t)(sizeof(Meta_SceneMaterialSpec) / sizeof(Meta_SceneMaterialSpec[0])),
        136u, 8u
    };

    inline const PropertyMetaInfo Meta_SceneNodeSpec[] =
    {
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "parent", "parent", "parent", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "geometry", "geometry", "geometry", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 80u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "recipe", "recipe", "recipe", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 120u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "material", "material", "material", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 160u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "mobility", "mobility", "mobility", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 200u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "visible", "visible", "visible", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 240u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "transform", "transform", "transform", "Default", "", "SceneTransformSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 244u, 44u, 0u, 0.0, 0.0, 0.0 },
    };

    inline const ClassMetaInfo Class_SceneNodeSpec =
    {
        "SceneNodeSpec", "", "",
        Meta_SceneNodeSpec, (uint32_t)(sizeof(Meta_SceneNodeSpec) / sizeof(Meta_SceneNodeSpec[0])),
        288u, 8u
    };

    inline const PropertyMetaInfo Meta_SceneCameraSpec[] =
    {
        { "enabled", "enabled", "enabled", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 1u, 0u, 0.0, 0.0, 0.0 },
        { "mode", "mode", "mode", "Default", "", "SceneCameraMode", "hgl::graph::SceneCameraMode", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 1u, 3u, 0u, 0.0, 0.0, 0.0 },
        { "position", "position", "position", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 12u, 0u, 0.0, 0.0, 0.0 },
        { "target", "target", "target", "Default", "", "hgl::math::Vector3f", nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 16u, 12u, 0u, 0.0, 0.0, 0.0 },
        { "distance", "distance", "distance", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 28u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "yaw", "yaw", "yaw", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 32u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "pitch", "pitch", "pitch", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 36u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "fov_deg", "fov_deg", "fov_deg", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "near_plane", "near_plane", "near_plane", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 44u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "far_plane", "far_plane", "far_plane", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 48u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "main", "main", "main", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 52u, 4u, 0u, 0.0, 0.0, 0.0 },
    };

    inline const ClassMetaInfo Class_SceneCameraSpec =
    {
        "SceneCameraSpec", "", "",
        Meta_SceneCameraSpec, (uint32_t)(sizeof(Meta_SceneCameraSpec) / sizeof(Meta_SceneCameraSpec[0])),
        56u, 4u
    };

    inline const PropertyMetaInfo Meta_SceneClearColorSpec[] =
    {
        { "enabled", "enabled", "enabled", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0 },
        { "value", "value", "value", "Default", "", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 16u, 0u, 0.0, 0.0, 0.0 },
    };

    inline const ClassMetaInfo Class_SceneClearColorSpec =
    {
        "SceneClearColorSpec", "", "",
        Meta_SceneClearColorSpec, (uint32_t)(sizeof(Meta_SceneClearColorSpec) / sizeof(Meta_SceneClearColorSpec[0])),
        20u, 4u
    };

    inline const PropertyMetaInfo Meta_SceneSpec[] =
    {
        { "schema", "schema", "schema", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 8u, 0u, 0.0, 0.0, 0.0 },
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "description", "description", "description", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 48u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "camera", "camera", "camera", "Default", "", "SceneCameraSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 88u, 56u, 0u, 0.0, 0.0, 0.0 },
        { "clear_color", "clear_color", "clear_color", "Default", "", "SceneClearColorSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 144u, 24u, 0u, 0.0, 0.0, 0.0 },
        { "materials", "materials", "materials", "Default", "", "ManagedArray<SceneMaterialSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 168u, 24u, 0u, 0.0, 0.0, 0.0 },
        { "nodes", "nodes", "nodes", "Default", "", "ManagedArray<SceneNodeSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 192u, 24u, 0u, 0.0, 0.0, 0.0 },
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
