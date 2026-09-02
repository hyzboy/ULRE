#include <hgl/mtl/MeshTemplateComposer.h>

#include "meshgen/MeshShaderAssembler.h"

namespace hgl::graph::mtl
{
    bool MeshTemplateComposer::Compose(
        const ComposeInput &input,
        ShaderDocument &out_document) const
    {
        const std::string empty;
        return GenerateMeshShaderDocument(
            input.node_config,
            input.varying_config,
            input.position_format,
            input.mode,
            input.max_invocations,
            out_document,
            input.resolved_input_glsl ? *input.resolved_input_glsl : empty,
            input.provider_glsl ? *input.provider_glsl : empty,
            input.stage_interface);
    }
}
