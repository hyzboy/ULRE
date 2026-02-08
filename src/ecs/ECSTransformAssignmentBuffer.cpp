/**
 * ECSTransformAssignmentBuffer.cpp - ECS Transform 分配缓冲实现
 */

#include"ECSTransformAssignmentBuffer.h"
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/ecs/TransformComponent.h>
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
    {
        MaxTransformCount = dev->GetUBORange() / sizeof(math::Matrix4f);
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

        mtl->BindUBO(&hgl::graph::mtl::SBS_LocalToWorld, transform_buffer);
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

    void ECSTransformAssignmentBuffer::StatTransform(const std::vector<RenderItem*>& items,graph::BufferAllocPolicy policy)
    {
        const size_t item_count = items.size();

        // 检查是否需要重新分配缓冲
        if (!transform_buffer)
        {
            transform_buffer_max_count = hgl::power_to_2(item_count);
        }
        else if (item_count > transform_buffer_max_count)
        {
            transform_buffer_max_count = hgl::power_to_2(item_count);
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
            transform_buffer = device->CreateUBO(sizeof(math::Matrix4f) * transform_buffer_max_count,
                                                 nullptr,
                                                 transform_policy);

        #ifdef _DEBUG
            graph::DebugUtils* du = device->GetDebugUtils();
            if (du)
            {
                du->SetBuffer(transform_buffer->GetBuffer(), "ECS:UBO:Buffer:LocalToWorld");
                du->SetDeviceMemory(transform_buffer->GetVkMemory(), "ECS:UBO:Memory:LocalToWorld");
            }
        #endif//_DEBUG
        }

        // 写入所有 LocalToWorld 矩阵
        math::Matrix4f* l2wp = (math::Matrix4f*)(transform_buffer->Map());

        for (size_t i = 0; i < item_count; i++)
        {
            RenderItem* item = items[i];

            if (!item)
            {
                *l2wp = math::Identity4f;
                ++l2wp;
                continue;
            }

            // 从 RenderItem 获取世界矩阵
            glm::mat4 world_matrix = item->GetWorldMatrix();

            // 转换 glm::mat4 到 hgl::math::Matrix4f
            // GLM是列主序，HGL也是列主序，可以直接memcpy
            *l2wp = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);

            ++l2wp;
        }

        transform_buffer->Unmap();
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

            glm::mat4 world_matrix = item->GetWorldMatrix();
            l2wp[transform_idx - first] = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
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
        if (static_only)
            StatTransform(items,graph::BufferAllocPolicy::GPUOnly);
        else
            StatTransform(items,graph::BufferAllocPolicy::Auto);

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
                transform_vab = device->CreateVAB(VK_FORMAT_R16_UINT, node_count, nullptr, vab_policy);
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
            uint16* transform_ptr = (uint16*)(transform_vab->DeviceBuffer::Map());

            for (size_t i = 0; i < item_count; i++)
            {
                RenderItem* item = items[i];

                if (!item)
                {
                    *transform_ptr = 0;
                    ++transform_ptr;
                    continue;
                }

                // 分配 transform_index
                item->transform_index = i;
                *transform_ptr = i;
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
}//namespace hgl::ecs
