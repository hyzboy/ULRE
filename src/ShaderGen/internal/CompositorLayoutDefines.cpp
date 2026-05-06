#include <hgl/shadergen/internal/CompositorLayoutDefines.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderLayoutResolver.h>
#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/SamplerGLSLEmitter.h>
#include <string>

namespace hgl::graph::mtl::internal {

bool ApplyCompositorLayoutDefines(MaterialCreateInfo &mci)
{
    ShaderCreateInfoVertex *vert = mci.GetVertexShader();
    ShaderCreateInfo       *frag = mci.GetStageShader(ShaderStage::Fragment);

    mci.Resort();
    const ShaderLayoutContract layout = hgl::graph::BuildShaderLayoutContract(mci);
    const std::string layout_defs = hgl::graph::EmitShaderLayoutDefines(layout);
    const MaterialDescriptorDB &mdi = mci.GetDescriptorInfo();
    const std::string vert_sampler_defs = vert ? hgl::graph::EmitSimpleSamplerGLSL(mdi, ShaderStage::Vertex)   : std::string();
    const std::string frag_sampler_defs = frag ? hgl::graph::EmitSimpleSamplerGLSL(mdi, ShaderStage::Fragment) : std::string();
    const std::string frag_mit_defs = frag ? hgl::graph::EmitMaterialInstanceTextureGLSL(mdi, ShaderStage::Fragment) : std::string();

    if (!layout_defs.empty() || !vert_sampler_defs.empty() || !frag_sampler_defs.empty() || !frag_mit_defs.empty())
    {
        if (vert)
            vert->SetFinalGLSL(hgl::graph::internal::InjectAfterVersion(vert->GetFinalGLSL(), layout_defs + vert_sampler_defs));

        if (frag)
            frag->SetFinalGLSL(hgl::graph::internal::InjectAfterVersion(frag->GetFinalGLSL(), layout_defs + frag_sampler_defs + frag_mit_defs));
    }

    return true;
}

} // namespace hgl::graph::mtl::internal