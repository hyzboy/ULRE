/**
 * ECSMaterialInstanceAssignmentBuffer.cpp - ECS 材质实例分配缓冲实现
 */

#include<hgl/ecs/support/ECSMaterialInstanceAssignmentBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKRenderAssign.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/module/BufferManager.h>

namespace hgl::ecs
{
    ECSMaterialInstanceAssignmentBuffer::ECSMaterialInstanceAssignmentBuffer(graph::BufferManager* bm, graph::Material* mtl)
        : buffer_manager(bm)
        , material(mtl)
        , material_instance_data_bytes(0)
        , material_instance_buffer(nullptr)
        , node_count(0)
        , material_instance_vab(nullptr)
        , material_instance_vab_buffer(nullptr)
    {
        if (mtl)
        {
            material_instance_data_bytes = mtl->GetMIDataBytes();
        }
    }

    void ECSMaterialInstanceAssignmentBuffer::BindMaterialInstance(graph::Material* mtl) const
    {
        if (!mtl)
        {
            std::cout << "[ECSMaterialInstanceAssignmentBuffer::BindMaterialInstance] WARNING: Material is null" << std::endl;
            return;
        }

        if (!material_instance_buffer)
        {
            std::cout << "[ECSMaterialInstanceAssignmentBuffer::BindMaterialInstance] WARNING: MI buffer not created" << std::endl;
            return;
        }

    #if defined(HGL_MI_USE_SSBO) && HGL_MI_USE_SSBO
        mtl->BindSSBO(hgl::graph::mtl::SBS_MaterialInstance.set_type,
                  hgl::graph::mtl::SBS_MaterialInstance.name,
                  material_instance_buffer);
    #else
        mtl->BindUBO(&hgl::graph::mtl::SBS_MaterialInstance, material_instance_buffer);
    #endif
    }

    void ECSMaterialInstanceAssignmentBuffer::Clear()
    {
        if (buffer_manager)
        {
            if (material_instance_buffer)
                buffer_manager->Release(material_instance_buffer);
            if (material_instance_vab)
                buffer_manager->Release(material_instance_vab);
            material_instance_buffer = nullptr;
            material_instance_vab = nullptr;
        }
        else
        {
            SAFE_CLEAR(material_instance_buffer);
            SAFE_CLEAR(material_instance_vab);
        }

        mi_set.Clear();
        node_count = 0;
        material_instance_vab_buffer = nullptr;
    }

    void ECSMaterialInstanceAssignmentBuffer::StatMaterialInstance(const std::vector<RenderItem*>& items)
    {
        const size_t item_count = items.size();

        mi_set.Clear();

        // 如果材质没有MI数据，直接返回
        if (material_instance_data_bytes <= 0)
        {
            std::cout << "[ECSMaterialInstanceAssignmentBuffer::StatMaterialInstance] Material has no MI data, skipping" << std::endl;
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

#if defined(HGL_MI_USE_SSBO) && HGL_MI_USE_SSBO
            material_instance_buffer = buffer_manager->CreateSSBO("ECS:MaterialInstanceData",
                                                                  buffer_size,
                                                                  nullptr,
                                                                  graph::SharingMode::Exclusive);
#else
            material_instance_buffer = buffer_manager->CreateUBO("ECS:MaterialInstanceData",
                                                                 buffer_size,
                                                                 nullptr,
                                                                 graph::SharingMode::Exclusive);
#endif
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
            std::cout << "[ECSMaterialInstanceAssignmentBuffer::StatMaterialInstance] WARNING: MI count ("
                      << unique_mi_count << ") exceeds material max count ("
                      << material->GetMIMaxCount() << ")" << std::endl;
        }

        // 合并材质实例数据到缓冲
        {
            uint8* mip = (uint8*)(material_instance_buffer->Map());

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

            material_instance_buffer->Unmap();
        }
    }

    void ECSMaterialInstanceAssignmentBuffer::UpdateMaterialInstanceData(RenderItem* item)
    {
        if (!item)
        {
            std::cout << "[ECSMaterialInstanceAssignmentBuffer::UpdateMaterialInstanceData] ERROR: Item is null" << std::endl;
            return;
        }

        if (!material_instance_vab)
        {
            std::cout << "[ECSMaterialInstanceAssignmentBuffer::UpdateMaterialInstanceData] ERROR: MI VAB not created" << std::endl;
            return;
        }

        const size_t offset = sizeof(uint16) * item->index;
        uint16* mip = (uint16*)(material_instance_vab->Map(offset, sizeof(uint16)));

        graph::MaterialInstance* mi = item->GetMaterialInstance();
        *mip = mi_set.Find(mi);

        material_instance_vab->Unmap();
    }

    void ECSMaterialInstanceAssignmentBuffer::WriteItems(const std::vector<RenderItem*>& items)
    {
        const size_t item_count = items.size();

        if (item_count == 0)
        {
            std::cout << "[ECSMaterialInstanceAssignmentBuffer::WriteItems] WARNING: No items to write" << std::endl;
            return;
        }

        // 1. 收集并写入材质实例数据
        StatMaterialInstance(items);

        // 如果没有MI数据，就不需要创建VAB
        if (material_instance_data_bytes <= 0)
        {
            std::cout << "[ECSMaterialInstanceAssignmentBuffer::WriteItems] No MI data, skipping VAB creation" << std::endl;
            return;
        }

        // 2. 创建或重用 Material Instance VAB（索引缓冲）
        {
            if (!material_instance_vab)
            {
                node_count = power_to_2(item_count);
            }
            else if (node_count < item_count)
            {
                node_count = power_to_2(item_count);
                if (buffer_manager)
                {
                    buffer_manager->Release(material_instance_vab);
                    material_instance_vab = nullptr;
                }
                else
                {
                    SAFE_CLEAR(material_instance_vab);
                }
            }

            if (!material_instance_vab)
            {
                if (buffer_manager)
                {
                    material_instance_vab = buffer_manager->CreateVAB(VK_FORMAT_R16_UINT, node_count);
                    material_instance_vab_buffer = material_instance_vab ? material_instance_vab->GetBuffer() : nullptr;

                #ifdef _DEBUG
                    auto device = buffer_manager->GetDevice();
                    graph::DebugUtils* du = device ? device->GetDebugUtils() : nullptr;
                    if (du && material_instance_vab)
                    {
                        du->SetBuffer(material_instance_vab->GetBuffer(), "ECS:VAB:Buffer:MaterialInstanceID");
                        du->SetDeviceMemory(material_instance_vab->GetVkMemory(), "ECS:VAB:Memory:MaterialInstanceID");
                    }
                #endif//_DEBUG
                }
            }
        }

        // 3. 生成材质实例索引列表
        {
            uint16* mi_ptr = (uint16*)(material_instance_vab->DeviceBuffer::Map());

            for (size_t i = 0; i < item_count; i++)
            {
                RenderItem* item = items[i];

                if (!item)
                {
                    *mi_ptr = 0;
                    ++mi_ptr;
                    continue;
                }

                graph::MaterialInstance* mi = item->GetMaterialInstance();
                uint16 mi_index = mi_set.Find(mi);
                *mi_ptr = mi_index;
                ++mi_ptr;

                // if (i < 5 || i >= item_count - 2)  // 只打印前几个和后几个
                // {
                //     std::cout << "[ECSMaterialInstanceAssignmentBuffer::WriteItems]   Item[" << i
                //               << "] -> MI_index=" << mi_index
                //               << ", MI=" << (void*)mi << std::endl;
                // }
            }

            material_instance_vab->Unmap();
        }
    }
}//namespace hgl::ecs
