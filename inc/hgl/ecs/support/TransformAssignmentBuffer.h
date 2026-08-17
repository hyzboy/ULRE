/**
 * TransformAssignmentBuffer.h - ECS渲染项Transform数据管理
 *
 * 针对 ECS 架构的 RenderItem 和 MaterialBatch 设计
 * 与 SceneGraph 的 TransformAssignmentBuffer 功能相同，但适配 ECS 数据结构
 */

#pragma once
#include<hgl/vk/VK.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/vk/VKRingBufferWrapper.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/ecs/support/TransformDataStorage.h>
#include<hgl/math/Vector.h>
#include<vector>

namespace hgl::graph
{
    class BufferManager;
    class DeviceBuffer;
    class ResourceDomainManager;
}

namespace hgl::ecs
{
    /**
     * ECS Transform 分配缓冲
     *
     * 职责：
     * - 管理所有 RenderItem 的 LocalToWorld 矩阵数据（SSBO）
     * - 生成 TransformID 行表（SSBO）
     * - 支持动态更新变换矩阵
     * 注意：类名中的 "Assignment" 为历史名（曾做变换分配）——现职责为
     * L2W 域 SSBO 写者（static/dynamic 段 + ring + 行表），改名评估过
     * 因引用面大而保留（W7 记录）
     */
    class TransformAssignmentBuffer
    {
    public:

    private:
        uint32_t MaxTransformCount;             ///<单个SSBO最大支持的变换数量
        graph::BufferManager* buffer_manager;   ///<BufferManager用于创建缓冲区
        graph::ResourceDomainManager* resource_domain_manager; ///<全局SSBO域管理器（可空）

    private:    // LocalToWorld矩阵数据
        uint32_t transform_buffer_max_count;    ///<LocalToWorld矩阵最大数量
        graph::DeviceBuffer* transform_buffer;  ///<LocalToWorld矩阵数据(SSBO)
        graph::BufferAllocPolicy transform_policy;     ///<Transform buffer allocation policy

        static std::vector<TransformAssignmentBuffer*> all_instances;
        graph::RingBufferWrapper ring_writer;

        void StatTransform(const size_t required_count,graph::BufferAllocPolicy policy);
        bool EnsureTransformIndexRowsCapacity(const uint32_t required_count);
        bool WriteTransformIndexRows(const uint32_t static_count, const uint32_t dynamic_count);

    private:    // 分发数据
        uint32_t transform_index_rows_max_count; ///<TransformID行表容量（uint32 行）
        graph::DeviceBuffer* transform_index_rows_buffer; ///<TransformID行表SSBO

    private:
        void Clear();

    public:
        TransformAssignmentBuffer(graph::BufferManager* bm,
                                  graph::ResourceDomainManager* rdm = nullptr,
                                  uint32_t ring_frames = HGL_L2W_RING_FRAMES);
        ~TransformAssignmentBuffer() { Clear(); }

        graph::DeviceBuffer* GetTransformDataBuffer() const { return transform_buffer; }
        graph::DeviceBuffer* GetTransformIndexRowsBuffer() const { return transform_index_rows_buffer; }

        /**
         * 绑定Transform数据到材质
         */
        void BindTransform(graph::ShaderProgram* mtl) const;

        void EnsureCapacity(const uint32_t static_count,const uint32_t dynamic_count,graph::BufferAllocPolicy policy);
        uint32_t GetDynamicBaseIndex(const uint32_t static_count,const uint32_t dynamic_count) const;

        void WriteStaticDirtyIndices(const TransformDataStorage& storage,
                         const std::vector<TransformDataStorage::HandleID>& handles,
                         const std::vector<uint32_t>& dirty_indices);
        void WriteDynamicDirtyIndices(const TransformDataStorage& storage,
                          const uint32_t static_count,
                          const std::vector<TransformDataStorage::HandleID>& handles,
                          const std::vector<uint32_t>& dirty_indices);

        static void SetFrameIndex(const uint32_t index);   ///<推进 ring 帧索引（Context::SetFrameIndex 转发）
    };
}//namespace hgl::ecs
