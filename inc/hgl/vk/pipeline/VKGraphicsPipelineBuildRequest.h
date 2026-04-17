#pragma once

#include<hgl/type/String.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<hgl/vk/pipeline/VKGplPipelineKey.h>

namespace hgl::graph{
class ShaderMaterialProgram;
class RenderTargetFormat;
class VertexInputLayout;
struct GraphicsPipelineData;
struct GraphicsRenderState;

struct GraphicsPipelineBuildRequest
{
    const ShaderMaterialProgram *material = nullptr;
    const VertexInputLayout *vil = nullptr;
    const RenderTargetFormat *render_format = nullptr;
    const GraphicsPipelineData *pipeline_data = nullptr;
    PrimitiveType primitive = PrimitiveType::Triangles;
    bool primitive_restart = false;
    AnsiString debug_name;
};

bool IsValidGraphicsPipelineBuildRequest(const GraphicsPipelineBuildRequest &req);

GplVertexInputKey  BuildVertexInputKey(const VertexInputLayout *vil);
GplPreRasterKey    BuildPreRasterKey(const GraphicsPipelineBuildRequest &req);
GplFragmentShaderKey BuildFragmentShaderKey(const GraphicsPipelineBuildRequest &req);
GplFragmentOutputKey BuildFragmentOutputKey(const RenderTargetFormat *rf);
GplLinkedPipelineKey BuildLinkedPipelineKey(const GraphicsPipelineBuildRequest &req,
                                            const GraphicsRenderState &state_profile);
}//namespace hgl::graph
