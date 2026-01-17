/**
 * ECSTransformAssignmentBuffer.h - ECS渲染项Transform数据管理
 * 
 * 针对 ECS 架构的 RenderItem 和 MaterialBatch 设计
 * 与 SceneGraph 的 TransformAssignmentBuffer 功能相同，但适配 ECS 数据结构
 */

#pragma once
#include<hgl/graph/VK.h>
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
    private:
        uint32_t MaxTransformCount;             ///<单个UBO最大支持的变换数量
        graph::VulkanDevice* device;            ///<Vulkan设备

    private:    // LocalToWorld矩阵数据
        uint32_t transform_buffer_max_count;    ///<LocalToWorld矩阵最大数量
        graph::DeviceBuffer* transform_buffer;  ///<LocalToWorld矩阵数据(UBO/SSBO)

        void StatTransform(const std::vector<RenderItem*>& items);

    private:    // 分发数据
        uint32_t node_count;                    ///<节点数量
        graph::VAB* transform_vab;              ///<LocalToWorld矩阵ID分发数据VAB(R16UI格式)
        VkBuffer transform_vab_buffer;          ///<LocalToWorld矩阵ID分发数据Buffer

    private:
        void Clear();

    public:
        ECSTransformAssignmentBuffer(graph::VulkanDevice* dev);
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
    };
}//namespace hgl::ecs
