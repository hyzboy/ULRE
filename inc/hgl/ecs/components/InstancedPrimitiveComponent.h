#pragma once

#include <hgl/ecs/components/PrimitiveComponent.h>
#include <hgl/vk/VK.h>

namespace hgl
{
    namespace graph
    {
        class DeviceBuffer;
        class IndirectMeshTaskBuffer;
    }
}

namespace hgl::ecs
{
    /**
     * InstancedPrimitiveComponent - Renderable component for multi-instance mesh rendering
     *
     * Inherits from PrimitiveComponent. Manages multiple instances of a geometry+recipe.
     * Supports both CPU-driven instancing and 100% GPU-driven indirect drawing.
     */
    class InstancedPrimitiveComponent : public PrimitiveComponent
    {
    private:
        uint32_t instance_count = 0;              ///< Active instance count to render
        uint32_t max_instances = 0;               ///< Allocated instance capacity
        uint32_t allocated_instance_capacity = 0; ///< Allocated slot count in RenderItemDataStorage

        // 4-ID BDA GPU Buffer bindings
        hgl::graph::DeviceBuffer *l2w_buffer = nullptr;                     ///< L2W matrices SSBO (BDA: l2w.mats[])
        hgl::graph::DeviceBuffer *l2w_index_buffer = nullptr;               ///< L2W index table SSBO (BDA: ResolveTransformID)
        hgl::graph::DeviceBuffer *mesh_draw_params_buffer = nullptr;        ///< MeshDrawCommand SSBO (BDA: cmds[gl_DrawID])
        hgl::graph::DeviceBuffer *material_data_rows_buffer = nullptr;      ///< MaterialInstanceAddresses SSBO (BDA: values[])

        // GPU Indirect Command Buffers (for GPU-Driven culling/dispatch)
        hgl::graph::IndirectMeshTaskBuffer *indirect_cmds_buffer = nullptr; ///< Indirect mesh task commands
        hgl::graph::DeviceBuffer *indirect_count_buffer = nullptr;          ///< Dynamic draw count buffer (optional)
        VkDeviceSize indirect_count_offset = 0;                             ///< Offset in indirect count buffer

        bool is_gpu_driven = false;                                         ///< If true, bypass CPU ICB / index table regeneration
        bool is_indirect = false;                                           ///< If true, use indirect commands

    protected:
        void EnsureRenderItemStorageAllocated() override;

    public:
        explicit InstancedPrimitiveComponent(const std::string &name = "InstancedPrimitive");
        ~InstancedPrimitiveComponent() override;

        void OnDetach() override;

        // ── 连续连号槽位分配 (CPU 驱动多实例与 GPU-Driven 全局对齐) ──
        bool AllocateContiguousInstances(uint32_t count);
        void ReleaseInstances();

        uint32_t GetAllocatedInstanceCapacity() const { return allocated_instance_capacity; }
        graph::RenderItemHandle GetInstanceHandle(uint32_t instance_idx) const;

        // ── 多实例 4-ID 属性设置 ──
        bool SetInstanceTransformID(uint32_t instance_idx, uint32_t transform_id);
        bool SetInstanceGeometryID(uint32_t instance_idx, uint32_t geometry_id);
        bool SetInstanceMaterialID(uint32_t instance_idx, uint32_t material_id);
        bool SetInstanceTextureID(uint32_t instance_idx, uint32_t texture_id);
        bool SetInstance4ID(uint32_t instance_idx, uint32_t transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id);
        bool SetAllInstances4ID(uint32_t base_transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id, bool sequential_transforms = true);

        // Instance counts
        void SetInstanceCount(uint32_t count);
        uint32_t GetInstanceCount() const { return instance_count; }

        void SetMaxInstances(uint32_t max_count);
        uint32_t GetMaxInstances() const { return max_instances; }

        // L2W Transform Buffer
        void SetL2WBuffer(hgl::graph::DeviceBuffer *buf) { l2w_buffer = buf; }
        hgl::graph::DeviceBuffer *GetL2WBuffer() const { return l2w_buffer; }

        // L2W Index Buffer
        void SetL2WIndexBuffer(hgl::graph::DeviceBuffer *buf) { l2w_index_buffer = buf; }
        hgl::graph::DeviceBuffer *GetL2WIndexBuffer() const { return l2w_index_buffer; }

        // Mesh Draw Params Buffer
        void SetMeshDrawParamsBuffer(hgl::graph::DeviceBuffer *buf) { mesh_draw_params_buffer = buf; }
        hgl::graph::DeviceBuffer *GetMeshDrawParamsBuffer() const { return mesh_draw_params_buffer; }

        // Material Data Rows Buffer
        void SetMaterialDataRowsBuffer(hgl::graph::DeviceBuffer *buf) { material_data_rows_buffer = buf; }
        hgl::graph::DeviceBuffer *GetMaterialDataRowsBuffer() const { return material_data_rows_buffer; }

        // Indirect Draw Commands
        void SetIndirectMeshTaskBuffer(hgl::graph::IndirectMeshTaskBuffer *buf)
        {
            indirect_cmds_buffer = buf;
            if (buf)
                is_indirect = true;
        }
        hgl::graph::IndirectMeshTaskBuffer *GetIndirectMeshTaskBuffer() const { return indirect_cmds_buffer; }

        void SetIndirectCountBuffer(hgl::graph::DeviceBuffer *buf, VkDeviceSize offset = 0)
        {
            indirect_count_buffer = buf;
            indirect_count_offset = offset;
        }
        hgl::graph::DeviceBuffer *GetIndirectCountBuffer() const { return indirect_count_buffer; }
        VkDeviceSize GetIndirectCountOffset() const { return indirect_count_offset; }

        // Mode flags
        void SetGPUDriven(bool enabled) { is_gpu_driven = enabled; }
        bool IsGPUDriven() const { return is_gpu_driven; }

        void SetIndirect(bool enabled) { is_indirect = enabled; }
        bool IsIndirect() const { return is_indirect; }
    };
}//namespace hgl::ecs
