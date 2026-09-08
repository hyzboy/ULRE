#pragma once

#include <hgl/mtl/SerializedDescriptorEntry.h>
#include <hgl/mtl/DescriptorResourceCatalog.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>
#include <cstdint>
#include <vector>

namespace hgl::graph::mtl::descriptor_builder_common
{
    using namespace hgl::graph::mtl;

    // 同构 Push 的通用入表函数（T4.2）：name/struct/semantic/ssbo_type 等差异
    // 由调用方以参数给出，消除每个 Push* 重复 9 行样板。新增顶点语义时只需
    // 加一个一行包装（数据在此集中，layer 由 semantic 推导）。
    inline void PushBySpec(
        std::vector<SerializedDescriptorEntry> &v,
        const DescriptorSetType set_type,
        const char *name,
        const char *struct_name,
        const DescriptorSemantic semantic,
        const SSBOType ssbo_type,
        const uint32_t stage_flags)
    {
        v.push_back({
            set_type, stage_flags,
            name, struct_name, nullptr, semantic,
            TextureSlot::BaseColor, DefaultMaterialPrivateDataSlot, ssbo_type,
            GetDescriptorSemanticLayer(semantic)
        });
    }

inline void PushViewport(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    PushBySpec(v, DescriptorSetType::Scene,
               "viewport", "ViewportInfo", DescriptorSemantic::ViewportInfo,
               SSBOType::UserDefined, stage_flags);
}

inline void PushCamera(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    PushBySpec(v, DescriptorSetType::Scene,
               "camera", "CameraInfo", DescriptorSemantic::CameraInfo,
               SSBOType::UserDefined, stage_flags);
}

inline void PushSky(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    PushBySpec(v, DescriptorSetType::Scene,
               "sky", "SkyInfo", DescriptorSemantic::SkyInfo,
               SSBOType::UserDefined, stage_flags);
}

inline void PushMaterialColorPalette(std::vector<SerializedDescriptorEntry> &v,
                                     const uint32_t stage_flags)
{
    PushBySpec(v, DescriptorSetType::Scene,
               "color_palette", "ColorPalette", DescriptorSemantic::MaterialColorPalette,
               SSBOType::UserDefined, stage_flags);
}

inline void MergeUBODescriptor(
    std::vector<SerializedDescriptorEntry> &v,
    const DescriptorSemantic semantic,
    const uint32_t stage_flags,
    const bool has_policy = false,
    const bool required = true,
    const bool allow_fallback = false)
{
    for (auto &entry : v)
    {
        if (entry.semantic_layer != DescriptorSemanticLayer::UBO
         || entry.semantic != semantic)
            continue;

        entry.stage_flags |= stage_flags;
        if (has_policy)
        {
            entry.has_requirement_policy = true;
            entry.required = entry.required || required;
            entry.allow_fallback = entry.allow_fallback && allow_fallback;
        }
        return;
    }

    switch (semantic)
    {
    case DescriptorSemantic::ViewportInfo:
        PushViewport(v, stage_flags);
        break;
    case DescriptorSemantic::CameraInfo:
        PushCamera(v, stage_flags);
        break;
    case DescriptorSemantic::SkyInfo:
        PushSky(v, stage_flags);
        break;
    case DescriptorSemantic::MaterialColorPalette:
        PushMaterialColorPalette(v, stage_flags);
        break;
    }
    if (has_policy && !v.empty())
    {
        SerializedDescriptorEntry &entry = v.back();
        entry.has_requirement_policy = true;
        entry.required = required;
        entry.allow_fallback = allow_fallback;
    }
}

inline void AppendDefinitionUBODescriptors(
    std::vector<SerializedDescriptorEntry> &v,
    const MaterialDefinition &definition,
    const uint32_t default_stage_flags,
    const uint32_t sky_stage_flags,
    const uint32_t color_palette_stage_flags)
{
    for (const DescriptorSemantic semantic : definition.ubo_requirements)
    {
        const uint32_t stage_flags =
            semantic == DescriptorSemantic::SkyInfo
                ? sky_stage_flags
                : semantic == DescriptorSemantic::MaterialColorPalette
                    ? color_palette_stage_flags
                    : default_stage_flags;
        MergeUBODescriptor(v, semantic, stage_flags);
    }
}

    // strcmp 包装（唯一实现，原三处副本收敛于此）：
    // 带 <0x10000 指针防御——manifest/描述符条目指针损坏时不比
    // 较、返回不等（原 MaterialShaderCompiler CStrEq 的防御语义），
    // 避免对垃圾指针解引用。
    inline bool CStrEqual(const char *lhs, const char *rhs) noexcept
    {
        if (lhs && reinterpret_cast<uintptr_t>(lhs) < 0x10000u)
            return false;
        if (rhs && reinterpret_cast<uintptr_t>(rhs) < 0x10000u)
            return false;
        return lhs && rhs && std::strcmp(lhs, rhs) == 0;
    }

inline bool BuildDefinitionShaderCodeResourceManifest(
    const MaterialDefinition &definition,
    ShaderCodeResourceManifest &manifest,
    const char *const *extra_roots = nullptr,
    const uint32 extra_root_count = 0,
    const ShaderCodeModuleRegistry *registry = nullptr)
{
    const char *roots[MaxShaderCodeResourceManifestCodeModules]{};
    uint32 root_count = 0;
    for (const auto &name : definition.code_module_requirements)
    {
        if (root_count >= MaxShaderCodeResourceManifestCodeModules)
            return false;
        roots[root_count++] = name.c_str();
    }

    if (extra_root_count > 0 && !extra_roots)
        return false;
    for (uint32 i = 0; i < extra_root_count; ++i)
    {
        if (root_count >= MaxShaderCodeResourceManifestCodeModules)
            return false;
        roots[root_count++] = extra_roots[i];
    }

    return BuildShaderCodeResourceManifest(roots, root_count, manifest, registry);
}

} // namespace hgl::graph::mtl::descriptor_builder_common
