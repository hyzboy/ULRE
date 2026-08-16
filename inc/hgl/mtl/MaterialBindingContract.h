#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/graph/ssbo/TextureSlot.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    struct MaterialRecipe;
    struct ShaderResourceSchema;
}
namespace hgl::graph::shadergen
{
    struct DescriptorContract;
    struct ShaderProgramKey;
}

namespace hgl::graph::mtl
{
    using namespace hgl::graph::shadergen;

    constexpr uint32 InvalidMaterialRecipeBindingIndex = ~uint32(0);

    enum class BindingSource : uint8
    {
        Asset = 0,
        DirectValue,
        Missing,
        Omitted
    };

    struct ResolvedTextureBinding
    {
        uint64 logical_resource_id = 0;
        uint64 asset_identity_hash = 0;
        uint64 asset_metadata_hash = 0;
        DescriptorSemantic semantic = DescriptorSemantic::MaterialTexture;
        TextureSlot texture_slot = TextureSlot::BaseColor;
        uint32 recipe_binding_index = InvalidMaterialRecipeBindingIndex;
        uint32 direct_value = 0;
        BindingSource source = BindingSource::Missing;
        bool required = false;
        bool allow_fallback = false;
    };

    inline bool operator==(
        const ResolvedTextureBinding &lhs,
        const ResolvedTextureBinding &rhs) noexcept
    {
        return lhs.logical_resource_id == rhs.logical_resource_id
            && lhs.asset_identity_hash == rhs.asset_identity_hash
            && lhs.asset_metadata_hash == rhs.asset_metadata_hash
            && lhs.semantic == rhs.semantic
            && lhs.texture_slot == rhs.texture_slot
            && lhs.recipe_binding_index == rhs.recipe_binding_index
            && lhs.direct_value == rhs.direct_value
            && lhs.source == rhs.source
            && lhs.required == rhs.required
            && lhs.allow_fallback == rhs.allow_fallback;
    }

    struct ResolvedDataBinding
    {
        uint64 logical_resource_id = 0;
        uint64 asset_identity_hash = 0;
        uint64 asset_metadata_hash = 0;
        DescriptorSemantic semantic = DescriptorSemantic::MaterialDataSlotData;
        uint32 data_slot = 0;
        uint32 ssbo_id = 0;
        uint32 data_index = 0;
        uint32 recipe_binding_index = InvalidMaterialRecipeBindingIndex;
        SSBOType ssbo_type = SSBOType::UserDefined;
        BindingSource source = BindingSource::Missing;
        bool use_data_index = false;
        bool shared_across_instances = false;
        bool required = false;
        bool allow_fallback = false;
    };

    inline bool operator==(
        const ResolvedDataBinding &lhs,
        const ResolvedDataBinding &rhs) noexcept
    {
        return lhs.logical_resource_id == rhs.logical_resource_id
            && lhs.asset_identity_hash == rhs.asset_identity_hash
            && lhs.asset_metadata_hash == rhs.asset_metadata_hash
            && lhs.semantic == rhs.semantic
            && lhs.data_slot == rhs.data_slot
            && lhs.ssbo_id == rhs.ssbo_id
            && lhs.data_index == rhs.data_index
            && lhs.recipe_binding_index == rhs.recipe_binding_index
            && lhs.ssbo_type == rhs.ssbo_type
            && lhs.source == rhs.source
            && lhs.use_data_index == rhs.use_data_index
            && lhs.shared_across_instances == rhs.shared_across_instances
            && lhs.required == rhs.required
            && lhs.allow_fallback == rhs.allow_fallback;
    }

    struct ResolvedBindingTable
    {
        uint64 program_key_digest = 0;
        uint64 source_binding_hash = 0;
        uint32 missing_required_count = 0;
        uint32 unused_recipe_texture_count = 0;
        uint32 unused_recipe_data_count = 0;
        ValueArray<ResolvedTextureBinding> textures;
        ValueArray<ResolvedDataBinding> data;

        bool IsValid() const noexcept;
        bool IsRuntimeReady() const noexcept;
        uint64 GetStableHash() const noexcept;
    };

    enum class BindingBuildError : uint8
    {
        None = 0,
        InvalidShaderProgramKey,
        DuplicateRecipeTexture,
        DuplicateRecipeData,
        InvalidBindingTable
    };

    struct BindingBuildDiagnostic
    {
        BindingBuildError error = BindingBuildError::None;
        TextureSlot texture_slot = TextureSlot::BaseColor;
        uint32 data_slot = 0;
        SSBOType ssbo_type = SSBOType::UserDefined;
    };

    const char *GetBindingBuildErrorName(
        BindingBuildError error) noexcept;
    const char *GetBindingSourceName(
        BindingSource source) noexcept;

    uint64 GetBindingSourceHash(
        const MaterialRecipe &recipe) noexcept;
    uint64 GetResolvedTextureAssetIdentityHash(
        const char *resource_id,
        uint32 resource_id_length) noexcept;
    uint64 GetResolvedDataAssetIdentityHash(
        SSBOType ssbo_type,
        uint32 ssbo_id,
        uint32 data_slot) noexcept;

    bool ValidateResolvedBindingTable(
        const ResolvedBindingTable &table) noexcept;

    bool SerializeResolvedBindingTable(
        const ResolvedBindingTable &table,
        ValueArray<uint8> &out_bytes);

    uint64 GetResolvedBindingTableHash(
        const ResolvedBindingTable &table) noexcept;
}
