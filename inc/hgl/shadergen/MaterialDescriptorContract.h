#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/shadergen/CanonicalShaderContract.h>
#include <string>
#include <vector>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    constexpr uint32 MaterialDescriptorContractSchemaVersion = 1;

    struct MaterialDescriptorContractEntry
    {
        ShaderDescriptorContractEntry canonical;
        std::string name;
        std::string struct_name;
        std::string glsl_type;
        uint32 ssbo_id = MakeRecipeSSBOId(0);
        bool has_explicit_policy = false;
    };

    struct MaterialDescriptorContract
    {
        uint32 schema_version = MaterialDescriptorContractSchemaVersion;
        std::vector<MaterialDescriptorContractEntry> entries;
    };

    bool BuildMaterialDescriptorContract(
        const mtl::FixedDescriptorEntry *entries,
        uint32 entry_count,
        MaterialDescriptorContract &out_contract);

    bool BuildMaterialDescriptorContract(
        const std::vector<mtl::FixedDescriptorEntry> &entries,
        MaterialDescriptorContract &out_contract);

    bool BuildEffectiveMaterialDescriptorContract(
        const MaterialDescriptorContract &base_contract,
        const std::vector<mtl::MaterialDataSlotDecl> *data_slot_decls,
        uint32 material_ssbo_stage_bits,
        MaterialDescriptorContract &out_contract);

    bool EnsureMaterialDescriptorContractVaryingResources(
        const mtl::MaterialVertexVaryingConfig &varying,
        MaterialDescriptorContract &in_out_contract);

    bool ConvertMaterialDescriptorContractToFixed(
        const MaterialDescriptorContract &contract,
        std::vector<mtl::FixedDescriptorEntry> &out_entries);

    bool BuildMaterialResourceLayoutFromDescriptorContract(
        const MaterialDescriptorContract &contract,
        MaterialResourceLayout &out_layout);

    bool ValidateMaterialDescriptorContract(
        const MaterialDescriptorContract &contract) noexcept;

    uint64 GetMaterialDescriptorContractHash(
        const MaterialDescriptorContract &contract,
        uint64 module_manifest_hash = 0) noexcept;
}
