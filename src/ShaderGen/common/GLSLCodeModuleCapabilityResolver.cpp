// GLSLCodeModuleCapabilityResolver.cpp — provider 图哈希与组合。
//
// 提供 GetGLSLCodeModuleProviderGraphHash / ComposeGLSLCodeModuleProviderGraph，
// BuildResolvedMaterialVertexABI 生产依赖。

#include <hgl/mtl/GLSLCodeModuleCapabilityResolver.h>

#include <hgl/util/hash/FNV1a.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        uint64 GetModuleStableID(
            const GLSLCodeModuleDefinition *module) noexcept
        {
            if (!module || !module->name)
                return 0xffffffffffffffffull;

            hgl::hash::FNV1aHasher64 h;
            h << module->name;
            return h;
        }
    }

    uint64 GetGLSLCodeModuleProviderGraphHash(
        const GLSLCodeModuleResolutionResult &result) noexcept
    {
        constexpr uint32 kMaxSelectionCount = 64;

        hgl::hash::FNV1aHasher64 h;
        h << static_cast<uint32>(result.selections.GetCount());

        uint32 order[kMaxSelectionCount] = {};
        const uint32 count = static_cast<uint32>(result.selections.GetCount());
        const uint32 bounded_count = count < kMaxSelectionCount ? count : kMaxSelectionCount;
        for (uint32 i = 0; i < bounded_count; ++i)
        {
            order[i] = i;
            for (uint32 j = i; j > 0; --j)
            {
                const auto &lhs = result.selections[static_cast<int>(order[j - 1])];
                const auto &rhs = result.selections[static_cast<int>(order[j])];
                const uint64 lhs_id = GetModuleStableID(lhs.provider);
                const uint64 rhs_id = GetModuleStableID(rhs.provider);
                const bool should_swap =
                    static_cast<uint32>(lhs.requirement) > static_cast<uint32>(rhs.requirement)
                    || (lhs.requirement == rhs.requirement && lhs_id > rhs_id);
                if (!should_swap)
                    break;
                const uint32 temp = order[j - 1];
                order[j - 1] = order[j];
                order[j] = temp;
            }
        }

        for (uint32 i = 0; i < bounded_count; ++i)
        {
            const auto &selection = result.selections[static_cast<int>(order[i])];
            h << selection.requirement;

            const auto &provider = *selection.provider;
            h << provider.name
              << provider.kind
              << provider.priority
              << provider.flags;

            h << provider.semantic_requirement_count;
            for (uint32 k = 0; k < provider.semantic_requirement_count; ++k)
                h << provider.semantic_requirements[k];

            h << provider.semantic_provide_count;
            for (uint32 k = 0; k < provider.semantic_provide_count; ++k)
                h << provider.semantic_provides[k];
        }

        if (count > kMaxSelectionCount)
            h << count;

        return h;
    }

    bool ComposeGLSLCodeModuleProviderGraph(
        const GLSLCodeModuleResolutionResult &result,
        std::string &out_glsl)
    {
        out_glsl.clear();
        if (!result.resolved)
            return false;

        const GLSLCodeModuleDefinition *emitted[64] = {};
        uint32 emitted_count = 0;
        for (int i = 0; i < result.selections.GetCount(); ++i)
        {
            const GLSLCodeModuleDefinition *provider = result.selections[i].provider;
            if (!provider || !provider->glsl_code)
                return false;

            bool already_emitted = false;
            for (uint32 k = 0; k < emitted_count; ++k)
            {
                if (emitted[k] == provider)
                {
                    already_emitted = true;
                    break;
                }
            }
            if (already_emitted)
                continue;

            if (emitted_count >= 64)
                return false;

            emitted[emitted_count++] = provider;
            out_glsl += "\n// GLSLCodeModule provider: ";
            out_glsl += provider->name ? provider->name : "Unknown";
            out_glsl += "\n";
            out_glsl += provider->glsl_code;
            if (out_glsl.empty() || out_glsl.back() != '\n')
                out_glsl += "\n";
        }

        return true;
    }
}
