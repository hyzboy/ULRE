#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/mtl/ShaderSemanticRegistry.h>
#include <hgl/mtl/ShaderStageBuildContext.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
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

    // ── ShaderInterfaceContract 体系已删除（2026-09 契约删减）───────────────
    // 原 GeometrySemanticContractEntry / ShaderDescriptorContractEntry /
    // ShaderEntryPointContract / ShaderInterfaceContract 及其 Validate/Serialize/
    // GetHash 仅服务于"把生产数据转成第二套表示再校验/哈希"的往返用法与
    // 回归门自测，生产零消费。校验已就地化：
    //   inter-stage 条目 → MaterialStageInterface.cpp::ValidateInterStageEntries
    //   描述符条目       → DescriptorContract.cpp::ValidateDescriptorContract
    //   描述符契约哈希   → DescriptorContract.cpp::GetDescriptorContractHash
    // 保留本头文件中生产实际消费的部分：InterStage/Output 契约。

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

    bool SerializeOutputContract(
        const OutputContract &contract,
        ValueArray<uint8> &out_bytes);
    uint64 GetOutputContractHash(
        const OutputContract &contract) noexcept;
}
