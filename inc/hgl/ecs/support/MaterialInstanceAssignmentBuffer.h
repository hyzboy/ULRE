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
#include<vector>
#include <hgl/type/UnorderedMap.h>
#include<hgl/common/RenderOptions.h>

namespace hgl::graph
{
    class BufferManager;
    class MaterialResourceDomain;
}

namespace hgl::ecs
{
    struct MaterialSlotEntry
    {
        graph::MaterialResourceDomain* domain = nullptr;
        int mi_id = -1;
        graph::Primitive* primitive_fallback = nullptr;
        const void* mi_data_ptr = nullptr;
        const void* mit_data_ptr = nullptr;
        uint32_t mit_data_bytes = 0;
    };

    /**
     * MaterialSlot 集合 - 优先按 domain+mi_id 去重；
     * 无 resolved slot 时回退按 Primitive* 去重。
     */
    class MaterialSlotSet
    {
    private:
        std::vector<MaterialSlotEntry> entries;
        hgl::UnorderedMap<uint64_t, uint16> slot_index_map;
        hgl::UnorderedMap<graph::Primitive*, uint16> primitive_index_map;

        static uint64_t MakeSlotKey(graph::MaterialResourceDomain* domain, int mi_id)
        {
            const uint64_t d = uint64_t(reinterpret_cast<uintptr_t>(domain));
            const uint32_t id = static_cast<uint32_t>(mi_id);
            return (d << 32) ^ uint64_t(id);
        }

    public:
        void Clear()
        {
            entries.clear();
            slot_index_map.Clear();
            primitive_index_map.Clear();
        }

        void Reserve(size_t count)
        {
            entries.reserve(count);
            slot_index_map.Reserve(count);
            primitive_index_map.Reserve(count);
        }

        uint16 AddResolved(graph::MaterialResourceDomain* domain,
                           int mi_id,
                           const void* mi_data_ptr,
                           const void* mit_data_ptr,
                           uint32_t mit_data_bytes)
        {
            if (!domain || mi_id < 0)
                return 0;

            const uint64_t key = MakeSlotKey(domain, mi_id);
            if (auto p = slot_index_map.GetValuePointer(key))
                return *p;

            const uint16 index = static_cast<uint16>(entries.size());
            MaterialSlotEntry e;
            e.domain = domain;
            e.mi_id = mi_id;
            e.mi_data_ptr = mi_data_ptr;
            e.mit_data_ptr = mit_data_ptr;
            e.mit_data_bytes = mit_data_bytes;
            entries.push_back(e);
            slot_index_map[key] = index;
            return index;
        }

        uint16 AddPrimitive(graph::Primitive* prim)
        {
            if (!prim)
                return 0;

            if (auto p = primitive_index_map.GetValuePointer(prim))
                return *p;

            const uint16 index = static_cast<uint16>(entries.size());
            MaterialSlotEntry e;
            e.primitive_fallback = prim;
            entries.push_back(e);
            primitive_index_map[prim] = index;
            return index;
        }

        uint16 FindResolved(graph::MaterialResourceDomain* domain, int mi_id) const
        {
            if (!domain || mi_id < 0)
                return 0;

            const uint64_t key = MakeSlotKey(domain, mi_id);
            auto p = slot_index_map.GetValuePointer(key);
            return p ? *p : 0;
        }

        uint16 FindPrimitive(graph::Primitive* prim) const
        {
            auto p = primitive_index_map.GetValuePointer(prim);
            return p ? *p : 0;
        }

        size_t GetCount() const { return entries.size(); }
        const MaterialSlotEntry* GetEntry(size_t i) const
        {
            if (i >= entries.size())
                return nullptr;
            return &entries[i];
        }

        auto begin() { return entries.begin(); }
        auto end() { return entries.end(); }
        auto begin() const { return entries.begin(); }
        auto end() const { return entries.end(); }
    };

    /**
     * ECS 材质实例分配缓冲
     *
     * 职责：
     * - 管理所有 RenderItem 的材质实例数据（SSBO）
     * - 去重材质实例，合并相同的MI数据
     * - 生成材质实例索引分发数据（SSBO）
     * - 为每个实例分配MI索引
     */
    class MaterialInstanceAssignmentBuffer
    {
    private:
        graph::BufferManager* buffer_manager;   ///<缓冲区管理器
        graph::MaterialTemplate* material;              ///<所属材质

    private:    // 材质实例数据
        MaterialSlotSet slot_set;           ///<MaterialSlot 集合（domain+mi_id 优先，Primitive* 回退）

        uint32_t material_instance_data_bytes;      ///<单个材质实例数据字节数
        graph::DeviceBuffer* material_instance_buffer;  ///<材质实例数据(UBO/SSBO)
        graph::RingBufferWrapper material_instance_ring_writer;

        void StatMaterialInstance(const std::vector<RenderItem*>& items);

    private:    // 分发数据（SSBO descriptor path）
        uint32_t node_count;                        ///<节点数量
        uint32_t material_instance_id_buffer_max_count; ///<MaterialInstanceID SSBO capacity (elements)
        graph::DeviceBuffer* material_instance_id_buffer;   ///<MaterialInstanceID data (SSBO, uint[])
        VkBuffer material_instance_id_vk_buffer;            ///<MaterialInstanceID VkBuffer cache

    private:    // MIT SSBO（TextureArray用：per-instance纹理层索引）
        uint32_t mit_data_bytes;                ///< per-entry MIT struct size; 0 = no TextureArray slots
        uint32_t mit_buffer_max_count;          ///< MIT SSBO capacity (elements)
        graph::DeviceBuffer* mit_buffer;        ///< MaterialInstanceTextureID SSBO

    private:
        void Clear();

    public:
        MaterialInstanceAssignmentBuffer(graph::BufferManager* bm, graph::MaterialTemplate* mtl);
        ~MaterialInstanceAssignmentBuffer() { Clear(); }

        /**
         * 获取MaterialInstanceID SSBO VkBuffer（用于绑定到管线）
         */
        const VkBuffer GetMaterialInstanceIDVkBuffer() const { return material_instance_id_vk_buffer; }

        /**
         * 绑定MaterialInstanceID SSBO到材质
         */
        void BindMaterialInstanceID(graph::MaterialTemplate* mtl) const;

        /**
         * 绑定材质实例数据到材质
         */
        void BindMaterialInstance(graph::MaterialTemplate* mtl) const;

        /**
         * 绑定MaterialInstanceTextureID SSBO到材质（TextureArray用）
         * 当材质实例包含 MIT 数据时自动填充并绑定；无 MIT 数据时为空操作。
         */
        void BindMaterialInstanceTextureID(graph::MaterialTemplate* mtl) const;

        /**
         * 写入所有RenderItem的材质实例数据
         * @param items RenderItem列表
         */
        void WriteItems(const std::vector<RenderItem*>& items);

        /**
         * 更新单个RenderItem的材质实例索引分发数据
         * @param item 需要更新的RenderItem
         */
        void UpdateMaterialInstanceData(RenderItem* item);
    };
}//namespace hgl::ecs


