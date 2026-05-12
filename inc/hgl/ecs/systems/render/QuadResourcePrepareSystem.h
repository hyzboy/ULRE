#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/mtl/RenderAlphaMode.h>
#include<hgl/mtl/MaterialPreset.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<hgl/graph/module/TextureDomainRegistry.h>
#include<hgl/type/String.h>
#include<glm/glm.hpp>
#include<string>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class MaterialBindingInstance;
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
     * - Delegate per-domain Texture2DArray management to TextureDomainRegistry.
     *
     * This system runs early in the render phase to ensure resources
     * are ready before QuadMaterialBindingSystem needs them.
     */
    class QuadResourcePrepareSystem : public System
    {
    public:

        /// Alias to the registry entry — keeps old callers working without copying data.
        using DomainResources = graph::TextureDomainRegistry::DomainEntry;

    private:

        class ECSContext* world = nullptr;

        // Shared resources used by all quads (legacy single-texture path)
        static graph::Primitive* shared_primitive;
        static graph::MaterialBindingInstance* shared_material_instance;
        static graph::RenderTargetFormat* shared_render_pass;
        static graph::Sampler* shared_sampler;

    public:

        QuadResourcePrepareSystem(const std::string& name = "QuadResourcePrepareSystem");
        ~QuadResourcePrepareSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

        // Accessors for shared resources (legacy path)
        static graph::Primitive* GetSharedPrimitive() { return shared_primitive; }
        static graph::MaterialBindingInstance* GetSharedMaterialInstance() { return shared_material_instance; }
        static graph::Sampler* GetSharedSampler() { return shared_sampler; }

        // Domain texture array management
        /// Register a texture path in a domain, returns the layer index.
        /// If the texture was already registered, returns existing layer.
        /// If the domain doesn't exist yet, creates it.
        static int RegisterDomainTexture(const std::string& domain_tag, const hgl::OSString& texture_path);

        /// Get domain resources, or nullptr if domain doesn't exist.
        static DomainResources* GetDomainResources(const std::string& domain_tag);

        static void SetPresetForWorld(const ECSContext* world, graph::GraphicsPipelinePreset preset);
        static graph::GraphicsPipelinePreset GetPresetForWorld(const ECSContext* world);

        static void SetPipelineForWorld(const ECSContext* world, graph::GraphicsPipelinePreset pipeline);
        static graph::GraphicsPipelinePreset GetPipelineForWorld(const ECSContext* world);
        static graph::RenderAlphaMode GetBlendModeForWorld(const ECSContext* world);

        static void SetChannelHintForWorld(const ECSContext* world, graph::TextureChannelHint hint);
        static graph::TextureChannelHint GetChannelHintForWorld(const ECSContext* world);

        static void SetFixedSizeForWorld(const ECSContext* world, bool fixed);
        static bool IsFixedSizeForWorld(const ECSContext* world);
        static graph::mtl::MaterialPreset GetBillboardPresetForWorld(const ECSContext* world);

        static void SetPreset(graph::GraphicsPipelinePreset preset);
        static graph::GraphicsPipelinePreset GetPreset();

        static void SetPipeline(graph::GraphicsPipelinePreset pipeline);
        static graph::GraphicsPipelinePreset GetPipeline();
        static graph::RenderAlphaMode GetBlendMode();

    public:

        void Update(float deltaTime) override;
        void Shutdown() override;

        /// Build (or rebuild) all domain texture arrays and ShaderMaterialPrograms.
        /// Called automatically from Update(), but may also be called explicitly by
        /// QuadMaterialBindingSystem after registering domain textures so that
        /// domain materials are ready on the very first frame.
        bool EnsureDomainResources();

    private:

        bool EnsureSharedResources();
        void ReleaseSharedResources();
        void ReleaseDomainResources();
    };
}//namespace hgl::ecs
