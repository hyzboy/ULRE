/**
 * MaterialInstanceAssignmentBuffer.cpp - ECS 材质实例分配缓冲实现
 */

#include<hgl/ecs/support/MaterialInstanceAssignmentBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/mtl/ShaderDataSchema.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/common/RenderOptions.h>
#include <algorithm>

namespace hgl::ecs
{
    namespace
    {
#ifdef _DEBUG
        graph::MaterialBindingInstance *ResolveMIStateOnly(const RenderItem *item)
        {
            if (!item)
                return nullptr;

            const auto state = item->GetResolvedMaterialState();
            auto *mi = state.binding_instance;

            auto *legacy_mi = item->GetResolvedBindingInstance();
            if (!mi && legacy_mi)
            {
                std::cout << "[MaterialInstanceAssignmentBuffer] DEBUG: state.binding_instance is null but legacy accessor returned non-null" << std::endl;
            }
            else if (mi && legacy_mi && mi != legacy_mi)
            {
                std::cout << "[MaterialInstanceAssignmentBuffer] DEBUG: state.binding_instance and legacy accessor mismatch" << std::endl;
            }

            return mi;
        }
#else
        graph::MaterialBindingInstance *ResolveMIStateOnly(const RenderItem *item)
        {
            if (!item)
                return nullptr;

            return item->GetResolvedMaterialState().binding_instance;
        }
#endif
    }

    MaterialInstanceAssignmentBuffer::MaterialInstanceAssignmentBuffer(graph::BufferManager* bm, graph::ShaderMaterialProgram* mtl)
        : buffer_manager(bm)
        , material(mtl)
        , material_instance_data_bytes(0)
        , material_instance_buffer(nullptr)
        , node_count(0)
        , material_instance_id_buffer_max_count(0)
        , material_instance_id_buffer(nullptr)
        , material_instance_id_vk_buffer(VK_NULL_HANDLE)
        , mit_data_bytes(0)
        , mit_buffer_max_count(0)
        , mit_buffer(nullptr)
    {
        if (mtl)
        {
            material_instance_data_bytes = graph::mtl::GetShaderDataSchemaInfo(mtl->GetShaderDataSchema()).byte_size;
        }
    }

    void MaterialInstanceAssignmentBuffer::BindMaterialInstanceID(graph::ShaderMaterialProgram* mtl) const
    {
        if (!mtl || !material_instance_id_buffer)
            return;

        auto *gpu = material_instance_id_buffer->GetGPUBuffer();
        if (!gpu)
            return;

        mtl->BindSSBO(hgl::graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceID,
                      gpu);
    }

    void MaterialInstanceAssignmentBuffer::BindMaterialInstanceTextureID(graph::ShaderMaterialProgram* mtl) const
    {
        if (!mtl || !mit_buffer)
            return;

        auto *gpu = mit_buffer->GetGPUBuffer();
        if (!gpu)
            return;

        mtl->BindSSBO(hgl::graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceTexture,
                      gpu);
    }

    void MaterialInstanceAssignmentBuffer::BindMaterialInstance(graph::ShaderMaterialProgram* mtl) const
    {
        if (!mtl)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::BindMaterialInstance] WARNING: ShaderMaterialProgram is null" << std::endl;
            return;
        }

        if (!material_instance_buffer)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::BindMaterialInstance] WARNING: MI buffer not created" << std::endl;
            return;
        }

        mtl->BindSSBO(hgl::graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceData,
                  material_instance_buffer->GetGPUBuffer());

        BindMaterialInstanceID(mtl);
    }

    void MaterialInstanceAssignmentBuffer::Clear()
    {
        if (buffer_manager)
        {
            if (material_instance_buffer)
                buffer_manager->Release(material_instance_buffer);
            material_instance_buffer = nullptr;
            if (material_instance_id_buffer)
                buffer_manager->Release(material_instance_id_buffer);
            material_instance_id_buffer = nullptr;
            if (mit_buffer)
                buffer_manager->Release(mit_buffer);
            mit_buffer = nullptr;
        }
        else
        {
            SAFE_CLEAR(material_instance_buffer);
            SAFE_CLEAR(material_instance_id_buffer);
            SAFE_CLEAR(mit_buffer);
        }

        mi_set.Clear();
        node_count = 0;
        material_instance_id_vk_buffer = VK_NULL_HANDLE;
        mit_data_bytes = 0;
        mit_buffer_max_count = 0;
    }

    void MaterialInstanceAssignmentBuffer::StatMaterialInstance(const std::vector<RenderItem*>& items)
    {
        const size_t item_count = items.size();

        mi_set.Clear();

        // 收集所有唯一的材质实例
        mi_set.Reserve(power_to_2(item_count));

        for (RenderItem *item : items)
        {
            if (!item)
                continue;

            graph::MaterialBindingInstance *mi = ResolveMIStateOnly(item);

            if (mi)
            {
                mi_set.Add(mi);
            }
        }

        const size_t unique_mi_count = mi_set.GetCount();

        // 当后如需验证 MI 数量上限可通过 domain->GetCapacity() 实现
        if (material_instance_data_bytes > 0)
        {
            const size_t needed = mi_set.GetCount();

            if (!material_instance_buffer || material_instance_buffer->GetGPUBuffer()->GetSize() < material_instance_data_bytes * needed)
            {
                if (buffer_manager)
                {
                    if (material_instance_buffer)
                        buffer_manager->Release(material_instance_buffer);

                    material_instance_buffer = buffer_manager->CreateSSBO("ECS:MaterialBindingInstanceData",
                                                                          material_instance_data_bytes * power_to_2(needed),
                                                                          nullptr,
                                                                          graph::SharingMode::Exclusive);
                }
                else
                {
                    SAFE_CLEAR(material_instance_buffer);
                }
            }

            if (!material_instance_buffer)
            {
                std::cout << "[MaterialInstanceAssignmentBuffer::StatMaterialInstance] WARNING: MI buffer allocation failed" << std::endl;
            }
            else
            {
                auto *mibuf = material_instance_buffer->GetGPUBuffer();
                uint8* mip = mibuf ? (uint8*)mibuf->Map(0, material_instance_data_bytes * mi_set.GetCount()) : nullptr;

                if (mip)
                {
                    for (graph::MaterialBindingInstance* mi : mi_set)
                    {
                        const void *mi_data = mi ? mi->GetMIData() : nullptr;

                        if (mi_data)
                            memcpy(mip, mi_data, material_instance_data_bytes);
                        else
                            memset(mip, 0, material_instance_data_bytes);

                        mip += material_instance_data_bytes;
                    }

                    mibuf->Unmap();
                }
            }
        }

        // 合并 MIT 数据（TextureArray per-instance 层索引）到 MIT SSBO
        {
            // Detect MIT entry size from the first MI that has MIT data
            if (mit_data_bytes == 0)
            {
                for (graph::MaterialBindingInstance* mi : mi_set)
                {
                    if (mi && mi->GetMITDataBytes() > 0)
                    {
                        mit_data_bytes = mi->GetMITDataBytes();
                        break;
                    }
                }
            }

            if (mit_data_bytes > 0)
            {
                const size_t needed = mi_set.GetCount();

                // Allocate or reallocate MIT SSBO as needed
                if (!mit_buffer || mit_buffer_max_count < needed)
                {
                    if (mit_buffer && buffer_manager)
                        buffer_manager->Release(mit_buffer);
                    else
                        SAFE_CLEAR(mit_buffer);

                    const uint32_t new_cap = power_to_2(needed);
                    if (buffer_manager)
                    {
                        mit_buffer = buffer_manager->CreateSSBO("ECS:MaterialInstanceTextureIDData",
                                                                 mit_data_bytes * new_cap,
                                                                 nullptr,
                                                                 graph::SharingMode::Exclusive);
                    }
                    mit_buffer_max_count = mit_buffer ? new_cap : 0;
                }

                if (mit_buffer)
                {
                    auto *mitgpu = mit_buffer->GetGPUBuffer();
                    uint8_t* mitp = mitgpu ? (uint8_t*)mitgpu->Map(0, mit_data_bytes * mi_set.GetCount()) : nullptr;

                    if (mitp)
                    {
                        for (graph::MaterialBindingInstance* mi : mi_set)
                        {
                            const void* mit_data = mi ? mi->GetMITData() : nullptr;
                            if (mit_data)
                                memcpy(mitp, mit_data, mit_data_bytes);
                            else
                                memset(mitp, 0, mit_data_bytes);
                            mitp += mit_data_bytes;
                        }

                        mitgpu->Unmap();
                    }
                }
            }
        }
    }

    void MaterialInstanceAssignmentBuffer::UpdateMaterialInstanceData(RenderItem* item)
    {
        if (!item)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::UpdateMaterialInstanceData] ERROR: Item is null" << std::endl;
            return;
        }

        if (!material_instance_id_buffer)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::UpdateMaterialInstanceData] ERROR: MI ID SSBO not created" << std::endl;
            return;
        }

        auto *gpu = material_instance_id_buffer->GetGPUBuffer();
        if (!gpu)
            return;

        const size_t offset = sizeof(uint32_t) * item->index;
        uint32_t* mip = (uint32_t*)(gpu->Map(offset, sizeof(uint32_t)));
        if (!mip)
            return;

        graph::MaterialBindingInstance* mi = ResolveMIStateOnly(item);
        *mip = static_cast<uint32_t>(mi_set.Find(mi));

        gpu->Unmap();
    }

    void MaterialInstanceAssignmentBuffer::WriteItems(const std::vector<RenderItem*>& items)
    {
        const size_t item_count = items.size();

        if (item_count == 0)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] WARNING: No items to write" << std::endl;
            return;
        }

        // 1. 收集并写入材质实例数据
        StatMaterialInstance(items);

        if (mi_set.GetCount() == 0)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] WARNING: No MaterialBindingInstanceData collected" << std::endl;
            return;
        }

        // 2. Create or reuse MaterialBindingInstanceID SSBO
        {
            if (!material_instance_id_buffer)
            {
                node_count = power_to_2(item_count);
            }
            else if (material_instance_id_buffer_max_count < item_count)
            {
                node_count = power_to_2(item_count);
                if (buffer_manager)
                {
                    buffer_manager->Release(material_instance_id_buffer);
                    material_instance_id_buffer = nullptr;
                }
                else
                {
                    SAFE_CLEAR(material_instance_id_buffer);
                }

                material_instance_id_buffer_max_count = 0;
                material_instance_id_vk_buffer = VK_NULL_HANDLE;
            }

            if (!material_instance_id_buffer)
            {
                if (buffer_manager)
                {
                    material_instance_id_buffer = buffer_manager->CreateSSBO(
                        "ECS:MaterialInstanceIDData",
                        sizeof(uint32_t) * node_count,
                        nullptr,
                        graph::SharingMode::Exclusive);

                    material_instance_id_buffer_max_count = material_instance_id_buffer ? node_count : 0;
                    material_instance_id_vk_buffer = material_instance_id_buffer
                        ? material_instance_id_buffer->GetGPUBuffer()->GetVkDeviceBuffer() : VK_NULL_HANDLE;

                #ifdef _DEBUG
                    auto device = buffer_manager->GetDevice();
                    graph::DebugUtils* du = device ? device->GetDebugUtils() : nullptr;
                    if (du && material_instance_id_buffer)
                    {
                        du->SetBuffer(material_instance_id_vk_buffer, "ECS:SSBO:Buffer:MaterialBindingInstanceID");
                    }
                #endif//_DEBUG
                }
            }

            if (!material_instance_id_buffer)
            {
                std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] WARNING: MI ID SSBO allocation failed" << std::endl;
                return;
            }
        }

        // 3. 生成材质实例索引列表
        {
            auto *mid_gpu = material_instance_id_buffer->GetGPUBuffer();
            uint32_t* mid_ptr = mid_gpu ? (uint32_t*)mid_gpu->Map(0, sizeof(uint32_t) * item_count) : nullptr;

            if (!mid_ptr)
            {
                std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] WARNING: MI ID SSBO map failed" << std::endl;
                return;
            }

            for (size_t i = 0; i < item_count; i++)
            {
                RenderItem* item = items[i];

                if (!item)
                {
                    *mid_ptr++ = 0;
                    continue;
                }

                graph::MaterialBindingInstance* mi = ResolveMIStateOnly(item);
                uint16 mi_index = mi ? mi_set.Find(mi) : 0;
                *mid_ptr++ = static_cast<uint32_t>(mi_index);

                // if (i < 5 || i >= item_count - 2)  // 只打印前几个和后几个
                // {
                //     std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems]   Item[" << i
                //               << "] -> MI_index=" << mi_index
                //               << ", MI=" << (void*)mi << std::endl;
                // }
            }

            if (mid_gpu)
                mid_gpu->Unmap();
        }
    }
}//namespace hgl::ecs
