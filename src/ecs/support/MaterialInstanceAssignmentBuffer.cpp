/**
 * MaterialInstanceAssignmentBuffer.cpp - ECS 材质实例分配缓冲实现
 */

#include<hgl/ecs/support/MaterialInstanceAssignmentBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/module/BufferManager.h>

namespace hgl::ecs
{
    MaterialInstanceAssignmentBuffer::MaterialInstanceAssignmentBuffer(graph::BufferManager* bm, graph::Material* mtl)
        : buffer_manager(bm)
        , material(mtl)
        , material_instance_data_bytes(0)
        , material_instance_buffer(nullptr)
        , material_instance_row_count(0)
        , material_instance_rows_buffer(nullptr)
        , data_index_row_count(0)
        , data_index_rows_buffer(nullptr)
        , texture_layer_row_count(0)
        , texture_layer_rows_buffer(nullptr)
    {
        if (mtl)
        {
            material_instance_data_bytes = mtl->GetMIDataBytes();
        }
    }

    void MaterialInstanceAssignmentBuffer::BindMaterialInstance(graph::Material* mtl) const
    {
        if (!mtl)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::BindMaterialInstance] WARNING: Material is null" << std::endl;
            return;
        }

        if (!material_instance_buffer)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::BindMaterialInstance] WARNING: MI buffer not created" << std::endl;
            return;
        }

        mtl->BindSSBO(hgl::graph::mtl::SBS_MaterialInstance.set_type,
                      hgl::graph::mtl::SBS_MaterialInstance.name,
                      material_instance_buffer->GetGPUBuffer());
    }

    void MaterialInstanceAssignmentBuffer::Clear()
    {
        if (buffer_manager)
        {
            if (material_instance_buffer)
                buffer_manager->Release(material_instance_buffer);
            if (material_instance_rows_buffer)
                buffer_manager->Release(material_instance_rows_buffer);
            if (data_index_rows_buffer)
                buffer_manager->Release(data_index_rows_buffer);
            if (texture_layer_rows_buffer)
                buffer_manager->Release(texture_layer_rows_buffer);
            material_instance_buffer = nullptr;
            material_instance_rows_buffer = nullptr;
            data_index_rows_buffer = nullptr;
            texture_layer_rows_buffer = nullptr;
        }
        else
        {
            SAFE_CLEAR(material_instance_buffer);
            SAFE_CLEAR(material_instance_rows_buffer);
            SAFE_CLEAR(data_index_rows_buffer);
            SAFE_CLEAR(texture_layer_rows_buffer);
        }

        mi_set.Clear();
        material_instance_row_count = 0;
        data_index_row_count = 0;
        texture_layer_row_count = 0;
    }

    void MaterialInstanceAssignmentBuffer::StatMaterialInstance(const std::vector<RenderItem*>& items)
    {
        const size_t item_count = items.size();

        mi_set.Clear();

        // 如果材质没有MI数据，直接返回
        if (material_instance_data_bytes <= 0)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::StatMaterialInstance] Material has no MI data, skipping" << std::endl;
            return;
        }

        // 检查是否需要重新分配缓冲
        if (!material_instance_buffer)
        {
            mi_set.Reserve(power_to_2(item_count));
        }
        else if (item_count > mi_set.GetAllocCount())
        {
            mi_set.Reserve(power_to_2(item_count));
            if (buffer_manager)
            {
                buffer_manager->Release(material_instance_buffer);
                material_instance_buffer = nullptr;
            }
            else
            {
                SAFE_CLEAR(material_instance_buffer);
            }
        }

        // 创建或重用 Material Instance UBO
        if (!material_instance_buffer && buffer_manager)
        {
            const size_t buffer_size = material_instance_data_bytes * mi_set.GetAllocCount();

            material_instance_buffer = buffer_manager->CreateSSBO("ECS:MaterialInstanceData",
                                                                  buffer_size,
                                                                  nullptr,
                                                                  graph::SharingMode::Exclusive);
        }

    if (!material_instance_buffer)
    {
        std::cout << "[MaterialInstanceAssignmentBuffer::StatMaterialInstance] WARNING: MI buffer allocation failed" << std::endl;
        return;
    }

        // 收集所有唯一的材质实例
        mi_set.Reserve(item_count);

        for (RenderItem *item : items)
        {
            if (!item)
                continue;

            graph::MaterialInstance *mi = item->GetMaterialInstance();

            if (mi)
            {
                mi_set.Add(mi);
            }
        }

        const size_t unique_mi_count = mi_set.GetCount();

        // 检查是否超出材质支持的最大数量
        if (material && unique_mi_count > material->GetMIMaxCount())
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::StatMaterialInstance] WARNING: MI count ("
                      << unique_mi_count << ") exceeds material max count ("
                      << material->GetMIMaxCount() << ")" << std::endl;
        }

        // 合并材质实例数据到缓冲
        {
            auto *mibuf = material_instance_buffer->GetGPUBuffer();
            uint8* mip = mibuf ? (uint8*)mibuf->Map(0, mibuf->GetSize()) : nullptr;

            for (graph::MaterialInstance* mi : mi_set)
            {
                if (!mi)
                    continue;

                const void *mi_data = mi->GetMIData();

                if (mi_data)
                {
                    memcpy(mip, mi_data, material_instance_data_bytes);
                }
                else
                {
                    // 如果MI数据为空，清零
                    memset(mip, 0, material_instance_data_bytes);
                }

                mip += material_instance_data_bytes;
            }

            if(mibuf) mibuf->Unmap();
        }
    }

    void MaterialInstanceAssignmentBuffer::UpdateMaterialInstanceData(RenderItem* item)
    {
        if (!item)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::UpdateMaterialInstanceData] ERROR: Item is null" << std::endl;
            return;
        }

        if (!material_instance_rows_buffer)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::UpdateMaterialInstanceData] ERROR: MI rows SSBO not created" << std::endl;
            return;
        }

        const size_t offset = sizeof(uint32) * item->index;
        auto *gpu = material_instance_rows_buffer->GetGPUBuffer();
        uint32 *mip = gpu ? static_cast<uint32 *>(gpu->Map(offset, sizeof(uint32))) : nullptr;

        graph::MaterialInstance* mi = item->GetMaterialInstance();
        if (mip)
        {
            *mip = static_cast<uint32>(mi_set.Find(mi));
            gpu->Unmap();
        }

    }

    void MaterialInstanceAssignmentBuffer::WriteItems(const std::vector<RenderItem*>& items,
                                                      const std::vector<uint32> *data_index_rows,
                                                      const std::vector<uint32> *texture_layer_rows,
                                                      const std::array<uint32_t, static_cast<size_t>(graph::mtl::TextureSlot::RANGE_SIZE)> *texture_slot_handles)
    {
        const size_t item_count = items.size();

        if (item_count == 0)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] WARNING: No items to write" << std::endl;
            return;
        }

        // 1. 收集并写入材质实例数据
        StatMaterialInstance(items);

        if (!material_instance_buffer)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] WARNING: MI buffer unavailable, skip write" << std::endl;
            return;
        }

        // 如果没有 MI 数据，就不需要创建行表 SSBO
        if (material_instance_data_bytes <= 0)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] No MI data, skipping row SSBO creation" << std::endl;
            return;
        }

        // 2. Material channel 三张实例行表 SSBO（instance -> row/index or per-instance handle table）
        // Must stay aligned with GLSL TEXTURE_SLOT_RANGE_SIZE (see bindless_textures.glsl).
        constexpr uint32_t kTextureSlotCount = static_cast<uint32_t>(graph::mtl::TextureSlot::RANGE_SIZE);
        const bool use_texture_handle_table = (texture_slot_handles != nullptr);
        const size_t texture_value_count = use_texture_handle_table
                                         ? (item_count * static_cast<size_t>(kTextureSlotCount))
                                         : item_count;
        auto ensure_row_ssbo = [&](graph::DeviceBuffer *&buffer, uint32_t &capacity, const char *name) -> bool
        {
            if (!buffer)
            {
                capacity = power_to_2(item_count);
            }
            else if (capacity < item_count)
            {
                capacity = power_to_2(item_count);
                if (buffer_manager)
                {
                    buffer_manager->Release(buffer);
                    buffer = nullptr;
                }
                else
                {
                    SAFE_CLEAR(buffer);
                }
            }

            if (!buffer && buffer_manager)
            {
                const VkDeviceSize byte_size = static_cast<VkDeviceSize>(capacity) * sizeof(uint32);
                buffer = buffer_manager->CreateSSBO(name, byte_size, nullptr, graph::SharingMode::Exclusive);
            }

            return buffer != nullptr;
        };

        auto ensure_texture_row_ssbo = [&](graph::DeviceBuffer *&buffer, uint32_t &capacity, const char *name) -> bool
        {
            const uint32_t required = static_cast<uint32_t>(texture_value_count);

            if (!buffer)
            {
                capacity = power_to_2(required > 0 ? required : 1u);
            }
            else if (capacity < required)
            {
                capacity = power_to_2(required > 0 ? required : 1u);
                if (buffer_manager)
                {
                    buffer_manager->Release(buffer);
                    buffer = nullptr;
                }
                else
                {
                    SAFE_CLEAR(buffer);
                }
            }

            if (!buffer && buffer_manager)
            {
                const VkDeviceSize byte_size = static_cast<VkDeviceSize>(capacity) * sizeof(uint32);
                buffer = buffer_manager->CreateSSBO(name, byte_size, nullptr, graph::SharingMode::Exclusive);
            }

            return buffer != nullptr;
        };

        if (!ensure_row_ssbo(material_instance_rows_buffer, material_instance_row_count, "ECS:MaterialInstanceRows")
         || !ensure_row_ssbo(data_index_rows_buffer, data_index_row_count, "ECS:DataIndexRows")
         || !ensure_texture_row_ssbo(texture_layer_rows_buffer, texture_layer_row_count, "ECS:TextureLayerRows"))
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] WARNING: rows SSBO allocation failed" << std::endl;
            return;
        }

#ifdef _DEBUG
        if (buffer_manager)
        {
            if (auto *vk_device = buffer_manager->GetDevice())
            {
                if (auto *du = vk_device->GetDebugUtils())
                {
                    if (material_instance_rows_buffer)
                        du->SetBuffer(material_instance_rows_buffer->GetBuffer(), "ECS.MaterialInstanceRows");
                    if (data_index_rows_buffer)
                        du->SetBuffer(data_index_rows_buffer->GetBuffer(), "ECS.MaterialDataIndexRows");
                    if (texture_layer_rows_buffer)
                        du->SetBuffer(texture_layer_rows_buffer->GetBuffer(),
                                      use_texture_handle_table ? "ECS.MaterialTextureLayerHandleTable" : "ECS.MaterialTextureLayerRows");
                }
            }
        }
#endif

        // 3. 生成材质实例索引列表
        {
            auto *mi_rows_gpu = material_instance_rows_buffer ? material_instance_rows_buffer->GetGPUBuffer() : nullptr;
            auto *data_rows_gpu = data_index_rows_buffer ? data_index_rows_buffer->GetGPUBuffer() : nullptr;
            auto *texture_rows_gpu = texture_layer_rows_buffer ? texture_layer_rows_buffer->GetGPUBuffer() : nullptr;
            uint32 *mi_row_ptr = mi_rows_gpu ? static_cast<uint32 *>(mi_rows_gpu->Map(0, static_cast<VkDeviceSize>(item_count) * sizeof(uint32))) : nullptr;
            uint32 *data_row_ptr = data_rows_gpu ? static_cast<uint32 *>(data_rows_gpu->Map(0, static_cast<VkDeviceSize>(item_count) * sizeof(uint32))) : nullptr;
            uint32 *texture_row_ptr = texture_rows_gpu ? static_cast<uint32 *>(texture_rows_gpu->Map(0, static_cast<VkDeviceSize>(texture_value_count) * sizeof(uint32))) : nullptr;

            if (!mi_row_ptr || !data_row_ptr || !texture_row_ptr)
            {
                std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] WARNING: row SSBO map failed" << std::endl;
                if (mi_row_ptr)
                    mi_rows_gpu->Unmap();
                if (data_row_ptr)
                    data_rows_gpu->Unmap();
                if (texture_row_ptr)
                    texture_rows_gpu->Unmap();
                return;
            }

            const uint32 max_valid_data_index = mi_set.GetCount() > 0
                                              ? static_cast<uint32>(mi_set.GetCount() - 1)
                                              : 0u;
            bool warned_invalid_data_index = false;

            for (size_t i = 0; i < item_count; i++)
            {
                RenderItem* item = items[i];

                if (!item)
                {
                    *mi_row_ptr = 0;
                    *data_row_ptr = 0;
                    *texture_row_ptr = 0;
                    ++mi_row_ptr;
                    ++data_row_ptr;
                    ++texture_row_ptr;
                    continue;
                }

                graph::MaterialInstance* mi = item->GetMaterialInstance();
                uint16 mi_index = mi_set.Find(mi);
                uint32 data_index = static_cast<uint32>(mi_index);
                uint32 texture_layer = 0;

                if (data_index_rows && i < data_index_rows->size())
                {
                    data_index = (*data_index_rows)[i];
                    if (data_index > max_valid_data_index)
                    {
                        if (!warned_invalid_data_index)
                        {
                            std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] WARNING: DataIndexID overflow detected, fallback to 0. "
                                      << "value=" << data_index
                                      << ", max_valid=" << max_valid_data_index
                                      << ", item_count=" << item_count
                                      << std::endl;
                            warned_invalid_data_index = true;
                        }
                        data_index = 0;
                    }
                }
                if (texture_layer_rows && i < texture_layer_rows->size())
                    texture_layer = (*texture_layer_rows)[i];

                *mi_row_ptr = static_cast<uint32>(mi_index);
                *data_row_ptr = data_index;
                ++mi_row_ptr;
                ++data_row_ptr;

                if (use_texture_handle_table)
                {
                    const size_t base = i * static_cast<size_t>(kTextureSlotCount);
                    for (size_t slot = 0; slot < static_cast<size_t>(kTextureSlotCount); ++slot)
                        texture_row_ptr[base + slot] = (*texture_slot_handles)[slot];
                }
                else
                {
                    *texture_row_ptr = texture_layer;
                    ++texture_row_ptr;
                }

                // if (i < 5 || i >= item_count - 2)  // 只打印前几个和后几个
                // {
                //     std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems]   Item[" << i
                //               << "] -> MI_index=" << mi_index
                //               << ", MI=" << (void*)mi << std::endl;
                // }
            }

            mi_rows_gpu->Unmap();
            data_rows_gpu->Unmap();
            texture_rows_gpu->Unmap();
        }
    }
}//namespace hgl::ecs
