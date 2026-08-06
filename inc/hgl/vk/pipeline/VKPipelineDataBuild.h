#pragma once

#include <hgl/mtl/PipelineConfig.h>

namespace hgl::graph
{
    struct PipelineData;

    /**
     * CN: 根据管线配置构建 PipelineData（替代旧的 PipelinePreset 内联表）。
     *     double_sided / alpha_cutoff 为 MaterialRecipe 顶层的材质属性，构建时叠加到配置之上。
     * EN: Build PipelineData from a material pipeline config (replaces the old PipelinePreset table).
     *     double_sided / alpha_cutoff are top-level MaterialRecipe material properties applied on top.
     */
    PipelineData *BuildPipelineData(const mtl::MaterialPipelineConfig &config,
                                    bool double_sided = false,
                                    float alpha_cutoff = 0.0f);
}//namespace hgl::graph
