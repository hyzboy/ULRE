#pragma once

#include<hgl/ecs/core/System.h>
#include<glm/glm.hpp>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class Material;
        class MaterialInstance;
        class Pipeline;
        class RenderPass;
        class Texture2D;
        class Sampler;
    }
}

namespace hgl::ecs
{
    class QuadComponent;

    /**
     * QuadRenderSystem
     *
     * Handles quad-related rendering resource management and updates.
     *
     * Responsibilities:
     * - Create and cache shared quad geometry
     * - Load and cache textures
     * - Manage sampler pool
     * - Create material instances with proper texture bindings
     *
     * Note: This system does NOT handle rotation or transformation.
     * That is handled by other systems (e.g., FacingTransformSystem for billboards).
     */
    class QuadRenderSystem : public System
    {
    private:

        class ECSContext* world = nullptr;
        static graph::Primitive* shared_primitive;
        static graph::MaterialInstance* shared_material_instance;
        static graph::Pipeline* shared_pipeline;
        static graph::RenderPass* shared_render_pass;
        static graph::Sampler* shared_sampler;

    public:

        QuadRenderSystem(const std::string& name = "QuadRenderSystem");
        ~QuadRenderSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

        static void SetSharedPrimitive(graph::Primitive* prim) { shared_primitive = prim; }
        static graph::Primitive* GetSharedPrimitive() { return shared_primitive; }

    public:

        void Update(float deltaTime) override;
        void Shutdown() override;

    private:

        // Helper methods for quad-specific operations
        bool EnsureSharedResources();
        bool EnsureQuadMaterial(QuadComponent* quad);
        void ReleaseSharedResources();
    };
}//namespace hgl::ecs
