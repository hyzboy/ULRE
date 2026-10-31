#pragma once

/// MaterialResourceManifestBuilder.h — 材质 code-module 资源清单构建（单一职责）
///
/// 2026-10-11（S1）：本档由 DescriptorBuilderCommon.h + DefinitionDescriptorBuilder.h 合并改名而来 ——
/// 「描述符契约」层整体删除后，这里只剩「code module 资源清单」一件事，旧名（descriptor_*）已名不符实。
/// definition 侧不再声明任何描述符：行表/顶点流/材质行经 pc_root 地址 + buffer_reference 寻址，
/// viewport / sky / shadow 走 global_addresses，纹理与采样器走 bindless ⇒ 唯一集合 = bindless。

#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/mtl/GlslModule.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/RenderOptions.h>
#include <cstring>
#include <cstdint>

namespace hgl::graph::mtl::resource_manifest_builder
{
    /// 字符串比较包装（null 安全）。
    /// 2026-10-11（S1）：原实现另有「指针值 < 0x10000 即视为损坏指针」的防御分支 —— x64 上合法
    /// C 字符串不可能落在该区间（低 64KB 不可映射）⇒ 该分支恒不触发，属装饰性守卫，已删。
    inline bool CStrEqual(const char *lhs, const char *rhs) noexcept
    {
        return lhs && rhs && std::strcmp(lhs, rhs) == 0;
    }

    inline bool BuildMaterialShaderCodeResourceManifest(
        const MaterialDefinition &definition,
        ShaderCodeResourceManifest &manifest,
        const char *const *extra_roots = nullptr,
        const uint32 extra_root_count = 0,
        const GlslModuleRegistry *registry = nullptr)
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
} // namespace hgl::graph::mtl::resource_manifest_builder
