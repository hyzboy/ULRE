#include <hgl/shadergen/PresetShaderCompiler.h>
#include "GLSLCompiler.h"

// Vulkan shader stage bits for CompileShader()
#ifndef VK_SHADER_STAGE_VERTEX_BIT
#define VK_SHADER_STAGE_VERTEX_BIT   0x00000001
#endif
#ifndef VK_SHADER_STAGE_FRAGMENT_BIT
#define VK_SHADER_STAGE_FRAGMENT_BIT 0x00000010
#endif

namespace hgl::graph
{
    PresetShaderCompiler::PresetShaderCompiler(const CompositorAssembler &assembler)
        : assembler_(assembler)
    {}

    CompiledSPV PresetShaderCompiler::CompileOne(
        const MaterialPresetDef &preset,
        const NewShaderPermutationKey &key,
        PassType pass
    ) const
    {
        CompiledSPV result{};

        // 1. 组合 GLSL
        auto assembled = assembler_.Assemble(
            preset.surface_type,
            BlendMode::Opaque,   // 第一版简化：从 PassType 推断 BlendMode
            pass,
            key.GetQualityTier(),
            key.GetPlatform()
        );

        if (!assembled.success)
        {
            result.error_message = assembled.error_message;
            result.success = false;
            return result;
        }

        // 2. 编译 VS
        SPVData *vs_spv = CompileShader(VK_SHADER_STAGE_VERTEX_BIT, assembled.vertex_glsl.c_str());
        if (!vs_spv || !vs_spv->result)
        {
            result.error_message = "VS compile failed";
            if (vs_spv && vs_spv->log)
                result.error_message += std::string(": ") + vs_spv->log;
            if (vs_spv) FreeSPVData(vs_spv);
            result.success = false;
            return result;
        }
        result.vertex_spv.assign(vs_spv->spv_data, vs_spv->spv_data + vs_spv->spv_length);
        FreeSPVData(vs_spv);

        // 3. 编译 FS
        SPVData *fs_spv = CompileShader(VK_SHADER_STAGE_FRAGMENT_BIT, assembled.fragment_glsl.c_str());
        if (!fs_spv || !fs_spv->result)
        {
            result.error_message = "FS compile failed";
            if (fs_spv && fs_spv->log)
                result.error_message += std::string(": ") + fs_spv->log;
            if (fs_spv) FreeSPVData(fs_spv);
            result.success = false;
            return result;
        }
        result.fragment_spv.assign(fs_spv->spv_data, fs_spv->spv_data + fs_spv->spv_length);
        FreeSPVData(fs_spv);

        result.success = true;
        return result;
    }

    bool PresetShaderCompiler::CompileAll(
        const MaterialPresetDef *presets,
        size_t preset_count,
        std::map<SPVCacheKey, CompiledSPV> &out_map,
        std::string &out_error
    ) const
    {
        // 第一版：只编译 ForwardOpaque pass, PC platform, Medium quality
        const PassType passes[] = { PassType::ForwardOpaque };

        for (size_t i = 0; i < preset_count; ++i)
        {
            const auto &preset = presets[i];

            for (auto pass : passes)
            {
                NewShaderPermutationKey key;
                key.SetSurfaceType(preset.surface_type);
                key.SetQualityTier(QualityTier::Medium);
                key.SetPlatform(PlatformBackend::PC);

                SPVCacheKey cache_key;
                cache_key.preset_id  = preset.preset_id;
                cache_key.packed_key = key.packed;
                cache_key.pass_type  = pass;

                auto compiled = CompileOne(preset, key, pass);
                if (!compiled.success)
                {
                    const char *name = preset.name.c_str();
                    out_error = "Failed to compile preset '"
                              + std::string(name ? name : "(unnamed)")
                              + "': " + compiled.error_message;
                    return false;
                }

                out_map[cache_key] = std::move(compiled);
            }
        }

        return true;
    }
}
