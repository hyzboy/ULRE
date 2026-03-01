/**
 * TransformAssignmentBuffer.cpp - ECS Transform 分配缓冲实现
 */

#include<hgl/graph/render/RenderOptions.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKRenderAssign.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<algorithm>
#include<limits>
#include<iostream>

namespace hgl::ecs
{
    namespace
    {
        struct IndexRange
        {
            uint32_t first = 0;
            uint32_t last = 0;
        };

        static std::vector<IndexRange> BuildMergedRangesFromIndices(const std::vector<uint32_t>& dirty_indices,
                                                                     const uint32_t handle_count)
        {
            std::vector<IndexRange> result;
            if (dirty_indices.empty() || handle_count == 0)
                return result;

            std::vector<uint32_t> sorted = dirty_indices;
            std::sort(sorted.begin(), sorted.end());
            sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

            uint32_t run_start = std::numeric_limits<uint32_t>::max();
            uint32_t run_last = std::numeric_limits<uint32_t>::max();

            for (const uint32_t idx : sorted)
            {
                if (idx >= handle_count)
                    continue;

                if (run_start == std::numeric_limits<uint32_t>::max())
                {
                    run_start = idx;
                    run_last = idx;
                    continue;
                }

                if (idx == run_last + 1)
                {
                    run_last = idx;
                    continue;
                }

                IndexRange range;
                range.first = run_start;
                range.last = run_last;
                result.push_back(range);

                run_start = idx;
                run_last = idx;
            }

            if (run_start != std::numeric_limits<uint32_t>::max())
            {
                IndexRange range;
                range.first = run_start;
                range.last = run_last;
                result.push_back(range);
            }

            return result;
        }
    }

    std::vector<TransformAssignmentBuffer*> TransformAssignmentBuffer::all_instances;

    TransformAssignmentBuffer::TransformAssignmentBuffer(graph::BufferManager* bm, const Mode m, uint32_t ring_frames)
        : buffer_manager(bm)
        , transform_buffer_max_count(0)
        , transform_buffer(nullptr)
        , transform_policy(graph::BufferAllocPolicy::Auto)
        , static_only(false)
        , mode(m)
        , node_count(0)
        , transform_vab(nullptr)
        , transform_vab_buffer(nullptr)
        , ring_writer(nullptr, sizeof(math::Matrix4f), ring_frames ? ring_frames : HGL_L2W_RING_FRAMES)
    {
#if defined(HGL_L2W_USE_SSBO)
        if (buffer_manager)
        {
            auto device = buffer_manager->GetDevice();
            if (device)
                MaxTransformCount = device->GetSSBORange() / sizeof(math::Matrix4f);
        }
#else
        if (buffer_manager)
        {
            auto device = buffer_manager->GetDevice();
            if (device)
                MaxTransformCount = device->GetUBORange() / sizeof(math::Matrix4f);
        }
#endif
        all_instances.push_back(this);
    }

    void TransformAssignmentBuffer::BindTransform(graph::Material* mtl) const
    {
        if (!mtl)
        {
            std::cout << "[TransformAssignmentBuffer::BindTransform] WARNING: Material is null" << std::endl;
            return;
        }

        if (!transform_buffer)
        {
            std::cout << "[TransformAssignmentBuffer::BindTransform] WARNING: Transform buffer not created" << std::endl;
            return;
        }

    #if defined(HGL_L2W_USE_SSBO)
        mtl->BindSSBO(hgl::graph::mtl::SBS_LocalToWorld.set_type,
                  hgl::graph::mtl::SBS_LocalToWorld.name,
                  transform_buffer->GetGPUBuffer());
    #else
        mtl->BindUBO(&hgl::graph::mtl::SBS_LocalToWorld, transform_buffer->GetGPUBuffer());
    #endif
    }

    void TransformAssignmentBuffer::EnsureCapacity(const uint32_t static_count,const uint32_t dynamic_count,graph::BufferAllocPolicy policy)
    {
        const uint32_t total_count = ring_writer.GetTotalCount(static_count, dynamic_count);
        StatTransform(total_count, policy);
    }

    uint32_t TransformAssignmentBuffer::GetDynamicBaseIndex(const uint32_t static_count,const uint32_t dynamic_count) const
    {
        return ring_writer.GetBaseIndex(static_count, dynamic_count);
    }

    uint32_t TransformAssignmentBuffer::GetTotalCount(const uint32_t static_count,const uint32_t dynamic_count) const
    {
        return ring_writer.GetTotalCount(static_count, dynamic_count);
    }

    void TransformAssignmentBuffer::WriteStaticFromStorage(const TransformDataStorage& storage,const uint32_t static_count)
    {
        if (!transform_buffer || static_count == 0)
            return;

        const auto &mats = storage.GetAllWorldMatrices();
        const uint32_t write_count = static_cast<uint32_t>(mats.size() < static_count ? mats.size() : static_count);
        if (write_count == 0)
            return;

        const VkDeviceSize map_size = sizeof(math::Matrix4f) * write_count;
        auto *tbuf = transform_buffer->GetGPUBuffer();
        math::Matrix4f* l2wp = tbuf ? (math::Matrix4f*)tbuf->Map(0, map_size) : nullptr;
        if (!l2wp)
            return;

        for (uint32_t i = 0; i < write_count; ++i)
        {
            l2wp[i] = *reinterpret_cast<const math::Matrix4f*>(&mats[i]);
        }

        if(tbuf) tbuf->Unmap();
    }

    void TransformAssignmentBuffer::WriteDynamicFromStorage(const TransformDataStorage& storage,const uint32_t static_count,const uint32_t dynamic_count)
    {
        if (!transform_buffer || dynamic_count == 0)
            return;

        const auto &mats = storage.GetAllWorldMatrices();
        const uint32_t write_count = static_cast<uint32_t>(mats.size() < dynamic_count ? mats.size() : dynamic_count);
        if (write_count == 0)
            return;

        math::Matrix4f* l2wp = (math::Matrix4f*)(ring_writer.MapDynamicRange(static_count, dynamic_count));
        if (!l2wp)
            return;

        for (uint32_t i = 0; i < write_count; ++i)
        {
            l2wp[i] = *reinterpret_cast<const math::Matrix4f*>(&mats[i]);
        }

        ring_writer.Unmap();
    }

    void TransformAssignmentBuffer::WriteStaticFromHandles(const TransformDataStorage& storage,
                                                              const std::vector<TransformDataStorage::HandleID>& handles)
    {
        const uint32_t write_count = static_cast<uint32_t>(handles.size());
        if (!transform_buffer || write_count == 0)
            return;

        const VkDeviceSize map_size = sizeof(math::Matrix4f) * write_count;
        auto *tbuf = transform_buffer->GetGPUBuffer();
        math::Matrix4f* l2wp = tbuf ? (math::Matrix4f*)tbuf->Map(0, map_size) : nullptr;
        if (!l2wp)
            return;

        for (uint32_t i = 0; i < write_count; ++i)
        {
            const auto handle = handles[i];
            const glm::mat4 world_matrix = storage.GetWorldMatrix(handle);
            l2wp[i] = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
        }

        if(tbuf) tbuf->Unmap();
    }

    void TransformAssignmentBuffer::WriteDynamicFromHandles(const TransformDataStorage& storage,
                                                               const uint32_t static_count,
                                                               const std::vector<TransformDataStorage::HandleID>& handles)
    {
        const uint32_t write_count = static_cast<uint32_t>(handles.size());
        if (!transform_buffer || write_count == 0)
            return;

        math::Matrix4f* l2wp = (math::Matrix4f*)(ring_writer.MapDynamicRange(static_count, write_count));
        if (!l2wp)
            return;

        for (uint32_t i = 0; i < write_count; ++i)
        {
            const auto handle = handles[i];
            const glm::mat4 world_matrix = storage.GetWorldMatrix(handle);
            l2wp[i] = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
        }

        ring_writer.Unmap();
    }

    void TransformAssignmentBuffer::WriteStaticDirtyIndices(const TransformDataStorage& storage,
                                                            const std::vector<TransformDataStorage::HandleID>& handles,
                                                            const std::vector<uint32_t>& dirty_indices)
    {
        if (!transform_buffer || handles.empty() || dirty_indices.empty())
            return;

        auto *tbuf = transform_buffer->GetGPUBuffer();
        if (!tbuf)
            return;

        const auto merged_ranges = BuildMergedRangesFromIndices(dirty_indices, static_cast<uint32_t>(handles.size()));
        if (merged_ranges.empty())
            return;

        std::vector<graph::IGPUBuffer::DirtyRange> flush_ranges;
        flush_ranges.reserve(merged_ranges.size());

        std::vector<math::Matrix4f> temp;
        for (const auto &range : merged_ranges)
        {
            const uint32_t first = static_cast<uint32_t>(range.first);
            const uint32_t last = static_cast<uint32_t>(range.last);
            const uint32_t count = last - first + 1;

            temp.resize(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                const auto handle = handles[first + i];
                const glm::mat4 world_matrix = storage.GetWorldMatrix(handle);
                temp[i] = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
            }

            const VkDeviceSize byte_offset = sizeof(math::Matrix4f) * static_cast<VkDeviceSize>(first);
            const VkDeviceSize byte_size = sizeof(math::Matrix4f) * static_cast<VkDeviceSize>(count);

            if (!tbuf->Write(temp.data(), byte_offset, byte_size))
                continue;

            flush_ranges.push_back({byte_offset, byte_size});
        }

        if (!flush_ranges.empty())
            transform_buffer->FlushRanges(flush_ranges.data(), flush_ranges.size());
    }

    void TransformAssignmentBuffer::WriteDynamicDirtyIndices(const TransformDataStorage& storage,
                                                             const uint32_t static_count,
                                                             const std::vector<TransformDataStorage::HandleID>& handles,
                                                             const std::vector<uint32_t>& dirty_indices)
    {
        if (!transform_buffer || handles.empty() || dirty_indices.empty())
            return;

        auto *tbuf = transform_buffer->GetGPUBuffer();
        if (!tbuf)
            return;

        const uint32_t dynamic_count = static_cast<uint32_t>(handles.size());
        const uint32_t base_index = ring_writer.GetBaseIndex(static_count, dynamic_count);

        const auto merged_ranges = BuildMergedRangesFromIndices(dirty_indices, dynamic_count);
        if (merged_ranges.empty())
            return;

        std::vector<graph::IGPUBuffer::DirtyRange> flush_ranges;
        flush_ranges.reserve(merged_ranges.size());

        std::vector<math::Matrix4f> temp;
        for (const auto &range : merged_ranges)
        {
            const uint32_t first = static_cast<uint32_t>(range.first);
            const uint32_t last = static_cast<uint32_t>(range.last);
            const uint32_t count = last - first + 1;

            temp.resize(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                const auto handle = handles[first + i];
                const glm::mat4 world_matrix = storage.GetWorldMatrix(handle);
                temp[i] = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
            }

            const VkDeviceSize logical_index = static_cast<VkDeviceSize>(base_index + first);
            const VkDeviceSize byte_offset = sizeof(math::Matrix4f) * logical_index;
            const VkDeviceSize byte_size = sizeof(math::Matrix4f) * static_cast<VkDeviceSize>(count);

            if (!tbuf->Write(temp.data(), byte_offset, byte_size))
                continue;

            flush_ranges.push_back({byte_offset, byte_size});
        }

        if (!flush_ranges.empty())
            transform_buffer->FlushRanges(flush_ranges.data(), flush_ranges.size());
    }

    void TransformAssignmentBuffer::Clear()
    {
        if (buffer_manager)
        {
            if (transform_buffer)
                buffer_manager->Release(transform_buffer);
            if (transform_vab)
                buffer_manager->Release(transform_vab);
            transform_buffer = nullptr;
            transform_vab = nullptr;
        }
        else
        {
            SAFE_CLEAR(transform_buffer);
            SAFE_CLEAR(transform_vab);
        }

        transform_buffer_max_count = 0;
        node_count = 0;
        transform_vab_buffer = nullptr;
        pending_updates.clear();
    }

    void TransformAssignmentBuffer::StatTransform(const size_t required_count,graph::BufferAllocPolicy policy)
    {
        // 检查是否需要重新分配缓冲
        if (!transform_buffer)
        {
            transform_buffer_max_count = hgl::power_to_2(required_count);
        }
        else if (required_count > transform_buffer_max_count)
        {
            transform_buffer_max_count = hgl::power_to_2(required_count);
            if (buffer_manager)
            {
                buffer_manager->Release(transform_buffer);
                transform_buffer = nullptr;
            }
            else
            {
                SAFE_CLEAR(transform_buffer);
            }
        }

        // Recreate if policy changed
        if (transform_buffer && transform_policy != policy)
        {
            if (buffer_manager)
            {
                buffer_manager->Release(transform_buffer);
                transform_buffer = nullptr;
            }
            else
            {
                SAFE_CLEAR(transform_buffer);
            }
        }

        transform_policy = policy;

        // 创建或重用 Transform UBO
        if (!transform_buffer && buffer_manager)
        {
#if defined(HGL_L2W_USE_SSBO)
            transform_buffer = buffer_manager->CreateSSBO("ECS:LocalToWorld",
                                                          sizeof(math::Matrix4f) * transform_buffer_max_count,
                                                          nullptr,
                                                          graph::SharingMode::Exclusive);
#else
            transform_buffer = buffer_manager->CreateUBO("ECS:LocalToWorld",
                                                         sizeof(math::Matrix4f) * transform_buffer_max_count,
                                                         nullptr,
                                                         graph::SharingMode::Exclusive);
#endif
        }

        ring_writer.SetBuffer(transform_buffer);
    }

    static bool GetStorageWorldMatrix(const RenderItem* item, math::Matrix4f& out)
    {
        if (!item)
        {
            out = math::Identity4f;
            return false;
        }

        auto transform = item->GetTransform();
        if (!transform)
        {
            out = math::Identity4f;
            return false;
        }

        const auto handle = transform->GetStorageHandle();
        if (handle == TransformDataStorage::INVALID_HANDLE)
        {
            out = math::Identity4f;
            return false;
        }

        const auto storage = TransformComponent::GetSharedStorage();

        if (!storage)
        {
            out = math::Identity4f;
            return false;
        }

        const glm::mat4 world_matrix = storage->GetWorldMatrix(handle);
        out = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
        return true;
    }

    void TransformAssignmentBuffer::UpdateTransformData(const std::vector<RenderItem*>& items, const int first, const int last)
    {
        (void)items;
        QueueUpdateRange(first,last);
    }

    void TransformAssignmentBuffer::QueueUpdateRange(const int first,const int last)
    {
        if (first > last)
            return;

        UpdateRange range;
        range.first = first;
        range.last = last;

        for (auto &pending : pending_updates)
        {
            if (range.last + 1 < pending.first || range.first > pending.last + 1)
                continue;

            pending.first = hgl_min(pending.first, range.first);
            pending.last = hgl_max(pending.last, range.last);
            return;
        }

        pending_updates.push_back(range);
    }

    void TransformAssignmentBuffer::WriteRange(const std::vector<RenderItem*>& items,const int first,const int last)
    {
        if (!transform_buffer || items.empty() || first > last)
            return;

        const size_t map_size = sizeof(math::Matrix4f) * (last - first + 1);
        const size_t map_offset = sizeof(math::Matrix4f) * first;

        auto *tbuf = transform_buffer->GetGPUBuffer();
        math::Matrix4f* l2wp = tbuf ? (math::Matrix4f*)tbuf->Map(map_offset, map_size) : nullptr;
        if (!l2wp)
            return;

        const size_t count = items.size();
        for (size_t i = 0; i < count; i++)
        {
            RenderItem* item = items[i];
            if (!item)
                continue;

            const uint32_t transform_idx = item->transform_index;
            if (transform_idx < static_cast<uint32_t>(first) || transform_idx > static_cast<uint32_t>(last))
                continue;

            math::Matrix4f l2w;
            GetStorageWorldMatrix(item, l2w);
            l2wp[transform_idx - first] = l2w;
        }

        if(tbuf) tbuf->Unmap();
    }

    void TransformAssignmentBuffer::WriteItems(const std::vector<RenderItem*>& items)
    {
        const size_t item_count = items.size();

        if (item_count == 0)
        {
            std::cout << "[TransformAssignmentBuffer::WriteItems] WARNING: No items to write" << std::endl;
            return;
        }

        last_items = &items;

        static_only = (mode == Mode::StaticOnly);

        std::vector<RenderItem*> static_items;
        std::vector<RenderItem*> movable_items;
        static_items.reserve(item_count);
        movable_items.reserve(item_count);

        for (auto *item : items)
        {
            if (!item)
                continue;

            auto transform = item->GetTransform();
            if (transform && !transform->IsMovable())
                static_items.push_back(item);
            else
                movable_items.push_back(item);
        }

        std::sort(static_items.begin(), static_items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                const auto at = a ? a->GetTransform() : nullptr;
                const auto bt = b ? b->GetTransform() : nullptr;
                const auto ah = at ? at->GetStorageHandle() : TransformDataStorage::INVALID_HANDLE;
                const auto bh = bt ? bt->GetStorageHandle() : TransformDataStorage::INVALID_HANDLE;

                if (ah == TransformDataStorage::INVALID_HANDLE && bh == TransformDataStorage::INVALID_HANDLE)
                    return false;
                if (ah == TransformDataStorage::INVALID_HANDLE)
                    return false;
                if (bh == TransformDataStorage::INVALID_HANDLE)
                    return true;
                return ah < bh;
            });

        const uint32_t static_count = static_cast<uint32_t>(static_items.size());
        const uint32_t dynamic_count = static_cast<uint32_t>(movable_items.size());
        const uint32_t ring_frames = ring_writer.GetRingFrames();
        const uint32_t ring_base = ring_writer.GetBaseIndex(static_count, dynamic_count);
        const uint32_t total_count = ring_writer.GetTotalCount(static_count, dynamic_count);
        const uint32_t max_transform_id = std::numeric_limits<graph::Assign::TransformID::ValueType>::max();

        if (sizeof(graph::Assign::TransformID::ValueType) == sizeof(uint16_t)
         && total_count > max_transform_id + 1)
        {
            std::cout << "[TransformAssignmentBuffer::WriteItems] WARNING: Transform count exceeds R16 limit ("
                      << total_count << ")" << std::endl;
        }

        last_static_count = static_count;
        last_dynamic_count = dynamic_count;

        if (static_only)
            StatTransform(total_count,graph::BufferAllocPolicy::GPUOnly);
        else
            StatTransform(total_count,graph::BufferAllocPolicy::Auto);

        uint32_t transform_index = 0;
        for (auto *item : static_items)
        {
            item->transform_index = transform_index++;
        }

        uint32_t dynamic_index = 0;
        for (auto *item : movable_items)
        {
            item->transform_index = ring_base + dynamic_index++;
        }

        auto *wbuf = transform_buffer->GetGPUBuffer();
        math::Matrix4f* l2wp = wbuf ? (math::Matrix4f*)wbuf->Map(0, wbuf->GetSize()) : nullptr;
        if (l2wp)
        {
            for (auto *item : static_items)
            {
                if (!item)
                    continue;

                const uint32_t idx = item->transform_index;
                math::Matrix4f l2w;
                GetStorageWorldMatrix(item, l2w);
                l2wp[idx] = l2w;
            }

            for (auto *item : movable_items)
            {
                if (!item)
                    continue;

                const uint32_t idx = item->transform_index;
                math::Matrix4f l2w;
                GetStorageWorldMatrix(item, l2w);
                l2wp[idx] = l2w;
            }

            if(wbuf) wbuf->Unmap();
        }

        // 2. 创建或重用 Transform VAB（索引缓冲）
        {
            if (!transform_vab)
            {
                node_count = power_to_2(item_count);
            }
            else if (node_count < item_count)
            {
                node_count = power_to_2(item_count);
                if (buffer_manager)
                {
                    buffer_manager->Release(transform_vab);
                    transform_vab = nullptr;
                }
                else
                {
                    SAFE_CLEAR(transform_vab);
                }
            }

            if (!transform_vab)
            {
                graph::BufferAllocPolicy vab_policy = static_only ? graph::BufferAllocPolicy::GPUOnly : graph::BufferAllocPolicy::Auto;
                if (buffer_manager)
                {
                    transform_vab = buffer_manager->CreateVAB(graph::Assign::TransformID::VAB_FMT, node_count, nullptr, vab_policy);
                    transform_vab_buffer = transform_vab ? transform_vab->GetVkBuffer() : nullptr;

                #ifdef _DEBUG
                    auto device = buffer_manager->GetDevice();
                    graph::DebugUtils* du = device ? device->GetDebugUtils() : nullptr;
                    if (du && transform_vab)
                    {
                        du->SetBuffer(transform_vab->GetVkBuffer(), "ECS:VAB:Buffer:TransformID");
                        du->SetDeviceMemory(transform_vab->GetVkMemory(), "ECS:VAB:Memory:TransformID");
                    }
                #endif//_DEBUG
                }
            }
        }

        // 3. 生成 transform 索引列表
        {
            graph::IGPUBuffer *transform_gpu = transform_vab->GetGPUBuffer();
            graph::Assign::TransformID::ValueType* transform_ptr =
                transform_gpu
                    ? (graph::Assign::TransformID::ValueType*)(transform_gpu->Map(0, transform_gpu->GetSize()))
                    : nullptr;
            bool warned_overflow = false;

            for (size_t i = 0; i < item_count; i++)
            {
                RenderItem* item = items[i];

                if (!item)
                {
                    *transform_ptr = 0;
                    ++transform_ptr;
                    continue;
                }

                const uint32_t idx = item->transform_index;
                if (idx > max_transform_id)
                {
                    if (!warned_overflow && sizeof(graph::Assign::TransformID::ValueType) == sizeof(uint16_t))
                    {
                        std::cout << "[TransformAssignmentBuffer::WriteItems] WARNING: TransformID overflow ("
                                  << idx << ")" << std::endl;
                        warned_overflow = true;
                    }

                    *transform_ptr = static_cast<graph::Assign::TransformID::ValueType>(0);
                }
                else
                {
                    *transform_ptr = static_cast<graph::Assign::TransformID::ValueType>(idx);
                }
                ++transform_ptr;

                // if (i < 5 || i >= item_count - 2)  // 只打印前几个和后几个，避免刷屏
                // {
                //     std::cout << "[TransformAssignmentBuffer::WriteItems]   Item[" << i
                //               << "] -> transform_index=" << item->transform_index << std::endl;
                // }
            }

            transform_vab->Unmap();
        }
    }

    void TransformAssignmentBuffer::FlushPendingUpdates()
    {
        if (pending_updates.empty() || !last_items || !transform_buffer)
            return;

        std::sort(pending_updates.begin(), pending_updates.end(),
            [](const UpdateRange &a, const UpdateRange &b)
            {
                return a.first < b.first;
            });

        std::vector<graph::IGPUBuffer::DirtyRange> dirty_ranges;
        dirty_ranges.reserve(pending_updates.size());

        const VkDeviceSize matrix_size = sizeof(math::Matrix4f);
        const VkDeviceSize buffer_size = transform_buffer->GetSize();

        for (const auto &range : pending_updates)
        {
            if (range.first < 0 || range.last < range.first)
                continue;

            WriteRange(*last_items, range.first, range.last);

            VkDeviceSize offset = matrix_size * static_cast<VkDeviceSize>(range.first);
            VkDeviceSize size = matrix_size * static_cast<VkDeviceSize>(range.last - range.first + 1);

            if (offset >= buffer_size)
                continue;

            if (offset + size > buffer_size)
                size = buffer_size - offset;

            if (size == 0)
                continue;

            dirty_ranges.push_back({offset, size});
        }

        if (!dirty_ranges.empty())
            transform_buffer->FlushRanges(dirty_ranges.data(), dirty_ranges.size());

        pending_updates.clear();
    }

    void TransformAssignmentBuffer::FlushAllPendingUpdates()
    {
        for (auto *inst : all_instances)
        {
            if (inst)
                inst->FlushPendingUpdates();
        }
    }

    void TransformAssignmentBuffer::AdvanceFrame()
    {
        for (auto *inst : all_instances)
        {
            if (inst)
                inst->ring_writer.AdvanceFrame();
        }
    }

    void TransformAssignmentBuffer::SetFrameIndex(const uint32_t index)
    {
        for (auto *inst : all_instances)
        {
            if (inst)
                inst->ring_writer.SetFrameIndex(index);
        }
    }
}//namespace hgl::ecs

