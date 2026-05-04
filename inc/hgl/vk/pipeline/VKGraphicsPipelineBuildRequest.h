#pragma once

#include<hgl/type/String.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<hgl/vk/pipeline/VKGplPipelineKey.h>
#include<cstdint>

namespace hgl::graph{
class ShaderMaterialProgram;
class RenderTargetFormat;
class VertexInputLayout;
struct GraphicsPipelineData;
struct GraphicsRenderState;

enum class GraphicsPipelineRequestMode : uint8_t
{
    Auto = 0,
    Vertex,
};

struct GraphicsPipelineBuildRequest
{
    const ShaderMaterialProgram *material = nullptr;
    const VertexInputLayout *vil = nullptr;
    const RenderTargetFormat *render_format = nullptr;
    const GraphicsPipelineData *pipeline_data = nullptr;
    PrimitiveType primitive = PrimitiveType::Triangles;
    bool primitive_restart = false;
    GraphicsPipelineRequestMode request_mode = GraphicsPipelineRequestMode::Auto;
    AnsiString debug_name;
};

const char *GetGraphicsPipelineRequestModeName(GraphicsPipelineRequestMode mode);

bool IsVertexInputIgnored(const GraphicsPipelineBuildRequest &req);

bool IsValidGraphicsPipelineBuildRequest(const GraphicsPipelineBuildRequest &req);

GplVertexInputKey  BuildVertexInputKey(const VertexInputLayout *vil);
GplPreRasterKey    BuildPreRasterKey(const GraphicsPipelineBuildRequest &req);
GplFragmentShaderKey BuildFragmentShaderKey(const GraphicsPipelineBuildRequest &req);
GplFragmentOutputKey BuildFragmentOutputKey(const RenderTargetFormat *rf);
GplLinkedPipelineKey BuildLinkedPipelineKey(const GraphicsPipelineBuildRequest &req,
                                            const GraphicsRenderState &state_profile);
}//namespace hgl::graph
