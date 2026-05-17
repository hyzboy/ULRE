#pragma once

#include <hgl/common/DescriptorSemantic.h>
#include <hgl/common/DescriptorSetTypeDef.h>

namespace hgl::graph
{
    // ─────────────────────────────────────────────────────────────────────────
    // ShaderRequirement
    //
    // 表示一个 .glsl 片段通过 @sfm:require 注解声明的单条资源依赖。
    //
    // binding 号不存储在此处，由 ShaderRequirementSet 按 set 内插入顺序自动分配。
    // ─────────────────────────────────────────────────────────────────────────
    struct ShaderRequirement
    {
        mtl::DescriptorKind kind       = mtl::DescriptorKind::UBO;
        DescriptorSetType set_type = DescriptorSetType::Unknow;

        // 语义名，与 DescriptorSemanticMeta::name 完全对应
        // 例: "camera", "viewport", "transform_id", "transform_data"
        const char *sem_name = nullptr;

        // 对应的 GLSL include 路径（例: "common/ubo_camera.glsl"）
        // 由 ShaderRequirementSet 从语义注册表查表填充，调用者无需手填。
        const char *glsl_include = nullptr;

        bool operator==(const ShaderRequirement &o) const noexcept
        {
            return kind == o.kind && set_type == o.set_type && sem_name == o.sem_name;
        }
    };

} // namespace hgl::graph
