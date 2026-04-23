#pragma once

#include<hgl/ecs/core/System.h>
#include<glm/glm.hpp>

namespace hgl::graph
{
    class Texture2D;
    class Sampler;
}

namespace hgl::ecs
{
    class Sprite2DComponent;

    /**
     * Sprite2DMaterialBindingSystem
     *
     * Binds textures and creates material instances for individual Sprite2D entities.
     *
     * Responsibilities:
     * - Assign the shared unit-square geometry (from Sprite2DResourcePrepareSystem) to each Sprite2DComponent
     * - Load textures per Sprite2DComponent
     * - Create per-sprite material instances with texture bindings
     * - Write per-MI Sprite2DTransform SSBO data (size, pivot, rotation, tint, flags)
     * - Handle texture dirty flags
     *
     * This system depends on Sprite2DResourcePrepareSystem having already
     * created the shared unit-square geometry.
     *
     * Step 3: geometry assignment + material binding.
     * Step 4 will register this system with RenderPipelineGroup.
     */
    class Sprite2DMaterialBindingSystem : public System
    {
    private:

        class ECSContext* world = nullptr;

    public:

        Sprite2DMaterialBindingSystem(const std::string& name = "Sprite2DMaterialBindingSystem");
        ~Sprite2DMaterialBindingSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

    public:

        void Update(float deltaTime) override;

    private:

        bool EnsureSpriteMaterial(Sprite2DComponent* sprite);
    };
}//namespace hgl::ecs
