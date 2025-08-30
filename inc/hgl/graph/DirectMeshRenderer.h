#pragma once

#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/VertexDataManager.h>

namespace hgl { namespace graph {

class RenderAssignBuffer;
struct CameraInfo;

} }

namespace hgl { namespace graph {
class OfflineMeshComponent;
}

/**
 * Direct Mesh Renderer
 * Renders OfflineMeshComponent directly without going through MaterialRenderList
 * Uses pre-computed AssignBuffer and draw commands for Instance/Indirect rendering
 */
class DirectMeshRenderer
{
private:
    VulkanDevice* device;
    RenderCmdBuffer* cmd_buf;
    
    Material* current_material;
    Pipeline* current_pipeline;
    CameraInfo* camera_info;

    // Rendering state tracking
    const MeshDataBuffer* last_data_buffer;
    const VDM* last_vdm;
    bool pipeline_bound;
    bool descriptors_bound;

    // Helper methods
    bool BindPipeline(Pipeline* pipeline);
    bool BindMaterial(Material* material);
    bool BindVAB(const hgl::graph::OfflineMeshComponent* component);
    bool BindAssignBuffer(RenderAssignBuffer* assign_buffer);

public:
    DirectMeshRenderer(VulkanDevice* dev);
    ~DirectMeshRenderer();

    /**
     * Set camera information for rendering
     */
    void SetCameraInfo(CameraInfo* ci) { camera_info = ci; }

    /**
     * Begin rendering with the specified command buffer
     */
    void Begin(RenderCmdBuffer* rcb);

    /**
     * Render a single offline mesh component directly
     */
    bool RenderDirect(hgl::graph::OfflineMeshComponent* component);

    /**
     * Render multiple offline mesh components with the same material/pipeline
     */
    void RenderBatch(hgl::graph::OfflineMeshComponent** components, uint32_t count);

    /**
     * End rendering and flush any pending commands
     */
    void End();

    /**
     * Clear all state
     */
    void Clear();

private:
    /**
     * Perform direct draw call for component
     */
    bool DrawDirect(const hgl::graph::OfflineMeshComponent* component);

    /**
     * Perform indirect draw call for component
     */
    bool DrawIndirect(const hgl::graph::OfflineMeshComponent* component);

    /**
     * Reset internal state
     */
    void ResetState();
};

}