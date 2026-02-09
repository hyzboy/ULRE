#pragma once

#include<hgl/ecs/System.h>
#include<cstdint>
#include<memory>
#include<unordered_map>

namespace hgl::ecs
{
    class ECSContext;
    class BoundingBoxComponent;

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
        std::unordered_map<const BoundingBoxComponent*, uint64_t> last_seen_version;

    public:

        BoundingBoxUpdateSystem(const std::string& name = "BoundingBoxUpdateSystem");
        ~BoundingBoxUpdateSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetUpdateEnabled(bool enabled) { update_enabled = enabled; }
        bool IsUpdateEnabled() const { return update_enabled; }

        void Update(float deltaTime) override;

    private:

        bool ShouldProcess(const std::shared_ptr<BoundingBoxComponent>& bbox, uint32_t update_mask);
        void MarkSeen(const std::shared_ptr<BoundingBoxComponent>& bbox);
    };
}//namespace hgl::ecs
