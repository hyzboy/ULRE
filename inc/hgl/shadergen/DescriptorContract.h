#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/shadergen/CanonicalShaderContract.h>
#include <string>
#include <vector>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    constexpr uint32 DescriptorContractSchemaVersion = 1;

    struct DescriptorContractEntry
    {
        ShaderDescriptorContractEntry canonical;
        std::string name;
        std::string struct_name;
        std::string glsl_type;
        uint32 ssbo_id = MakeRecipeSSBOId(0);
        bool has_explicit_policy = false;
    };

    struct DescriptorContract
    {
        uint32 schema_version = DescriptorContractSchemaVersion;
        std::vector<DescriptorContractEntry> entries;
    };

    bool BuildDescriptorContract(
        const mtl::FixedDescriptorEntry *entries,
        uint32 entry_count,
        DescriptorContract &out_contract);

    bool BuildDescriptorContract(
        const std::vector<mtl::FixedDescriptorEntry> &entries,
        DescriptorContract &out_contract);

    bool BuildEffectiveDescriptorContract(
        const DescriptorContract &base_contract,
        const std::vector<mtl::MaterialDataSlotDecl> *data_slot_decls,
        uint32 material_ssbo_stage_bits,
        DescriptorContract &out_contract);

    bool EnsureDescriptorContractVaryingResources(
        const mtl::MaterialVertexVaryingConfig &varying,
        DescriptorContract &in_out_contract);

    bool ConvertDescriptorContractToFixed(
        const DescriptorContract &contract,
        std::vector<mtl::FixedDescriptorEntry> &out_entries);

    bool BuildMaterialResourceLayoutFromDescriptorContract(
        const DescriptorContract &contract,
        ShaderResourceSchema &out_layout);

    bool ValidateDescriptorContract(
        const DescriptorContract &contract) noexcept;

    uint64 GetDescriptorContractHash(
        const DescriptorContract &contract,
        uint64 module_manifest_hash = 0) noexcept;
}
