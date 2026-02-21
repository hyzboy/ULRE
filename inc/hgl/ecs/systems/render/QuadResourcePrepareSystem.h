#pragma once

#include<hgl/ecs/core/System.h>
#include<glm/glm.hpp>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class MaterialInstance;
        class Pipeline;
        class RenderPass;
        class Sampler;
    }
}

namespace hgl::ecs
{
    /**
     * QuadResourcePrepareSystem
     *
     * Prepares shared rendering resources for all quads.
     *
     * Responsibilities:
     * - Create and cache shared quad geometry (single point billboard)
     * - Create shared material instance and pipeline
     * - Create shared sampler for texture sampling
     *
     * This system runs early in the render phase to ensure resources
     * are ready before QuadMaterialBindingSystem needs them.
     */
    class QuadResourcePrepareSystem : public System
    {
    private:

        class ECSContext* world = nullptr;

        // Shared resources used by all quads
        static graph::Primitive* shared_primitive;
        static graph::MaterialInstance* shared_material_instance;
        static graph::Pipeline* shared_pipeline;
        static graph::RenderPass* shared_render_pass;
        static graph::Sampler* shared_sampler;

    public:

        QuadResourcePrepareSystem(const std::string& name = "QuadResourcePrepareSystem");
        ~QuadResourcePrepareSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

        // Accessors for shared resources
        static graph::Primitive* GetSharedPrimitive() { return shared_primitive; }
        static graph::MaterialInstance* GetSharedMaterialInstance() { return shared_material_instance; }
        static graph::Pipeline* GetSharedPipeline() { return shared_pipeline; }
        static graph::Sampler* GetSharedSampler() { return shared_sampler; }

    public:

        void Update(float deltaTime) override;
        void Shutdown() override;

    private:

        bool EnsureSharedResources();
        void ReleaseSharedResources();
    };
}//namespace hgl::ecs
