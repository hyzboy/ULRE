#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/SerializedDescriptorEntry.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <string>
#include <vector>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    // C1-T2：DescriptorContract 直接承载规范化后的 SerializedDescriptorEntry[]
    //（AppendEntry 就地完整规范化：ID/ssbo_type/layer/policy 全填回）。
    // 契约删减（2026-09）：原单成员包装结构降级为类型别名——
    // 条目数组本身就是契约，类型数量不再超过表达力。
    using DescriptorContract = std::vector<SerializedDescriptorEntry>;

    bool BuildDescriptorContract(
        const mtl::SerializedDescriptorEntry *entries,
        uint32 entry_count,
        DescriptorContract &out_contract);

    bool BuildDescriptorContract(
        const std::vector<mtl::SerializedDescriptorEntry> &entries,
        DescriptorContract &out_contract);

    bool BuildEffectiveDescriptorContract(
        const DescriptorContract &base_contract,
        SSBOType material_private_data,
        uint32 material_ssbo_stage_bits,
        DescriptorContract &out_contract);

    bool EnsureDescriptorContractVaryingResources(
        const mtl::MaterialVertexVaryingConfig &varying,
        DescriptorContract &in_out_contract);

    bool BuildResourceSchemaFromContract(
        const DescriptorContract &contract,
        ShaderResourceSchema &out_schema);

    bool ValidateDescriptorContract(
        const DescriptorContract &contract) noexcept;

    uint64 GetDescriptorContractHash(
        const DescriptorContract &contract,
        uint64 module_manifest_hash = 0) noexcept;
}
