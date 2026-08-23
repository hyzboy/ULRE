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
        TaskMeshFragment            = Task | Mesh | Fragment
    };

    // 所有实际存在的图形 stage 位（mesh shader 唯一顶点路径，VS/Tess/Geometry 已彻底废弃；
    // 引擎无 Task stage，descriptor / push constant 只声明 Mesh + Fragment）。
    // 注意：descriptor/push constant 的 stageFlags 只需包含实际访问的 stage；
    // 声明不存在的 stage 位虽合法（Vulkan 允许超集），但零兼容原则下不保留。
    constexpr uint32_t kMeshFragment =
        VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // ── shader stage 工具函数（原 VertexInputDef.h，VBO 输入类删除后迁移至此）──
    const unsigned int GetShaderCountByBits(const uint32_t bits);
    const unsigned int GetMaxShaderStage(const uint32_t bits);
    const char *GetShaderStageName(const VkShaderStageFlagBits &);
    const unsigned int GetShaderStageFlagBits(const char *,int len=0);
}
