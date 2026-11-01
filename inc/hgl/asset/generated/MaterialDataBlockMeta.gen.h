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
#include <hgl/graph/asset/MaterialDataBlockSpec.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>

namespace hgl::asset::generated
{
    inline const char *AssetEnumName_GlobalSSBOType(const hgl::graph::GlobalSSBOType v)
    { return hgl::graph::GetGlobalSSBOTypeName(v); }

    inline bool AssetEnumParse_GlobalSSBOType(const char *s, hgl::graph::GlobalSSBOType &out)
    { return hgl::graph::ParseGlobalSSBOType(s, out); }

    inline const char *AssetEnumName64_GlobalSSBOType(const int64_t v)
    { return AssetEnumName_GlobalSSBOType(static_cast<hgl::graph::GlobalSSBOType>(v)); }

    inline bool AssetEnumParse64_GlobalSSBOType(const char *s, int64_t &out)
    {
        hgl::graph::GlobalSSBOType tmp{};
        if (!AssetEnumParse_GlobalSSBOType(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    extern const ClassMetaInfo Class_MaterialDataRowSpec;
    extern const ClassMetaInfo Class_MaterialDataBlockSpec;

    inline const PropertyMetaInfo Meta_MaterialDataRowSpec[] =
    {
        { "base_color", "base_color", "base_color", "PBR", "基础颜色", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataRowSpec, base_color), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataRowSpec::base_color)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "metallic", "metallic", "metallic", "PBR", "金属度", "float", nullptr, nullptr, PropertyKind::Float, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataRowSpec, metallic), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataRowSpec::metallic)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "roughness", "roughness", "roughness", "PBR", "粗糙度", "float", nullptr, nullptr, PropertyKind::Float, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataRowSpec, roughness), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataRowSpec::roughness)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "normal_scale", "normal_scale", "normal_scale", "PBR", "法线强度", "float", nullptr, nullptr, PropertyKind::Float, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataRowSpec, normal_scale), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataRowSpec::normal_scale)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "fresnel", "fresnel", "fresnel", "PBR", "菲涅尔", "float", nullptr, nullptr, PropertyKind::Float, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataRowSpec, fresnel), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataRowSpec::fresnel)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "color", "color", "color", "Emissive", "自发光颜色", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataRowSpec, color), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataRowSpec::color)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "trans_color", "trans_color", "trans_color", "Transmission", "透射色（落盘时打包成 unorm4x8）", "Color4f", nullptr, nullptr, PropertyKind::Vector4f, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataRowSpec, trans_color), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataRowSpec::trans_color)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_MaterialDataRowSpec =
    {
        "MaterialDataRowSpec", "", "一行材质数据的**授权形态**（三种内容行的并集）。 缺省值与 `PBRSurfaceRow` / `EmissiveSurfaceRow` / `TransmissionSurfaceRow` 的类内缺省值保持一致：文件里不写的键，落到 GPU 上必须与 C++ 直接构造该行 得到的字节完全相同。",
        Meta_MaterialDataRowSpec, (uint32_t)(sizeof(Meta_MaterialDataRowSpec) / sizeof(Meta_MaterialDataRowSpec[0])),
        (uint32_t)sizeof(hgl::graph::MaterialDataRowSpec), (uint32_t)alignof(hgl::graph::MaterialDataRowSpec)
    };

    inline const PropertyMetaInfo Meta_MaterialDataBlockSpec[] =
    {
        { "schema", "schema", "schema", "Block", "结构版本，只接受 1", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataBlockSpec, schema), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataBlockSpec::schema)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "name", "name", "name", "Block", "块名（注册表去重 / 诊断）", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataBlockSpec, name), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataBlockSpec::name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "target", "target", "target", "Block", "内容行类型（决定行宽与物理布局）", "GlobalSSBOType", "hgl::graph::GlobalSSBOType", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataBlockSpec, target), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataBlockSpec::target)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_GlobalSSBOType, &AssetEnumName64_GlobalSSBOType },
        { "rows", "rows", "rows", "Block", "行数据（顺序即池内行序）", "ValueArray<MaterialDataRowSpec>", nullptr, nullptr, PropertyKind::ValueArray, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::MaterialDataBlockSpec, rows), (uint32_t)sizeof(decltype(hgl::graph::MaterialDataBlockSpec::rows)), 0u, 0.0, 0.0, 0.0, &Class_MaterialDataRowSpec, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_MaterialDataBlockSpec =
    {
        "MaterialDataBlockSpec", "", "一个完整的数据块：**同一种**内容行的定长序列。 `target` 只能是三个内容行类型之一（`GetGlobalSSBOTypeStructStride != 0`）； `MaterialData`（组件行）**不在此列**——组件行说的是「引用哪一行内容」， 其中的 `surface_ssbo_id` / `surface_data_index` 是运行期行号，不能写进文件。",
        Meta_MaterialDataBlockSpec, (uint32_t)(sizeof(Meta_MaterialDataBlockSpec) / sizeof(Meta_MaterialDataBlockSpec[0])),
        (uint32_t)sizeof(hgl::graph::MaterialDataBlockSpec), (uint32_t)alignof(hgl::graph::MaterialDataBlockSpec)
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::MaterialDataRowSpec *) { return &Class_MaterialDataRowSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::MaterialDataBlockSpec *) { return &Class_MaterialDataBlockSpec; }
} // namespace hgl::asset::generated
