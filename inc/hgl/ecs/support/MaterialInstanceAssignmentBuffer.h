/**
 * MaterialInstanceAssignmentBuffer.h - ECS材质实例数据管理
 *
 * 针对 ECS 架构的 RenderItem 和 MaterialBatch 设计
 * 与 SceneGraph 的 MaterialInstanceAssignmentBuffer 功能相同，但适配 ECS 数据结构
 */

#pragma once
#include<hgl/vk/VK.h>
#include<hgl/vk/VKRingBufferWrapper.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<vector>
#include <hgl/type/UnorderedMap.h>

namespace hgl::graph
{
    class BufferManager;
}

namespace hgl::ecs
{
    /**
     * 材质实例集合 - 用于去重和索引管理
     */
    class MaterialInstanceSet
    {
    private:
        std::vector<graph::MaterialInstance*> instances;
        hgl::UnorderedMap<graph::MaterialInstance*, uint16> index_map;

    public:
        void Clear()
        {
            instances.clear();
            index_map.Clear();
        }

        void Reserve(size_t count)
        {
            instances.reserve(count);
            index_map.Reserve(count);
        }

        void Add(graph::MaterialInstance* mi)
        {
            if (!mi)
                return;

            if (!index_map.ContainsKey(mi))
            {
                uint16 index = static_cast<uint16>(instances.size());
                instances.push_back(mi);
                index_map[mi] = index;
            }
        }

        uint16 Find(graph::MaterialInstance* mi) const
        {
            auto index = index_map.GetValuePointer(mi);
            return index ? *index : 0;
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
    class MaterialInstanceAssignmentBuffer
    {
    private:
        graph::BufferManager* buffer_manager;   ///<缓冲区管理器
        graph::Material* material;              ///<所属材质

    private:    // 材质实例数据
        MaterialInstanceSet mi_set;         ///<材质实例集合（去重）

        uint32_t material_instance_data_bytes;      ///<单个材质实例数据字节数
        graph::DeviceBuffer* material_instance_buffer;  ///<材质实例数据(UBO/SSBO)
        graph::RingBufferWrapper material_instance_ring_writer;

        void StatMaterialInstance(const std::vector<RenderItem*>& items);

    private:    // 分发数据
        uint32_t node_count;                        ///<节点数量
        graph::VAB* material_instance_vab;          ///<材质实例ID分发数据VAB（当前为 R16UI）
        VkBuffer material_instance_vab_buffer;      ///<材质实例ID分发数据Buffer
        uint32_t data_index_node_count;             ///<DataIndex节点数量
        graph::VAB* data_index_vab;                 ///<DataIndex行索引分发VAB（格式见 Assign::DataIndexID）
        VkBuffer data_index_vab_buffer;             ///<DataIndex行索引分发Buffer
        uint32_t texture_layer_node_count;          ///<TextureLayer节点数量
        graph::VAB* texture_layer_vab;              ///<TextureLayer行索引分发VAB（格式见 Assign::TextureLayerID）
        VkBuffer texture_layer_vab_buffer;          ///<TextureLayer行索引分发Buffer

    private:
        void Clear();

    public:
        MaterialInstanceAssignmentBuffer(graph::BufferManager* bm, graph::Material* mtl);
        ~MaterialInstanceAssignmentBuffer() { Clear(); }

        /**
         * 获取MaterialInstance VAB缓冲（用于绑定到管线）
         */
        const VkBuffer GetMaterialInstanceVAB() const { return material_instance_vab_buffer; }

        /**
         * Phase 3 迁移接口：DataIndex 间接表行索引 VAB
         * 当前为独立分发缓冲，数值与 MI 索引保持一致，后续会切换为真实 DataIndex 行索引。
         */
        const VkBuffer GetDataIndexVAB() const { return data_index_vab_buffer; }

        /**
         * TextureLayer 间接表行索引 VAB（为后续绘制输入接线预留）。
         */
        const VkBuffer GetTextureLayerVAB() const { return texture_layer_vab_buffer; }

        /**
         * 绑定材质实例数据到材质
         */
        void BindMaterialInstance(graph::Material* mtl) const;

        /**
         * 写入所有RenderItem的材质实例数据
         * @param items RenderItem列表
         * @param data_index_rows 可选 DataIndex 行索引（与 items 一一对应）
         * @param texture_layer_rows 可选 TextureLayer 行索引（与 items 一一对应）
         */
        void WriteItems(const std::vector<RenderItem*>& items,
                        const std::vector<uint32> *data_index_rows = nullptr,
                        const std::vector<uint32> *texture_layer_rows = nullptr);

        /**
         * 更新单个RenderItem的材质实例数据
         * @param item 需要更新的RenderItem
         */
        void UpdateMaterialInstanceData(RenderItem* item);
    };
}//namespace hgl::ecs
