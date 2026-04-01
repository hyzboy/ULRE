#pragma once

#include<hgl/type/String.h>
#include<hgl/vk/VKPrimitiveType.h>

namespace hgl::graph{
class Material;
class RenderFormat;
struct PipelineData;

struct GplPipelineRequest
{
    const Material *material = nullptr;
    const VIL *vil = nullptr;
    const RenderFormat *render_format = nullptr;
    const PipelineData *pipeline_data = nullptr;
    PrimitiveType primitive = PrimitiveType::Triangles;
    bool primitive_restart = false;
    AnsiString debug_name;
};
}//namespace hgl::graph
