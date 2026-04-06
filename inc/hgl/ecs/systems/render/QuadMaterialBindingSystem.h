#pragma once

#include<hgl/ecs/core/System.h>
#include<glm/glm.hpp>

namespace hgl
{
    namespace graph
    {
        class Texture2D;
        class Sampler;
    }
}

namespace hgl::ecs
{
    class QuadComponent;

    /**
     * QuadMaterialBindingSystem
     *
     * Binds textures and creates material instances for individual quad entities.
     *
     * Responsibilities:
     * - Load textures for each QuadComponent
     * - Create per-quad material instances with texture bindings
     * - Assign shared primitive to quads that don't have one
     * - Handle texture dirty flags
     *
     * This system depends on QuadResourcePrepareSystem having already
     * created the shared resources (geometry, base material, sampler).
     */
    class QuadMaterialBindingSystem : public System
    {
    private:

        class ECSContext* world = nullptr;

    public:

        QuadMaterialBindingSystem(const std::string& name = "QuadMaterialBindingSystem");
        ~QuadMaterialBindingSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

    public:

        void Update(float deltaTime) override;

    private:

        bool EnsureQuadMaterial(QuadComponent* quad);
        bool EnsureQuadMaterialDomain(QuadComponent* quad);  ///< Domain texture-array path
    };
}//namespace hgl::ecs
