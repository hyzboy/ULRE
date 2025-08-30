#pragma once

#include<hgl/component/OfflineMeshComponent.h>
#include<hgl/graph/DirectMeshRenderer.h>

/**
 * Example usage of OfflineMeshComponent and DirectMeshRenderer
 * 
 * This demonstrates how to use the offline mesh component system
 * to bypass MaterialRenderList and do direct Instance/Indirect rendering
 */

namespace hgl { namespace graph {

/**
 * Offline Batch Builder
 * Helper class to build offline batches from regular MeshComponents
 */
class OfflineBatchBuilder
{
private:
    VulkanDevice* device;
    Material* target_material;
    Pipeline* target_pipeline;
    
    struct BatchData
    {
        hgl::graph::Mesh* mesh;
        uint32_t instance_count;
        uint32_t first_instance;
    };
    
    std::vector<BatchData> batch_items;
    
public:
    OfflineBatchBuilder(VulkanDevice* dev, Material* material, Pipeline* pipeline);
    ~OfflineBatchBuilder();
    
    /**
     * Add a mesh component to the batch
     */
    void AddMeshComponent(const hgl::graph::MeshComponent* component, uint32_t instance_count = 1);
    
    /**
     * Build the offline mesh component with pre-computed data
     */
    hgl::graph::OfflineMeshComponent* BuildOfflineMesh();
    
    /**
     * Clear all batch data
     */
    void Clear();

private:
    /**
     * Build AssignBuffer from collected mesh components
     */
    RenderAssignBuffer* BuildAssignBuffer();
    
    /**
     * Build draw commands for indirect rendering
     */
    void BuildDrawCommands(hgl::graph::OfflineMeshComponentData* data);
};

/**
 * Example usage function
 */
inline void ExampleUsage()
{
    // Example code showing how to use the offline system:
    /*
    VulkanDevice* device = GetVulkanDevice();
    Material* material = GetMaterial();
    Pipeline* pipeline = GetPipeline();
    
    // Create offline batch builder
    OfflineBatchBuilder builder(device, material, pipeline);
    
    // Add regular mesh components to the batch
    for (auto* meshComp : regular_mesh_components)
    {
        builder.AddMeshComponent(meshComp, instance_count);
    }
    
    // Build offline mesh component
    OfflineMeshComponent* offlineMesh = builder.BuildOfflineMesh();
    
    // Create direct renderer
    DirectMeshRenderer renderer(device);
    renderer.SetCameraInfo(camera_info);
    
    // Render directly without MaterialRenderList
    renderer.Begin(command_buffer);
    renderer.RenderDirect(offlineMesh);
    renderer.End();
    */
}

} }