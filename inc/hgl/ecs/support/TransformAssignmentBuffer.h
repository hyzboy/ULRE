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
}

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
    class TransformAssignmentBuffer
    {
    public:

        enum class Mode
        {
            StaticOnly,
            MovableOnly
        };

    private:
        uint32_t MaxTransformCount;             ///<单个SSBO最大支持的变换数量
        graph::BufferManager* buffer_manager;   ///<BufferManager用于创建缓冲区

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

        static std::vector<TransformAssignmentBuffer*> all_instances;
        graph::RingBufferWrapper ring_writer;

        math::Vector3d camera_offset_{0.0, 0.0, 0.0};  ///<Camera-Relative Rendering 用相机世界坐标偏移

        void ApplyCameraRelativeOffset(math::Matrix4f& m) const
        {
            m[3][0] -= static_cast<float>(camera_offset_.x);
            m[3][1] -= static_cast<float>(camera_offset_.y);
            m[3][2] -= static_cast<float>(camera_offset_.z);
        }

        void StatTransform(const size_t required_count,graph::BufferAllocPolicy policy);
        void QueueUpdateRange(const int first,const int last);
        void WriteRange(const std::vector<RenderItem*>& items,const int first,const int last);
        void SplitStaticAndMovableItems(const std::vector<RenderItem*>& items,
                        std::vector<RenderItem*>& static_items,
                        std::vector<RenderItem*>& movable_items) const;
        void SortStaticItemsByHandle(std::vector<RenderItem*>& static_items) const;
        void AssignTransformIndices(std::vector<RenderItem*>& static_items,
                        std::vector<RenderItem*>& movable_items,
                        const uint32_t ring_base) const;
        bool WriteAllLocalToWorld(const std::vector<RenderItem*>& static_items,
                      const std::vector<RenderItem*>& movable_items,
                      const uint32_t static_count,
                      const uint32_t dynamic_count,
                      const uint32_t total_count);
        bool EnsureTransformIDBufferCapacity(const size_t item_count,graph::BufferAllocPolicy policy);
        bool WriteTransformIDBuffer(const std::vector<RenderItem*>& items,
                        const size_t item_count,
                        const uint32_t max_transform_id);
        bool EnsureTransformVABCapacity(const size_t item_count);
        bool WriteTransformIDVAB(const std::vector<RenderItem*>& items,
                     const size_t item_count,
                     const uint32_t max_transform_id);

    private:    // 分发数据（TransformID）
        uint32_t transform_id_buffer_max_count; ///<TransformID descriptor buffer capacity
        graph::DeviceBuffer* transform_id_buffer; ///<TransformID data(UBO/SSBO)
        VkBuffer transform_id_vk_buffer;         ///<TransformID descriptor-backed VkBuffer

    private:    // 分发数据（VAB legacy path）
        uint32_t node_count;                    ///<节点数量
        graph::VAB* transform_vab;              ///<LocalToWorld矩阵ID分发数据VAB(R16UI格式)
        VkBuffer transform_vab_buffer;          ///<LocalToWorld矩阵ID分发数据Buffer

    private:
        void Clear();

    public:
        TransformAssignmentBuffer(graph::BufferManager* bm, const Mode m = Mode::MovableOnly, uint32_t ring_frames = HGL_L2W_RING_FRAMES);
        ~TransformAssignmentBuffer() { Clear(); }

        /**
         * 设置 Camera-Relative Rendering 偏移（每帧更新）
         */
        void SetCameraOffset(const math::Vector3d& offset) { camera_offset_ = offset; }

        /**
         * 获取Transform VAB缓冲（用于绑定到管线）
         */
        const VkBuffer GetTransformVAB() const { return transform_vab_buffer; }

        graph::DeviceBuffer* GetTransformDataBuffer() const { return transform_buffer; }
        graph::DeviceBuffer* GetTransformIDDataBuffer() const { return transform_id_buffer; }
        const VkBuffer GetTransformIDVkBuffer() const { return transform_id_vk_buffer; }

        /**
         * 绑定Transform数据到材质
         */
        void BindTransform(graph::Material* mtl) const;
        void BindTransformID(graph::Material* mtl) const;

        void EnsureCapacity(const uint32_t static_count,const uint32_t dynamic_count,graph::BufferAllocPolicy policy);
        uint32_t GetDynamicBaseIndex(const uint32_t static_count,const uint32_t dynamic_count) const;
        uint32_t GetTotalCount(const uint32_t static_count,const uint32_t dynamic_count) const;

        void WriteStaticFromStorage(const TransformDataStorage& storage,const uint32_t static_count);
        void WriteDynamicFromStorage(const TransformDataStorage& storage,const uint32_t static_count,const uint32_t dynamic_count);
        void WriteStaticFromHandles(const TransformDataStorage& storage,
                        const std::vector<TransformDataStorage::HandleID>& handles);
        void WriteDynamicFromHandles(const TransformDataStorage& storage,
                         const uint32_t static_count,
                         const std::vector<TransformDataStorage::HandleID>& handles);
        void WriteStaticDirtyIndices(const TransformDataStorage& storage,
                         const std::vector<TransformDataStorage::HandleID>& handles,
                         const std::vector<uint32_t>& dirty_indices);
        void WriteDynamicDirtyIndices(const TransformDataStorage& storage,
                          const uint32_t static_count,
                          const std::vector<TransformDataStorage::HandleID>& handles,
                          const std::vector<uint32_t>& dirty_indices);

        /**
         * 写入所有RenderItem的变换数据
         * @param items RenderItem列表
         */
        void WriteItems(const std::vector<RenderItem*>& items);
        void WriteTransformIDs(const std::vector<RenderItem*>& items);

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
        static void SetFrameIndex(const uint32_t index);
    };
}//namespace hgl::ecs

