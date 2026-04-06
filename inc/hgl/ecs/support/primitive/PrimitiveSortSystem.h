#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * PrimitiveSortSystem - RenderSort phase for primitives
     *
     * Sorts collected primitive items by distance and material.
     * Derived from SortSystem to provide unified System interface.
     */
    class PrimitiveSortSystem : public SortSystem
    {
    public:
        explicit PrimitiveSortSystem(const std::string& name = "PrimitiveSortSystem");
        ~PrimitiveSortSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnSort(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
