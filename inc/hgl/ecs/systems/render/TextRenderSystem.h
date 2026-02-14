#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/graph/font/TextLayout.h>
#include<cstdint>
#include<vector>

namespace hgl
{
    namespace graph
    {
        class RenderFramework;
        class FontSource;
        class TileFont;
        class Material;
        class Primitive;
        class TextGeometry;
        class MaterialInstance;
        class Pipeline;
        class Sampler;
    }
}

namespace hgl::ecs
{
    class ECSContext;

    /**
     * TextRenderSystem
     *
     * Manages text rendering resources internally. Batches text by FontSource,
     * builds TextGeometry, and creates Primitive objects. Does not use ECS components
     * for runtime resources — all resources are managed internally by this system.
     */
    class TextRenderSystem : public System
    {
    private:

        struct RenderResources
        {
            // Per-font resources
            graph::TileFont* tile_font = nullptr;              // Owned by system
            graph::Material* material = nullptr;               // Owned by MaterialManager
            graph::Pipeline* pipeline = nullptr;               // Owned by RenderPass
            graph::Sampler* sampler = nullptr;                 // Owned by SamplerManager

            // Per-batch resources (same font)
            graph::layout::CharStyle char_style{};
            graph::TextGeometry* geometry = nullptr;           // Owned by system
            graph::Primitive* primitive = nullptr;             // Owned by PrimitiveManager
            graph::MaterialInstance* material_instance = nullptr; // Owned by MaterialManager
            
            uint32_t last_draw_char_count = 0;
            uint32_t last_string_count = 0;
        };

        ECSContext* world = nullptr;
        graph::RenderFramework* framework = nullptr;

        hgl::UnorderedMap<graph::FontSource*, RenderResources> resources_by_font;

    public:

        TextRenderSystem(const std::string& name = "TextRenderSystem");
        ~TextRenderSystem() override;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetRenderFramework(graph::RenderFramework* rf) { framework = rf; }

        void Update(float deltaTime) override;

        /**
         * Get all primitives that should be rendered.
         * Called by TextRenderSubmitSystem during RenderPostProcess phase.
         */
        void GetRenderPrimitives(std::vector<graph::Primitive*>& out_primitives) const;

    private:

        RenderResources* GetOrCreateResources(graph::FontSource* font_source, uint32_t estimate_chars);
    };
}//namespace hgl::ecs

