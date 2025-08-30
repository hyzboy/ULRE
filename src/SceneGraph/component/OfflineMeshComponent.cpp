#include<hgl/component/OfflineMeshComponent.h>
#include"../render/RenderAssignBuffer.h"

COMPONENT_NAMESPACE_BEGIN

// OfflineMeshComponentData implementation
OfflineMeshComponentData::OfflineMeshComponentData() : MeshComponentData()
{
    assign_buffer = nullptr;
    instance_count = 0;
    first_instance = 0;
    draw_commands = nullptr;
    draw_indexed_commands = nullptr;
    command_count = 0;
}

OfflineMeshComponentData::OfflineMeshComponentData(Mesh* m) : MeshComponentData(m)
{
    assign_buffer = nullptr;
    instance_count = 0;
    first_instance = 0;
    draw_commands = nullptr;
    draw_indexed_commands = nullptr;
    command_count = 0;
}

OfflineMeshComponentData::~OfflineMeshComponentData()
{
    // Note: assign_buffer is managed externally, don't delete it here
    // draw_commands and draw_indexed_commands are also managed externally
}

void OfflineMeshComponentData::SetAssignBuffer(hgl::graph::RenderAssignBuffer* buffer)
{
    assign_buffer = buffer;
}

void OfflineMeshComponentData::SetInstanceData(uint32_t count, uint32_t first)
{
    instance_count = count;
    first_instance = first;
}

void OfflineMeshComponentData::SetDrawCommands(VkDrawIndirectCommand* commands, uint32_t count)
{
    draw_commands = commands;
    command_count = count;
    draw_indexed_commands = nullptr; // Clear the other type
}

void OfflineMeshComponentData::SetDrawIndexedCommands(VkDrawIndexedIndirectCommand* commands, uint32_t count)
{
    draw_indexed_commands = commands;
    command_count = count;
    draw_commands = nullptr; // Clear the other type
}

// OfflineMeshComponentManager implementation
Component* OfflineMeshComponentManager::CreateComponent(ComponentDataPtr cdp)
{
    return new OfflineMeshComponent(cdp, this);
}

Component* OfflineMeshComponentManager::CreateComponent(Mesh* mesh, hgl::graph::RenderAssignBuffer* assign_buffer)
{
    OfflineMeshComponentData* data = new OfflineMeshComponentData(mesh);
    data->SetAssignBuffer(assign_buffer);
    
    ComponentDataPtr cdp(data);
    return new OfflineMeshComponent(cdp, this);
}

// OfflineMeshComponent implementation
OfflineMeshComponent::OfflineMeshComponent(ComponentDataPtr cdp, OfflineMeshComponentManager* cm)
    : MeshComponent(cdp, cm)
{
}

Component* OfflineMeshComponent::Duplication()
{
    OfflineMeshComponent* omc = (OfflineMeshComponent*)MeshComponent::Duplication();
    
    if (!omc)
        return omc;
    
    // The offline data is shared, so no additional copying needed
    return omc;
}

OfflineMeshComponentData* OfflineMeshComponent::GetOfflineData()
{
    return dynamic_cast<OfflineMeshComponentData*>(GetData());
}

const OfflineMeshComponentData* OfflineMeshComponent::GetOfflineData() const
{
    return dynamic_cast<const OfflineMeshComponentData*>(GetData());
}

hgl::graph::RenderAssignBuffer* OfflineMeshComponent::GetAssignBuffer() const
{
    const OfflineMeshComponentData* data = GetOfflineData();
    return data ? data->assign_buffer : nullptr;
}

uint32_t OfflineMeshComponent::GetInstanceCount() const
{
    const OfflineMeshComponentData* data = GetOfflineData();
    return data ? data->instance_count : 0;
}

uint32_t OfflineMeshComponent::GetFirstInstance() const
{
    const OfflineMeshComponentData* data = GetOfflineData();
    return data ? data->first_instance : 0;
}

const VkDrawIndirectCommand* OfflineMeshComponent::GetDrawCommands(uint32_t& count) const
{
    const OfflineMeshComponentData* data = GetOfflineData();
    if (data && data->draw_commands)
    {
        count = data->command_count;
        return data->draw_commands;
    }
    count = 0;
    return nullptr;
}

const VkDrawIndexedIndirectCommand* OfflineMeshComponent::GetDrawIndexedCommands(uint32_t& count) const
{
    const OfflineMeshComponentData* data = GetOfflineData();
    if (data && data->draw_indexed_commands)
    {
        count = data->command_count;
        return data->draw_indexed_commands;
    }
    count = 0;
    return nullptr;
}

bool OfflineMeshComponent::HasOfflineData() const
{
    const OfflineMeshComponentData* data = GetOfflineData();
    return data && data->assign_buffer && (data->draw_commands || data->draw_indexed_commands);
}

const bool OfflineMeshComponent::CanRender() const
{
    // First check base class requirements
    if (!MeshComponent::CanRender())
        return false;
    
    // Then check offline-specific requirements
    return HasOfflineData();
}

COMPONENT_NAMESPACE_END