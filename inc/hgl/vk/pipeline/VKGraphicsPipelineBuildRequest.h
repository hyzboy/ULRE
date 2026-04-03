#pragma once

#include<hgl/type/String.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<hgl/vk/pipeline/VKGplPipelineKey.h>

namespace hgl::graph{
class Material;
class RenderFormat;
class VertexInputLayout;
struct GraphicsPipelineData;
struct RenderStateProfile;

struct GraphicsPipelineBuildRequest
{
    const Material *material = nullptr;
    const VertexInputLayout *vil = nullptr;
    const RenderFormat *render_format = nullptr;
    const GraphicsPipelineData *pipeline_data = nullptr;
    PrimitiveType primitive = PrimitiveType::Triangles;
    bool primitive_restart = false;
    AnsiString debug_name;
};

bool IsValidGraphicsPipelineBuildRequest(const GraphicsPipelineBuildRequest &req);

GplVertexInputKey  BuildVertexInputKey(const VertexInputLayout *vil);
GplPreRasterKey    BuildPreRasterKey(const GraphicsPipelineBuildRequest &req);
GplFragmentShaderKey BuildFragmentShaderKey(const GraphicsPipelineBuildRequest &req);
GplFragmentOutputKey BuildFragmentOutputKey(const RenderFormat *rf);
GplLinkedPipelineKey BuildLinkedPipelineKey(const GraphicsPipelineBuildRequest &req,
                                            const RenderStateProfile &state_profile);
}//namespace hgl::graph
