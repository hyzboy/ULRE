#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/mtl/BlendMode.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<glm/glm.hpp>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class MaterialInstance;
        class GraphicsPipeline;
        class RenderTargetFormat;
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
        static graph::GraphicsPipeline* shared_pipeline;
        static graph::RenderTargetFormat* shared_render_pass;
        static graph::Sampler* shared_sampler;

    public:

        QuadResourcePrepareSystem(const std::string& name = "QuadResourcePrepareSystem");
        ~QuadResourcePrepareSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

        // Accessors for shared resources
        static graph::Primitive* GetSharedPrimitive() { return shared_primitive; }
        static graph::MaterialInstance* GetSharedMaterialInstance() { return shared_material_instance; }
        static graph::GraphicsPipeline* GetSharedPipeline() { return shared_pipeline; }
        static graph::Sampler* GetSharedSampler() { return shared_sampler; }

        static void SetPresetForWorld(const ECSContext* world, graph::GraphicsPipelinePreset preset);
        static graph::GraphicsPipelinePreset GetPresetForWorld(const ECSContext* world);

        static void SetPipelineForWorld(const ECSContext* world, graph::GraphicsPipelinePreset pipeline);
        static graph::GraphicsPipelinePreset GetPipelineForWorld(const ECSContext* world);
        static graph::BlendMode GetBlendModeForWorld(const ECSContext* world);

        static void SetChannelHintForWorld(const ECSContext* world, graph::TextureChannelHint hint);
        static graph::TextureChannelHint GetChannelHintForWorld(const ECSContext* world);

        static void SetPreset(graph::GraphicsPipelinePreset preset);
        static graph::GraphicsPipelinePreset GetPreset();

        static void SetPipeline(graph::GraphicsPipelinePreset pipeline);
        static graph::GraphicsPipelinePreset GetPipeline();
        static graph::BlendMode GetBlendMode();
        static graph::GraphicsPipeline* CreateConfiguredPipeline(graph::RenderTargetFormat* render_pass,
                     graph::MaterialInstance* material_instance,
                     const ECSContext* world = nullptr);

    public:

        void Update(float deltaTime) override;
        void Shutdown() override;

    private:

        bool EnsureSharedResources();
        void ReleaseSharedResources();
    };
}//namespace hgl::ecs
