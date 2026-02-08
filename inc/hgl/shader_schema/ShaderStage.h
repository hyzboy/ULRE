#pragma once

#include<hgl/shader_schema/VkTypes.h>

VK_NAMESPACE_BEGIN

enum class ShaderStage:uint32_t
{
    Vertex                      = VK_SHADER_STAGE_VERTEX_BIT,
    TessControl                 = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    TessEval                    = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    Geometry                    = VK_SHADER_STAGE_GEOMETRY_BIT,
    Fragment                    = VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute                     = VK_SHADER_STAGE_COMPUTE_BIT,
    Task                        = VK_SHADER_STAGE_TASK_BIT_EXT,
    Mesh                        = VK_SHADER_STAGE_MESH_BIT_EXT,
    ClusterCulling              = VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI,

    VertexFragment              = Vertex | Fragment,
    VertexGeometryFragment      = Vertex | Geometry | Fragment,
    Tessellation                = TessControl | TessEval,

    TaskMesh                    = Task | Mesh,
    TaskMeshFragment            = Task | Mesh | Fragment,

    AllGraphics                 = VK_SHADER_STAGE_ALL_GRAPHICS,

    ENUM_CLASS_RANGE(Vertex,ClusterCulling)
};

VK_NAMESPACE_END

// Backward compatibility aliases for hgl::graph
namespace hgl::graph
{
    using hgl::shader_schema::ShaderStage;
}//namespace hgl::graph
