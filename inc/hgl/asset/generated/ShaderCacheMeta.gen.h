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
#include <hgl/mtl/ShaderArtifactContract.h>
#include <hgl/mtl/ShaderComputeProgramManifest.h>
#include <hgl/mtl/ShaderLinkSpec.h>
#include <hgl/mtl/ShaderProgramManifest.h>
#include <hgl/mtl/ShaderStageKey.h>

namespace hgl::asset::generated
{
    extern const ClassMetaInfo Class_ShaderStageKey;
    extern const ClassMetaInfo Class_ShaderLinkSpec;
    extern const ClassMetaInfo Class_ShaderProgramArtifactMetadata;
    extern const ClassMetaInfo Class_ShaderProgramManifestEntry;
    extern const ClassMetaInfo Class_ShaderProgramManifestPayload;
    extern const ClassMetaInfo Class_ShaderComputeProgramManifestEntry;
    extern const ClassMetaInfo Class_ShaderComputeProgramManifestPayload;

    inline const PropertyMetaInfo Meta_ShaderStageKey[] =
    {
        { "stage", "stage", "stage", "Default", "", "ShaderStage", nullptr, nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderStageKey, stage), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderStageKey::stage)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "definition_hash", "definition_hash", "definition_hash", "Default", "", "uint64", nullptr, nullptr, PropertyKind::UInt64, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderStageKey, definition_hash), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderStageKey::definition_hash)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "compile_inputs_hash", "compile_inputs_hash", "compile_inputs_hash", "Default", "", "uint64", nullptr, nullptr, PropertyKind::UInt64, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderStageKey, compile_inputs_hash), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderStageKey::compile_inputs_hash)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "interface_hash", "interface_hash", "interface_hash", "Default", "", "uint64", nullptr, nullptr, PropertyKind::UInt64, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderStageKey, interface_hash), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderStageKey::interface_hash)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "compiler_hash", "compiler_hash", "compiler_hash", "Default", "", "uint64", nullptr, nullptr, PropertyKind::UInt64, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderStageKey, compiler_hash), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderStageKey::compiler_hash)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_ShaderStageKey =
    {
        "ShaderStageKey", "", "一个 stage 的缓存身份 = 它**实际吃进去的全部输入** + 编译器 profile。 四个哈希按「失效时能一眼看出坏在哪」分工，互不掩盖： · definition_hash     —— 主着色器文本本身，逐字节 · compile_inputs_hash —— 文本之外的**全部**输入：`#include` 闭包、 注入的 define/sampler、options 文本 （`generated/frag_options.glsl` 与 `generated/mesh_options.glsl`）、资源/模板契约 · interface_hash      —— 接口形状（varying / 输出附件 / 领域+purpose+覆盖判定） · compiler_hash       —— 编译器 profile（目标 Vulkan/SPIR-V + 代码生成代数） 历史包袱说明：options 摘要一度被编码成一行 `// domain-options-key <digest>` 注释拼在主着色器尾部，靠 definition_hash 顺带覆盖它。那是把「输入」伪装成 「文本」——读着色器的人会以为那行注释有语义，编译器只当垃圾，而任何按文本 比对的断言都得先剥掉它。现在 options 直接进 compile_inputs_hash，落盘的主 着色器就是纯粹的可编译 GLSL。 序列化派生物由 `[group.ShaderCache]`（`src/Tools/AssetGen/asset_gen.toml`） 生成；`stage` 按整数序列化（`ShaderStage` 登记为 integer_enum，位标志 组合值没有逐值拼写表可言）。",
        Meta_ShaderStageKey, (uint32_t)(sizeof(Meta_ShaderStageKey) / sizeof(Meta_ShaderStageKey[0])),
        (uint32_t)sizeof(hgl::graph::mtl::ShaderStageKey), (uint32_t)alignof(hgl::graph::mtl::ShaderStageKey)
    };

    inline const PropertyMetaInfo Meta_ShaderLinkSpec[] =
    {
        { "mesh_stage", "mesh_stage", "mesh_stage", "Default", "", "ShaderStageKey", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderLinkSpec, mesh_stage), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderLinkSpec::mesh_stage)), 0u, 0.0, 0.0, 0.0, &Class_ShaderStageKey, PropertyKind::None, nullptr, nullptr },
        { "fragment_stage", "fragment_stage", "fragment_stage", "Default", "", "ShaderStageKey", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderLinkSpec, fragment_stage), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderLinkSpec::fragment_stage)), 0u, 0.0, 0.0, 0.0, &Class_ShaderStageKey, PropertyKind::None, nullptr, nullptr },
        { "link_contract_hash", "link_contract_hash", "link_contract_hash", "Default", "", "uint64", nullptr, nullptr, PropertyKind::UInt64, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderLinkSpec, link_contract_hash), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderLinkSpec::link_contract_hash)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_ShaderLinkSpec =
    {
        "ShaderLinkSpec", "", "字段序列化由 `[group.ShaderCache]` 生成的反射表驱动（派生逻辑 `IsValid` / `BuildKey` 不参与序列化，留在本文件手写）。",
        Meta_ShaderLinkSpec, (uint32_t)(sizeof(Meta_ShaderLinkSpec) / sizeof(Meta_ShaderLinkSpec[0])),
        (uint32_t)sizeof(hgl::graph::mtl::ShaderLinkSpec), (uint32_t)alignof(hgl::graph::mtl::ShaderLinkSpec)
    };

    inline const PropertyMetaInfo Meta_ShaderProgramArtifactMetadata[] =
    {
        { "program_key_digest", "program_key_digest", "program_key_digest", "Default", "", "uint64", nullptr, nullptr, PropertyKind::UInt64, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderProgramArtifactMetadata, program_key_digest), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderProgramArtifactMetadata::program_key_digest)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "generated_source_digest", "generated_source_digest", "generated_source_digest", "Default", "", "uint64", nullptr, nullptr, PropertyKind::UInt64, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderProgramArtifactMetadata, generated_source_digest), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderProgramArtifactMetadata::generated_source_digest)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_ShaderProgramArtifactMetadata =
    {
        "ShaderProgramArtifactMetadata", "", "program artifact 的落盘元数据。 缓存命中判定要的是「这份 SPV 是不是我这个 link 的产物」，而 link 的全部 身份就是 `link.BuildKey()`。所以这里只留两项：program 键摘要，和事后校验 用的 `generated_source_digest`。 早期版本把 program 键的每个分量（module graph / interface / output contract / 两个 stage digest / compiler profile / device target）逐一平铺成 9 个字段， 逐个序列化、逐个比对。其中 `device_target_hash` 与 `compiler_profile_hash` 是同一个值（都来自 `GetShaderCompilerProfileHash`），其余 6 个都是 stage 键 的重复投影——一致性检查的真正来源只有一个：键本身。平铺只会制造「两个字段 该不该同步」的维护负担。",
        Meta_ShaderProgramArtifactMetadata, (uint32_t)(sizeof(Meta_ShaderProgramArtifactMetadata) / sizeof(Meta_ShaderProgramArtifactMetadata[0])),
        (uint32_t)sizeof(hgl::graph::mtl::ShaderProgramArtifactMetadata), (uint32_t)alignof(hgl::graph::mtl::ShaderProgramArtifactMetadata)
    };

    inline const PropertyMetaInfo Meta_ShaderProgramManifestEntry[] =
    {
        { "lookup_key", "lookup_key", "lookup_key", "Default", "", "uint64", nullptr, nullptr, PropertyKind::UInt64, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderProgramManifestEntry, lookup_key), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderProgramManifestEntry::lookup_key)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "program_link", "program_link", "program_link", "Default", "", "ShaderLinkSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderProgramManifestEntry, program_link), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderProgramManifestEntry::program_link)), 0u, 0.0, 0.0, 0.0, &Class_ShaderLinkSpec, PropertyKind::None, nullptr, nullptr },
        { "requires_recipe_runtime_rows", "requires_recipe_runtime_rows", "requires_recipe_runtime_rows", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderProgramManifestEntry, requires_recipe_runtime_rows), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderProgramManifestEntry::requires_recipe_runtime_rows)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "requires_recipe_payload_row", "requires_recipe_payload_row", "requires_recipe_payload_row", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderProgramManifestEntry, requires_recipe_payload_row), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderProgramManifestEntry::requires_recipe_payload_row)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_ShaderProgramManifestEntry =
    {
        "ShaderProgramManifestEntry", "", "一条已编译完成的程序。本结构不保存变体身份（变体身份已经哈希进 `lookup_key`，再存一份只会给缓存投毒留下第二套口径）。 字段序列化由 `[group.ShaderCache]` 生成的反射表驱动；派生判断 `IsValid()` / `IsConsistent()` 留在本文件手写。",
        Meta_ShaderProgramManifestEntry, (uint32_t)(sizeof(Meta_ShaderProgramManifestEntry) / sizeof(Meta_ShaderProgramManifestEntry[0])),
        (uint32_t)sizeof(hgl::graph::mtl::ShaderProgramManifestEntry), (uint32_t)alignof(hgl::graph::mtl::ShaderProgramManifestEntry)
    };

    inline const PropertyMetaInfo Meta_ShaderProgramManifestPayload[] =
    {
        { "format_version", "format_version", "format_version", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderProgramManifestPayload, format_version), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderProgramManifestPayload::format_version)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "entries", "entries", "entries", "Default", "", "ValueArray<ShaderProgramManifestEntry>", nullptr, nullptr, PropertyKind::ValueArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderProgramManifestPayload, entries), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderProgramManifestPayload::entries)), 0u, 0.0, 0.0, 0.0, &Class_ShaderProgramManifestEntry, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_ShaderProgramManifestPayload =
    {
        "ShaderProgramManifestPayload", "", "磁盘载荷（v3 起 = FlatBuffers 生成的 payload 的反射形状）。 编译环境戳（`compiler_profile_hash`/`codegen_generation`）不在这里 —— 它们 由外层 `ShaderArtifactFileHeader` 的 key_digest/source_digest 承载，清单类 只在 Load 时从头部取；载荷里重复一份只会给「头与体不一致」留缝。",
        Meta_ShaderProgramManifestPayload, (uint32_t)(sizeof(Meta_ShaderProgramManifestPayload) / sizeof(Meta_ShaderProgramManifestPayload[0])),
        (uint32_t)sizeof(hgl::graph::mtl::ShaderProgramManifestPayload), (uint32_t)alignof(hgl::graph::mtl::ShaderProgramManifestPayload)
    };

    inline const PropertyMetaInfo Meta_ShaderComputeProgramManifestEntry[] =
    {
        { "program_key", "program_key", "program_key", "Default", "", "uint64", nullptr, nullptr, PropertyKind::UInt64, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderComputeProgramManifestEntry, program_key), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderComputeProgramManifestEntry::program_key)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "compute_stage", "compute_stage", "compute_stage", "Default", "", "ShaderStageKey", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderComputeProgramManifestEntry, compute_stage), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderComputeProgramManifestEntry::compute_stage)), 0u, 0.0, 0.0, 0.0, &Class_ShaderStageKey, PropertyKind::None, nullptr, nullptr },
        { "push_constant_size", "push_constant_size", "push_constant_size", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderComputeProgramManifestEntry, push_constant_size), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderComputeProgramManifestEntry::push_constant_size)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_ShaderComputeProgramManifestEntry =
    {
        "ShaderComputeProgramManifestEntry", "", "字段序列化由 `[group.ShaderCache]` 生成的反射表驱动；派生判断 `IsValid()` 留在本文件手写。",
        Meta_ShaderComputeProgramManifestEntry, (uint32_t)(sizeof(Meta_ShaderComputeProgramManifestEntry) / sizeof(Meta_ShaderComputeProgramManifestEntry[0])),
        (uint32_t)sizeof(hgl::graph::mtl::ShaderComputeProgramManifestEntry), (uint32_t)alignof(hgl::graph::mtl::ShaderComputeProgramManifestEntry)
    };

    inline const PropertyMetaInfo Meta_ShaderComputeProgramManifestPayload[] =
    {
        { "format_version", "format_version", "format_version", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderComputeProgramManifestPayload, format_version), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderComputeProgramManifestPayload::format_version)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "entries", "entries", "entries", "Default", "", "ValueArray<ShaderComputeProgramManifestEntry>", nullptr, nullptr, PropertyKind::ValueArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::mtl::ShaderComputeProgramManifestPayload, entries), (uint32_t)sizeof(decltype(hgl::graph::mtl::ShaderComputeProgramManifestPayload::entries)), 0u, 0.0, 0.0, 0.0, &Class_ShaderComputeProgramManifestEntry, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_ShaderComputeProgramManifestPayload =
    {
        "ShaderComputeProgramManifestPayload", "", "磁盘载荷（v3 起 = FlatBuffers 生成的 payload 的反射形状）。 编译环境戳不在这里，由外层 `ShaderArtifactFileHeader` 承载（与 `ShaderProgramManifestPayload` 同一套理由）。",
        Meta_ShaderComputeProgramManifestPayload, (uint32_t)(sizeof(Meta_ShaderComputeProgramManifestPayload) / sizeof(Meta_ShaderComputeProgramManifestPayload[0])),
        (uint32_t)sizeof(hgl::graph::mtl::ShaderComputeProgramManifestPayload), (uint32_t)alignof(hgl::graph::mtl::ShaderComputeProgramManifestPayload)
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::ShaderStageKey *) { return &Class_ShaderStageKey; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::ShaderLinkSpec *) { return &Class_ShaderLinkSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::ShaderProgramArtifactMetadata *) { return &Class_ShaderProgramArtifactMetadata; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::ShaderProgramManifestEntry *) { return &Class_ShaderProgramManifestEntry; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::ShaderProgramManifestPayload *) { return &Class_ShaderProgramManifestPayload; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::ShaderComputeProgramManifestEntry *) { return &Class_ShaderComputeProgramManifestEntry; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::mtl::ShaderComputeProgramManifestPayload *) { return &Class_ShaderComputeProgramManifestPayload; }
} // namespace hgl::asset::generated
