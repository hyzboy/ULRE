#include <hgl/mtl/MeshTemplateComposer.h>

#include "meshgen/MeshTemplateEmitter.h"

namespace hgl::graph::mtl
{
    bool MeshTemplateComposer::Compose(
        const ComposeInput &input,
        ShaderDocument &out_document) const
    {
        return EmitMeshTemplateDocument(
            input.node_config,
            input.varying_config,
            input.position_format,
            input.mode,
            input.max_invocations,
            out_document,
            input.resolved_input_document,
            input.provider_document,
            input.stage_interface);
    }
}
