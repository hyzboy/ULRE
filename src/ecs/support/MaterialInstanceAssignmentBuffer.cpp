/**
 * MaterialInstanceAssignmentBuffer.cpp - ECS 材质实例分配缓冲实现
 */

#include<hgl/ecs/support/MaterialInstanceAssignmentBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKDomainMaterialBinding.h>
#include<hgl/vk/VKInstanceDataDomain.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/common/RenderOptions.h>
#include <algorithm>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    MaterialInstanceAssignmentBuffer::MaterialInstanceAssignmentBuffer(graph::BufferManager* bm, graph::MaterialTemplate* mtl)
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
            material_instance_data_bytes = mtl->GetMIDataBytes();
        }
    }

    void MaterialInstanceAssignmentBuffer::BindMaterialInstanceID(graph::MaterialTemplate* mtl) const
    {
        if (!mtl || !material_instance_id_buffer)
            return;

        auto *gpu = material_instance_id_buffer->GetGPUBuffer();
        if (!gpu)
            return;

        mtl->BindSSBO(hgl::graph::mtl::SSBODescriptorSemantic::MaterialInstanceID,
                      gpu);
    }

    void MaterialInstanceAssignmentBuffer::BindMaterialInstanceID(graph::DomainMaterialBinding* binding) const
    {
        if (!binding || !material_instance_id_buffer)
            return;

        auto *gpu = material_instance_id_buffer->GetGPUBuffer();
        if (!gpu)
            return;

        binding->BindSSBO(hgl::graph::mtl::SSBODescriptorSemantic::MaterialInstanceID,
                          gpu);
    }

    void MaterialInstanceAssignmentBuffer::BindMaterialInstanceTextureID(graph::MaterialTemplate* mtl) const
    {
        if (!mtl)
            return;

        if (!mit_buffer)
        {
            if (mtl->GetTextureArraySlotFlags() != 0)
            {
                static uint64_t s_missing_mit_buffer = 0;
                if ((++s_missing_mit_buffer <= 8u) || ((s_missing_mit_buffer & (s_missing_mit_buffer - 1)) == 0))
                {
                    std::cerr << "[MIAB::BindMIT] ERROR: material requires MIT SSBO but no MIT buffer was created: material="
                              << static_cast<const void *>(mtl)
                              << "(" << mtl->GetName().c_str() << ")"
                              << " array_slot_flags=0x" << std::hex << unsigned(mtl->GetTextureArraySlotFlags()) << std::dec
                              << " total_errors=" << static_cast<unsigned long long>(s_missing_mit_buffer)
                              << std::endl;
                }
            }
            return;
        }

        auto *gpu = mit_buffer->GetGPUBuffer();
        if (!gpu)
        {
            if (mtl->GetTextureArraySlotFlags() != 0)
            {
                static uint64_t s_missing_mit_gpu = 0;
                if ((++s_missing_mit_gpu <= 8u) || ((s_missing_mit_gpu & (s_missing_mit_gpu - 1)) == 0))
                {
                    std::cerr << "[MIAB::BindMIT] ERROR: material requires MIT SSBO but MIT GPU buffer is null: material="
                              << static_cast<const void *>(mtl)
                              << "(" << mtl->GetName().c_str() << ")"
                              << " array_slot_flags=0x" << std::hex << unsigned(mtl->GetTextureArraySlotFlags()) << std::dec
                              << " total_errors=" << static_cast<unsigned long long>(s_missing_mit_gpu)
                              << std::endl;
                }
            }
            return;
        }

        mtl->BindSSBO(hgl::graph::mtl::SSBODescriptorSemantic::MaterialInstanceTextureID,
                      gpu);
    }

    void MaterialInstanceAssignmentBuffer::BindMaterialInstanceTextureID(graph::DomainMaterialBinding* binding) const
    {
        if (!binding)
            return;

        graph::MaterialTemplate *mtl = binding->GetMaterial();
        if (!mtl)
            return;

        if (!mit_buffer)
        {
            if (mtl->GetTextureArraySlotFlags() != 0)
            {
                static uint64_t s_missing_mit_buffer = 0;
                if ((++s_missing_mit_buffer <= 8u) || ((s_missing_mit_buffer & (s_missing_mit_buffer - 1)) == 0))
                {
                    std::cerr << "[MIAB::BindMIT] ERROR: domain binding requires MIT SSBO but no MIT buffer was created: binding="
                              << static_cast<const void *>(binding)
                              << " material=" << static_cast<const void *>(mtl)
                              << "(" << mtl->GetName().c_str() << ")"
                              << " array_slot_flags=0x" << std::hex << unsigned(mtl->GetTextureArraySlotFlags()) << std::dec
                              << " total_errors=" << static_cast<unsigned long long>(s_missing_mit_buffer)
                              << std::endl;
                }
            }
            return;
        }

        auto *gpu = mit_buffer->GetGPUBuffer();
        if (!gpu)
        {
            if (mtl->GetTextureArraySlotFlags() != 0)
            {
                static uint64_t s_missing_mit_gpu = 0;
                if ((++s_missing_mit_gpu <= 8u) || ((s_missing_mit_gpu & (s_missing_mit_gpu - 1)) == 0))
                {
                    std::cerr << "[MIAB::BindMIT] ERROR: domain binding requires MIT SSBO but MIT GPU buffer is null: binding="
                              << static_cast<const void *>(binding)
                              << " material=" << static_cast<const void *>(mtl)
                              << "(" << mtl->GetName().c_str() << ")"
                              << " array_slot_flags=0x" << std::hex << unsigned(mtl->GetTextureArraySlotFlags()) << std::dec
                              << " total_errors=" << static_cast<unsigned long long>(s_missing_mit_gpu)
                              << std::endl;
                }
            }
            return;
        }

        binding->BindSSBO(hgl::graph::mtl::SSBODescriptorSemantic::MaterialInstanceTextureID,
                          gpu);
    }

    void MaterialInstanceAssignmentBuffer::BindMaterialInstance(graph::MaterialTemplate* mtl) const
    {
        if (!mtl)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::BindMaterialInstance] WARNING: MaterialTemplate is null" << std::endl;
            return;
        }

        if (!material_instance_buffer)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::BindMaterialInstance] WARNING: MI buffer not created" << std::endl;
            return;
        }

        mtl->BindSSBO(hgl::graph::mtl::SSBODescriptorSemantic::MaterialInstanceData,
                  material_instance_buffer->GetGPUBuffer());

        BindMaterialInstanceID(mtl);
    }

    void MaterialInstanceAssignmentBuffer::BindMaterialInstance(graph::DomainMaterialBinding* binding) const
    {
        if (!binding || !material_instance_buffer)
            return;

        binding->BindSSBO(hgl::graph::mtl::SSBODescriptorSemantic::MaterialInstanceData,
                          material_instance_buffer->GetGPUBuffer());

        BindMaterialInstanceID(binding);
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

        slot_set.Clear();
        node_count = 0;
        material_instance_id_vk_buffer = VK_NULL_HANDLE;
        mit_data_bytes = 0;
        mit_buffer_max_count = 0;
    }

    void MaterialInstanceAssignmentBuffer::StatMaterialInstance(const std::vector<RenderItem*>& items)
    {
        const size_t item_count = items.size();

        slot_set.Clear();

        uint32_t skipped_non_instance_resolved = 0;

        // 收集唯一 MaterialSlot：优先使用 RenderItem resolved_slot（domain+mi_id），
        // 无 resolved_slot 时回退到 Primitive* 去重，保证兼容旧路径。
        slot_set.Reserve(power_to_2(item_count));

        for (RenderItem *item : items)
        {
            if (!item)
                continue;

            const auto& binding = item->GetEntityMaterialBinding();

            if (binding.IsDrawBindingValid() && binding.domain_handle.IsValid() && binding.mi_id >= 0)
            {
                const void* mi_data_ptr = binding.domain->GetMIData(binding.mi_id);
                const uint32_t mit_bytes = binding.mit_count * sizeof(uint32_t);
                slot_set.AddResolved(binding.domain_handle,
                                     binding.mi_id,
                                     mi_data_ptr,
                                     binding.mit_data,
                                     mit_bytes);
                continue;
            }

            // Phase B cleanup: in domain-direct mode, resolved non-instanced
            // slots (material/domain present but mi_id < 0) should not force a
            // Primitive* fallback slot into MIAB.
            if (use_resolved_domain_mi_id
             && binding.material_template
             && binding.domain_handle.IsValid()
             && binding.mi_id < 0)
            {
                ++skipped_non_instance_resolved;
                continue;
            }

            graph::Primitive *prim = item->GetPrimitive();
            if (prim)
                slot_set.AddPrimitive(prim);
        }

        const size_t unique_slot_count = slot_set.GetCount();

        // 检查是否超出材质支持的最大数量
        if (material && unique_slot_count > material->GetMIMaxCount())
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::StatMaterialInstance] WARNING: prim count ("
                      << unique_slot_count << ") exceeds material max count ("
                      << material->GetMIMaxCount() << ")" << std::endl;
        }

        // Diagnostic: log first few frames
        {
            static uint32_t s_stat_tick = 0;
            if (++s_stat_tick <= 3u)
            {
                std::cout << "[MIAB::Stat] material=" << (void*)material
                          << "(" << (material ? material->GetName().c_str() : "null") << ")"
                          << " mi_data_bytes=" << material_instance_data_bytes
                          << " items=" << item_count
                          << " unique_slots=" << unique_slot_count
                          << std::endl;
                uint32_t slot_idx = 0;
                for (const auto &entry : slot_set)
                {
                    std::cout << "[MIAB::Stat]   Slot[" << slot_idx++
                              << "] domain_handle=" << entry.domain_handle.id
                              << " mi_id=" << entry.mi_id
                              << " prim_fallback=" << (void*)entry.primitive_fallback
                              << " data=" << entry.mi_data_ptr
                              << std::endl;
                }

                if (skipped_non_instance_resolved > 0)
                {
                    std::cout << "[MIAB::Stat]   skipped_non_instance_resolved="
                              << skipped_non_instance_resolved
                              << " (domain-direct mode)"
                              << std::endl;
                }
            }
        }

        // MI 数据上传与 MI ID 分发解耦：仅当存在 MI payload 时才分配/写入 MI SSBO
        if (material_instance_data_bytes > 0)
        {
            const size_t needed = slot_set.GetCount();

            if (!material_instance_buffer || material_instance_buffer->GetGPUBuffer()->GetSize() < material_instance_data_bytes * needed)
            {
                if (buffer_manager)
                {
                    if (material_instance_buffer)
                        buffer_manager->Release(material_instance_buffer);

                    material_instance_buffer = buffer_manager->CreateSSBO("ECS:MaterialInstanceData",
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
                uint8* mip = mibuf ? (uint8*)mibuf->Map(0, material_instance_data_bytes * slot_set.GetCount()) : nullptr;

                if (mip)
                {
                    for (const auto &entry : slot_set)
                    {
                        const void *mi_data = entry.mi_data_ptr;

                        if (!mi_data && entry.primitive_fallback)
                            mi_data = entry.primitive_fallback->GetMIData();

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
            // Detect MIT entry size from the first Primitive that has MIT data
            if (mit_data_bytes == 0)
            {
                for (const auto &entry : slot_set)
                {
                    if (entry.mit_data_bytes > 0)
                    {
                        mit_data_bytes = entry.mit_data_bytes;
                        break;
                    }

                    if (entry.primitive_fallback && entry.primitive_fallback->GetMITDataBytes() > 0)
                    {
                        mit_data_bytes = entry.primitive_fallback->GetMITDataBytes();
                        break;
                    }
                }
            }

            if (mit_data_bytes > 0)
            {
                const size_t needed = slot_set.GetCount();

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
                    uint8_t* mitp = mitgpu ? (uint8_t*)mitgpu->Map(0, mit_data_bytes * slot_set.GetCount()) : nullptr;

                    if (mitp)
                    {
                        for (const auto &entry : slot_set)
                        {
                            const void* mit_data = entry.mit_data_ptr;
                            uint32_t src_bytes = entry.mit_data_bytes;

                            if ((!mit_data || src_bytes == 0) && entry.primitive_fallback)
                            {
                                mit_data = entry.primitive_fallback->GetMITData();
                                src_bytes = entry.primitive_fallback->GetMITDataBytes();
                            }

                            if (mit_data)
                            {
                                const uint32_t copy_bytes = std::min<uint32_t>(mit_data_bytes, src_bytes);
                                memcpy(mitp, mit_data, copy_bytes);
                                if (copy_bytes < mit_data_bytes)
                                    memset(mitp + copy_bytes, 0, mit_data_bytes - copy_bytes);
                            }
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

        uint16 mi_index = 0;
        const auto& binding = item->GetEntityMaterialBinding();
        if (binding.material_template && binding.domain_handle.IsValid() && binding.mi_id >= 0)
        {
            if (use_resolved_domain_mi_id)
                mi_index = static_cast<uint16>(binding.mi_id);
            else
                mi_index = slot_set.FindResolved(binding.domain_handle, binding.mi_id);
        }
        else if (use_resolved_domain_mi_id
              && binding.material_template
              && binding.domain_handle.IsValid()
              && binding.mi_id < 0)
        {
            // Non-instanced resolved slot in domain-direct mode.
            // Keep deterministic default index and avoid Primitive* fallback.
            mi_index = 0;
        }
        else
        {
            graph::Primitive* prim = item->GetPrimitive();
            mi_index = prim ? slot_set.FindPrimitive(prim) : 0;
        }

        *mip = static_cast<uint32_t>(mi_index);

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

        if (slot_set.GetCount() == 0)
        {
            std::cout << "[MaterialInstanceAssignmentBuffer::WriteItems] WARNING: No Primitives collected" << std::endl;
            return;
        }

        // Diagnostic
        {
            static uint32_t s_write_tick = 0;
            if (++s_write_tick <= 3u)
                std::cout << "[MIAB::Write] items=" << item_count
                          << " unique_slots=" << slot_set.GetCount()
                          << " mi_id_buf=" << (void*)material_instance_id_buffer
                          << " mi_data_buf=" << (void*)material_instance_buffer
                          << std::endl;
        }

        // 2. Create or reuse MaterialInstanceID SSBO
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
                        du->SetBuffer(material_instance_id_vk_buffer, "ECS:SSBO:Buffer:MaterialInstanceID");
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
            uint32_t non_instance_resolved_mid_defaulted = 0;
            uint32_t primitive_fallback_mid_count = 0;

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

                uint16 mi_index = 0;
                const auto& binding = item->GetEntityMaterialBinding();
                if (binding.material_template && binding.domain_handle.IsValid() && binding.mi_id >= 0)
                {
                    if (use_resolved_domain_mi_id)
                        mi_index = static_cast<uint16>(binding.mi_id);
                    else
                        mi_index = slot_set.FindResolved(binding.domain_handle, binding.mi_id);
                }
                else if (use_resolved_domain_mi_id
                      && binding.material_template
                      && binding.domain_handle.IsValid()
                      && binding.mi_id < 0)
                {
                    mi_index = 0;
                    ++non_instance_resolved_mid_defaulted;
                }
                else
                {
                    graph::Primitive* prim = item->GetPrimitive();
                    mi_index = prim ? slot_set.FindPrimitive(prim) : 0;
                    ++primitive_fallback_mid_count;
                }
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

            static uint32_t s_mid_assign_tick = 0;
            if (++s_mid_assign_tick <= 8u)
            {
                std::cout << "[MIAB::MIDAssign] items=" << item_count
                          << " resolved_domain_mode=" << (use_resolved_domain_mi_id ? 1 : 0)
                          << " non_instance_resolved_defaulted=" << non_instance_resolved_mid_defaulted
                          << " primitive_fallback_count=" << primitive_fallback_mid_count
                          << std::endl;
            }
        }
    }
}//namespace hgl::ecs
