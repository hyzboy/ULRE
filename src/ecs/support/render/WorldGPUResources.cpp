#include <hgl/ecs/support/render/WorldGPUResources.h>
#include <hgl/ecs/support/render/WorldEntityRenderState.h>
#include <hgl/ecs/support/render/WorldRenderData.h>
#include <hgl/ecs/support/render/WorldRenderEnvironment.h>
#include <hgl/ecs/support/storage/EntityGPUTable.h>
#include <hgl/ecs/support/storage/EntityRenderIDTable.h>
#include <hgl/ecs/support/storage/LightRowTable.h>
#include <hgl/ecs/support/storage/CameraInfoStorage.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/module/EnvironmentManager.h>
#include <hgl/graph/ubo/WorldAddresses.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/buffer/StructView.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/log/Log.h>
#include <cstring>
#include <cstdio>
#include <vector>

namespace hgl::ecs
{
    namespace
    {
        /// 通用脏行上传辅助函数：收敛 EntityGPU / EntityRenderID / LightRow 的逐字同构上传循环
        template<typename Table, typename RowType, typename View, typename RowGetter>
        void UploadDirtyRowsHelper(Table &table, DirtyRowSet &dirty, View *view, uint32_t capacity_per_copy, RowGetter &&getter)
        {
            if (!view || dirty.GetDirtyCount() == 0)
                return;

            static thread_local std::vector<uint32_t> dirty_rows;
            dirty.CollectDirtyRows(dirty_rows);

            auto *base = reinterpret_cast<RowType *>(view->Data());
            if (!base)
                return;

            constexpr uint32_t kCopies = uint32_t(graph::kWorldAddressesSlotCount);

            for (uint32_t copy = 0; copy < kCopies; ++copy)
            {
                RowType *dst = base + size_t(copy) * capacity_per_copy;

                for (const uint32_t index : dirty_rows)
                {
                    const RowType *src = getter(table, index);
                    if (src)
                        dst[index] = *src;
                }
            }

            view->Commit();
            dirty.ClearDirty();
        }

        template<typename RowType>
        bool BuildWorldRowTable(const char *tag, const char *name_suffix, VkDeviceSize bytes,
                                hgl::graph::GraphicsContext *gc,
                                hgl::graph::VulkanDevice *gpu_device,
                                const char *world_name,
                                graph::BufferUpdateClass update_class,
                                graph::DeviceBuffer *&out_buffer,
                                graph::StructView<RowType> *&out_view,
                                uint64_t &out_addr)
        {
            if (!gc)
            {
                GLogError("[%s] graphics context is null", tag);
                return false;
            }

            auto *bm = gc->GetBufferManager();
            if (!bm)
            {
                GLogError("[%s] buffer manager is null", tag);
                return false;
            }

            char name_buf[192];
            std::snprintf(name_buf, sizeof(name_buf), "World:%s:%s", world_name, name_suffix);

            out_buffer = bm->CreateSSBO(name_buf, bytes);
            if (!out_buffer)
            {
                GLogError("[%s] CreateSSBO failed", tag);
                return false;
            }

            out_buffer->SetUpdateClass(update_class);

            out_view = graph::StructView<RowType>::Create(out_buffer, false);
            if (!out_view)
            {
                GLogError("[%s] StructView create failed", tag);
                return false;
            }

            out_addr = gpu_device
                ? gpu_device->GetBufferDeviceAddressAligned16(out_buffer->GetBuffer())
                : 0;
            if (out_addr == 0)
            {
                GLogError("[%s] 取不到设备地址（16B 对齐 / SHADER_DEVICE_ADDRESS usage 检查）", tag);
                return false;
            }

            return true;
        }
    }

    WorldGPUResources::WorldGPUResources(WorldRenderEnvironment *env,
                                         WorldEntityRenderState *entity_state,
                                         WorldRenderData *data,
                                         const AnsiString &name)
        : environment(env)
        , entity_render_state(entity_state)
        , render_data(data)
        , world_name(name)
    {
    }

    WorldGPUResources::~WorldGPUResources()
    {
    }

    void WorldGPUResources::Release(graph::BufferManager *bm)
    {
        if (world_addresses_table)
        {
            delete world_addresses_table;
            world_addresses_table = nullptr;
        }

        if (bm)
        {
            if (world_addresses_buffer)
            {
                bm->Release(world_addresses_buffer);
                world_addresses_buffer = nullptr;
            }

            if (entity_gpu_buffer)
            {
                bm->Release(entity_gpu_buffer);
                entity_gpu_buffer = nullptr;
            }

            if (entity_render_id_buffer)
            {
                bm->Release(entity_render_id_buffer);
                entity_render_id_buffer = nullptr;
            }

            if (light_row_buffer)
            {
                bm->Release(light_row_buffer);
                light_row_buffer = nullptr;
            }

            if (shadow_info_buffer)
            {
                bm->Release(shadow_info_buffer);
                shadow_info_buffer = nullptr;
            }
        }

        world_addresses_addr = 0;

        entity_gpu_view = nullptr;
        entity_gpu_table_addr = 0;

        entity_render_id_view = nullptr;
        entity_render_id_table_addr = 0;

        light_row_view = nullptr;
        light_row_table_addr = 0;

        shadow_info_view = nullptr;
        shadow_info_addr = 0;
    }

    bool WorldGPUResources::InitializeWorldAddressesTable()
    {
        if (world_addresses_table)
            return true;

        if (!environment || !entity_render_state || !render_data)
            return false;

        auto *gc = environment->GetGraphicsContext();
        auto *gpu_device = environment->GetGPUDevice();
        const char *world_name_cstr = world_name.c_str();

        if (!BuildWorldRowTable<graph::WorldAddresses>(
                "WorldGPUResources::InitializeWorldAddressesTable", "AddressesTable",
                VkDeviceSize(graph::kWorldAddressesSlotStride) * graph::kWorldAddressesSlotCount,
                gc, gpu_device, world_name_cstr,
                graph::BufferUpdateClass::Default,
                world_addresses_buffer, world_addresses_table, world_addresses_addr))
            return false;

        if (auto *base = reinterpret_cast<uint8_t *>(world_addresses_table->Data()))
        {
            for (uint32_t slot = 0; slot < graph::kWorldAddressesSlotCount; ++slot)
                *reinterpret_cast<graph::WorldAddresses *>(
                    base + size_t(slot) * graph::kWorldAddressesSlotStride) = graph::WorldAddresses{};
        }

        world_addresses_table->Commit();

        if (!InitializeEntityGPUTable())
            return false;

        if (!InitializeEntityRenderIDTable())
            return false;

        if (!InitializeLightRowTable())
            return false;

        if (!InitializeShadowInfoTable())
            return false;

        SyncWorldAddresses(0);
        return true;
    }

    void WorldGPUResources::UploadEntityGPUDirtyRows(const uint32_t /*frame_slot*/)
    {
        if (!entity_gpu_buffer || !entity_render_state)
            return;

        auto &entity_gpu_table = entity_render_state->EntityGPU();
        UploadDirtyRowsHelper<EntityGPUTable, EntityGPU>(
            entity_gpu_table, entity_gpu_table.GetDirtySet(), entity_gpu_view, EntityGPUTable::kCapacity,
            [](const EntityGPUTable &t, uint32_t idx) { return t.Row(idx); });
    }

    void WorldGPUResources::UploadEntityRenderIDDirtyRows()
    {
        if (!entity_render_id_buffer || !entity_render_state)
            return;

        auto &entity_render_id_table = entity_render_state->EntityRenderIDs();
        UploadDirtyRowsHelper<EntityRenderIDTable, graph::RenderItemDescriptor>(
            entity_render_id_table, entity_render_id_table.GetDirtySet(), entity_render_id_view, EntityRenderIDTable::kRowStride,
            [](const EntityRenderIDTable &t, uint32_t idx) { return t.FlatRow(idx); });
    }

    void WorldGPUResources::UploadLightRowDirtyRows()
    {
        if (!light_row_buffer || !render_data)
            return;

        auto &light_row_table = render_data->Lights();
        UploadDirtyRowsHelper<LightRowTable, graph::LightRow>(
            light_row_table, light_row_table.GetDirtySet(), light_row_view, LightRowTable::kCapacity,
            [](const LightRowTable &t, uint32_t idx) { return t.Row(idx); });
    }

    bool WorldGPUResources::InitializeEntityRenderIDTable()
    {
        if (entity_render_id_buffer)
            return true;

        if (!environment || !entity_render_state || !render_data)
            return false;

        auto *gc = environment->GetGraphicsContext();
        auto *gpu_device = environment->GetGPUDevice();
        const char *world_name_cstr = world_name.c_str();

        if (!BuildWorldRowTable<graph::RenderItemDescriptor>(
                "WorldGPUResources::InitializeEntityRenderIDTable", "EntityRenderIDTable",
                VkDeviceSize(graph::kWorldAddressesSlotCount)
                    * VkDeviceSize(EntityRenderIDTable::kRowStride)
                    * VkDeviceSize(sizeof(graph::RenderItemDescriptor)),
                gc, gpu_device, world_name_cstr,
                graph::BufferUpdateClass::Default,
                entity_render_id_buffer, entity_render_id_view, entity_render_id_table_addr))
            return false;

        if (auto *rows = reinterpret_cast<graph::RenderItemDescriptor *>(
                entity_render_id_view->Data()))
        {
            for (uint32_t i = 0; i < EntityRenderIDTable::kRowStride * graph::kWorldAddressesSlotCount; ++i)
                rows[i] = graph::RenderItemDescriptor{};
        }

        entity_render_id_view->Commit();

        GLogInfo("[WorldGPUResources] 实体 4-ID 表就绪：%s  %u 行 × %zu B = %zu KB／份 × %u 份（帧槽）= %zu KB，"
                "帧槽 0 基址=0x%llX（消费者：GPU 剔除 / CPU 参考对拍 / 形态 B gather；"
                "每帧地址=基址+帧槽×每份字节，经 WorldAddresses.addr_entity_render_ids 下发）",
                world_name_cstr, EntityRenderIDTable::kRowStride,
                sizeof(graph::RenderItemDescriptor),
                size_t(EntityRenderIDTable::kRowStride) * sizeof(graph::RenderItemDescriptor) / 1024,
                uint32_t(graph::kWorldAddressesSlotCount),
                size_t(graph::kWorldAddressesSlotCount) * EntityRenderIDTable::kRowStride
                    * sizeof(graph::RenderItemDescriptor) / 1024,
                (unsigned long long)entity_render_id_table_addr);

        return true;
    }

    bool WorldGPUResources::InitializeEntityGPUTable()
    {
        if (entity_gpu_buffer)
            return true;

        if (!environment || !entity_render_state || !render_data)
            return false;

        auto *gc = environment->GetGraphicsContext();
        auto *gpu_device = environment->GetGPUDevice();
        const char *world_name_cstr = world_name.c_str();

        if (!BuildWorldRowTable<EntityGPU>(
                "WorldGPUResources::InitializeEntityGPUTable", "EntityGPUTable",
                VkDeviceSize(graph::kWorldAddressesSlotCount)
                    * VkDeviceSize(EntityGPUTable::kCapacity) * VkDeviceSize(sizeof(EntityGPU)),
                gc, gpu_device, world_name_cstr,
                graph::BufferUpdateClass::Default,
                entity_gpu_buffer, entity_gpu_view, entity_gpu_table_addr))
            return false;

        if (auto *base = reinterpret_cast<EntityGPU *>(entity_gpu_view->Data()))
        {
            for (uint32_t copy = 0; copy < graph::kWorldAddressesSlotCount; ++copy)
            {
                EntityGPU *rows = base + size_t(copy) * EntityGPUTable::kCapacity;

                for (uint32_t i = 0; i < EntityGPUTable::kCapacity; ++i)
                    rows[i] = EntityGPU{};
            }
        }

        entity_gpu_view->Commit();

        GLogInfo("[WorldGPUResources] 实体行表就绪：%s  %u 行 × %zu B = %zu KB／份 × %u 份（帧槽）= %zu KB，"
                "帧槽 0 基址=0x%llX（消费者：GPU 剔除读实体行；每帧地址=基址+帧槽×每份字节，"
                "经 WorldAddresses.addr_entity_gpu 下发）",
                world_name_cstr, EntityGPUTable::kCapacity, sizeof(EntityGPU),
                size_t(EntityGPUTable::kCapacity) * sizeof(EntityGPU) / 1024,
                uint32_t(graph::kWorldAddressesSlotCount),
                size_t(graph::kWorldAddressesSlotCount) * EntityGPUTable::kCapacity
                    * sizeof(EntityGPU) / 1024,
                (unsigned long long)entity_gpu_table_addr);

        return true;
    }

    bool WorldGPUResources::InitializeLightRowTable()
    {
        if (light_row_buffer)
            return true;

        if (!environment || !entity_render_state || !render_data)
            return false;

        auto &light_row_table = render_data->Lights();
        light_row_table.Ensure();

        const VkDeviceSize one_copy = VkDeviceSize(LightRowTable::kCapacity)
                                    * VkDeviceSize(sizeof(graph::LightRow));

        auto *gc = environment->GetGraphicsContext();
        auto *gpu_device = environment->GetGPUDevice();
        const char *world_name_cstr = world_name.c_str();

        if (!BuildWorldRowTable<graph::LightRow>(
                "WorldGPUResources::InitializeLightRowTable", "LightRows",
                one_copy * VkDeviceSize(graph::kWorldAddressesSlotCount),
                gc, gpu_device, world_name_cstr,
                graph::BufferUpdateClass::Default,
                light_row_buffer, light_row_view, light_row_table_addr))
            return false;

        if (auto *base = reinterpret_cast<graph::LightRow *>(light_row_view->Data()))
        {
            for (uint32_t copy = 0; copy < graph::kWorldAddressesSlotCount; ++copy)
            {
                graph::LightRow *rows = base + size_t(copy) * LightRowTable::kCapacity;

                for (uint32_t i = 0; i < LightRowTable::kCapacity; ++i)
                    rows[i] = (i == graph::kMainLightRowIndex)
                            ? graph::MakeDefaultMainLight()
                            : graph::LightRow{};
            }
        }

        light_row_view->Commit();
        light_row_table.GetDirtySet().ClearDirty();

        GLogInfo("[WorldGPUResources] 光源行表就绪：%s  %u 行 × %zu B = %zu B／份 × %u 份（帧槽），"
                "帧槽 0 基址=0x%llX（行 0 = 主方向光；每帧地址=基址+帧槽×每份字节，"
                "经 WorldAddresses.addr_lights 下发）",
                world_name_cstr, LightRowTable::kCapacity, sizeof(graph::LightRow),
                size_t(one_copy),
                uint32_t(graph::kWorldAddressesSlotCount),
                (unsigned long long)light_row_table_addr);

        return true;
    }

    bool WorldGPUResources::InitializeShadowInfoTable()
    {
        if (shadow_info_buffer)
            return true;

        if (!environment || !entity_render_state || !render_data)
            return false;

        const VkDeviceSize one_copy = VkDeviceSize(sizeof(graph::ShadowInfo));

        auto *gc = environment->GetGraphicsContext();
        auto *gpu_device = environment->GetGPUDevice();
        const char *world_name_cstr = world_name.c_str();

        if (!BuildWorldRowTable<graph::ShadowInfo>(
                "WorldGPUResources::InitializeShadowInfoTable", "ShadowInfo",
                one_copy * VkDeviceSize(graph::kWorldAddressesSlotCount),
                gc, gpu_device, world_name_cstr,
                graph::BufferUpdateClass::Deferred,
                shadow_info_buffer, shadow_info_view, shadow_info_addr))
            return false;

        if (auto *base = reinterpret_cast<graph::ShadowInfo *>(shadow_info_view->Data()))
        {
            for (uint32_t copy = 0; copy < graph::kWorldAddressesSlotCount; ++copy)
                base[copy] = graph::ShadowInfo{};
        }

        shadow_info_view->Commit();

        GLogInfo("[WorldGPUResources] 影子数据表就绪：%s  %zu B × %u 份（帧槽）= %zu KB，帧槽 0 基址=0x%llX"
                "（世界私有；写者 = MainLightShadowSystem 的 EditShadowInfo + 提交点 CommitShadowInfo；"
                "每帧地址=基址+帧槽×每份字节，经 WorldAddresses.addr_shadow 下发）",
                world_name_cstr, size_t(one_copy),
                uint32_t(graph::kWorldAddressesSlotCount),
                size_t(graph::kWorldAddressesSlotCount) * size_t(one_copy) / 1024,
                (unsigned long long)shadow_info_addr);

        return true;
    }

    void WorldGPUResources::CommitShadowInfo(const uint32_t frame_slot)
    {
        if (!shadow_info_view || !render_data)
            return;

        auto *base = reinterpret_cast<graph::ShadowInfo *>(shadow_info_view->Data());
        if (!base)
            return;

        const uint32_t slot = frame_slot % graph::kWorldAddressesSlotCount;

        base[slot] = render_data->Shadow();
        shadow_info_view->Commit();
    }

    uint64_t WorldGPUResources::GetWorldAddressesAddress(const uint32_t frame_slot) const
    {
        if (world_addresses_addr == 0)
            return 0;

        return world_addresses_addr
             + uint64_t(frame_slot % graph::kWorldAddressesSlotCount)
             * uint64_t(graph::kWorldAddressesSlotStride);
    }

    bool WorldGPUResources::SyncWorldAddresses(const uint32_t frame_index,
                                               const uint64_t transform_address,
                                               const uint64_t model_expansion_address)
    {
        if (!world_addresses_table || !environment || !entity_render_state || !render_data)
            return false;

        auto *base = reinterpret_cast<uint8_t *>(world_addresses_table->Data());
        if (!base)
            return false;

        const uint32_t slot = frame_index % graph::kWorldAddressesSlotCount;

        auto *dst = reinterpret_cast<graph::WorldAddresses *>(
            base + size_t(slot) * graph::kWorldAddressesSlotStride);

        graph::WorldAddresses want{};

        const auto pick = [](const uint64_t resolved, const uint64_t previous) -> uint64_t
        { return resolved != 0 ? resolved : previous; };
        const auto slot_offset = [slot](const uint64_t b, const uint64_t per_slot_bytes) -> uint64_t
        { return b != 0 ? (b + static_cast<uint64_t>(slot) * per_slot_bytes) : 0; };

        auto *camera_info_storage = render_data->CameraRows();
        want.addr_camera_info = pick((camera_info_storage && camera_info_storage->IsReady())
                                     ? camera_info_storage->GetGPUBase() : 0,
                                     dst->addr_camera_info);

        want.addr_entity_gpu = pick(
            slot_offset(entity_gpu_table_addr,
                        static_cast<uint64_t>(EntityGPUTable::kCapacity) * sizeof(EntityGPU)),
            dst->addr_entity_gpu);

        want.addr_entity_render_ids = pick(
            slot_offset(entity_render_id_table_addr,
                        static_cast<uint64_t>(EntityRenderIDTable::kRowStride)
                        * sizeof(graph::RenderItemDescriptor)),
            dst->addr_entity_render_ids);

        want.addr_lights = pick(
            slot_offset(light_row_table_addr,
                        static_cast<uint64_t>(LightRowTable::kCapacity) * sizeof(graph::LightRow)),
            dst->addr_lights);

        {
            want.addr_l2w = pick(transform_address, dst->addr_l2w);
        }

        UploadEntityGPUDirtyRows(slot);
        UploadEntityRenderIDDirtyRows();
        UploadLightRowDirtyRows();

        const uint32_t active_env_profile = environment->GetEnvProfileID();
        auto *env_manager = environment->ResolveEnvManager();
        const uint64_t ambient_env_addr = env_manager
            ? env_manager->GetAmbientEnvAddress(active_env_profile)
            : 0;
        want.addr_ambient_env = pick(ambient_env_addr, dst->addr_ambient_env);

        {
            want.addr_model_expansion = pick(model_expansion_address, dst->addr_model_expansion);
        }

        want.addr_shadow = pick(
            slot_offset(shadow_info_addr, sizeof(graph::ShadowInfo)),
            dst->addr_shadow);

        const uint64_t old_shadow = dst->addr_shadow;
        const uint64_t old_ambient_env = dst->addr_ambient_env;

        if (std::memcmp(dst, &want, sizeof(graph::WorldAddresses)) == 0)
            return false;

        *dst = want;
        world_addresses_table->Commit();

        if (old_shadow != want.addr_shadow || old_ambient_env != want.addr_ambient_env)
            GLogInfo("[WorldGPUResources::SyncWorldAddresses] 世界 \"%s\" 地址入表：profile=%u "
                    "shadow[%u]=0x%llX ambient_env=0x%llX",
                    world_name.c_str(), active_env_profile, slot,
                    static_cast<unsigned long long>(want.addr_shadow),
                    static_cast<unsigned long long>(want.addr_ambient_env));

        return true;
    }
} // namespace hgl::ecs
