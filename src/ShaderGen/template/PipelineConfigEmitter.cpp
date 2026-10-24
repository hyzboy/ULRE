#include <hgl/mtl/PipelineConfigEmitter.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <cstring>
#include <string>

namespace hgl::graph::mtl
{
    namespace
    {
        // 名称超长时宁可失败：截断会产出一个指向别处的 #include，
        // 症状会从「槽位配错了」变成「GLSL 里出现陌生符号」。
        bool CopyBounded(
            char *dst,
            const int capacity,
            const char *src) noexcept
        {
            if (!dst || capacity <= 0 || !src)
                return false;
            const size_t length = std::strlen(src);
            if (length >= static_cast<size_t>(capacity))
                return false;
            std::memcpy(dst, src, length + 1);
            return true;
        }

        const RenderTemplateModuleRoot *FindModuleRoot(
            const ResolvedRenderTemplate &resolved,
            const ShaderModuleSlotRole role) noexcept
        {
            return resolved.request.FindModuleRoot(role);
        }
    }// namespace

    bool PipelineConfigEntry::operator==(const PipelineConfigEntry &other) const
    {
        return role == other.role
            && std::strcmp(module_name, other.module_name) == 0
            && std::strcmp(include_path, other.include_path) == 0;
    }

    bool EmitPipelineConfig(
        const ResolvedRenderTemplate &resolved,
        PipelineConfig &out_config) noexcept
    {
        out_config = PipelineConfig{};

        if (!resolved.definition)
            return false;

        const RenderTemplateDefinition &definition = *resolved.definition;
        if (definition.slot_count > static_cast<uint32>(PipelineConfig::MaxEntries))
            return false;

        for (uint32 index = 0; index < definition.slot_count; ++index)
        {
            const RenderTemplateSlot &slot = definition.slots[index];
            const RenderTemplateModuleRoot *root = FindModuleRoot(resolved, slot.role);

            if (!root)
            {
                // 必需槽缺失 = 硬失败。静默跳过会生成一个缺少 EvalLighting /
                // EvalSurface 等符号的 shader，把配置错误伪装成 GLSL 错误。
                if (slot.required)
                    return false;
                continue;
            }

            if (root->include_path.IsEmpty())
                return false;

            PipelineConfigEntry &entry = out_config.entries[out_config.entry_count];
            entry.role = slot.role;
            if (!CopyBounded(entry.module_name,
                             PipelineConfigEntry::MaxModuleNameBytes,
                             root->module_name.c_str()))
                return false;
            if (!CopyBounded(entry.include_path,
                             PipelineConfigEntry::MaxIncludePathBytes,
                             root->include_path.c_str()))
                return false;
            ++out_config.entry_count;
        }

        out_config.hash = ComputePipelineConfigHash(out_config);
        return out_config.hash != 0;
    }

    int FindPipelineConfigEntry(
        const PipelineConfig &config,
        const ShaderModuleSlotRole role) noexcept
    {
        for (uint32 index = 0; index < config.entry_count; ++index)
        {
            if (config.entries[index].role == role)
                return static_cast<int>(index);
        }
        return -1;
    }

    void EmitPipelineConfigText(const PipelineConfig &config, std::string &out)
    {
        for (uint32 index = 0; index < config.entry_count; ++index)
        {
            out += "#include \"";
            out += config.entries[index].include_path;
            out += "\"\n";
        }
    }

    uint64 ComputePipelineConfigHash(const PipelineConfig &config)
    {
        std::string text;
        EmitPipelineConfigText(config, text);
        if (text.empty())
            return 0;

        hgl::hash::FNV1aHasher64 hasher;
        hasher.AppendBytes(text.data(), text.size());
        return hasher;
    }
}// namespace hgl::graph::mtl
