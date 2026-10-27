#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/type/String.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    /// program 级身份 = 两个 stage 的身份 + 一条 link 契约摘要。
    ///
    /// 早期版本这里是六个哈希（另含 resource_layout / vertex_input / render_target /
    /// compiler）。前三个已被 stage 键传递性地包含：资源布局折在各 stage 的
    /// compile_inputs_hash 里，顶点格式折在 mesh 的 interface_hash 里，编译器 profile
    /// 就在两个 stage 键中。换句话说它们只能在「两个 stage 都没变」时变——不可能，
    /// 于是纯属重复维度：多一份要序列化、要比对、还要跟 stage 键保持一致的字段。
    ///
    /// 留下的 link_contract_hash 覆盖**只属于 link 层**的东西：资源布局身份与输出
    /// 契约（attachment / render target 形状）。它也是唯一一个 stage 键装不下的维度。
    struct ShaderProgramKey
    {
        uint64 mesh_stage_digest = 0;       // mesh shader 顶点处理 stage（VS 已彻底废弃）
        uint64 fragment_stage_digest = 0;
        uint64 link_contract_hash = 0;

        uint64 GetDigest() const noexcept
        {
            hgl::hash::FNV1aHasher64 h;

            h << mesh_stage_digest
              << fragment_stage_digest
              << link_contract_hash;

            return h;
        }

        AnsiString ToString() const
        {
            return AnsiString("program-") + AnsiString::numberOf(GetDigest());
        }

        bool operator==(const ShaderProgramKey &rhs) const noexcept
        {
            return mesh_stage_digest == rhs.mesh_stage_digest
                && fragment_stage_digest == rhs.fragment_stage_digest
                && link_contract_hash == rhs.link_contract_hash;
        }

        bool operator<(const ShaderProgramKey &rhs) const noexcept
        {
            if (mesh_stage_digest != rhs.mesh_stage_digest)
                return mesh_stage_digest < rhs.mesh_stage_digest;
            if (fragment_stage_digest != rhs.fragment_stage_digest)
                return fragment_stage_digest < rhs.fragment_stage_digest;
            return link_contract_hash < rhs.link_contract_hash;
        }
    };
}
