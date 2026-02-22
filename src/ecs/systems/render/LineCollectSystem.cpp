#include<hgl/ecs/systems/render/LineCollectSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/LinesComponent.h>
#include<hgl/ecs/components/BoundingBoxComponent.h>
#include<hgl/ecs/components/VisibilityComponent.h>
#include<hgl/ecs/support/VisibilityDataStorage.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/math/geometry/Frustum.h>
#include<glm/glm.hpp>

namespace
{
    class FrustumLineVisibilityCuller final : public hgl::ecs::ILineVisibilityCuller
    {
    private:
        hgl::math::Frustum frustum;
        bool valid = false;

    public:
        void BeginFrame(hgl::ecs::ECSContext* world) override
        {
            valid = false;
            if (!world)
                return;

            auto camera_system = world->GetSystem<hgl::ecs::CameraSystem>();
            if (!camera_system)
                return;

            const auto* camera_info = camera_system->GetCameraInfo();
            if (!camera_info)
                return;

            frustum.SetMatrix(camera_info->vp);
            valid = true;
        }

        hgl::ecs::LineCullResult Evaluate(const hgl::ecs::LinesComponent* /*lines*/, const hgl::ecs::BoundingBoxComponent* bbox) const override
        {
            if (!valid)
                return hgl::ecs::LineCullResult::Visible;

            if (!bbox || !bbox->HasWorldAABB())
                return hgl::ecs::LineCullResult::Visible;

            const auto& world_aabb = bbox->GetWorldAABB();
            const glm::vec3 center = world_aabb.GetCenter();
            const glm::vec3 extent = world_aabb.GetExtent();
            const float radius = glm::length(extent);

            if (frustum.SphereIn(center, radius) == hgl::math::Frustum::Scope::OUTSIDE)
                return hgl::ecs::LineCullResult::CulledByFrustum;

            return hgl::ecs::LineCullResult::Visible;
        }
    };

    class HZBLineVisibilityCuller final : public hgl::ecs::ILineVisibilityCuller
    {
    public:
        void BeginFrame(hgl::ecs::ECSContext* /*world*/) override {}

        hgl::ecs::LineCullResult Evaluate(const hgl::ecs::LinesComponent* /*lines*/, const hgl::ecs::BoundingBoxComponent* /*bbox*/) const override
        {
            // TODO: integrate HZB occlusion test.
            return hgl::ecs::LineCullResult::Visible;
        }
    };
}

namespace hgl::ecs
{
    static inline void HashCombineU64(uint64_t& seed, const uint64_t value)
    {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    LineCollectSystem::LineCollectSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect);
        SetRenderElementType("Line");
        visibility_culler = std::make_unique<FrustumLineVisibilityCuller>();
    }

    void LineCollectSystem::Update(float /*deltaTime*/)
    {
        visible_components.clear();
        stats = LineCollectStats{};
        visible_set_signature = 1469598103934665603ULL;
        visible_dirty_count = 0;

        if (!world)
            return;

        if (visibility_culler)
            visibility_culler->BeginFrame(world);

        std::vector<std::shared_ptr<LinesComponent>> components;
        world->GetComponents<LinesComponent>(components);

        for (const auto& comp : components)
        {
            if (!comp)
                continue;

            ++stats.total_components;

            if (!comp->visible || comp->lines.empty())
            {
                ++stats.culled_by_visibility;
                continue;
            }

            Entity* owner = comp->GetOwner();
            if (!owner)
                continue;

            if (auto visibility = owner->GetComponent<VisibilityComponent>())
            {
                if (!visibility->IsVisible())
                {
                    ++stats.culled_by_visibility;
                    continue;
                }
            }

            auto bbox = owner->GetComponent<BoundingBoxComponent>();
            if (visibility_culler)
            {
                const auto cull_result = visibility_culler->Evaluate(comp.get(), bbox.get());
                if (cull_result == LineCullResult::CulledByFrustum)
                {
                    ++stats.culled_by_frustum;
                    continue;
                }

                if (cull_result == LineCullResult::CulledByHZB)
                {
                    ++stats.culled_by_hzb;
                    continue;
                }
            }

            visible_components.push_back(comp);
            ++stats.visible_components;

            const uint64_t key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(comp.get()));
            HashCombineU64(visible_set_signature, key);

            if (comp->dirty)
                ++visible_dirty_count;
        }
    }
}
