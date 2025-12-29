#pragma once

namespace hgl
{
    namespace graph
    {
        class Material;
        class Pipeline;
    }
}

namespace hgl::ecs
{
    /**
     * Material/Pipeline index for batching
     * Similar to hgl::graph::PipelineMaterialIndex
     */
    struct MaterialPipelineKey
    {
        hgl::graph::Material* material;
        hgl::graph::Pipeline* pipeline;

        MaterialPipelineKey(hgl::graph::Material* m = nullptr, hgl::graph::Pipeline* p = nullptr)
            : material(m), pipeline(p) {}

        bool operator<(const MaterialPipelineKey& other) const
        {
            if (material < other.material) return true;
            if (material > other.material) return false;
            return pipeline < other.pipeline;
        }

        bool operator==(const MaterialPipelineKey& other) const
        {
            return material == other.material && pipeline == other.pipeline;
        }
    };
}//namespace hgl::ecs
