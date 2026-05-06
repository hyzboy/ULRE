#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/CompositorTemplateRouter.h>
#include <hgl/shadergen/internal/CompositorAssembleDiagnostics.h>
#include <hgl/shadergen/internal/CompositorTemplateCompose.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/mtl/PassExpansion.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <cstdio>

namespace
{
    hgl::graph::CompositorAssembler::AssembleResult MakeError(std::string message)
    {
        hgl::graph::CompositorAssembler::AssembleResult result;
        result.error_message = std::move(message);
        // success is already false by default
        return result;
    }
}

namespace hgl::graph
{
    CompositorAssembler::CompositorAssembler()
        : CompositorAssembler(GetShaderLibraryPath())
    {}

    CompositorAssembler::CompositorAssembler(const std::string &shader_library_path)
        : source_cache_(shader_library_path)
    {}

    std::string CompositorAssembler::InjectDefines(const std::string &source, const mtl::MaterialVariantKey &key) const
    {
        std::string defines;
        {
            char buf[128] = {};
            const uint32 shadow_mode = 0u;
            std::snprintf(buf,
                          sizeof(buf),
                          "#define SURFACE_TYPE %d\n"
                          "#define SHADOW_MODE %u\n",
                          static_cast<int>(key.surface_type),
                          shadow_mode);
            defines += buf;
        }

        if (key.HasAnyTextureMode(mtl::TextureSourceMode::Array))
        {
            defines += "#define TEXTURE_ARRAY_MODE\n";
        }

        if(defines.empty())
            return source;

        return hgl::graph::internal::InjectAfterVersion(source, defines);
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc
    ) const
    {
        AssembleResult result{};

        const std::string surface_rel = desc.surface_function_path.empty()
            ? hgl::graph::GetSurfaceFunctionPath(key.surface_type)
            : desc.surface_function_path;

        // VS: non-compositor custom path (e.g. 2D shader files) → ReadFileCached;
        //     empty or compositor/ prefix → key-derived generation.
        std::string vs_source;
        if (!desc.vs_template_path.empty() && !hgl::graph::IsCompositorTemplatePath(desc.vs_template_path))
        {
            std::string read_error;
            if (!source_cache_.ReadFileCached(desc.vs_template_path, vs_source, read_error))
            {
                return MakeError(internal::BuildCompositorReadFailureMessage(
                    "VS",
                    desc.vs_template_path,
                    source_cache_.GetShaderLibraryPath() + "/" + desc.vs_template_path,
                    read_error));
            }
        }
        else
        {
            vs_source = internal::BuildVertexTemplateFromKey(key);
        }

        // FS: same routing logic.
        std::string fs_source;
        if (!desc.fs_template_path.empty() && !hgl::graph::IsCompositorTemplatePath(desc.fs_template_path))
        {
            std::string read_error;
            if (!source_cache_.ReadFileCached(desc.fs_template_path, fs_source, read_error))
            {
                return MakeError(internal::BuildCompositorReadFailureMessage(
                    "FS",
                    desc.fs_template_path,
                    source_cache_.GetShaderLibraryPath() + "/" + desc.fs_template_path,
                    read_error));
            }
        }
        else
        {
            fs_source = internal::BuildFragmentTemplateFromKey(key, key.blend_mode, surface_rel);
        }

        if (vs_source.empty())
            return MakeError(internal::BuildCompositorPreprocessFailureMessage(
                "VS", desc.vs_template_path, "BuildVertexTemplateFromKey produced empty source", vs_source));

        if (fs_source.empty())
            return MakeError(internal::BuildCompositorPreprocessFailureMessage(
                "FS", desc.fs_template_path, "BuildFragmentTemplateFromKey produced empty source", fs_source));

        vs_source = InjectDefines(vs_source, key);
        fs_source = InjectDefines(fs_source, key);

        result.vertex_glsl   = std::move(vs_source);
        result.fragment_glsl = std::move(fs_source);
        result.success       = true;
        return result;
    }

    std::span<const PassType> CompositorAssembler::GetPassTypesForBlendMode(RenderAlphaMode blend)
    {
        return mtl::GetPassTypesForBlendMode(blend);
    }
}
