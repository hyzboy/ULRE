#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/ecs/support/TransformDataStorage.h>
#include<cstdint>
#include<memory>
#include <hgl/type/UnorderedMap.h>

namespace hgl::ecs
{
    class ECSContext;
    class BoundingBoxComponent;
    class TransformComponent;

    /**
     * BoundingBoxUpdateSystem
     *
     * Updates local bounding boxes from render primitives.
     * Uses version and change masks to avoid redundant work.
     */
    class BoundingBoxUpdateSystem : public System
    {
    private:

        ECSContext* world = nullptr;
        bool update_enabled = true;
        hgl::UnorderedMap<const BoundingBoxComponent*, uint64_t> last_seen_version;
        hgl::UnorderedMap<TransformDataStorage::HandleID, uint64_t> last_seen_transform_version;

    public:

        BoundingBoxUpdateSystem(const std::string& name = "BoundingBoxUpdateSystem");
        ~BoundingBoxUpdateSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetUpdateEnabled(bool enabled) { update_enabled = enabled; }
        bool IsUpdateEnabled() const { return update_enabled; }

        void Update(float deltaTime) override;

    private:

        bool ShouldProcess(const std::shared_ptr<BoundingBoxComponent>& bbox,
                           const std::shared_ptr<TransformComponent>& transform,
                           uint32_t bbox_update_mask,
                           uint32_t transform_update_mask);
        void MarkSeen(const std::shared_ptr<BoundingBoxComponent>& bbox,
                      const std::shared_ptr<TransformComponent>& transform);
    };
}//namespace hgl::ecs


