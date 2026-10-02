#pragma once

#include<hgl/vk/VKDevice.h>
#include<hgl/ecs/core/System.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/TransformID.h>
#include<vector>
#include<memory>
#include <hgl/type/UnorderedMap.h>
#include<cstdint>

namespace hgl::ecs
{
    class TransformAssignmentBuffer;
    class RenderPrimitiveSystem;
    /**
     * TransformSystem
     *
     * Centralized update for the world's transform rows.
        * - Updates dirty movable transforms per tick
        * - Static transforms are updated only on explicit call
     */
    class TransformSystem : public System
    {
    private:

        ECSContext* world = nullptr;
        bool updateMovable = true;
        TransformAssignmentBuffer* transform_buffer = nullptr;
        uint32_t last_static_count = 0;
        uint32_t last_dynamic_count = 0;
        bool static_dirty = true;
        std::vector<TransformDataStorage::HandleID> static_handles;
        std::vector<TransformDataStorage::HandleID> dynamic_handles;
        // handle → **槽位**（B1b：不是名单排名；每帧由世界的槽位表派生，值本身稳定）
        hgl::UnorderedMap<TransformDataStorage::HandleID, uint32_t> static_slot_map;
        hgl::UnorderedMap<TransformDataStorage::HandleID, uint32_t> dynamic_slot_map;
        hgl::UnorderedMap<TransformDataStorage::HandleID, uint64_t> last_seen_version;
        hgl::UnorderedMap<TransformDataStorage::HandleID, uint64_t> last_uploaded_version;
        // W5 帧级 once：同帧多次调用（RenderGraph pass 循环 + Line 管线）
        // 只执行一次全量提交——ring 段每帧填一次，所有 pass 读同一帧段


    public:

        TransformSystem(const std::string& name = "TransformSystem");
        ~TransformSystem() override;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetDevice(graph::VulkanDevice* dev) {}  ///<Deprecated: BufferManager is now obtained from ECSContext's RenderContext
        TransformAssignmentBuffer* GetTransformBuffer() const { return transform_buffer; }
        void SetUpdateMovable(bool enabled) { updateMovable = enabled; }
        bool IsUpdateMovableEnabled() const { return updateMovable; }

        void Update(float deltaTime) override;
        void UpdateStaticDirty();
        void SubmitTransformUpdates();

        void EnsureTransformBuffer();
        /// 动态段基址（B1b：预算决定，与槽位表无关）
        uint32_t GetDynamicBaseIndex() const;
        uint32_t GetStaticCount() const { return static_cast<uint32_t>(static_handles.size()); }
        uint32_t GetDynamicCount() const { return static_cast<uint32_t>(dynamic_handles.size()); }
        /// 变换的**稳定槽位**（B1b）：`out_slot` = 段内槽位
        /// L2W 行号 = 静态 `1 + slot` / 动态 `GetDynamicBaseIndex() + slot`
        bool TryGetTransformSlot(TransformDataStorage::HandleID handle, bool movable, uint32_t& out_slot) const;
        void RefreshHandleOrder();

    private:

        bool ShouldUpdateTransform(const TransformID id, uint32_t update_mask);
        void MarkTransformSeen(const TransformID id);
        void UpdateTransformRow(const TransformID id);
    };
}//namespace hgl::ecs


