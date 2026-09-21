/**
 * TransformAssignmentBuffer.h - ECS渲染项Transform数据管理
 *
 * 针对 ECS 架构的 RenderItem 和 MaterialBatch 设计
 * 与 SceneGraph 的 TransformAssignmentBuffer 功能相同，但适配 ECS 数据结构
 */

#pragma once
#include<hgl/vk/VK.h>
#include<hgl/vk/buffer/BufferMemory.h>
#include<hgl/vk/buffer/RingLayout.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/ecs/support/TransformDataStorage.h>
#include<hgl/math/Vector.h>
#include<vector>

namespace hgl::graph
{
    class BufferManager;
    class DeviceBuffer;
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
     *
     * 所有权：本类是世界（ECSContext）私有设施，L2W/L2WIndex 缓冲由
     * BufferManager 创建、由本类独占——不注册进 SSBOBufferRegistry。
     * shader 侧地址经 RootAddresses push constants（每 MaterialBatch 一次，
     * PipelineMaterialRenderer 从 GetTransformDataBuffer() 取）下发，
     * 全局域注册自 BDA 化后已无消费者（曾因同域互踩导致多世界悬空）。
     */
    class TransformAssignmentBuffer
    {
    public:

    private:
        uint32_t MaxTransformCount;             ///<单个SSBO最大支持的变换数量
        graph::BufferManager* buffer_manager;   ///<BufferManager用于创建缓冲区

    private:    // LocalToWorld矩阵数据
        uint32_t transform_buffer_max_count;    ///<LocalToWorld矩阵最大数量
        graph::DeviceBuffer* transform_buffer;  ///<LocalToWorld矩阵数据(SSBO)
        graph::BufferAllocPolicy transform_policy;     ///<Transform buffer allocation policy

        graph::RingLayout ring_layout;   ///<静态段 + 动态段×帧数 的环形行布局算术（不持有 buffer）

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
                                  uint32_t ring_frames = HGL_L2W_RING_FRAMES);
        ~TransformAssignmentBuffer() { Clear(); }

        graph::DeviceBuffer* GetTransformDataBuffer() const { return transform_buffer; }
        graph::DeviceBuffer* GetTransformIndexRowsBuffer() const { return transform_index_rows_buffer; }

        void EnsureCapacity(const uint32_t static_count,const uint32_t dynamic_count,graph::BufferAllocPolicy policy);
        uint32_t GetDynamicBaseIndex(const uint32_t static_count,const uint32_t dynamic_count) const;

        void WriteStaticDirtyIndices(const TransformDataStorage& storage,
                         const std::vector<TransformDataStorage::HandleID>& handles,
                         const std::vector<uint32_t>& dirty_indices);
        void WriteDynamicDirtyIndices(const TransformDataStorage& storage,
                          const uint32_t static_count,
                          const std::vector<TransformDataStorage::HandleID>& handles,
                          const std::vector<uint32_t>& dirty_indices);

        /// 推进 ring 帧索引。世界私有：由本世界的 ECSContext::SetFrameIndex
        /// 经 TransformSystem::GetTransformBuffer() 直推（不再全局广播——
        /// 那会让一个世界推进触达所有世界，且静态实例表析构不摘除留悬空）
        void SetFrameIndex(const uint32_t index);
    };
}//namespace hgl::ecs
