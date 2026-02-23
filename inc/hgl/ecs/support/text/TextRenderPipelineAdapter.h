#pragma once

#include <hgl/ecs/support/RenderPipelineBase.h>
#include <hgl/ecs/support/TextRenderPipeline.h>
#include <memory>
#include <string>
#include <vector>

namespace hgl::graph { class Primitive; }

namespace hgl::ecs
{
    class ECSContext;

    /**
     * TextRenderPipelineAdapter - RenderPipelineBase adapter for TextRenderPipeline
     *
     * Owns a TextRenderPipeline instance (created in the constructor).
     * Conforms to the unified RenderPipelineBase interface.
     */
    class TextRenderPipelineAdapter : public RenderPipelineBase
    {
    private:
        ECSContext* context_ = nullptr;
        std::unique_ptr<TextRenderPipeline> impl_;

        static const std::string kName;

    public:
        explicit TextRenderPipelineAdapter(ECSContext* context);
        ~TextRenderPipelineAdapter() override = default;

        const std::string& GetName()  const override;
        ECSContext*         GetWorld() const override;

        bool PrepareFrame() override;
        void RunCollect()   override;
        void RunCull()      override {}    // Not used for Text
        void RunSort()      override {}    // Not used for Text
        void RunBuild()     override;
        void RunSync()      override;
        void GetRenderPrimitives(std::vector<hgl::graph::Primitive*>& out) const override;
        void Render(hgl::graph::RenderCmdBuffer* cmd) override;
        void Shutdown()     override {}
    };

}  // namespace hgl::ecs
