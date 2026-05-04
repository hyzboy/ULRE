#pragma once

#include <vulkan/vulkan.h>

namespace hgl::graph
{
    enum class ShaderStage:uint32_t
    {
        Vertex                      = VK_SHADER_STAGE_VERTEX_BIT,
        TessControl                 = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        TessEval                    = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        Geometry                    = VK_SHADER_STAGE_GEOMETRY_BIT,
        Fragment                    = VK_SHADER_STAGE_FRAGMENT_BIT,
        Compute                     = VK_SHADER_STAGE_COMPUTE_BIT,
        ClusterCulling              = VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI,

        VertexFragment              = Vertex | Fragment,
        VertexGeometryFragment      = Vertex | Geometry | Fragment,
        Tessellation                = TessControl | TessEval,

        AllGraphics                 = VK_SHADER_STAGE_ALL_GRAPHICS
    };
}
