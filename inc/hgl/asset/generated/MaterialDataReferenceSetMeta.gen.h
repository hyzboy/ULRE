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
#include <hgl/graph/asset/MaterialDataReferenceSet.h>

namespace hgl::asset::generated
{
    extern const ClassMetaInfo Class_MaterialDataReference;
    extern const ClassMetaInfo Class_MaterialDataReferenceSet;

    inline const PropertyMetaInfo Meta_MaterialDataReference[] =
    {
        { "uri", "uri", "uri", "Default", "", "OSString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataReference, uri), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataReference::uri)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "row", "row", "row", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataReference, row), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataReference::row)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_MaterialDataReference =
    {
        "MaterialDataReference", "", "",
        Meta_MaterialDataReference, (uint32_t)(sizeof(Meta_MaterialDataReference) / sizeof(Meta_MaterialDataReference[0])),
        (uint32_t)sizeof(hgl::graph::MaterialDataReference), (uint32_t)alignof(hgl::graph::MaterialDataReference)
    };

    inline const PropertyMetaInfo Meta_MaterialDataReferenceSet[] =
    {
        { "schema", "schema", "schema", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataReferenceSet, schema), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataReferenceSet::schema)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "id", "id", "id", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataReferenceSet, id), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataReferenceSet::id)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "instances", "instances", "instances", "Default", "", "ManagedArray<MaterialDataReference>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataReferenceSet, instances), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataReferenceSet::instances)), 0u, 0.0, 0.0, 0.0, &Class_MaterialDataReference, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_MaterialDataReferenceSet =
    {
        "MaterialDataReferenceSet", "", "",
        Meta_MaterialDataReferenceSet, (uint32_t)(sizeof(Meta_MaterialDataReferenceSet) / sizeof(Meta_MaterialDataReferenceSet[0])),
        (uint32_t)sizeof(hgl::graph::MaterialDataReferenceSet), (uint32_t)alignof(hgl::graph::MaterialDataReferenceSet)
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::MaterialDataReference *) { return &Class_MaterialDataReference; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::MaterialDataReferenceSet *) { return &Class_MaterialDataReferenceSet; }
} // namespace hgl::asset::generated
