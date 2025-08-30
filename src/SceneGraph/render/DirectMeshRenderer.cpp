#include<hgl/graph/DirectMeshRenderer.h>
#include<hgl/component/OfflineMeshComponent.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/CameraInfo.h>
#include"../render/RenderAssignBuffer.h"

namespace hgl { namespace graph {

DirectMeshRenderer::DirectMeshRenderer(VulkanDevice* dev)
{
    device = dev;
    cmd_buf = nullptr;
    current_material = nullptr;
    current_pipeline = nullptr;
    camera_info = nullptr;
    
    last_data_buffer = nullptr;
    last_vdm = nullptr;
    pipeline_bound = false;
    descriptors_bound = false;
}

DirectMeshRenderer::~DirectMeshRenderer()
{
    // Nothing to cleanup as we don't own the resources
}

void DirectMeshRenderer::Begin(RenderCmdBuffer* rcb)
{
    cmd_buf = rcb;
    ResetState();
}

bool DirectMeshRenderer::BindPipeline(Pipeline* pipeline)
{
    if (!pipeline || !cmd_buf)
        return false;
        
    if (current_pipeline != pipeline)
    {
        cmd_buf->BindPipeline(pipeline);
        current_pipeline = pipeline;
        pipeline_bound = true;
        descriptors_bound = false; // Need to rebind descriptors after pipeline change
    }
    
    return true;
}

bool DirectMeshRenderer::BindMaterial(Material* material)
{
    if (!material || !cmd_buf)
        return false;
        
    if (current_material != material || !descriptors_bound)
    {
        cmd_buf->BindDescriptorSets(material);
        current_material = material;
        descriptors_bound = true;
    }
    
    return true;
}

bool DirectMeshRenderer::BindVAB(const hgl::graph::OfflineMeshComponent* component)
{
    if (!component || !cmd_buf)
        return false;
        
    Mesh* mesh = component->GetMesh();
    if (!mesh)
        return false;
        
    const MeshDataBuffer* data_buffer = mesh->GetDataBuffer();
    if (!data_buffer)
        return false;
        
    // Only rebind if data buffer changed
    if (last_data_buffer != data_buffer)
    {
        // Bind vertex attribute buffers
        if (data_buffer->vab_count > 0 && data_buffer->vab_list)
        {
            cmd_buf->BindVAB(data_buffer->vab_count, data_buffer->vab_list, data_buffer->vab_offset);
        }
        
        // Bind index buffer if present
        if (data_buffer->ibo)
        {
            cmd_buf->BindIBO(data_buffer->ibo);
        }
        
        last_data_buffer = data_buffer;
        last_vdm = data_buffer->vdm;
    }
    
    return true;
}

bool DirectMeshRenderer::BindAssignBuffer(RenderAssignBuffer* assign_buffer)
{
    if (!assign_buffer || !current_material || !cmd_buf)
        return false;
        
    assign_buffer->Bind(current_material);
    return true;
}

bool DirectMeshRenderer::RenderDirect(hgl::graph::OfflineMeshComponent* component)
{
    if (!component || !cmd_buf)
        return false;
        
    if (!component->CanRender())
        return false;
        
    Mesh* mesh = component->GetMesh();
    if (!mesh)
        return false;
        
    // Bind pipeline
    if (!BindPipeline(component->GetPipeline()))
        return false;
        
    // Bind material
    if (!BindMaterial(component->GetMaterial()))
        return false;
        
    // Bind VAB
    if (!BindVAB(component))
        return false;
        
    // Bind assign buffer
    RenderAssignBuffer* assign_buffer = component->GetAssignBuffer();
    if (assign_buffer)
    {
        if (!BindAssignBuffer(assign_buffer))
            return false;
    }
    
    // Perform the actual drawing
    if (component->HasOfflineData())
    {
        return DrawIndirect(component);
    }
    else
    {
        return DrawDirect(component);
    }
}

void DirectMeshRenderer::RenderBatch(hgl::graph::OfflineMeshComponent** components, uint32_t count)
{
    if (!components || count == 0 || !cmd_buf)
        return;
        
    for (uint32_t i = 0; i < count; i++)
    {
        RenderDirect(components[i]);
    }
}

bool DirectMeshRenderer::DrawDirect(const hgl::graph::OfflineMeshComponent* component)
{
    if (!component || !cmd_buf)
        return false;
        
    Mesh* mesh = component->GetMesh();
    if (!mesh)
        return false;
        
    const MeshRenderData* render_data = mesh->GetRenderData();
    if (!render_data)
        return false;
        
    uint32_t instance_count = component->GetInstanceCount();
    uint32_t first_instance = component->GetFirstInstance();
    
    if (instance_count == 0)
        instance_count = 1;
        
    // Use the existing Draw method from RenderCmdBuffer
    cmd_buf->Draw(last_data_buffer, render_data, instance_count, first_instance);
    
    return true;
}

bool DirectMeshRenderer::DrawIndirect(const hgl::graph::OfflineMeshComponent* component)
{
    if (!component || !cmd_buf)
        return false;
        
    uint32_t command_count = 0;
    
    // Try indexed indirect draw first
    const VkDrawIndexedIndirectCommand* indexed_commands = component->GetDrawIndexedCommands(command_count);
    if (indexed_commands && command_count > 0)
    {
        // For now, we'll fall back to direct draw with the command data since
        // we don't have an indirect buffer setup. In a full implementation,
        // you would create an indirect buffer and use DrawIndexedIndirect method.
        
        Mesh* mesh = component->GetMesh();
        if (!mesh) return false;
        
        const MeshRenderData* render_data = mesh->GetRenderData();
        if (!render_data) return false;
        
        // Use the Draw method from RenderCmdBuffer with data from the first command
        cmd_buf->Draw(last_data_buffer, render_data, 
                     indexed_commands[0].instanceCount, 
                     indexed_commands[0].firstInstance);
        return true;
    }
    
    // Try non-indexed indirect draw
    const VkDrawIndirectCommand* commands = component->GetDrawCommands(command_count);
    if (commands && command_count > 0)
    {
        // Similar fallback for non-indexed commands
        Mesh* mesh = component->GetMesh();
        if (!mesh) return false;
        
        const MeshRenderData* render_data = mesh->GetRenderData();
        if (!render_data) return false;
        
        cmd_buf->Draw(last_data_buffer, render_data,
                     commands[0].instanceCount,
                     commands[0].firstInstance);
        return true;
    }
    
    // Fallback to direct draw
    return DrawDirect(component);
}

void DirectMeshRenderer::End()
{
    // Flush any pending operations
    cmd_buf = nullptr;
}

void DirectMeshRenderer::Clear()
{
    ResetState();
    cmd_buf = nullptr;
}

void DirectMeshRenderer::ResetState()
{
    current_material = nullptr;
    current_pipeline = nullptr;
    last_data_buffer = nullptr;
    last_vdm = nullptr;
    pipeline_bound = false;
    descriptors_bound = false;
}

} }