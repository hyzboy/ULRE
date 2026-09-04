#pragma once

#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/MeshShaderMode.h>
#include <hgl/mtl/ShaderDocument.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>

namespace hgl::graph::mtl
{
    class MeshTemplateComposer
    {
    public:
        struct ComposeInput
        {
            VertexShaderNodeConfig node_config;
            MaterialVertexVaryingConfig varying_config;
            VkFormat position_format = VK_FORMAT_UNDEFINED;
            MeshShaderMode mode = MeshShaderMode::VertexPassthrough;
            uint32 max_invocations = 0;
            const ShaderDocument *resolved_input_document = nullptr;
            const ShaderDocument *provider_document = nullptr;
            const ValueArray<InterStageSemanticContractEntry>
                *stage_interface = nullptr;
        };

        bool Compose(
            const ComposeInput &input,
            ShaderDocument &out_document) const;
    };
}
