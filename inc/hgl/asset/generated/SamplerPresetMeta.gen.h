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
#include <hgl/mtl/SamplerPreset.h>

namespace hgl::asset::generated
{
    extern const ClassMetaInfo Class_SamplerPresetSpec;
    extern const ClassMetaInfo Class_SamplerPresetFileSpec;

    inline const PropertyMetaInfo Meta_SamplerPresetSpec[] =
    {
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetSpec, name), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetSpec::name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "mag_filter", "mag_filter", "mag_filter", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetSpec, mag_filter), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetSpec::mag_filter)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "min_filter", "min_filter", "min_filter", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetSpec, min_filter), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetSpec::min_filter)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "mipmap_mode", "mipmap_mode", "mipmap_mode", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetSpec, mipmap_mode), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetSpec::mipmap_mode)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "address_mode", "address_mode", "address_mode", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetSpec, address_mode), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetSpec::address_mode)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "anisotropy", "anisotropy", "anisotropy", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetSpec, anisotropy), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetSpec::anisotropy)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "max_anisotropy", "max_anisotropy", "max_anisotropy", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetSpec, max_anisotropy), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetSpec::max_anisotropy)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "max_lod", "max_lod", "max_lod", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetSpec, max_lod), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetSpec::max_lod)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "compare_op", "compare_op", "compare_op", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetSpec, compare_op), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetSpec::compare_op)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_SamplerPresetSpec =
    {
        "SamplerPresetSpec", "", "contract/sampler.toml 里一个 [[samplers]] 条目的字段规约（文本契约层）。 由 `[group.SamplerPreset]`（src/Tools/AssetGen/asset_gen.toml）生成序列化器， 缺键 = 保留这里的类内缺省值；枚举一律用文本（空串 = 引擎默认）， 非法拼写在解析层报错（拼写真源 = hgl/mtl/VkEnumText.h）。 与 VkSamplerCreateInfo 的转换在 SamplerPresetLibrary::Load 中完成。",
        Meta_SamplerPresetSpec, (uint32_t)(sizeof(Meta_SamplerPresetSpec) / sizeof(Meta_SamplerPresetSpec[0])),
        (uint32_t)sizeof(hgl::graph::mtl::SamplerPresetSpec), (uint32_t)alignof(hgl::graph::mtl::SamplerPresetSpec)
    };

    inline const PropertyMetaInfo Meta_SamplerPresetFileSpec[] =
    {
        { "schema", "schema", "schema", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetFileSpec, schema), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetFileSpec::schema)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "samplers", "samplers", "samplers", "Default", "", "hgl::ManagedArray<SamplerPresetSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::SamplerPresetFileSpec, samplers), (uint32_t)sizeof(decltype(hgl::graph::mtl::SamplerPresetFileSpec::samplers)), 0u, 0.0, 0.0, 0.0, &Class_SamplerPresetSpec, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_SamplerPresetFileSpec =
    {
        "SamplerPresetFileSpec", "", "sampler.toml 文件根规约：schema 版本 + 采样器条目表。",
        Meta_SamplerPresetFileSpec, (uint32_t)(sizeof(Meta_SamplerPresetFileSpec) / sizeof(Meta_SamplerPresetFileSpec[0])),
        (uint32_t)sizeof(hgl::graph::mtl::SamplerPresetFileSpec), (uint32_t)alignof(hgl::graph::mtl::SamplerPresetFileSpec)
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::SamplerPresetSpec *) { return &Class_SamplerPresetSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::SamplerPresetFileSpec *) { return &Class_SamplerPresetFileSpec; }
} // namespace hgl::asset::generated
