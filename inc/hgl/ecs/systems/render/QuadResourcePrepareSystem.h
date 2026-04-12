#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/mtl/RenderAlphaMode.h>
#include<hgl/mtl/MaterialPreset.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<hgl/type/String.h>
#include<glm/glm.hpp>
#include<memory>
#include<string>
#include<unordered_map>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class MaterialTemplate;
        class RenderTargetFormat;
        class Sampler;
        class Texture2DArray;
        class DomainMaterialBinding;
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
     * - Manage per-domain Texture2DArray + DomainMaterialBinding for batched rendering
     *
     * This system runs early in the render phase to ensure resources
     * are ready before QuadMaterialBindingSystem needs them.
     */
    class QuadResourcePrepareSystem : public System
    {
    public:

        /// Per-domain resources for texture array batching.
        /// All billboards sharing the same domain_tag use a single Texture2DArray
        /// with per-MI layer selection, enabling single draw-call per domain.
        struct DomainResources
        {
            std::string                             domain_tag;
            graph::Texture2DArray*                  texture_array   = nullptr;
            graph::MaterialTemplate*                        material        = nullptr;
            graph::DomainMaterialBinding*           dmb             = nullptr;
            std::weak_ptr<graph::Sampler>           sampler;
            graph::Primitive*                       primitive       = nullptr;
            uint32_t                                max_layers      = 0;        ///< allocated capacity
            uint32_t                                used_layers     = 0;        ///< next free layer index
            std::unordered_map<hgl::OSString, uint32_t> path_to_layer;          ///< texture path → layer index
            bool                                    dirty           = false;    ///< new textures added since last build
        };

    private:

        class ECSContext* world = nullptr;

        // Shared resources used by all quads (legacy single-texture path)
        static graph::Primitive* shared_primitive;
        static graph::RenderTargetFormat* shared_render_pass;
        static std::weak_ptr<graph::Sampler> shared_sampler;

        // Per-domain texture array resources
        static std::unordered_map<std::string, DomainResources> s_domain_resources;

    public:

        QuadResourcePrepareSystem(const std::string& name = "QuadResourcePrepareSystem");
        ~QuadResourcePrepareSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

        // Accessors for shared resources (legacy path)
        static graph::Primitive* GetSharedPrimitive() { return shared_primitive; }
        static graph::Sampler*   GetSharedSampler()   { return shared_sampler.lock().get(); }

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

    private:

        bool EnsureSharedResources();
        bool EnsureDomainResources();
        void ReleaseSharedResources();
        void ReleaseDomainResources();
    };
}//namespace hgl::ecs
