#pragma once

#include<hgl/component/MeshComponent.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>

namespace hgl { namespace graph {
class RenderAssignBuffer;
} }

COMPONENT_NAMESPACE_BEGIN

/**
 * Offline Mesh Component Data
 * Contains pre-compiled rendering data including AssignBuffer
 */
struct OfflineMeshComponentData : public MeshComponentData
{
    hgl::graph::RenderAssignBuffer* assign_buffer;
    
    uint32_t instance_count;                    ///< Number of instances for this mesh
    uint32_t first_instance;                    ///< Starting instance index
    
    // Pre-computed indirect draw commands
    VkDrawIndirectCommand* draw_commands;       ///< For non-indexed rendering
    VkDrawIndexedIndirectCommand* draw_indexed_commands; ///< For indexed rendering
    uint32_t command_count;                     ///< Number of draw commands

public:
    OfflineMeshComponentData();
    OfflineMeshComponentData(Mesh* m);
    virtual ~OfflineMeshComponentData();

    COMPONENT_DATA_CLASS_BODY(OfflineMesh)

    /**
     * Set pre-computed assign buffer
     */
    void SetAssignBuffer(hgl::graph::RenderAssignBuffer* buffer);
    
    /**
     * Set instance data for rendering
     */
    void SetInstanceData(uint32_t count, uint32_t first);
    
    /**
     * Set pre-computed draw commands
     */
    void SetDrawCommands(VkDrawIndirectCommand* commands, uint32_t count);
    void SetDrawIndexedCommands(VkDrawIndexedIndirectCommand* commands, uint32_t count);
};

class OfflineMeshComponentManager : public MeshComponentManager
{
public:
    COMPONENT_MANAGER_CLASS_BODY(OfflineMesh)

public:
    OfflineMeshComponentManager() = default;

    Component* CreateComponent(ComponentDataPtr cdp) override;
    Component* CreateComponent(Mesh* mesh, hgl::graph::RenderAssignBuffer* assign_buffer);
};

/**
 * Offline Mesh Component
 * Special mesh component with pre-compiled rendering data that bypasses MaterialRenderList
 */
class OfflineMeshComponent : public MeshComponent
{
public:
    COMPONENT_CLASS_BODY(OfflineMesh)

public:
    OfflineMeshComponent(ComponentDataPtr cdp, OfflineMeshComponentManager* cm);
    virtual ~OfflineMeshComponent() = default;

    virtual Component* Duplication() override;

    OfflineMeshComponentData* GetOfflineData();
    const OfflineMeshComponentData* GetOfflineData() const;

    /**
     * Get pre-computed assign buffer for direct rendering
     */
    hgl::graph::RenderAssignBuffer* GetAssignBuffer() const;

    /**
     * Get instance rendering data
     */
    uint32_t GetInstanceCount() const;
    uint32_t GetFirstInstance() const;

    /**
     * Get pre-computed draw commands
     */
    const VkDrawIndirectCommand* GetDrawCommands(uint32_t& count) const;
    const VkDrawIndexedIndirectCommand* GetDrawIndexedCommands(uint32_t& count) const;

    /**
     * Check if this component has pre-computed rendering data
     */
    bool HasOfflineData() const;

    /**
     * Override CanRender to check offline data availability
     */
    const bool CanRender() const override;
};

COMPONENT_NAMESPACE_END