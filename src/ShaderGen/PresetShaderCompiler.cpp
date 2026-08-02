#include <hgl/shadergen/PresetShaderCompiler.h>
#include "GLSLCompiler.h"
#include "common/VertexShaderAssembler.h"

// Vulkan shader stage bits for CompileShader()
#ifndef VK_SHADER_STAGE_VERTEX_BIT
#define VK_SHADER_STAGE_VERTEX_BIT   0x00000001
#endif
#ifndef VK_SHADER_STAGE_FRAGMENT_BIT
#define VK_SHADER_STAGE_FRAGMENT_BIT 0x00000010
#endif

namespace hgl::graph
{
    namespace
    {
        using namespace hgl::graph::mtl;

        static std::string BuildPresetSkyVertexShader()
        {
            return
                "#version 450\n\n"
                "#include \"common/descriptor_macros.glsl\"\n"
                "#include \"common/scene_ubo.glsl\"\n"
                "SCENE_CAMERA_UBO;\n"
                "#include \"common/l2w_ssbo.glsl\"\n"
                "L2W_SSBO;\n"
                "#include \"common/instance_rows_ssbo.glsl\"\n"
                "L2W_INDEX_ROWS_SSBO;\n\n"
                "#include \"vertex/s1_input_vec3.glsl\"\n"
                "#include \"vertex/s2_passthrough3d.glsl\"\n"
                "#include \"vertex/helpers/orient_world.glsl\"\n\n"
                "layout(location=0) out vec3 fragDirection;\n\n"
                "void main()\n"
                "{\n"
                "    vec4 local_pos = GetLocalPos();\n"
                "    vec4 world_pos = GetL2W() * local_pos;\n"
                "    fragDirection = normalize(Position);\n"
                "    gl_Position = camera.vp * world_pos;\n"
                "}\n";
        }

        static std::string BuildPresetVertexShader(const SurfaceType surface)
        {
            if (surface == SurfaceType::Sky)
                return BuildPresetSkyVertexShader();

            VertexShaderNodeConfig node_cfg = MakeDefault3DNodeConfig();
            VertexVaryingConfig varying_cfg;
            std::string extra_attrs;

            switch (surface)
            {
            case SurfaceType::Standard:
            case SurfaceType::Skin:
            case SurfaceType::Hair:
            case SurfaceType::Cloth:
            case SurfaceType::Eye:
            case SurfaceType::Foliage:
            case SurfaceType::ClearCoat:
            case SurfaceType::Water:
                varying_cfg.emit_data_index_id = true;
                varying_cfg.emit_texture_layer_id = true;
                varying_cfg.texture_layer_id_uses_data_index = true;
                varying_cfg.emit_world_pos = true;
                varying_cfg.emit_world_normal = true;
                varying_cfg.emit_uv0 = true;
                extra_attrs =
                    "layout(location=1) in vec3 Normal;\n"
                    "layout(location=2) in vec2 TexCoord;\n";
                break;
            case SurfaceType::Unlit:
            default:
                varying_cfg.emit_data_index_id = true;
                varying_cfg.emit_texture_layer_id = true;
                varying_cfg.texture_layer_id_uses_data_index = true;
                break;
            }

            return GenerateVertexShader(
                node_cfg,
                varying_cfg,
                VK_FORMAT_R32G32B32_SFLOAT,
                extra_attrs,
                "ShaderLibrary");
        }
    }

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

        const std::string vertex_glsl = BuildPresetVertexShader(preset.surface_type);

        // 2. 编译 VS
        SPVData *vs_spv = CompileShader(VK_SHADER_STAGE_VERTEX_BIT, vertex_glsl.c_str());
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
