#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/mtl/ShaderProgramKey.h>
#include <hgl/mtl/ShaderStageBuildContext.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    struct ShaderLinkSpec
    {
        ShaderStageKey vertex_stage;
        ShaderStageKey mesh_stage;    // mesh shader 材质（替代 vertex_stage）
        ShaderStageKey fragment_stage;
        // 注：管线状态（BlendMode/深度/模板）由运行时管，不参与 program key——
        // 曾存在恒 0 的 pipeline_state_hash 维度（BlendMode 不覆盖 GLSL 生成），
        // 保留恒 0 维度只会制造"缓存 key 覆盖管线状态"的假安全感，已删除。
        uint64 resource_layout_hash = 0;
        uint64 vertex_input_hash = 0;
        uint64 render_target_hash = 0;
        uint64 compiler_hash = 0;

        bool IsValid() const noexcept
        {
            return (vertex_stage.stage == ShaderStage::Vertex
                 || mesh_stage.stage == ShaderStage::Mesh)
                && fragment_stage.stage == ShaderStage::Fragment;
        }

        ShaderProgramKey BuildKey() const noexcept
        {
            ShaderProgramKey key;
            key.vertex_stage_digest = vertex_stage.GetDigest();
            key.mesh_stage_digest = mesh_stage.GetDigest();
            key.fragment_stage_digest = fragment_stage.GetDigest();
            key.resource_layout_hash = resource_layout_hash;
            key.vertex_input_hash = vertex_input_hash;
            key.render_target_hash = render_target_hash;
            key.compiler_hash = compiler_hash;
            return key;
        }
    };
}
