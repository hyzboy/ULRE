#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::graph
{
    class Geometry;
    class Sampler;
}

namespace hgl::ecs
{
    class ECSContext;

    /**
     * Sprite2DResourcePrepareSystem
     *
     * Creates and caches the shared unit-square geometry (1×1, centered at origin,
     * vec2 Position + vec2 TexCoord) used by all Sprite2D entities.
     *
     * Step 2: geometry only — no material, no pipeline, no render group.
     * Step 4 will bind this geometry to a material and pipeline.
     */
    class Sprite2DResourcePrepareSystem : public System
    {
    private:
        class ECSContext* world = nullptr;
        graph::Geometry*  shared_unit_square_geometry = nullptr;
        graph::Sampler*   shared_sampler = nullptr;

    public:
        explicit Sprite2DResourcePrepareSystem(const std::string& name = "Sprite2DResourcePrepareSystem");
        ~Sprite2DResourcePrepareSystem() override = default;

        void SetWorld(ECSContext* w) { world = w; }

        /// Returns the shared unit-square geometry, or nullptr if not yet created.
        graph::Geometry* GetSharedGeometry() const { return shared_unit_square_geometry; }

        /// Returns the shared sampler, or nullptr if not yet created.
        graph::Sampler* GetSharedSampler() const { return shared_sampler; }

        void Update(float deltaTime) override;
        void Shutdown() override;

    private:
        bool EnsureSharedResources();
    };
} // namespace hgl::ecs
