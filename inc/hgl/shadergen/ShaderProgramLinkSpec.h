#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/shadergen/ShaderProgramKey.h>
#include <hgl/shadergen/ShaderStageBuildSpec.h>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    struct ShaderProgramLinkSpec
    {
        ShaderStageKey vertex_stage;
        ShaderStageKey fragment_stage;
        uint64 resource_layout_hash = 0;
        uint64 vertex_input_hash = 0;
        uint64 pipeline_state_hash = 0;
        uint64 render_target_hash = 0;
        uint64 compiler_hash = 0;

        bool IsValid() const noexcept
        {
            return vertex_stage.stage == ShaderStage::Vertex
                && fragment_stage.stage == ShaderStage::Fragment;
        }

        ShaderProgramKey BuildKey() const noexcept
        {
            ShaderProgramKey key;
            key.vertex_stage_digest = vertex_stage.GetDigest();
            key.fragment_stage_digest = fragment_stage.GetDigest();
            key.resource_layout_hash = resource_layout_hash;
            key.vertex_input_hash = vertex_input_hash;
            key.pipeline_state_hash = pipeline_state_hash;
            key.render_target_hash = render_target_hash;
            key.compiler_hash = compiler_hash;
            return key;
        }
    };
}
