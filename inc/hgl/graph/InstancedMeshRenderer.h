#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKVABList.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>
#include<hgl/component/InstancedMeshComponent.h>

VK_NAMESPACE_BEGIN

class VulkanDevice;
class RenderCmdBuffer;
class RenderAssignBuffer;
struct CameraInfo;

/**
 * 实例化网格渲染器
 * 直接渲染InstancedMeshComponent，绕过MaterialRenderList
 */
class InstancedMeshRenderer
{
    VulkanDevice* device;
    RenderCmdBuffer* cmd_buf;
    CameraInfo* camera_info;

private:
    // 间接绘制缓冲区
    IndirectDrawBuffer* icb_draw;
    IndirectDrawIndexedBuffer* icb_draw_indexed;

    // VAB列表管理
    VABList* vab_list;

    // 缓存的数据
    const MeshDataBuffer* last_data_buffer;
    const VDM* last_vdm;
    const MeshRenderData* last_render_data;

    void ReallocICB(uint32_t count);
    void WriteICB(VkDrawIndirectCommand* dicp, const InstancedMeshComponent* comp);
    void WriteICB(VkDrawIndexedIndirectCommand* diicp, const InstancedMeshComponent* comp);

    bool BindVAB(const InstancedMeshComponent* comp);
    void CleanupResources();

public:
    InstancedMeshRenderer(VulkanDevice* d);
    ~InstancedMeshRenderer();

    void SetCameraInfo(CameraInfo* ci) { camera_info = ci; }

    /**
     * 渲染单个InstancedMeshComponent
     */
    bool Render(hgl::graph::InstancedMeshComponent* comp, RenderCmdBuffer* rcb);

    /**
     * 批量渲染多个InstancedMeshComponent（可选实现）
     */
    void RenderBatch(const ArrayList<hgl::graph::InstancedMeshComponent*>& components, RenderCmdBuffer* rcb);
};//class InstancedMeshRenderer

VK_NAMESPACE_END