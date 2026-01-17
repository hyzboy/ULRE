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
    ECSTransformAssignmentBuffer::ECSTransformAssignmentBuffer(graph::VulkanDevice* dev)
        : device(dev)
        , transform_buffer_max_count(0)
        , transform_buffer(nullptr)
        , node_count(0)
        , transform_vab(nullptr)
        , transform_vab_buffer(nullptr)
    {
        MaxTransformCount = dev->GetUBORange() / sizeof(math::Matrix4f);
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
    }

    void ECSTransformAssignmentBuffer::StatTransform(const std::vector<RenderItem*>& items)
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

        // 创建或重用 Transform UBO
        if (!transform_buffer)
        {
            transform_buffer = device->CreateUBO(sizeof(math::Matrix4f) * transform_buffer_max_count);

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
        if (!transform_buffer)
        {
            std::cout << "[ECSTransformAssignmentBuffer::UpdateTransformData] ERROR: Transform buffer not created" << std::endl;
            return;
        }

        if (items.empty())
        {
            std::cout << "[ECSTransformAssignmentBuffer::UpdateTransformData] WARNING: Empty items list" << std::endl;
            return;
        }

        const size_t count = items.size();
        const size_t map_size = sizeof(math::Matrix4f) * (last - first + 1);
        const size_t map_offset = sizeof(math::Matrix4f) * first;

        math::Matrix4f* l2wp = (math::Matrix4f*)(transform_buffer->Map(map_offset, map_size));

        for (size_t i = 0; i < count; i++)
        {
            RenderItem* item = items[i];

            if (!item)
                continue;

            const uint32_t transform_idx = item->transform_index;

            // 检查索引是否在更新范围内
            if (transform_idx < first || transform_idx > last)
                continue;

            // 获取世界矩阵
            glm::mat4 world_matrix = item->GetWorldMatrix();

            // 写入到缓冲的正确位置
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

        // 1. 写入变换矩阵数据
        StatTransform(items);

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
                transform_vab = device->CreateVAB(VK_FORMAT_R16_UINT, node_count);
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
}//namespace hgl::ecs
