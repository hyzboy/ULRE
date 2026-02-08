/**
 * ECSTransformAssignmentBuffer.cpp - ECS Transform 分配缓冲实现
 */

#include<hgl/graph/RenderOptions.h>
#include"ECSTransformAssignmentBuffer.h"
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/ecs/TransformComponent.h>
#include<algorithm>
#include<limits>
#include<iostream>

namespace hgl::ecs
{
    std::vector<ECSTransformAssignmentBuffer*> ECSTransformAssignmentBuffer::all_instances;

    ECSTransformAssignmentBuffer::ECSTransformAssignmentBuffer(graph::VulkanDevice* dev, const Mode m)
        : device(dev)
        , transform_buffer_max_count(0)
        , transform_buffer(nullptr)
        , transform_policy(graph::BufferAllocPolicy::Auto)
        , static_only(false)
        , mode(m)
        , node_count(0)
        , transform_vab(nullptr)
        , transform_vab_buffer(nullptr)
        , ring_writer(nullptr, sizeof(math::Matrix4f), HGL_L2W_RING_FRAMES)
    {
#if defined(HGL_L2W_USE_SSBO)
        MaxTransformCount = dev->GetSSBORange() / sizeof(math::Matrix4f);
#else
        MaxTransformCount = dev->GetUBORange() / sizeof(math::Matrix4f);
#endif
        all_instances.push_back(this);
    }

    void ECSTransformAssignmentBuffer::BindTransform(graph::Material* mtl) const
    {
        if (!mtl)
        {
            std::cout << "[ECSTransformAssignmentBuffer::BindTransform] WARNING: Material is null" << std::endl;
            return;
        }

        if (!transform_buffer)
        {
            std::cout << "[ECSTransformAssignmentBuffer::BindTransform] WARNING: Transform buffer not created" << std::endl;
            return;
        }

    #if defined(HGL_L2W_USE_SSBO)
        mtl->BindSSBO(hgl::graph::mtl::SBS_LocalToWorld.set_type,
                  hgl::graph::mtl::SBS_LocalToWorld.name,
                  transform_buffer);
    #else
        mtl->BindUBO(&hgl::graph::mtl::SBS_LocalToWorld, transform_buffer);
    #endif
    }

    void ECSTransformAssignmentBuffer::Clear()
    {
        SAFE_CLEAR(transform_buffer);
        SAFE_CLEAR(transform_vab);

        transform_buffer_max_count = 0;
        node_count = 0;
        transform_vab_buffer = nullptr;
        pending_updates.clear();
    }

    void ECSTransformAssignmentBuffer::StatTransform(const size_t required_count,graph::BufferAllocPolicy policy)
    {
        // 检查是否需要重新分配缓冲
        if (!transform_buffer)
        {
            transform_buffer_max_count = hgl::power_to_2(required_count);
        }
        else if (required_count > transform_buffer_max_count)
        {
            transform_buffer_max_count = hgl::power_to_2(required_count);
            SAFE_CLEAR(transform_buffer);
        }

        // Recreate if policy changed
        if (transform_buffer && transform_policy != policy)
        {
            SAFE_CLEAR(transform_buffer);
        }

        transform_policy = policy;

        // 创建或重用 Transform UBO
        if (!transform_buffer)
        {
#if defined(HGL_L2W_USE_SSBO)
            transform_buffer = device->CreateSSBO(sizeof(math::Matrix4f) * transform_buffer_max_count,
                                                  nullptr,
                                                  transform_policy);
#else
            transform_buffer = device->CreateUBO(sizeof(math::Matrix4f) * transform_buffer_max_count,
                                                 nullptr,
                                                 transform_policy);
#endif

        #ifdef _DEBUG
            graph::DebugUtils* du = device->GetDebugUtils();
            if (du)
            {
#if defined(HGL_L2W_USE_SSBO)
                du->SetBuffer(transform_buffer->GetBuffer(), "ECS:SSBO:Buffer:LocalToWorld");
                du->SetDeviceMemory(transform_buffer->GetVkMemory(), "ECS:SSBO:Memory:LocalToWorld");
#else
                du->SetBuffer(transform_buffer->GetBuffer(), "ECS:UBO:Buffer:LocalToWorld");
                du->SetDeviceMemory(transform_buffer->GetVkMemory(), "ECS:UBO:Memory:LocalToWorld");
#endif
            }
        #endif//_DEBUG
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

        const auto storage = transform->IsMovable()
            ? TransformComponent::GetDynamicStorage()
            : TransformComponent::GetStaticStorage();

        if (!storage)
        {
            out = math::Identity4f;
            return false;
        }

        const glm::mat4 world_matrix = storage->GetWorldMatrix(handle);
        out = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
        return true;
    }

    void ECSTransformAssignmentBuffer::UpdateTransformData(const std::vector<RenderItem*>& items, const int first, const int last)
    {
        (void)items;
        QueueUpdateRange(first,last);
    }

    void ECSTransformAssignmentBuffer::QueueUpdateRange(const int first,const int last)
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

    void ECSTransformAssignmentBuffer::WriteRange(const std::vector<RenderItem*>& items,const int first,const int last)
    {
        if (!transform_buffer || items.empty() || first > last)
            return;

        const size_t map_size = sizeof(math::Matrix4f) * (last - first + 1);
        const size_t map_offset = sizeof(math::Matrix4f) * first;

        math::Matrix4f* l2wp = (math::Matrix4f*)(transform_buffer->Map(map_offset, map_size));
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

        transform_buffer->Unmap();
    }

    void ECSTransformAssignmentBuffer::WriteItems(const std::vector<RenderItem*>& items)
    {
        const size_t item_count = items.size();

        if (item_count == 0)
        {
            std::cout << "[ECSTransformAssignmentBuffer::WriteItems] WARNING: No items to write" << std::endl;
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
            std::cout << "[ECSTransformAssignmentBuffer::WriteItems] WARNING: Transform count exceeds R16 limit ("
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

        math::Matrix4f* l2wp = (math::Matrix4f*)(transform_buffer->Map());
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

            transform_buffer->Unmap();
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
                SAFE_CLEAR(transform_vab);
            }

            if (!transform_vab)
            {
                graph::BufferAllocPolicy vab_policy = static_only ? graph::BufferAllocPolicy::GPUOnly : graph::BufferAllocPolicy::Auto;
                transform_vab = device->CreateVAB(graph::Assign::TransformID::VAB_FMT, node_count, nullptr, vab_policy);
                transform_vab_buffer = transform_vab->GetBuffer();

            #ifdef _DEBUG
                graph::DebugUtils* du = device->GetDebugUtils();
                if (du)
                {
                    du->SetBuffer(transform_vab->GetBuffer(), "ECS:VAB:Buffer:TransformID");
                    du->SetDeviceMemory(transform_vab->GetVkMemory(), "ECS:VAB:Memory:TransformID");
                }
            #endif//_DEBUG
            }
        }

        // 3. 生成 transform 索引列表
        {
            graph::Assign::TransformID::ValueType* transform_ptr =
                (graph::Assign::TransformID::ValueType*)(transform_vab->DeviceBuffer::Map());
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
                        std::cout << "[ECSTransformAssignmentBuffer::WriteItems] WARNING: TransformID overflow ("
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
                //     std::cout << "[ECSTransformAssignmentBuffer::WriteItems]   Item[" << i
                //               << "] -> transform_index=" << item->transform_index << std::endl;
                // }
            }

            transform_vab->Unmap();
        }
    }

    void ECSTransformAssignmentBuffer::FlushPendingUpdates()
    {
        if (pending_updates.empty() || !last_items)
            return;

        for (const auto &range : pending_updates)
        {
            WriteRange(*last_items, range.first, range.last);
        }

        pending_updates.clear();
    }

    void ECSTransformAssignmentBuffer::FlushAllPendingUpdates()
    {
        for (auto *inst : all_instances)
        {
            if (inst)
                inst->FlushPendingUpdates();
        }
    }

    void ECSTransformAssignmentBuffer::AdvanceFrame()
    {
        for (auto *inst : all_instances)
        {
            if (inst)
                inst->ring_writer.AdvanceFrame();
        }
    }

    void ECSTransformAssignmentBuffer::SetFrameIndex(const uint32_t index)
    {
        for (auto *inst : all_instances)
        {
            if (inst)
                inst->ring_writer.SetFrameIndex(index);
        }
    }
}//namespace hgl::ecs
