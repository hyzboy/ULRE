/**
 * ECSMaterialInstanceAssignmentBuffer.h - ECS材质实例数据管理
 * 
 * 针对 ECS 架构的 RenderItem 和 MaterialBatch 设计
 * 与 SceneGraph 的 MaterialInstanceAssignmentBuffer 功能相同，但适配 ECS 数据结构
 */

#pragma once
#include<hgl/graph/VK.h>
#include<hgl/ecs/RenderItem.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<vector>
#include<unordered_map>

namespace hgl::ecs
{
    /**
     * 材质实例集合 - 用于去重和索引管理
     */
    class MaterialInstanceSet
    {
    private:
        std::vector<graph::MaterialInstance*> instances;
        std::unordered_map<graph::MaterialInstance*, uint16> index_map;

    public:
        void Clear()
        {
            instances.clear();
            index_map.clear();
        }

        void Reserve(size_t count)
        {
            instances.reserve(count);
            index_map.reserve(count);
        }

        void Add(graph::MaterialInstance* mi)
        {
            if (!mi)
                return;

            if (index_map.find(mi) == index_map.end())
            {
                uint16 index = static_cast<uint16>(instances.size());
                instances.push_back(mi);
                index_map[mi] = index;
            }
        }

        uint16 Find(graph::MaterialInstance* mi) const
        {
            auto it = index_map.find(mi);
            return (it != index_map.end()) ? it->second : 0;
        }

        size_t GetCount() const { return instances.size(); }
        size_t GetAllocCount() const { return instances.capacity(); }

        const std::vector<graph::MaterialInstance*>& GetInstances() const { return instances; }

        // 迭代器支持
        auto begin() { return instances.begin(); }
        auto end() { return instances.end(); }
        auto begin() const { return instances.begin(); }
        auto end() const { return instances.end(); }
    };

    /**
     * ECS 材质实例分配缓冲
     * 
     * 职责：
     * - 管理所有 RenderItem 的材质实例数据（UBO/SSBO）
     * - 去重材质实例，合并相同的MI数据
     * - 生成材质实例索引分发数据（VAB）
     * - 为每个实例分配MI索引
     */
    class ECSMaterialInstanceAssignmentBuffer
    {
    private:
        graph::VulkanDevice* device;        ///<Vulkan设备
        graph::Material* material;          ///<所属材质

    private:    // 材质实例数据
        MaterialInstanceSet mi_set;         ///<材质实例集合（去重）

        uint32_t material_instance_data_bytes;      ///<单个材质实例数据字节数
        graph::DeviceBuffer* material_instance_buffer;  ///<材质实例数据(UBO/SSBO)

        void StatMaterialInstance(const std::vector<RenderItem*>& items);

    private:    // 分发数据
        uint32_t node_count;                        ///<节点数量
        graph::VAB* material_instance_vab;          ///<材质实例ID分发数据VAB(R16UI格式)
        VkBuffer material_instance_vab_buffer;      ///<材质实例ID分发数据Buffer

    private:
        void Clear();

    public:
        ECSMaterialInstanceAssignmentBuffer(graph::VulkanDevice* dev, graph::Material* mtl);
        ~ECSMaterialInstanceAssignmentBuffer() { Clear(); }

        /**
         * 获取MaterialInstance VAB缓冲（用于绑定到管线）
         */
        const VkBuffer GetMaterialInstanceVAB() const { return material_instance_vab_buffer; }

        /**
         * 绑定材质实例数据到材质
         */
        void BindMaterialInstance(graph::Material* mtl) const;

        /**
         * 写入所有RenderItem的材质实例数据
         * @param items RenderItem列表
         */
        void WriteItems(const std::vector<RenderItem*>& items);

        /**
         * 更新单个RenderItem的材质实例数据
         * @param item 需要更新的RenderItem
         */
        void UpdateMaterialInstanceData(RenderItem* item);
    };
}//namespace hgl::ecs
