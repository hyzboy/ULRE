#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/mtl/ShaderStageKey.h>
#include <hgl/mtl/ShaderProgramKey.h>

namespace hgl::graph::mtl
{
    struct ShaderLinkSpec
    {
        ShaderStageKey mesh_stage;    // mesh shader 顶点处理 stage（VS 已彻底废弃）
        ShaderStageKey fragment_stage;
        // 注：管线状态（混合/深度/模板）由运行时管，不参与 program key——
        // 恒 0 维度只会制造"缓存 key 覆盖管线状态"的假安全感。
        //
        // link_contract_hash = 资源布局身份 + 输出契约（attachment / render target 形状）。
        // 这两样都不属于任何单个 stage 的编译输入（资源布局已各自折进 stage 的
        // compile_inputs_hash，输出契约到 link 期才确定），因此在这里单独汇总一份，
        // 而不是在 program key 里再摊成三个独立字段。
        uint64 link_contract_hash = 0;

        bool IsValid() const noexcept
        {
            return mesh_stage.stage == ShaderStage::Mesh
                && fragment_stage.stage == ShaderStage::Fragment;
        }

        ShaderProgramKey BuildKey() const noexcept
        {
            ShaderProgramKey key;
            key.mesh_stage_digest = mesh_stage.GetDigest();
            key.fragment_stage_digest = fragment_stage.GetDigest();
            key.link_contract_hash = link_contract_hash;
            return key;
        }
    };
}
