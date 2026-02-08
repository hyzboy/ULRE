/**
 * ECSTransformAssignmentBuffer.h - ECS渲染项Transform数据管理
 *
 * 针对 ECS 架构的 RenderItem 和 MaterialBatch 设计
 * 与 SceneGraph 的 TransformAssignmentBuffer 功能相同，但适配 ECS 数据结构
 */

#pragma once
#include<hgl/graph/VK.h>
#include<hgl/graph/VKMemory.h>
#include<hgl/ecs/RenderItem.h>
#include<vector>

namespace hgl::ecs
{
    /**
     * ECS Transform 分配缓冲
     *
     * 职责：
     * - 管理所有 RenderItem 的 LocalToWorld 矩阵数据（UBO/SSBO）
     * - 生成 Transform 索引分发数据（VAB）
     * - 为每个实例分配唯一的 transform_index
     * - 支持动态更新变换矩阵
     */
    class ECSTransformAssignmentBuffer
    {
    public:

        enum class Mode
        {
            StaticOnly,
            MovableOnly
        };

    private:
        uint32_t MaxTransformCount;             ///<单个SSBO最大支持的变换数量
        graph::VulkanDevice* device;            ///<Vulkan设备

    private:    // LocalToWorld矩阵数据
        uint32_t transform_buffer_max_count;    ///<LocalToWorld矩阵最大数量
        graph::DeviceBuffer* transform_buffer;  ///<LocalToWorld矩阵数据(UBO/SSBO)
        graph::BufferAllocPolicy transform_policy;     ///<Transform buffer allocation policy
        bool static_only;                       ///<Only static transforms in this batch

        Mode mode;

        struct UpdateRange
        {
            int first=0;
            int last=0;
        };
        std::vector<UpdateRange> pending_updates;
        const std::vector<RenderItem*>* last_items=nullptr;

        uint32_t last_static_count=0;
        uint32_t last_dynamic_count=0;

        static std::vector<ECSTransformAssignmentBuffer*> all_instances;
        static uint32_t ring_frame_index;

        void StatTransform(const size_t required_count,graph::BufferAllocPolicy policy);
        void QueueUpdateRange(const int first,const int last);
        void WriteRange(const std::vector<RenderItem*>& items,const int first,const int last);

    private:    // 分发数据
        uint32_t node_count;                    ///<节点数量
        graph::VAB* transform_vab;              ///<LocalToWorld矩阵ID分发数据VAB(R16UI格式)
        VkBuffer transform_vab_buffer;          ///<LocalToWorld矩阵ID分发数据Buffer

    private:
        void Clear();

    public:
        ECSTransformAssignmentBuffer(graph::VulkanDevice* dev, const Mode m = Mode::MovableOnly);
        ~ECSTransformAssignmentBuffer() { Clear(); }

        /**
         * 获取Transform VAB缓冲（用于绑定到管线）
         */
        const VkBuffer GetTransformVAB() const { return transform_vab_buffer; }

        /**
         * 绑定Transform数据到材质
         */
        void BindTransform(graph::Material* mtl) const;

        /**
         * 写入所有RenderItem的变换数据
         * @param items RenderItem列表
         */
        void WriteItems(const std::vector<RenderItem*>& items);

        /**
         * 更新变换数据（用于动态对象）
         * @param items 需要更新的RenderItem列表
         * @param first 第一个索引
         * @param last 最后一个索引
         */
        void UpdateTransformData(const std::vector<RenderItem*>& items, const int first, const int last);

        /**
         * Flush pending update ranges once per frame
         */
        void FlushPendingUpdates();
        static void FlushAllPendingUpdates();

        static void AdvanceFrame();
    };
}//namespace hgl::ecs
