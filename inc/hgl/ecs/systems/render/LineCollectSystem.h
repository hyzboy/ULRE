#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class ECSContext;
    class LinesComponent;
    class BoundingBoxComponent;

    enum class LineCullResult : uint8_t
    {
        Visible = 0,
        CulledByFrustum,
        CulledByHZB,
    };

    struct LineCollectStats
    {
        uint32_t total_components = 0;
        uint32_t visible_components = 0;
        uint32_t culled_by_visibility = 0;
        uint32_t culled_by_frustum = 0;
        uint32_t culled_by_hzb = 0;

        float GetCullRatio() const
        {
            if (total_components == 0)
                return 0.0f;

            const uint32_t culled = culled_by_visibility + culled_by_frustum + culled_by_hzb;
            return static_cast<float>(culled) / static_cast<float>(total_components);
        }
    };

    class ILineVisibilityCuller
    {
    public:
        virtual ~ILineVisibilityCuller() = default;
        virtual void BeginFrame(ECSContext* world) = 0;
        virtual LineCullResult Evaluate(const LinesComponent* lines, const BoundingBoxComponent* bbox) const = 0;
    };

    class LineCollectSystem : public System
    {
    private:
        ECSContext* world = nullptr;
        std::vector<std::shared_ptr<LinesComponent>> visible_components;
        std::unique_ptr<ILineVisibilityCuller> visibility_culler;
        LineCollectStats stats;
        uint64_t visible_set_signature = 0;
        uint32_t visible_dirty_count = 0;

    public:
        explicit LineCollectSystem(const std::string& name = "LineCollectSystem");
        ~LineCollectSystem() override = default;

        void SetWorld(ECSContext* w) { world = w; }
        void SetVisibilityCuller(std::unique_ptr<ILineVisibilityCuller> culler)
        {
            visibility_culler = std::move(culler);
        }
        const std::vector<std::shared_ptr<LinesComponent>>& GetVisibleComponents() const { return visible_components; }
        const LineCollectStats& GetStats() const { return stats; }
        uint64_t GetVisibleSetSignature() const { return visible_set_signature; }
        uint32_t GetVisibleDirtyCount() const { return visible_dirty_count; }

        void Update(float deltaTime) override;
    };
}
