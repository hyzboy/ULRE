#pragma once

#include<hgl/vk/VKDevice.h>
#include<hgl/ecs/core/System.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TransformComponent.h>
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
     * Centralized update for TransformComponent.
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
        hgl::UnorderedMap<TransformDataStorage::HandleID, uint32_t> static_index_map;
        hgl::UnorderedMap<TransformDataStorage::HandleID, uint32_t> dynamic_index_map;
        hgl::UnorderedMap<TransformDataStorage::HandleID, uint64_t> last_seen_version;
        hgl::UnorderedMap<TransformDataStorage::HandleID, uint64_t> last_uploaded_version;

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
        uint32_t GetDynamicBaseIndex(const uint32_t static_count,const uint32_t dynamic_count) const;
        uint32_t GetStaticCount() const { return static_cast<uint32_t>(static_handles.size()); }
        uint32_t GetDynamicCount() const { return static_cast<uint32_t>(dynamic_handles.size()); }
        bool TryGetTransformGroupIndex(TransformDataStorage::HandleID handle, bool movable, uint32_t& out_index) const;
        void RefreshHandleOrder();

    private:

        void UpdateStaticTransformRecursive(const std::shared_ptr<TransformComponent>& comp);
        bool ShouldUpdateTransform(const std::shared_ptr<TransformComponent>& comp, uint32_t update_mask);
        void MarkTransformSeen(const std::shared_ptr<TransformComponent>& comp);
    };
}//namespace hgl::ecs


