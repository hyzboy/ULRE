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
#include <hgl/graph/asset/MaterialTextureConfiguration.h>

namespace hgl::asset::generated
{
    extern const ClassMetaInfo Class_MaterialTextureAssetReference;
    extern const ClassMetaInfo Class_MaterialTextureConfiguration;

    inline const PropertyMetaInfo Meta_MaterialTextureAssetReference[] =
    {
        { "uri", "uri", "uri", "Default", "", "OSString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialTextureAssetReference, uri), (uint32_t)sizeof(decltype(hgl::graph::MaterialTextureAssetReference::uri)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "array_layer", "array_layer", "array_layer", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialTextureAssetReference, array_layer), (uint32_t)sizeof(decltype(hgl::graph::MaterialTextureAssetReference::array_layer)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "auto_mipmaps", "auto_mipmaps", "auto_mipmaps", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialTextureAssetReference, auto_mipmaps), (uint32_t)sizeof(decltype(hgl::graph::MaterialTextureAssetReference::auto_mipmaps)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_MaterialTextureAssetReference =
    {
        "MaterialTextureAssetReference", "", "",
        Meta_MaterialTextureAssetReference, (uint32_t)(sizeof(Meta_MaterialTextureAssetReference) / sizeof(Meta_MaterialTextureAssetReference[0])),
        (uint32_t)sizeof(hgl::graph::MaterialTextureAssetReference), (uint32_t)alignof(hgl::graph::MaterialTextureAssetReference)
    };

    inline const PropertyMetaInfo Meta_MaterialTextureConfiguration[] =
    {
        { "schema", "schema", "schema", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialTextureConfiguration, schema), (uint32_t)sizeof(decltype(hgl::graph::MaterialTextureConfiguration::schema)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "textures", "textures", "textures", "Default", "", "ManagedArray<MaterialTextureAssetReference>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialTextureConfiguration, textures), (uint32_t)sizeof(decltype(hgl::graph::MaterialTextureConfiguration::textures)), 0u, 0.0, 0.0, 0.0, &Class_MaterialTextureAssetReference, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_MaterialTextureConfiguration =
    {
        "MaterialTextureConfiguration", "", "",
        Meta_MaterialTextureConfiguration, (uint32_t)(sizeof(Meta_MaterialTextureConfiguration) / sizeof(Meta_MaterialTextureConfiguration[0])),
        (uint32_t)sizeof(hgl::graph::MaterialTextureConfiguration), (uint32_t)alignof(hgl::graph::MaterialTextureConfiguration)
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::MaterialTextureAssetReference *) { return &Class_MaterialTextureAssetReference; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::MaterialTextureConfiguration *) { return &Class_MaterialTextureConfiguration; }
} // namespace hgl::asset::generated
