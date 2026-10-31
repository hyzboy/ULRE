#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/mtl/ShaderSemanticRegistry.h>
#include <hgl/mtl/ShaderStageValueType.h>
#include <hgl/type/ValueArray.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    using ShaderContractStableID = uint64;

    enum class ShaderProgramPurpose : uint8
    {
        ForwardColor = 0,
        DepthOnly,
        ShadowDepth
    };

    struct InterStageSemanticContractEntry
    {
        InterStageSemantic semantic = InterStageSemantic::Unknown;
        ShaderSemanticScalarType scalar_type =
            ShaderSemanticScalarType::Unknown;
        InterStageInterpolation interpolation =
            InterStageInterpolation::Smooth;
        uint8 component_count = 0;
        uint8 location_width = 0;
        uint32 location = InvalidShaderSemanticLocation;
    };

    inline bool operator==(
        const InterStageSemanticContractEntry &lhs,
        const InterStageSemanticContractEntry &rhs) noexcept
    {
        return lhs.semantic == rhs.semantic
            && lhs.scalar_type == rhs.scalar_type
            && lhs.interpolation == rhs.interpolation
            && lhs.component_count == rhs.component_count
            && lhs.location_width == rhs.location_width
            && lhs.location == rhs.location;
    }

    // 显式逐字段哈希：泛型 FNV1aHasher::operator<<(const T&) 折的是整块结构体
    // 字节（含 padding），与内存布局耦合——本结构体 5 个 uint8 后接 uint32，
    // 中间至少 3 字节 padding，其取值取决于栈上残留。声明键一旦吃到 padding，
    // 「同一输入」在不同调用路径下可能算出不同的键（缓存永不命中），更糟的是
    // 让键的语义变得不可解释。缓存键只能依赖语义字段。
    // 同款处理见 GlslModule.h 的 GlslModuleSemanticRequirement。
    inline hgl::hash::FNV1aHasher64 &operator<<(
        hgl::hash::FNV1aHasher64 &h,
        const InterStageSemanticContractEntry &v) noexcept
    {
        h << v.semantic
          << v.scalar_type
          << v.interpolation
          << v.component_count
          << v.location_width
          << v.location;
        return h;
    }

    // ── 校验已就地化（不再经第二套表示中转）───────────────────────────
    //   inter-stage 条目 → MaterialStageInterface.cpp::ValidateInterStageEntries

    struct ShaderOutputAttachmentContract
    {
        ShaderContractStableID write_semantic_id = 0;
        ShaderStageValueType value_type = ShaderStageValueType::Unknown;
        uint32 location = 0;
        uint32 location_width = 1;
        uint32 flags = 0;
    };

    inline bool operator==(
        const ShaderOutputAttachmentContract &lhs,
        const ShaderOutputAttachmentContract &rhs) noexcept
    {
        return lhs.write_semantic_id == rhs.write_semantic_id
            && lhs.value_type == rhs.value_type
            && lhs.location == rhs.location
            && lhs.location_width == rhs.location_width
            && lhs.flags == rhs.flags;
    }

    struct OutputContract
    {
        ShaderProgramPurpose purpose = ShaderProgramPurpose::ForwardColor;
        bool depth_only = false;
        ValueArray<ShaderOutputAttachmentContract> attachments;
    };

    bool ValidateOutputContract(const OutputContract &contract) noexcept;

    bool BuildOutputContract(
        ShaderProgramPurpose purpose,
        OutputContract &out_contract) noexcept;

    bool SerializeOutputContract(
        const OutputContract &contract,
        ValueArray<uint8> &out_bytes);
    uint64 GetOutputContractHash(
        const OutputContract &contract) noexcept;
}
