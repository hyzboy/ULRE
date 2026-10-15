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
    /**
     * **L2W 行号约定**（唯一具名权威；2026-10-14 由 `TransformAssignmentBuffer.cpp` 的文件私有常量上提
     * —— 原先跨文件的写者各自用裸 `slot + 1` 复述本约定，审计 §4.2「硬编码 0/1 号约定无单一权威」）。
     *
     *   · 行 `kIdentityL2WSlot`（= 0）恒为**单位矩阵**；
     *   · **静态**第 `slot` 个对象在行 `kFirstObjectL2WSlot + slot`；
     *   · **动态**第 `slot` 个对象在行 `GetDynamicBaseIndex() + slot`（预算基址，随帧槽轮转）。
     *
     * 读侧：`TransformSystem`（写者）、线路径（`LineRenderPipeline`，自己的同一算式）与 GPU 剔除写出的
     * per-batch `l2w_index` 表（`ShaderLibrary/ecs/entity_gpu_cull.glsl`）。
     */
    constexpr uint32_t kIdentityL2WSlot    = 0;   ///< 行 0 = 单位矩阵
    constexpr uint32_t kFirstObjectL2WSlot = 1;   ///< 静态首对象 = 行 1

    static_assert(kIdentityL2WSlot == 0u, "行 0 恒为单位矩阵");
    static_assert(kFirstObjectL2WSlot == kIdentityL2WSlot + 1u,
                  "静态首对象必须紧接单位矩阵行 —— 改这里等于改全仓 L2W 行号约定");

    class TransformAssignmentBuffer
    {
    public:

    private:
        uint32_t MaxTransformCount;             ///<单个SSBO最大支持的变换数量
        graph::BufferManager* buffer_manager;   ///<BufferManager用于创建缓冲区

        // ── 声明式行预算（T10/B1a）──
        // 容量在**启动期一次性**按预算建好，此后**不再重建**（行号/基址因此固定）；
        // 超预算只做**一次性明确告警**，不扩容 —— 与材质行 arena「上限不扩容」同口径。
        uint32_t max_static_rows;               ///<静态段行预算（P-2 拍板：4096）
        uint32_t max_dynamic_rows;              ///<动态段行预算（P-2 拍板：4096）
        bool     budget_warned = false;         ///<超预算一次性告警闩

    private:    // LocalToWorld矩阵数据
        uint32_t transform_buffer_max_count;    ///<LocalToWorld矩阵行数（由预算算出的固定值）
        graph::DeviceBuffer* transform_buffer;  ///<LocalToWorld矩阵数据(SSBO)

        graph::RingLayout ring_layout;   ///<静态段 + 动态段×帧数 的环形行布局算术（不持有 buffer）

        /// 按预算**一次性**建立 L2W SSBO（只在首次调用时创建，永不重建）
        bool AllocateBudgetBuffers();

    private:
        void Clear();

        void WriteDirtyIndicesInternal(const char* label,
                                       uint32_t base_index,
                                       const TransformDataStorage& storage,
                                       const std::vector<TransformDataStorage::HandleID>& handles,
                                       const std::vector<uint32_t>& dirty_indices);

    public:
        TransformAssignmentBuffer(graph::BufferManager* bm,
                                  uint32_t ring_frames,
                                  uint32_t max_static_rows,
                                  uint32_t max_dynamic_rows);
        ~TransformAssignmentBuffer() { Clear(); }

        graph::DeviceBuffer* GetTransformDataBuffer() const { return transform_buffer; }

        /// 每帧入口：只做「预算内校验」，**不扩容、不重建**（容量在首次调用时按预算一次性建好）。
        /// 注：旧全局 index 行表已删（B1b-2）——`addr_l2w_index` 由 per-batch 行表供给。
        void EnsureBudget(const uint32_t static_count,const uint32_t dynamic_count);
        /// 动态段基址（B1b：由**预算**决定，不再随"当前名单长度"漂移；与 buffer 固定容量同源）
        uint32_t GetDynamicBaseIndex() const;

        void WriteStaticDirtyIndices(const TransformDataStorage& storage,
                         const std::vector<TransformDataStorage::HandleID>& handles,
                         const std::vector<uint32_t>& dirty_indices);
        /// `handles` = 该段的**槽位表**（下标即槽位，含空洞）⇒ `dirty_indices` 里是槽位
        void WriteDynamicDirtyIndices(const TransformDataStorage& storage,
                          const std::vector<TransformDataStorage::HandleID>& handles,
                          const std::vector<uint32_t>& dirty_indices);

        /// 推进 ring 帧索引。世界私有：由本世界的 ECSContext::SetFrameIndex
        /// 经 TransformSystem::GetTransformBuffer() 直推（不再全局广播——
        /// 那会让一个世界推进触达所有世界，且静态实例表析构不摘除留悬空）
        void SetFrameIndex(const uint32_t index);
    };
}//namespace hgl::ecs
