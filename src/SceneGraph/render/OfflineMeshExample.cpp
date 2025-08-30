#include<hgl/graph/OfflineMeshExample.h>
#include<hgl/component/MeshComponent.h>
#include"../render/RenderAssignBuffer.h"
#include<hgl/graph/Mesh.h>
#include<vector>
#include<cstring>

namespace hgl { namespace graph {

OfflineBatchBuilder::OfflineBatchBuilder(VulkanDevice* dev, Material* material, Pipeline* pipeline)
{
    device = dev;
    target_material = material;
    target_pipeline = pipeline;
}

OfflineBatchBuilder::~OfflineBatchBuilder()
{
    Clear();
}

void OfflineBatchBuilder::AddMeshComponent(const hgl::graph::MeshComponent* component, uint32_t instance_count)
{
    if (!component || !component->CanRender())
        return;
        
    Mesh* mesh = component->GetMesh();
    if (!mesh)
        return;
        
    // Check if the mesh is compatible with our target material/pipeline
    if (component->GetMaterial() != target_material || 
        component->GetPipeline() != target_pipeline)
        return;
        
    BatchData data;
    data.mesh = mesh;
    data.instance_count = instance_count;
    data.first_instance = static_cast<uint32_t>(batch_items.size()); // Sequential assignment
    
    batch_items.push_back(data);
}

hgl::graph::OfflineMeshComponent* OfflineBatchBuilder::BuildOfflineMesh()
{
    if (batch_items.empty())
        return nullptr;
        
    // For this example, we'll use the first mesh as the representative
    // In a real implementation, you might want to merge multiple meshes
    Mesh* primary_mesh = batch_items[0].mesh;
    
    // Create offline component data
    OfflineMeshComponentData* data = new OfflineMeshComponentData(primary_mesh);
    
    // Build assign buffer from collected data
    RenderAssignBuffer* assign_buffer = BuildAssignBuffer();
    if (assign_buffer)
    {
        data->SetAssignBuffer(assign_buffer);
    }
    
    // Calculate total instance count
    uint32_t total_instances = 0;
    for (const auto& item : batch_items)
    {
        total_instances += item.instance_count;
    }
    
    data->SetInstanceData(total_instances, 0);
    
    // Build draw commands
    BuildDrawCommands(data);
    
    // Create component manager (in real usage, this would be managed globally)
    static OfflineMeshComponentManager manager; // Simple static for example
    
    ComponentDataPtr cdp(data);
    return new OfflineMeshComponent(cdp, &manager);
}

void OfflineBatchBuilder::Clear()
{
    batch_items.clear();
}

RenderAssignBuffer* OfflineBatchBuilder::BuildAssignBuffer()
{
    if (!device || !target_material || batch_items.empty())
        return nullptr;
        
    // Create RenderAssignBuffer
    // Note: In a real implementation, you would need to properly initialize
    // the LocalToWorld matrices and material instance data
    RenderAssignBuffer* assign_buffer = new RenderAssignBuffer(device, target_material);
    
    // TODO: In a complete implementation, you would:
    // 1. Collect all LocalToWorld matrices from the batch items
    // 2. Collect all material instance data 
    // 3. Write this data to the assign buffer
    // 4. Set up the assign data mapping
    
    return assign_buffer;
}

void OfflineBatchBuilder::BuildDrawCommands(hgl::graph::OfflineMeshComponentData* data)
{
    if (!data || batch_items.empty())
        return;
        
    // For this example, we'll create simple draw commands
    // In a real implementation, you might optimize by merging compatible draws
    
    std::vector<VkDrawIndirectCommand> draw_commands;
    std::vector<VkDrawIndexedIndirectCommand> indexed_commands;
    
    uint32_t current_first_instance = 0;
    
    for (const auto& item : batch_items)
    {
        const MeshRenderData* render_data = item.mesh->GetRenderData();
        if (!render_data)
            continue;
            
        const MeshDataBuffer* data_buffer = item.mesh->GetDataBuffer();
        if (!data_buffer)
            continue;
            
        if (data_buffer->ibo)
        {
            // Indexed draw
            VkDrawIndexedIndirectCommand cmd;
            cmd.indexCount = render_data->index_count;
            cmd.instanceCount = item.instance_count;
            cmd.firstIndex = render_data->first_index;
            cmd.vertexOffset = render_data->vertex_offset;
            cmd.firstInstance = current_first_instance;
            
            indexed_commands.push_back(cmd);
        }
        else
        {
            // Non-indexed draw
            VkDrawIndirectCommand cmd;
            cmd.vertexCount = render_data->vertex_count;
            cmd.instanceCount = item.instance_count;
            cmd.firstVertex = render_data->first_vertex;
            cmd.firstInstance = current_first_instance;
            
            draw_commands.push_back(cmd);
        }
        
        current_first_instance += item.instance_count;
    }
    
    // Set the appropriate commands on the data
    if (!indexed_commands.empty())
    {
        // In a real implementation, you would allocate and copy this data
        // For this example, we'll just set the pointer (memory management needed)
        VkDrawIndexedIndirectCommand* cmd_data = new VkDrawIndexedIndirectCommand[indexed_commands.size()];
        memcpy(cmd_data, indexed_commands.data(), indexed_commands.size() * sizeof(VkDrawIndexedIndirectCommand));
        data->SetDrawIndexedCommands(cmd_data, static_cast<uint32_t>(indexed_commands.size()));
    }
    else if (!draw_commands.empty())
    {
        VkDrawIndirectCommand* cmd_data = new VkDrawIndirectCommand[draw_commands.size()];
        memcpy(cmd_data, draw_commands.data(), draw_commands.size() * sizeof(VkDrawIndirectCommand));
        data->SetDrawCommands(cmd_data, static_cast<uint32_t>(draw_commands.size()));
    }
}

} }