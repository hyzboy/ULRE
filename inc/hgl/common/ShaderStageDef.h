#pragma once

#include <vulkan/vulkan.h>

namespace hgl::graph
{
    enum class ShaderStage:uint32_t
    {
        Fragment                    = VK_SHADER_STAGE_FRAGMENT_BIT,
        Compute                     = VK_SHADER_STAGE_COMPUTE_BIT,
        Task                        = VK_SHADER_STAGE_TASK_BIT_EXT,
        Mesh                        = VK_SHADER_STAGE_MESH_BIT_EXT,
        ClusterCulling              = VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI,

        MeshFragment                = Mesh | Fragment,

        TaskMesh                    = Task | Mesh,
        TaskMeshFragment            = Task | Mesh | Fragment,

        AllGraphics                 = VK_SHADER_STAGE_ALL_GRAPHICS
    };

    // 顶点处理阶段（MeshShader 方向：彻底废弃 VS/Tess/Geometry）——mesh shader 使用
    // descriptor（顶点数据 SSBO / L2W / camera / palette 等），stage 位须覆盖 mesh。
    // 注意 VK_SHADER_STAGE_ALL_GRAPHICS 不含 MESH/TASK 位，必须显式追加。
    constexpr uint32_t kAllGraphicsOrMesh =
        VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MESH_BIT_EXT;
}
