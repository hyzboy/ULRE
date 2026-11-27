#pragma once

#include <cstdint>
#include <hgl/type/String.h>
#include <hgl/graph/ubo/WorldAddresses.h>
#include <hgl/graph/ubo/ShadowInfo.h>
#include <hgl/graph/ubo/LightRow.h>
#include <hgl/graph/render/RenderItemDescriptor.h>
#include <hgl/ecs/core/entity/EntityGPU.h>
#include <hgl/vk/buffer/StructView.h>
#include <vulkan/vulkan.h>

namespace hgl::graph
{
    class DeviceBuffer;
    class BufferManager;
    class GraphicsContext;
    class VulkanDevice;
    class EnvironmentManager;
}

namespace hgl::ecs
{
    class EntityGPUTable;
    class EntityRenderIDTable;
    class LightRowTable;
    class CameraInfoStorage;

    class IWorldGPUResourcesSource
    {
    public:
        virtual ~IWorldGPUResourcesSource() = default;
        virtual graph::GraphicsContext *GPUResourcesGraphicsContext() = 0;
        virtual graph::VulkanDevice *GPUResourcesDevice() = 0;
        virtual const AnsiString &GPUResourcesWorldName() const = 0;
        virtual EntityGPUTable &GPUResourcesEntityTable() = 0;
        virtual EntityRenderIDTable &GPUResourcesRenderIDTable() = 0;
        virtual LightRowTable &GPUResourcesLightTable() = 0;
        virtual const graph::ShadowInfo &GPUResourcesShadowInfo() const = 0;
        virtual CameraInfoStorage *GPUResourcesCameraStorage() = 0;
        virtual uint64_t GPUResourcesTransformAddress() = 0;
        virtual uint32_t GPUResourcesEnvironmentProfile() const = 0;
        virtual uint64_t GPUResourcesAmbientAddress(uint32_t profile) const = 0;
        virtual uint64_t GPUResourcesModelExpansionAddress() = 0;
    };

    /**
     * @brief 世界 GPU 资源底座
     *
     * 封装世界私有的设备镜像缓冲（SSBO）、持久 StructView、BDA 设备地址，
     * 以及每帧地址表（WorldAddresses）同步与脏行上传。
     */
    class WorldGPUResources
    {
    private:
        IWorldGPUResourcesSource *source = nullptr;

        // 1. 世界地址表
        graph::StructView<graph::WorldAddresses> *world_addresses_table = nullptr;
        graph::DeviceBuffer *world_addresses_buffer = nullptr;
        uint64_t world_addresses_addr = 0;

        // 2. 实体行表
        graph::StructView<EntityGPU> *entity_gpu_view = nullptr;
        graph::DeviceBuffer *entity_gpu_buffer = nullptr;
        uint64_t entity_gpu_table_addr = 0;

        // 3. 实体 4-ID 表
        graph::StructView<graph::RenderItemDescriptor> *entity_render_id_view = nullptr;
        graph::DeviceBuffer *entity_render_id_buffer = nullptr;
        uint64_t entity_render_id_table_addr = 0;

        // 4. 光源行表
        graph::StructView<graph::LightRow> *light_row_view = nullptr;
        graph::DeviceBuffer *light_row_buffer = nullptr;
        uint64_t light_row_table_addr = 0;

        // 5. 影子数据表
        graph::StructView<graph::ShadowInfo> *shadow_info_view = nullptr;
        graph::DeviceBuffer *shadow_info_buffer = nullptr;
        uint64_t shadow_info_addr = 0;

    public:
        explicit WorldGPUResources(IWorldGPUResourcesSource *source = nullptr);
        ~WorldGPUResources();

        bool InitializeWorldAddressesTable();
        bool InitializeEntityGPUTable();
        bool InitializeEntityRenderIDTable();
        bool InitializeLightRowTable();
        bool InitializeShadowInfoTable();

        void UploadEntityGPUDirtyRows(const uint32_t frame_slot);
        void UploadEntityRenderIDDirtyRows();
        void UploadLightRowDirtyRows();

        void CommitShadowInfo(const uint32_t frame_slot);
        bool SyncWorldAddresses(const uint32_t frame_index);

        uint64_t GetWorldAddressesAddress(const uint32_t frame_slot) const;

        void Release(graph::BufferManager *bm);

        // ── 访问器 ──
        graph::StructView<graph::WorldAddresses> *GetWorldAddressesTable() const { return world_addresses_table; }
        graph::DeviceBuffer *GetWorldAddressesBuffer() const { return world_addresses_buffer; }
        uint64_t GetWorldAddressesAddr() const { return world_addresses_addr; }

        graph::StructView<EntityGPU> *GetEntityGPUView() const { return entity_gpu_view; }
        graph::DeviceBuffer *GetEntityGPUBuffer() const { return entity_gpu_buffer; }
        uint64_t GetEntityGPUTableAddr() const { return entity_gpu_table_addr; }

        graph::StructView<graph::RenderItemDescriptor> *GetEntityRenderIDView() const { return entity_render_id_view; }
        graph::DeviceBuffer *GetEntityRenderIDBuffer() const { return entity_render_id_buffer; }
        uint64_t GetEntityRenderIDTableAddr() const { return entity_render_id_table_addr; }

        graph::StructView<graph::LightRow> *GetLightRowView() const { return light_row_view; }
        graph::DeviceBuffer *GetLightRowBuffer() const { return light_row_buffer; }
        uint64_t GetLightRowTableAddr() const { return light_row_table_addr; }

        graph::StructView<graph::ShadowInfo> *GetShadowInfoView() const { return shadow_info_view; }
        graph::DeviceBuffer *GetShadowInfoBuffer() const { return shadow_info_buffer; }
        uint64_t GetShadowInfoAddr() const { return shadow_info_addr; }
    };
} // namespace hgl::ecs
