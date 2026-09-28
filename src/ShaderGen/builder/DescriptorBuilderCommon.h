#pragma once

#include <hgl/mtl/SerializedDescriptorEntry.h>
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

    // 注：viewport 退场（S3）后，definition 侧不再有任何 UBO 声明 ⇒ PushBySpec /
    // PushViewport / MergeUBODescriptor / AppendDefinitionUBODescriptors 整组删除。
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
