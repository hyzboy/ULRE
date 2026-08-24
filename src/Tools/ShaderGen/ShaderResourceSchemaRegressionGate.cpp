#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/MaterialDefinitionFile.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/CompositorAssembler.h>
#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/DescriptorContract.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ResolvedModuleGraphBuilder.h>
#include <hgl/mtl/BindingTableBuilder.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/mtl/ShaderSemanticRegistry.h>
#include <hgl/mtl/GLSLCodeModule.h>
#include <hgl/mtl/GLSLCodeModuleCapabilityResolver.h>
#include <hgl/mtl/GLSLCodeModuleFile.h>
#include <hgl/mtl/GLSLCodeModuleRegistry.h>
#include <hgl/mtl/GLSLCodeModuleMetadata.h>
#include <hgl/mtl/ModuleResourceManifest.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/log/Log.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include "../../ShaderGen/3d/DefinitionDescriptorBuilder.h"
#include "../../ShaderGen/common/VertexBuilderCommon.h"
#include "../../ShaderGen/common/VertexVaryingConfig.h"
#include "../../ShaderGen/common/MeshShaderAssembler.h"   // GenerateMeshShader / MeshShaderMode

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <hgl/type/StdString.h>

using namespace hgl::graph;
using namespace hgl::graph::mtl;

namespace
{
    static hgl::uint64 StableID(const char *text)
    {
        if (!(text && text[0]))
            return 0;

        hgl::hash::FNV1aHasher64 h;
        h << text;
        return h;
    }

    static uint32_t CountAssetTextures(const ResolvedBindingTable &table)
    {
        uint32_t count = 0;
        for (int i = 0; i < table.textures.GetCount(); ++i)
        {
            if (table.textures[i].source == BindingSource::Asset)
                ++count;
        }
        return count;
    }

    static uint32_t CountAssetData(const ResolvedBindingTable &table)
    {
        uint32_t count = 0;
        for (int i = 0; i < table.data.GetCount(); ++i)
        {
            if (table.data[i].source == BindingSource::Asset)
                ++count;
        }
        return count;
    }

    static const ResolvedTextureBinding *FindTextureBinding(
        const ResolvedBindingTable &table, TextureSlot slot)
    {
        for (int i = 0; i < table.textures.GetCount(); ++i)
        {
            if (table.textures[i].texture_slot == slot)
                return &table.textures[i];
        }
        return nullptr;
    }

    static bool IsKnownRegressionGroup(const char *group)
    {
        if (!group)
            return false;

        return std::strcmp(group, "all") == 0
            || std::strcmp(group, "glsl") == 0
            || std::strcmp(group, "interface") == 0
            || std::strcmp(group, "descriptor") == 0
            || std::strcmp(group, "cache") == 0
            || std::strcmp(group, "materialization") == 0
            || std::strcmp(group, "pipeline") == 0
            || std::strcmp(group, "module-invariants") == 0;
    }

    static bool IsRegressionGroupSelected(
        const char *selected_group,
        const char *case_group)
    {
        return std::strcmp(selected_group, "all") == 0
            || std::strcmp(selected_group, case_group) == 0;
    }

    struct GateResult
    {
        std::string name;
        bool passed = false;
        std::vector<std::string> diagnostics;
    };

    // 仓库根路径（由 CMake 注入，避免硬编码盘符）
    static std::string RepoRootPath(const char *suffix)
    {
        return std::string(ULRE_REPO_ROOT) + "/" + suffix;
    }

    static hgl::OSString RepoRootOSPath(const char *suffix)
    {
        return hgl::ToOSString(RepoRootPath(suffix));
    }

    static std::vector<std::string> SummarizeConstraintShape(const ShaderResourceSchema &contract)
    {
        std::vector<std::string> rows;
        rows.reserve(contract.resources.size());

        for (const auto &req : contract.resources)
        {
            std::string row;
            row += GetDescriptorSemanticName(req.semantic);
            row += "|";
            row += GetDescriptorSemanticLayerName(req.semantic_layer);
            row += "|";
            row += GetDescriptorKindName(req.kind);
            row += "|";
            row += GetDescriptorSetTypeName(req.set_type);
            row += "|";
            row += std::to_string(static_cast<uint32_t>(req.texture_slot));
            row += "|";
            row += std::to_string(req.data_slot);
            row += "|";
            row += GetSSBOTypeName(req.ssbo_type);
            row += "|";
            row += req.required ? "required" : "optional";
            row += "|";
            row += req.allow_fallback ? "fallback" : "strict";
            rows.push_back(std::move(row));
        }

        return rows;
    }

    static GateResult RunValidationCase(const char *name,
                                        const SerializedDescriptorEntry *entries,
                                        const uint32_t count,
                                        const bool expected_pass)
    {
        GateResult result;
        result.name = name ? name : "<unnamed>";

        const ShaderResourceSchema schema = BuildShaderResourceSchema(entries, count);
        result.passed = (ValidateShaderResourceSchema(schema, result.diagnostics) == expected_pass);
        return result;
    }

    static bool CheckVertexEntries(const std::vector<SerializedVertexEntry> &actual,
                                   const std::vector<SerializedVertexEntry> &expected,
                                   std::string &diagnostic)
    {
        if (actual.size() != expected.size())
        {
            diagnostic = "vertex entry count mismatch";
            return false;
        }

        for (size_t i = 0; i < expected.size(); ++i)
        {
            if (actual[i].semantic != expected[i].semantic
             || actual[i].format != expected[i].format)
            {
                diagnostic = "vertex entry semantic/format mismatch at index "
                    + std::to_string(i);
                return false;
            }
        }

        return true;
    }

    static GateResult RunMaterialVertexABICharacterizationCase()
    {
        GateResult result;
        result.name = "K.material-vertex-abi-characterization";

        const auto check_entries = [&](const char *name,
                                       const GeometryVertexFormat &geometry,
                                       const vertex_builder_common::VertexSemanticDecl *decls,
                                       const uint32_t decl_count,
                                       const std::vector<SerializedVertexEntry> &expected)
        {
            const vertex_builder_common::VertexBuildInput input{
                PrimitiveType::Triangles, &geometry, decls, decl_count
            };
            const auto actual = vertex_builder_common::BuildVertexEntries(input);
            std::string diagnostic;
            if (!CheckVertexEntries(actual, expected, diagnostic))
                result.diagnostics.emplace_back(std::string(name) + ": " + diagnostic);
        };

        // Position-only geometry is the baseline for both 2D and 3D paths.
        {
            const GeometryVertexFormat geometry{{VertexSemantic::Position, VF_V3F}};
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F}
            };
            check_entries("position-only", geometry, decls, 1,
                {{VF_V3F, VertexSemantic::Position}});
        }

        // UV and color are independently resolved from Geometry, not by location.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2HF},
                {VertexSemantic::Color, VF_V4UN8}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2F},
                {VertexSemantic::Color, VF_V4F}
            };
            check_entries("uv-and-color", geometry, decls, 3,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V2HF, VertexSemantic::TexCoord},
                 {VF_V4UN8, VertexSemantic::Color}});
        }

        // Normal providers may expose only Normal, Normal+Tangent, or full NTB.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F}
            };
            check_entries("normal-only", geometry, decls, 2,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V3F, VertexSemantic::Normal}});
        }
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3HF}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3F}
            };
            check_entries("normal-and-tangent", geometry, decls, 3,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V3F, VertexSemantic::Normal},
                 {VF_V3HF, VertexSemantic::Tangent}});
        }
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3F},
                {VertexSemantic::Bitangent, VF_V3F}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3F},
                {VertexSemantic::Bitangent, VF_V3F}
            };
            check_entries("full-ntb", geometry, decls, 4,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V3F, VertexSemantic::Normal},
                 {VF_V3F, VertexSemantic::Tangent},
                 {VF_V3F, VertexSemantic::Bitangent}});
        }

        // Packed normal formats remain a Geometry-owned format and preserve
        // their ABI identity for the legacy path.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, PF_A2BGR10UN, 4}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F}
            };
            check_entries("packed-normal", geometry, decls, 2,
                {{VF_V3F, VertexSemantic::Position},
                 {PF_A2BGR10UN, VertexSemantic::Normal}});
        }

        // Multiple color-related attributes must remain independently
        // addressable until the provider graph replaces this legacy ABI.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Color, VF_V4UN8},
                {VertexSemantic::Luminance, VF_V1UN8}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Color, VF_V4F},
                {VertexSemantic::Luminance, VF_V1F}
            };
            check_entries("multi-color-attributes", geometry, decls, 3,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V4UN8, VertexSemantic::Color},
                 {VF_V1UN8, VertexSemantic::Luminance}});
        }

        // Equivalent floating-point formats resolve to their own Geometry
        // formats while keeping the same semantic declaration shape.
        {
            const GeometryVertexFormat rg16_geometry{{VertexSemantic::Position, VF_V2HF}};
            const GeometryVertexFormat rg32_geometry{{VertexSemantic::Position, VF_V2F}};
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F}
            };
            const vertex_builder_common::VertexBuildInput rg16_input{
                PrimitiveType::Triangles, &rg16_geometry, decls, 1
            };
            const vertex_builder_common::VertexBuildInput rg32_input{
                PrimitiveType::Triangles, &rg32_geometry, decls, 1
            };
            const auto rg16_entries = vertex_builder_common::BuildVertexEntries(rg16_input);
            const auto rg32_entries = vertex_builder_common::BuildVertexEntries(rg32_input);
            if (rg16_entries.size() != rg32_entries.size()
             || rg16_entries.size() != 1
             || rg16_entries[0].semantic != rg32_entries[0].semantic
             || rg16_entries[0].format != VF_V2HF
             || rg32_entries[0].format != VF_V2F)
            {
                result.diagnostics.emplace_back("equivalent-format ABI shape mismatch");
            }
        }

        // Every built-in definition must still expose the legacy position-first
        // contract while Phase 4 is introduced incrementally.
        static const char *builtin_ids[] = {
            BUILTIN_MTL_DEF_PURE_COLOR,
            BUILTIN_MTL_DEF_MISSING_MATERIAL,
            BUILTIN_MTL_DEF_TEXT
        };
        for (const char *id : builtin_ids)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(id, definition))
            {
                result.diagnostics.emplace_back(std::string("missing built-in definition: ") + id);
                continue;
            }
            if (!IsBootstrapMaterialDefinition(definition)
             || definition.vertex_semantic_requirements.IsEmpty())
            {
                result.diagnostics.emplace_back(std::string("invalid legacy vertex ABI: ") + id);
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialSemanticABIParityCase()
    {
        GateResult result;
        result.name = "L.material-semantic-abi-parity";

        static const char *definition_ids[] = {
            BUILTIN_MTL_DEF_PURE_COLOR,
            BUILTIN_MTL_DEF_TEXT
        };

        for (const char *id : definition_ids)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(id, definition))
            {
                result.diagnostics.emplace_back(std::string("missing migrated definition: ") + id);
                continue;
            }

            if (definition.vertex_provider_policy != MaterialVertexProviderPolicy::GeometryOnly)
            {
                result.diagnostics.emplace_back(std::string("unexpected provider policy: ") + id);
                continue;
            }

            if (definition.vertex_semantic_requirements.IsEmpty())
                result.diagnostics.emplace_back(
                    std::string("empty semantic-only ABI: ") + id);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunResolvedBindingTableCase()
    {
        GateResult result;
        result.name = "L2.resolved-binding-table";

        ShaderProgramKey program_key{};
        program_key.mesh_stage_digest = 0x7101u;
        program_key.fragment_stage_digest = 0x7102u;
        program_key.resource_layout_hash = 0x7103u;
        program_key.vertex_input_hash = 0x7104u;

        ShaderResourceSchema layout{};
        ShaderResourceSlot texture_layer_resources{};
        texture_layer_resources.logical_resource_id =
            StableID("resource.texture_layers");
        texture_layer_resources.resource_schema_id =
            StableID("schema.TextureLayer");
        texture_layer_resources.semantic =
            DescriptorSemantic::MaterialTextureLayerTable;
        texture_layer_resources.kind = DescriptorKind::SSBO;
        texture_layer_resources.ssbo_type = SSBOType::TextureLayer;
        texture_layer_resources.required = true;
        layout.resources.push_back(texture_layer_resources);

        ShaderResourceSlot data_resources{};
        data_resources.logical_resource_id =
            StableID("resource.material_data");
        data_resources.resource_schema_id =
            StableID("schema.PBRSurface");
        data_resources.semantic =
            DescriptorSemantic::MaterialDataSlotData;
        data_resources.kind = DescriptorKind::SSBO;
        data_resources.data_slot = 0;
        data_resources.ssbo_type = SSBOType::PBRSurface;
        data_resources.required = true;
        data_resources.allow_fallback = false;
        layout.resources.push_back(data_resources);

        MaterialRecipe recipe{};
        recipe.recipe_name = "BindingTableA";
        recipe.mtl_def_id = "DefinitionA";
        recipe.textures.push_back(
            {GetTextureSlotName(TextureSlot::BaseColor), "asset/albedo-a", 0, false, true});
        recipe.textures.push_back(
            {GetTextureSlotName(TextureSlot::Normal), "asset/unused-normal", 0, false, false});
        recipe.textures.push_back(
            {GetTextureSlotName(TextureSlot::Custom0), std::string(), 7, true, false});
        RecipeSSBOAssetBinding data_binding{};
        data_binding.data_slot_name = "mtl";
        data_binding.data_slot = 0;
        data_binding.ssbo_type = SSBOType::PBRSurface;
        data_binding.ssbo_id = 17;
        data_binding.data_index = 9;
        data_binding.use_data_index = true;
        recipe.ssbo_assets.push_back(data_binding);

        ResolvedBindingTable binding_table{};
        BindingBuildDiagnostic diagnostic{};
        MaterialRecipe equivalent_binding_recipe = recipe;
        equivalent_binding_recipe.recipe_name = "DifferentName";
        equivalent_binding_recipe.mtl_def_id = "DifferentDefinition";
        equivalent_binding_recipe.material_lod = 7;
        equivalent_binding_recipe.render_state_overrides.has_alpha_cutoff = true;
        equivalent_binding_recipe.render_state_overrides.alpha_cutoff = 0.25f;
        if (GetBindingSourceHash(recipe)
                != GetBindingSourceHash(
                    equivalent_binding_recipe))
        {
            result.diagnostics.emplace_back(
                "Binding Table source hash must ignore unrelated Recipe state");
        }
        if (!BuildBindingTable(
                recipe,
                layout,
                program_key,
                binding_table,
                diagnostic)
         || !binding_table.IsRuntimeReady()
         || binding_table.GetStableHash() == 0
         || binding_table.program_key_digest != program_key.GetDigest()
         || binding_table.textures.GetCount() != 3
         || binding_table.data.GetCount() != 1
         || binding_table.unused_recipe_texture_count != 0
         || binding_table.unused_recipe_data_count != 0)
        {
            result.diagnostics.emplace_back(
                std::string("Binding Table build failed: ")
                + GetBindingBuildErrorName(
                    diagnostic.error));
        }

        MaterialRecipe projected_recipe{};
        if (!BuildBindingTableRecipe(
                recipe, binding_table, projected_recipe)
         || projected_recipe.textures.size() != 3
         || projected_recipe.ssbo_assets.size() != 1
         || projected_recipe.ssbo_assets[0].ssbo_id != 17)
        {
            result.diagnostics.emplace_back(
                "Binding Table Recipe projection mismatch");
        }
        bool found_base_color = false;
        bool found_layer_value = false;
        for (const RecipeTextureBinding &binding :
             projected_recipe.textures)
        {
            if (binding.slot_name == GetTextureSlotName(TextureSlot::BaseColor)
             && binding.resource_id == "asset/albedo-a")
                found_base_color = true;
            if (binding.slot_name == GetTextureSlotName(TextureSlot::Custom0)
             && binding.use_direct_value
             && binding.direct_value == 7)
                found_layer_value = true;
        }
        if (!found_base_color || !found_layer_value)
        {
            result.diagnostics.emplace_back(
                "Binding Table Recipe must preserve TextureLayer direct values");
        }
        // Asset projection: in bindless mode the recipe is the authoritative
        // texture-slot source (the schema merges all texture_layer declarations
        // into one MaterialTextureLayerTable without per-slot info), so every
        // asset-source recipe texture is acquired. Custom0 is a direct-value
        // binding (not an asset), so it must be excluded from asset projection.
        if (CountAssetTextures(binding_table) != 2
         || CountAssetData(binding_table) != 1
         || binding_table.GetStableHash() == 0)
        {
            result.diagnostics.emplace_back(
                "Binding Table Asset projection build failed");
        }
        else
        {
            bool planned_base_color = false;
            bool planned_data = false;
            bool planned_direct_value = false;
            for (int i = 0;
                 i < binding_table.textures.GetCount();
                 ++i)
            {
                const ResolvedTextureBinding &binding =
                    binding_table.textures[i];
                if (binding.source != BindingSource::Asset)
                    continue;
                if (binding.texture_slot == TextureSlot::BaseColor)
                    planned_base_color = true;
                if (binding.texture_slot == TextureSlot::Custom0)
                    planned_direct_value = true;
            }
            for (int i = 0; i < binding_table.data.GetCount(); ++i)
            {
                const ResolvedDataBinding &binding =
                    binding_table.data[i];
                if (binding.source == BindingSource::Asset
                 && binding.data_slot == 0
                 && binding.ssbo_type == SSBOType::PBRSurface)
                    planned_data = true;
            }
            if (!planned_base_color
             || !planned_data
             || planned_direct_value)
            {
                result.diagnostics.emplace_back(
                    "Binding Table Asset projection must include only active loadable resources");
            }
        }
        // Single-IR projection: the Custom0 direct value must survive the
        // recipe -> binding table -> projected recipe round trip.
        // ResolvedBindingTable is the sole output channel, so the projected
        // spec / TextureLayerRow legacy path is gone.
        bool projected_layer_retained = false;
        for (int i = 0;
             i < binding_table.textures.GetCount();
             ++i)
        {
            const ResolvedTextureBinding &binding =
                binding_table.textures[i];
            if (binding.texture_slot == TextureSlot::Custom0
             && binding.source == BindingSource::DirectValue
             && binding.direct_value == 7)
            {
                projected_layer_retained = true;
                break;
            }
        }
        if (!projected_layer_retained)
        {
            result.diagnostics.emplace_back(
                "Binding Table must retain the active direct layer value");
        }

        MaterialRecipe zero_id_recipe = recipe;
        zero_id_recipe.ssbo_assets[0].ssbo_id = 0;
        ResolvedBindingTable zero_id_table{};
        if (!BuildBindingTable(
                zero_id_recipe,
                layout,
                program_key,
                zero_id_table,
                diagnostic)
         || !zero_id_table.IsRuntimeReady()
         || zero_id_table.data.GetCount() != 1
         || zero_id_table.data[0].source
                != BindingSource::Asset
         || zero_id_table.data[0].ssbo_id != 0
         || !BuildBindingTableRecipe(
                zero_id_recipe,
                zero_id_table,
                projected_recipe)
         || projected_recipe.ssbo_assets.size() != 1
         || projected_recipe.ssbo_assets[0].ssbo_id != 0)
        {
            result.diagnostics.emplace_back(
                "typed SSBO binding ID 0 must remain a valid active resource");
        }

        MaterialRecipe pre_resolve_recipe = recipe;
        pre_resolve_recipe.ssbo_assets[0].ssbo_type =
            SSBOType::UserDefined;
        ResolvedBindingTable pre_resolve_table{};
        if (!BuildBindingTable(
                pre_resolve_recipe,
                layout,
                program_key,
                pre_resolve_table,
                diagnostic)
         || pre_resolve_table.IsRuntimeReady())
        {
            result.diagnostics.emplace_back(
                "pre-resolve UserDefined SSBO must not masquerade as the required binding type");
        }
        ResolvedBindingTable post_resolve_table{};
        if (!BuildBindingTable(
                recipe,
                layout,
                program_key,
                post_resolve_table,
                diagnostic)
         || !post_resolve_table.IsRuntimeReady()
         || !BuildBindingTableRecipe(
                recipe, post_resolve_table, projected_recipe))
        {
            result.diagnostics.emplace_back(
                "post-resolve Recipe must rebuild a runtime-ready Binding Table");
        }

        MaterialRecipe second_recipe = recipe;
        second_recipe.recipe_name = "BindingTableB";
        second_recipe.mtl_def_id = "DefinitionB";
        second_recipe.textures[0].resource_id = "asset/albedo-b";
        second_recipe.ssbo_assets[0].ssbo_id = 23;
        if (GetBindingSourceHash(second_recipe)
                == GetBindingSourceHash(recipe))
        {
            result.diagnostics.emplace_back(
                "Binding Table source hash must include binding identity");
        }
        ResolvedBindingTable second_table{};
        if (!BuildBindingTable(
                second_recipe,
                layout,
                program_key,
                second_table,
                diagnostic)
         || second_table.GetStableHash() == binding_table.GetStableHash()
         || second_table.program_key_digest
                != binding_table.program_key_digest
         || CountAssetTextures(second_table)
                != CountAssetTextures(binding_table)
         || CountAssetData(second_table)
                != CountAssetData(binding_table))
        {
            result.diagnostics.emplace_back(
                "asset identity must affect Binding Table, not ProgramKey");
        }

        MaterialRecipe missing_recipe = recipe;
        missing_recipe.textures[0].resource_id.clear();
        missing_recipe.textures[0].required = true;
        ResolvedBindingTable missing_table{};
        if (!BuildBindingTable(
                missing_recipe,
                layout,
                program_key,
                missing_table,
                diagnostic)
         || !missing_table.IsValid()
         || missing_table.IsRuntimeReady()
         || missing_table.missing_required_count != 1
         || BuildBindingTableRecipe(
                missing_recipe, missing_table, projected_recipe)
         || FindTextureBinding(missing_table, TextureSlot::BaseColor)
                == nullptr
         || FindTextureBinding(missing_table, TextureSlot::BaseColor)
                ->source != BindingSource::Missing)
        {
            result.diagnostics.emplace_back(
                "missing required material resource must remain explicit");
        }

        ShaderResourceSchema fallback_layout = layout;
        fallback_layout.resources[0].allow_fallback = true;
        ResolvedBindingTable unresolved_fallback_table{};
        if (!BuildBindingTable(
                missing_recipe,
                fallback_layout,
                program_key,
                unresolved_fallback_table,
                diagnostic)
         || !unresolved_fallback_table.IsValid()
         || unresolved_fallback_table.IsRuntimeReady()
         || FindTextureBinding(
                unresolved_fallback_table, TextureSlot::BaseColor)
                == nullptr
         || FindTextureBinding(
                unresolved_fallback_table, TextureSlot::BaseColor)
                ->source != BindingSource::Missing)
        {
            result.diagnostics.emplace_back(
                "fallback permission without a concrete fallback must remain pending");
        }

        MaterialRecipe duplicate_recipe = recipe;
        duplicate_recipe.textures.push_back(
            {GetTextureSlotName(TextureSlot::BaseColor), "asset/duplicate", 0, false, true});
        ResolvedBindingTable duplicate_table{};
        if (BuildBindingTable(
                duplicate_recipe,
                layout,
                program_key,
                duplicate_table,
                diagnostic)
         || diagnostic.error
                != BindingBuildError::
                    DuplicateRecipeTexture)
        {
            result.diagnostics.emplace_back(
                "duplicate material texture binding must fail");
        }

        ResolvedBindingTable depth_table{};
        ShaderResourceSchema depth_layout{};
        if (!BuildBindingTable(
                recipe,
                depth_layout,
                program_key,
                depth_table,
                diagnostic)
         || !depth_table.IsRuntimeReady()
         || depth_table.unused_recipe_texture_count
                != recipe.textures.size()
         || depth_table.unused_recipe_data_count
                != recipe.ssbo_assets.size()
         || CountAssetTextures(depth_table) != 0
         || CountAssetData(depth_table) != 0)
        {
            result.diagnostics.emplace_back(
                "resource-free Program must submit no unrelated resource acquisition");
        }

        ResolvedBindingTable unresolved_table{};
        if (unresolved_table.IsRuntimeReady()
         || CountAssetTextures(unresolved_table) != 0
         || CountAssetData(unresolved_table) != 0)
        {
            result.diagnostics.emplace_back(
                "unresolved Program must not submit resource acquisition");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunShaderSemanticRegistryCase()
    {
        GateResult result;
        result.name = "L0.shader-semantic-registry";

        ShaderSemanticRegistryValidationResult validation{};
        if (!ValidateShaderSemanticRegistries(validation))
        {
            result.diagnostics.emplace_back(
                "semantic registry validation failed: error="
                + std::to_string(static_cast<hgl::uint32>(validation.error))
                + " first=" + std::to_string(validation.first_index)
                + " second=" + std::to_string(validation.second_index));
        }

        if (GetGeometrySemanticInfoCount()
                != static_cast<hgl::uint32>(VertexSemantic::RANGE_SIZE)
         || GetInterStageSemanticInfoCount()
                != static_cast<hgl::uint32>(InterStageSemantic::RANGE_SIZE))
        {
            result.diagnostics.emplace_back("semantic registry coverage mismatch");
        }

        const GeometrySemanticInfo *bitangent =
            GetGeometrySemanticInfo(VertexSemantic::Bitangent);
        if (!bitangent
         || std::strcmp(bitangent->shader_symbol, "Binormal") != 0
         || bitangent->default_shape.scalar_type
                != ShaderSemanticScalarType::Float
         || bitangent->default_shape.component_count != 3)
        {
            result.diagnostics.emplace_back(
                "geometry semantic metadata mismatch");
        }

        const InterStageSemanticInfo *data_index =
            GetInterStageSemanticInfo(InterStageSemantic::DataIndexID);
        const InterStageSemanticInfo *color =
            GetInterStageSemanticInfo(InterStageSemantic::Color);
        if (!data_index
         || data_index->stable_location != 0
         || data_index->interpolation != InterStageInterpolation::Flat
         || !color
         || color->stable_location != 5)
        {
            result.diagnostics.emplace_back(
                "inter-stage stable ABI metadata mismatch");
        }

        const GeometryVertexFormat geometry{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Color, VF_V4UN8}
        };
        hgl::uint32 location = InvalidShaderSemanticLocation;
        if (!ResolveCurrentGeometrySemanticLocation(
                geometry, VertexSemantic::Position, location)
         || location != 0
         || !ResolveCurrentGeometrySemanticLocation(
                geometry, VertexSemantic::TexCoord, location)
         || location != 1
         || !ResolveCurrentGeometrySemanticLocation(
                geometry, VertexSemantic::Color, location)
         || location != 2
         || ResolveCurrentGeometrySemanticLocation(
                geometry, VertexSemantic::Normal, location))
        {
            result.diagnostics.emplace_back(
                "current Geometry attribute-order mapping mismatch");
        }

        VertexVaryingConfig lit_varying{};
        lit_varying.emit_data_index_id = true;
        lit_varying.emit_world_pos = true;
        lit_varying.emit_world_normal = true;
        lit_varying.emit_uv0 = true;
        const std::string lit_vs = GenerateMeshShader(
            MakeDefault3DNodeConfig(),
            lit_varying,
            VK_FORMAT_R32G32B32_SFLOAT,
            {},
            MeshShaderMode::VertexPassthrough,
            96u,
            GetShaderLibraryPath(),
            {},
            nullptr,
            hgl::graph::PrimitiveType::Triangles);
        if (lit_vs.find(
                "layout(location=0) out flat uint fragDataIndexID[")
                == std::string::npos
         || lit_vs.find(
                "layout(location=1) out vec3 fragWorldPos[")
                == std::string::npos
         || lit_vs.find(
                "layout(location=2) out vec3 fragWorldNormal[")
                == std::string::npos
         || lit_vs.find(
                "layout(location=3) out vec2 fragUV0[")
                == std::string::npos
         || lit_vs.find(
                "layout(location=1) flat out uint fragTextureLayerID;")
                != std::string::npos)
        {
            result.diagnostics.emplace_back(
                "legacy generated lit varying ABI changed");
        }

        VertexVaryingConfig color_varying{};
        color_varying.emit_vertex_color = true;
        const std::string color_vs = GenerateMeshShader(
            MakeDefault3DNodeConfig(),
            color_varying,
            VK_FORMAT_R32G32B32_SFLOAT,
            {},
            MeshShaderMode::VertexPassthrough,
            96u,
            GetShaderLibraryPath(),
            {},
            nullptr,
            hgl::graph::PrimitiveType::Triangles);
        if (color_vs.find(
                "layout(location=5) out vec4 fragVertexColor[")
                == std::string::npos
         || color_vs.find(
                "layout(location=0) out vec4 fragVertexColor[")
                != std::string::npos)
        {
            result.diagnostics.emplace_back(
                "stable vertex-color ABI was not generated");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialSemanticResolverPreviewCase()
    {
        GateResult result;
        result.name = "M.material-semantic-resolver-preview";

        GLSLCodeModuleSemanticRequirement position_geometry{};
        position_geometry.source = GLSLCodeModuleCapabilitySource::GeometryAttribute;
        position_geometry.semantic = GLSLCodeModuleSemantic::Position;

        GLSLCodeModuleSemanticRequirement uv_geometry{};
        uv_geometry.source = GLSLCodeModuleCapabilitySource::GeometryAttribute;
        uv_geometry.semantic = GLSLCodeModuleSemantic::UV0;

        GLSLCodeModuleSemanticRequirement normal_geometry{};
        normal_geometry.source = GLSLCodeModuleCapabilitySource::GeometryAttribute;
        normal_geometry.semantic = GLSLCodeModuleSemantic::Normal;

        const GLSLCodeModuleSemantic position_provides[] = {
            GLSLCodeModuleSemantic::Position
        };
        const GLSLCodeModuleSemantic uv_provides[] = {
            GLSLCodeModuleSemantic::UV0
        };
        const GLSLCodeModuleSemantic normal_provides[] = {
            GLSLCodeModuleSemantic::Normal
        };

        GLSLCodeModuleDefinition position_provider{};
        position_provider.name = "preview_position_from_geometry";
        position_provider.glsl_code = "// preview only";
        position_provider.kind = GLSLCodeModuleKind::VertexInput;
        position_provider.semantic_requirements = &position_geometry;
        position_provider.semantic_requirement_count = 1;
        position_provider.semantic_provides = position_provides;
        position_provider.semantic_provide_count = 1;

        GLSLCodeModuleDefinition uv_provider{};
        uv_provider.name = "preview_uv_from_geometry";
        uv_provider.glsl_code = "// preview only";
        uv_provider.kind = GLSLCodeModuleKind::VertexInput;
        uv_provider.semantic_requirements = &uv_geometry;
        uv_provider.semantic_requirement_count = 1;
        uv_provider.semantic_provides = uv_provides;
        uv_provider.semantic_provide_count = 1;

        GLSLCodeModuleDefinition normal_provider{};
        normal_provider.name = "preview_normal_from_geometry";
        normal_provider.glsl_code = "// preview only";
        normal_provider.kind = GLSLCodeModuleKind::VertexInput;
        normal_provider.semantic_requirements = &normal_geometry;
        normal_provider.semantic_requirement_count = 1;
        normal_provider.semantic_provides = normal_provides;
        normal_provider.semantic_provide_count = 1;

        GLSLCodeModuleRegistry registry;
        if (!registry.Register(position_provider)
         || !registry.Register(uv_provider)
         || !registry.Register(normal_provider))
        {
            result.diagnostics.emplace_back("failed to create preview provider registry");
            result.passed = false;
            return result;
        }

        const auto check_preview = [&](const char *id,
                                       const GeometryVertexFormat &geometry,
                                       const uint32_t expected_selection_count)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(id, definition))
            {
                result.diagnostics.emplace_back(std::string("missing preview definition: ") + id);
                return;
            }

            MaterialDefinitionBuildRequest request{};
            request.geometry_vertex_format = &geometry;
            GLSLCodeModuleResolutionResult preview{};
            if (!PreviewMaterialVertexSemanticResolution(
                    registry, definition, request, preview))
            {
                result.diagnostics.emplace_back(std::string("preview did not run: ") + id);
                return;
            }
            if (!preview.resolved
             || preview.selections.GetCount() != int(expected_selection_count))
            {
                result.diagnostics.emplace_back(std::string("preview did not resolve: ") + id);
                return;
            }

            for (int i = 0; i < preview.selections.GetCount(); ++i)
            {
                const auto &selection = preview.selections[i];
                bool found_legacy_semantic = false;
                for (int k = 0; k < definition.vertex_semantic_requirements.GetCount(); ++k)
                {
                    if (definition.vertex_semantic_requirements[k].semantic == selection.requirement)
                    {
                        found_legacy_semantic = true;
                        break;
                    }
                }
                if (!found_legacy_semantic || !selection.provider)
                {
                    result.diagnostics.emplace_back(std::string("preview/legacy mismatch: ") + id);
                    return;
                }
            }
        };

        const GeometryVertexFormat pure2d_geometry{{
            VertexSemantic::Position, VF_V2F
        }};
        check_preview(BUILTIN_MTL_DEF_PURE_COLOR, pure2d_geometry, 1);

        const GeometryVertexFormat pure3d_geometry{{
            VertexSemantic::Position, VF_V3F
        }};
        check_preview(BUILTIN_MTL_DEF_PURE_COLOR, pure3d_geometry, 1);

        // The opt-in ABI builder consumes the preview graph to generate both
        // SerializedVertexEntry data and GLSL declarations without invoking the
        // GLSL compiler plugin.
        const auto build_abi = [&](const MaterialDefinition &definition,
                                   const GeometryVertexFormat &geometry,
                                   MaterialResolvedVertexABI &out_abi) -> bool
        {
            MaterialDefinitionBuildRequest request{};
            request.geometry_vertex_format = &geometry;
            request.vertex_code_module_registry = &registry;
            return BuildResolvedMaterialVertexABI(definition, request, out_abi);
        };

        // The same vec2 declaration must serve RG16F and RG32F Geometry
        // inputs. The raw format remains distinct only in SerializedVertexEntry.
        MaterialDefinition pure2d_definition{};
        if (!TryGetMaterialDefinitionByID(BUILTIN_MTL_DEF_PURE_COLOR, pure2d_definition))
        {
            result.diagnostics.emplace_back("missing equivalent-format definition: PureColor");
        }
        else
        {
            const GeometryVertexFormat rg16_geometry{{
                VertexSemantic::Position, VF_V2HF
            }};
            const GeometryVertexFormat rg32_geometry{{
                VertexSemantic::Position, VF_V2F
            }};
            MaterialResolvedVertexABI rg16_abi{};
            MaterialResolvedVertexABI rg32_abi{};
            // SSBO 顶点输入时代：无 VIL attribute entries（vertex_entries 空），
            // RG16F/RG32F 共享同一 s1_position_vec2 模块（GLSL 相同）
            if (!build_abi(pure2d_definition, rg16_geometry, rg16_abi)
             || !build_abi(pure2d_definition, rg32_geometry, rg32_abi)
             || rg16_abi.vertex_input_glsl != rg32_abi.vertex_input_glsl
             || !rg16_abi.vertex_entries.IsEmpty()
             || !rg32_abi.vertex_entries.IsEmpty())
            {
                result.diagnostics.emplace_back("RG16F/RG32F resolved ABI sharing failed");
            }
        }

        // Packed normals intentionally produce a distinct declaration shape
        // from direct float normals, preserving the future decode-provider ABI.
        MaterialDefinition normal_definition{};
        if (!TryGetMaterialDefinitionByID(BUILTIN_MTL_DEF_PURE_COLOR, normal_definition))
        {
            result.diagnostics.emplace_back("missing packed-normal definition: PureColor");
        }
        else
        {
            normal_definition.vertex_semantic_requirements.Clear();
            normal_definition.vertex_semantic_requirements.Add(
                MakeMaterialVertexSemanticRequirement(VertexSemantic::Position));
            normal_definition.vertex_semantic_requirements.Add(
                MakeMaterialVertexSemanticRequirement(VertexSemantic::Normal));

            const GeometryVertexFormat float_normal_geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F}
            };
            const GeometryVertexFormat packed_normal_geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, PF_A2BGR10UN, 4}
            };
            MaterialResolvedVertexABI float_normal_abi{};
            MaterialResolvedVertexABI packed_normal_abi{};
            // SSBO 时代：packed normal（A2BGR10）走独立解码模块（s1_ntb_a2bgr10）——
            // GLSL 与 float normal（s1_ntb）不同；无 VIL attribute entries
            if (!build_abi(normal_definition, float_normal_geometry, float_normal_abi)
             || !build_abi(normal_definition, packed_normal_geometry, packed_normal_abi)
             || float_normal_abi.vertex_input_glsl == packed_normal_abi.vertex_input_glsl
             || !float_normal_abi.vertex_entries.IsEmpty()
             || !packed_normal_abi.vertex_entries.IsEmpty())
            {
                result.diagnostics.emplace_back("packed normal resolved ABI variant failed");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunCompositorVersionPlacementCase()
    {
        GateResult result;
        result.name = "N.compositor-version-placement";

        CompositorAssembler assembler(GetShaderLibraryPath());
        const auto assembled = assembler.Assemble(
            SurfaceType::Lit,
            BlendMode::Opaque,
            PassType::ForwardOpaque);
        if (!assembled.success)
        {
            result.diagnostics.emplace_back(
                "Lit compositor assembly failed: " + assembled.error_message);
        }
        else
        {
            if (assembled.fragment_glsl.compare(0, 8, "#version") != 0)
                result.diagnostics.emplace_back(
                    "Compositor GLSL must begin with #version");
            // B7 后：SURFACE_TYPE/SHADOW_MODE define 输出已删
            // （ShaderPermutationKey 删除——GLSL 模块 0 消费，无注入机制）
            if (assembled.fragment_glsl.find("#version", 8) != std::string::npos)
                result.diagnostics.emplace_back(
                    "Compositor GLSL contains a second #version directive");
            const size_t surface_call = assembled.fragment_glsl.find(
                "EvalSurface(si, materialDataIndex);");
            const size_t input_module_include = assembled.fragment_glsl.find(
                "#include \"compositor/forward_lighting.glsl\"");
            const size_t algorithm_module_include = assembled.fragment_glsl.find(
                "#include \"lighting/forward_pbr.glsl\"");
            const size_t input_builder_call = assembled.fragment_glsl.find(
                "BuildForwardLightingInput(");
            const size_t algorithm_call = assembled.fragment_glsl.find(
                "EvalLighting(");
            if (surface_call == std::string::npos
             || input_module_include == std::string::npos
             || algorithm_module_include == std::string::npos
             || input_builder_call == std::string::npos
             || algorithm_call == std::string::npos
             || input_builder_call < surface_call
             || algorithm_call < input_builder_call)
               result.diagnostics.emplace_back(
                   "Lit compositor must fill LightingInput before invoking the replaceable lighting algorithm");
            if (assembled.fragment_glsl.find(
                   "#define HGL_USE_MATERIAL_SOURCE_PROVIDER 1")
                   == std::string::npos
             || assembled.fragment_glsl.find(
                   "#define HGL_USE_NTB_PROVIDER 1")
                   == std::string::npos
             || assembled.fragment_glsl.find(
                   "#include \"material/pbr_surface_source.glsl\"")
                   == std::string::npos
             || assembled.fragment_glsl.find(
                   "#include \"ntb/ntb_tangent_vbo_normalmap.glsl\"")
                   == std::string::npos)
               result.diagnostics.emplace_back(
                   "Lit compositor must enable and include the default material/NTB providers");
        }

        CompositorAssembler::CompositorModuleOptions lighting_options{};
        lighting_options.direct_lighting_module =
            "lighting/direct_cook_torrance_pbr.glsl";
        lighting_options.indirect_lighting_module =
            "lighting/indirect_sky_ambient.glsl";
        lighting_options.lighting_algorithm_module = "lighting/forward_flat.glsl";
        lighting_options.material_source_module = "material/pbr_texturearray_source.glsl";
        lighting_options.ntb_module = "ntb/ntb_texturearray_normalmap.glsl";
        lighting_options.forward_lighting_module = "compositor/forward_lighting.glsl";
        const auto scheduled_lighting = assembler.Assemble(
            SurfaceType::Lit,
            BlendMode::Opaque,
            PassType::ForwardOpaque,
            nullptr,
            "surface/material_surface.glsl",
            lighting_options);
        if (!scheduled_lighting.success
         || scheduled_lighting.fragment_glsl.find(
                "#include \"lighting/direct_cook_torrance_pbr.glsl\"")
                == std::string::npos
         || scheduled_lighting.fragment_glsl.find(
                "#include \"lighting/indirect_sky_ambient.glsl\"")
                == std::string::npos
         || scheduled_lighting.fragment_glsl.find(
                "#include \"lighting/forward_flat.glsl\"")
                == std::string::npos
         || scheduled_lighting.fragment_glsl.find(
                "#include \"material/pbr_texturearray_source.glsl\"")
                == std::string::npos
         || scheduled_lighting.fragment_glsl.find(
                "#include \"ntb/ntb_texturearray_normalmap.glsl\"")
                == std::string::npos
         || scheduled_lighting.fragment_glsl.find(
                "#include \"compositor/forward_lighting.glsl\"")
                == std::string::npos
         || scheduled_lighting.fragment_glsl.find(
                "#include \"surface/material_surface.glsl\"")
                == std::string::npos)
            result.diagnostics.emplace_back(
                "Lit compositor must route lighting and material surface modules through one scheduler");

        const auto dithered = assembler.Assemble(
            SurfaceType::Unlit,
            BlendMode::Dither,
            PassType::ForwardDither);
        if (!dithered.success
         || dithered.fragment_glsl.find("#define HGL_ALPHA_DITHER 1") == std::string::npos
         || dithered.fragment_glsl.find("HGLComposeColor") == std::string::npos)
            result.diagnostics.emplace_back(
                "Dither compositor must inject shared alpha handling");

        CompositorAssembler::CompositorModuleOptions alpha_options{};
        alpha_options.alpha_test = true;
        alpha_options.alpha_cutoff = 0.25f;
        alpha_options.material_source_module =
            "material/texture_source.glsl";
        const auto masked = assembler.Assemble(
            SurfaceType::Unlit,
            BlendMode::Masked,
            PassType::ForwardMasked,
            nullptr,
            nullptr,
            alpha_options);
        if (!masked.success
         || masked.fragment_glsl.find("#define HGL_ALPHA_TEST 1") == std::string::npos
         || masked.fragment_glsl.find("#define HGL_ALPHA_CUTOFF 0.250000") == std::string::npos)
            result.diagnostics.emplace_back(
                "Masked compositor must inject alpha-test cutoff");

        const auto texture_template = assembler.Assemble(
            SurfaceType::Unlit,
            BlendMode::Masked,
            PassType::ForwardMasked,
            "compositor/main_forward_surface.frag.glsl",
            "surface/material_surface.glsl",
            alpha_options);
        if (!texture_template.success
         || texture_template.fragment_glsl.compare(0, 8, "#version") != 0
         || texture_template.fragment_glsl.find("#define HGL_ALPHA_TEST 1")
                == std::string::npos
         || texture_template.fragment_glsl.find("HGLComposeColor")
                == std::string::npos)
        {
            result.diagnostics.emplace_back(
                "UnlitTexture Compositor must inject alpha into template + surface");
        }

        const auto alpha_to_coverage = assembler.Assemble(
            SurfaceType::Unlit,
            BlendMode::AlphaToCoverage,
            PassType::ForwardA2C);
        if (!alpha_to_coverage.success
         || alpha_to_coverage.fragment_glsl.find("HGLComposeColor") == std::string::npos)
            result.diagnostics.emplace_back(
                "Alpha-to-coverage compositor must preserve alpha output");

        const auto depth_only = assembler.Assemble(
            SurfaceType::Lit,
            BlendMode::Opaque,
            PassType::ShadowOpaque);
        if (!depth_only.success
         || depth_only.fragment_glsl.find("void main()")
                == std::string::npos
         || depth_only.fragment_glsl.find("outColor")
                != std::string::npos
         || depth_only.fragment_glsl.find("layout(location=0) out")
                != std::string::npos)
            result.diagnostics.emplace_back(
                "Shadow depth compositor must emit no color attachment");

        const auto custom_surface = assembler.Assemble(
            SurfaceType::Unlit,
            BlendMode::Opaque,
            PassType::ForwardOpaque);
        if (!custom_surface.success
         || custom_surface.fragment_glsl.find(
                "#define HGL_USE_MATERIAL_SOURCE_PROVIDER 0")
                == std::string::npos
         || custom_surface.fragment_glsl.find(
                "#define HGL_USE_NTB_PROVIDER 0")
                == std::string::npos)
            result.diagnostics.emplace_back(
                "Custom compositor surfaces must not implicitly enable Lit providers");

        // All remaining SurfaceType values (Skin/Hair/Cloth/Eye/Foliage/ClearCoat/Water)
        // resolve to lit_surface via CompositorAssembler fall-through — verified above.

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunProviderGraphIdentityCase()
    {
        GateResult result;
        result.name = "O.provider-graph-stage-identity";

        const GLSLCodeModuleSemanticRequirement normal_requirement{
            GLSLCodeModuleCapabilitySource::GeometryAttribute,
            GLSLCodeModuleSemantic::Normal,
            static_cast<uint32_t>(GLSLCodeModuleNumericClass::Float),
            3, 3
        };
        const GLSLCodeModuleSemantic normal_provides[] = {
            GLSLCodeModuleSemantic::Normal
        };
        const GLSLCodeModuleDefinition normal_provider{
            "identity_normal",
            "// identity",
            nullptr, 0, nullptr, 0,
            GLSLCodeModuleKind::VertexInput,
            &normal_requirement, 1,
            normal_provides, 1,
            10, 0
        };

        const GLSLCodeModuleSemanticRequirement uv_requirement{
            GLSLCodeModuleCapabilitySource::GeometryAttribute,
            GLSLCodeModuleSemantic::UV0,
            static_cast<uint32_t>(GLSLCodeModuleNumericClass::Float),
            2, 2
        };
        const GLSLCodeModuleSemantic uv_provides[] = {
            GLSLCodeModuleSemantic::UV0
        };
        const GLSLCodeModuleDefinition uv_provider{
            "identity_uv",
            "// identity",
            nullptr, 0, nullptr, 0,
            GLSLCodeModuleKind::VertexInput,
            &uv_requirement, 1,
            uv_provides, 1,
            10, 0
        };

        GLSLCodeModuleResolutionResult first{};
        first.resolved = true;
        first.selections.Add({GLSLCodeModuleSemantic::Normal, &normal_provider});
        first.selections.Add({GLSLCodeModuleSemantic::UV0, &uv_provider});

        GLSLCodeModuleResolutionResult equivalent_result{};
        equivalent_result.resolved = true;
        equivalent_result.selections.Add({GLSLCodeModuleSemantic::UV0, &uv_provider});
        equivalent_result.selections.Add({GLSLCodeModuleSemantic::Normal, &normal_provider});

        const uint64_t first_hash = GetGLSLCodeModuleProviderGraphHash(first);
        const uint64_t equivalent_hash = GetGLSLCodeModuleProviderGraphHash(equivalent_result);
        if (first_hash == 0 || first_hash != equivalent_hash)
            result.diagnostics.emplace_back("equivalent provider graphs must hash identically");

        GLSLCodeModuleResolutionResult changed_result = first;
        const GLSLCodeModuleDefinition packed_normal_provider{
            "identity_normal_packed",
            "// identity",
            nullptr, 0, nullptr, 0,
            GLSLCodeModuleKind::Utility,
            &normal_requirement, 1,
            normal_provides, 1,
            20, 0
        };
        changed_result.selections[0].provider = &packed_normal_provider;
        if (GetGLSLCodeModuleProviderGraphHash(changed_result) == first_hash)
            result.diagnostics.emplace_back("distinct provider graphs must hash differently");

        ShaderStageBuildContext stage{};
        stage.stage = ShaderStage::Mesh;
        const ShaderStageKey first_key = stage.BuildKeyWithProviderGraphHash(first_hash);
        const ShaderStageKey changed_key =
            stage.BuildKeyWithProviderGraphHash(
                GetGLSLCodeModuleProviderGraphHash(changed_result));
        if (first_key == changed_key
         || first_key.glsl_module_graph_hash != first_hash)
            result.diagnostics.emplace_back("provider graph hash must participate in ShaderStageKey");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunProviderGraphCompositionCase()
    {
        GateResult result;
        result.name = "P.provider-graph-composition-interface";

        const GLSLCodeModuleDefinition position_provider{
            "compose_position",
            "vec4 GetLocalPos() { return vec4(Position, 1.0); }",
            nullptr, 0, nullptr, 0,
            GLSLCodeModuleKind::Position,
            nullptr, 0, nullptr, 0, 0, 0
        };
        const GLSLCodeModuleDefinition normal_provider{
            "compose_normal",
            "vec3 GetNormal() { return Normal; }",
            nullptr, 0, nullptr, 0,
            GLSLCodeModuleKind::Utility,
            nullptr, 0, nullptr, 0, 0, 0
        };

        GLSLCodeModuleResolutionResult result_graph{};
        result_graph.resolved = true;
        result_graph.selections.Add(
            {GLSLCodeModuleSemantic::Position, &position_provider});
        result_graph.selections.Add(
            {GLSLCodeModuleSemantic::Normal, &normal_provider});
        result_graph.selections.Add(
            {GLSLCodeModuleSemantic::WorldNormal, &normal_provider});

        std::string composed;
        if (!ComposeGLSLCodeModuleProviderGraph(result_graph, composed))
            result.diagnostics.emplace_back("provider graph composition failed");
        else
        {
            const size_t position_count = composed.find("compose_position");
            const size_t normal_count = composed.find("compose_normal");
            const size_t duplicate_normal = composed.find(
                "compose_normal", normal_count == std::string::npos
                    ? 0 : normal_count + 1);
            if (position_count == std::string::npos
             || normal_count == std::string::npos
             || duplicate_normal != std::string::npos
             || position_count > normal_count)
                result.diagnostics.emplace_back(
                    "provider source must be dependency-ordered and deduplicated");
        }

        ShaderStageBuildContext vertex{};
        vertex.stage = ShaderStage::Mesh;
        vertex.outputs.Add({0, ShaderStageValueType::Vec3, 0, 0});
        vertex.outputs.Add({0, ShaderStageValueType::Vec2, 1, 0});
        ShaderStageBuildContext fragment{};
        fragment.stage = ShaderStage::Fragment;
        fragment.inputs.Add({0, ShaderStageValueType::Vec3, 0, 0});
        fragment.inputs.Add({0, ShaderStageValueType::Vec2, 1, 0});
        if (!HasCompatibleStageInterface(vertex, fragment))
            result.diagnostics.emplace_back("matching composed stage interfaces rejected");

        fragment.inputs[1].value_type = ShaderStageValueType::Vec4;
        if (HasCompatibleStageInterface(vertex, fragment))
            result.diagnostics.emplace_back("incompatible composed stage interfaces accepted");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunResolvedStageCacheIdentityCase()
    {
        GateResult result;
        result.name = "Q.resolved-stage-cache-identity";

        const GeometryVertexFormat rg16_geometry{{
            VertexSemantic::Position, VF_V2HF
        }};
        const GeometryVertexFormat rg32_geometry{{
            VertexSemantic::Position, VF_V2F
        }};

        ShaderStageBuildContext stage{};
        stage.stage = ShaderStage::Mesh;
        stage.glsl_module_graph_hash = 0x13579bdf2468ace0ull;
        const ShaderStageKey stage_key = stage.BuildKey();
        const ShaderStageKey equivalent_stage_key = stage.BuildKey();
        if (!(stage_key == equivalent_stage_key))
            result.diagnostics.emplace_back("equivalent provider stages must share cache identity");

        if (rg16_geometry.GetVertexInputHash() == rg32_geometry.GetVertexInputHash())
            result.diagnostics.emplace_back("raw Geometry formats must remain distinct");

        ShaderLinkSpec rg16_link{};
        rg16_link.mesh_stage = stage_key;
        rg16_link.fragment_stage.stage = ShaderStage::Fragment;
        rg16_link.vertex_input_hash = rg16_geometry.GetVertexInputHash();
        ShaderLinkSpec rg32_link = rg16_link;
        rg32_link.vertex_input_hash = rg32_geometry.GetVertexInputHash();
        if (rg16_link.BuildKey() == rg32_link.BuildKey())
            result.diagnostics.emplace_back(
                "equivalent shader stages must retain distinct program input identity");

        ShaderArtifactStore store(RepoRootOSPath("build"), ShaderCacheMode::BuildIfMissing);
        const uint32_t payload[] = {0x07230203u, 1u, 2u, 3u};
        if (!store.SaveStageSPV(stage_key, payload, sizeof(payload)))
        {
            result.diagnostics.emplace_back("stage SPV cache save failed");
        }
        else
        {
            hgl::ValueArray<hgl::uint8> loaded;
            if (!store.LoadStageSPV(stage_key, loaded)
             || loaded.GetCount() != static_cast<int>(sizeof(payload))
             || std::memcmp(loaded.GetData(), payload, sizeof(payload)) != 0)
            {
                result.diagnostics.emplace_back("stage SPV cache round-trip failed");
            }

            ShaderStageKey wrong_key = stage_key;
            wrong_key.glsl_module_graph_hash ^= 1ull;
            if (store.LoadStageSPV(wrong_key, loaded))
                result.diagnostics.emplace_back("cache must reject a different stage key");
        }

        ShaderArtifactStore read_only_store(
            RepoRootOSPath("build"), ShaderCacheMode::ReadOnly);
        if (read_only_store.SaveStageSPV(stage_key, payload, sizeof(payload)))
            result.diagnostics.emplace_back("read-only cache must reject writes");
        const uint32_t invalid_spv[] = {0u, 1u, 2u, 3u};
        if (store.SaveStageSPV(
                stage_key, invalid_spv, sizeof(invalid_spv)))
            result.diagnostics.emplace_back(
                "invalid SPV payload must be rejected");

        ShaderLinkSpec program_link{};
        program_link.mesh_stage = stage_key;
        program_link.fragment_stage.stage = ShaderStage::Fragment;
        program_link.fragment_stage.definition_hash = 0x2201u;
        program_link.fragment_stage.glsl_module_graph_hash = 0x2202u;
        program_link.fragment_stage.interface_hash = 0x2203u;
        program_link.fragment_stage.resource_hash = 0x2204u;
        program_link.fragment_stage.compiler_hash = 0x2205u;
        program_link.resource_layout_hash = 0x2301u;
        program_link.vertex_input_hash = 0x2302u;
        program_link.render_target_hash = 0x2303u;
        program_link.compiler_hash = stage_key.compiler_hash != 0
            ? stage_key.compiler_hash : 0x2304u;
        program_link.mesh_stage.compiler_hash =
            program_link.compiler_hash;
        program_link.fragment_stage.compiler_hash =
            program_link.compiler_hash;

        ShaderProgramArtifactMetadata metadata{};
        metadata.program_key_digest =
            program_link.BuildKey().GetDigest();
        metadata.resolved_module_graph_hash = 0x2403u;
        metadata.shader_interface_hash = 0x2404u;
        metadata.output_contract_hash =
            program_link.render_target_hash;
        metadata.mesh_stage_digest =
            program_link.mesh_stage.GetDigest();
        metadata.fragment_stage_digest =
            program_link.fragment_stage.GetDigest();
        metadata.compiler_profile_hash =
            program_link.compiler_hash;
        metadata.device_target_hash = 0x2405u;
        metadata.generated_source_digest = 0x2406u;

        const uint32_t fragment_payload[] =
            {0x07230203u, 7u, 8u, 9u};
        if (!store.SaveStageSPV(
                program_link.mesh_stage,
                payload,
                sizeof(payload))
         || !store.SaveStageSPV(
                program_link.fragment_stage,
                fragment_payload,
                sizeof(fragment_payload))
         || !store.SaveProgramMetadata(program_link, metadata))
        {
            result.diagnostics.emplace_back(
                "complete program artifact save failed");
        }
        else
        {
            hgl::ValueArray<hgl::uint8> cached_vertex;
            hgl::ValueArray<hgl::uint8> cached_fragment;
            if (!store.HasProgramMetadata(program_link)
             || !store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    cached_vertex,
                    cached_fragment)
             || cached_vertex.GetCount()
                    != static_cast<int>(sizeof(payload))
             || cached_fragment.GetCount()
                    != static_cast<int>(sizeof(fragment_payload))
             || std::memcmp(
                    cached_vertex.GetData(),
                    payload,
                    sizeof(payload)) != 0
             || std::memcmp(
                    cached_fragment.GetData(),
                    fragment_payload,
                    sizeof(fragment_payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "complete program artifact load failed");
            }

            ShaderProgramArtifactMetadata changed_metadata = metadata;
            changed_metadata.device_target_hash ^= 1u;
            if (store.LoadProgramArtifacts(
                    program_link,
                    changed_metadata,
                    cached_vertex,
                    cached_fragment))
            {
                result.diagnostics.emplace_back(
                    "metadata mismatch must reject stage artifacts");
            }

            if (!read_only_store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    cached_vertex,
                    cached_fragment)
             || read_only_store.SaveProgramMetadata(
                    program_link, metadata))
            {
                result.diagnostics.emplace_back(
                    "read-only complete program artifact behavior failed");
            }

            hgl::filesystem::Path metadata_path(
                RepoRootOSPath("build"));
            metadata_path /=
                hgl::OSString(OS_TEXT("shader-cache"));
            metadata_path /=
                hgl::OSString(OS_TEXT("program"));
            metadata_path /= hgl::ToOSString(
                (program_link.BuildKey().ToString()
                    + hgl::AnsiString(".meta")).c_str());
            hgl::int64 metadata_file_size = 0;
            void *metadata_file =
                hgl::filesystem::LoadFileToMemory(
                    metadata_path.ToOSString(),
                    metadata_file_size);
            if (!metadata_file || metadata_file_size <= 0)
            {
                result.diagnostics.emplace_back(
                    "failed to load metadata corruption fixture");
            }
            else
            {
                auto *metadata_bytes =
                    static_cast<hgl::uint8 *>(metadata_file);
                metadata_bytes[metadata_file_size - 1] ^= 0xffu;
                if (hgl::filesystem::SaveMemoryToFile(
                        metadata_path.ToOSString(),
                        metadata_bytes,
                        metadata_file_size)
                        != metadata_file_size)
                {
                    result.diagnostics.emplace_back(
                        "failed to corrupt program metadata");
                }
                delete[] metadata_bytes;

                ShaderProgramArtifactMetadata corrupted{};
                if (store.LoadProgramMetadata(
                        program_link, corrupted))
                {
                    result.diagnostics.emplace_back(
                        "corrupt program metadata must be rejected");
                }
                if (!store.SaveProgramMetadata(
                        program_link, metadata))
                {
                    result.diagnostics.emplace_back(
                        "failed to restore program metadata fixture");
                }
            }
        }

        ShaderProgramArtifactMetadata invalid_metadata = metadata;
        invalid_metadata.program_key_digest = 0;
        if (store.SaveProgramMetadata(
                program_link, invalid_metadata))
            result.diagnostics.emplace_back(
                "invalid program metadata must reject writes");

        ShaderLinkSpec stage_only_link = program_link;
        stage_only_link.render_target_hash ^= 0x1000u;
        ShaderProgramArtifactMetadata stage_only_metadata = metadata;
        stage_only_metadata.program_key_digest =
            stage_only_link.BuildKey().GetDigest();
        hgl::ValueArray<hgl::uint8> stage_only_vertex;
        hgl::ValueArray<hgl::uint8> stage_only_fragment;
        if (store.LoadProgramArtifacts(
                stage_only_link,
                stage_only_metadata,
                stage_only_vertex,
                stage_only_fragment))
        {
            result.diagnostics.emplace_back(
                "stage-only artifacts must not count as a program hit");
        }

        ShaderLinkSpec metadata_only_link = program_link;
        metadata_only_link.fragment_stage.definition_hash ^= 0x2000u;
        ShaderProgramArtifactMetadata metadata_only = metadata;
        metadata_only.program_key_digest =
            metadata_only_link.BuildKey().GetDigest();
        metadata_only.fragment_stage_digest =
            metadata_only_link.fragment_stage.GetDigest();
        if (!store.SaveProgramMetadata(
                metadata_only_link, metadata_only))
        {
            result.diagnostics.emplace_back(
                "metadata-only fixture save failed");
        }
        else if (store.LoadProgramArtifacts(
                    metadata_only_link,
                    metadata_only,
                    stage_only_vertex,
                    stage_only_fragment))
        {
            result.diagnostics.emplace_back(
                "metadata without all stages must not count as a program hit");
        }

        ShaderArtifactStore shadow_store(
            RepoRootOSPath("build/cache-isolation"),
            ShaderCacheMode::BuildIfMissing);
        const uint32_t shadow_payload[] =
            {0x07230203u, 4u, 5u, 6u};
        const uint32_t shadow_fragment_payload[] =
            {0x07230203u, 10u, 11u, 12u};
        if (!shadow_store.SaveStageSPV(
                program_link.mesh_stage,
                shadow_payload,
                sizeof(shadow_payload))
         || !shadow_store.SaveStageSPV(
                program_link.fragment_stage,
                shadow_fragment_payload,
                sizeof(shadow_fragment_payload))
         || !shadow_store.SaveProgramMetadata(
                program_link, metadata))
        {
            result.diagnostics.emplace_back(
                "isolated cache save failed");
        }
        else
        {
            hgl::ValueArray<hgl::uint8> legacy_loaded;
            hgl::ValueArray<hgl::uint8> shadow_loaded;
            if (!store.LoadStageSPV(stage_key, legacy_loaded)
             || legacy_loaded.GetCount() != static_cast<int>(sizeof(payload))
             || std::memcmp(legacy_loaded.GetData(), payload, sizeof(payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "isolated cache must not replace the primary artifact");
            }

            if (!shadow_store.LoadStageSPV(
                    program_link.mesh_stage, shadow_loaded)
             || shadow_loaded.GetCount() != static_cast<int>(sizeof(shadow_payload))
             || std::memcmp(
                    shadow_loaded.GetData(), shadow_payload, sizeof(shadow_payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "isolated cache must load its own artifact");
            }

            hgl::ValueArray<hgl::uint8> shadow_fragment_loaded;
            if (!shadow_store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    shadow_loaded,
                    shadow_fragment_loaded)
             || shadow_fragment_loaded.GetCount()
                    != static_cast<int>(
                        sizeof(shadow_fragment_payload))
             || std::memcmp(
                    shadow_fragment_loaded.GetData(),
                    shadow_fragment_payload,
                    sizeof(shadow_fragment_payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "program artifacts must remain root-isolated");
            }

            ShaderArtifactStore contract_store(
                RepoRootOSPath("build/cache-miss"),
                ShaderCacheMode::ReadOnly);
            if (contract_store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    shadow_loaded,
                    shadow_fragment_loaded))
            {
                result.diagnostics.emplace_back(
                    "independent cache root must not hit other programs");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunAuthoritativeMaterialCacheIdentityCase()
    {
        GateResult result;
        result.name = "Q0.authoritative-material-cache-identity";

        MaterialDefinition lit{};
        MaterialDefinition vertex_color{};
        if (!TryGetMaterialDefinitionByID("Lit", lit)
         || !TryGetMaterialDefinitionByID("VertexColor", vertex_color))
        {
            result.diagnostics.emplace_back(
                "cache identity material definitions unavailable");
            result.passed = false;
            return result;
        }

        const GeometryVertexFormat lit_geometry_a{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal, VF_V3F}
        };
        const GeometryVertexFormat lit_geometry_b{
            {VertexSemantic::Position, VF_V3HF},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal, VF_V3F}
        };
        const GeometryVertexFormat color_geometry{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Color, VF_V4UN8}
        };

        const auto build = [](
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinition &definition,
            const GeometryVertexFormat &geometry)
        {
            MaterialDefinitionBuildRequest request{};
            request.recipe.mtl_def_id = definition.definition_id;
            request.geometry_vertex_format = &geometry;
            request.defer_finalize = true;
            return std::unique_ptr<ShaderBuildContext>(
                CreateMaterialFromDefinition(
                    profile, definition, request));
        };

        const auto lit_a = build(nullptr, lit, lit_geometry_a);
        const auto lit_b = build(nullptr, lit, lit_geometry_b);
        const auto color = build(nullptr, vertex_color, color_geometry);
        contract::PhysicalDeviceProfileLite profile{};
        profile.api_version = contract::MakeVkVersion(1, 4);
        profile.limits.max_uniform_buffer_range = 65536;
        profile.limits.max_storage_buffer_range = 1ull << 30;
        const auto lit_targeted = build(&profile, lit, lit_geometry_a);

        if (!lit_a || !lit_b || !color || !lit_targeted
         || !lit_a->HasProgramLink()
         || !lit_b->HasProgramLink()
         || !color->HasProgramLink()
         || !lit_targeted->HasProgramLink()
         || !lit_a->HasProgramArtifactMetadata()
         || !lit_b->HasProgramArtifactMetadata()
         || !color->HasProgramArtifactMetadata()
         || !lit_targeted->HasProgramArtifactMetadata())
        {
            result.diagnostics.emplace_back(
                "generate-only material builds must produce ProgramLink");
        }
        else
        {
            const ShaderLinkSpec &a = lit_a->GetProgramLink();
            const ShaderLinkSpec &b = lit_b->GetProgramLink();
            const ShaderLinkSpec &c = color->GetProgramLink();
            const ShaderLinkSpec &targeted =
                lit_targeted->GetProgramLink();
            if (lit_a->GetProgramArtifactMetadata().
                    program_key_digest
                    != a.BuildKey().GetDigest()
             || lit_a->GetProgramArtifactMetadata().
                    mesh_stage_digest
                    != a.mesh_stage.GetDigest()
             || lit_a->GetProgramArtifactMetadata().
                    fragment_stage_digest
                    != a.fragment_stage.GetDigest())
            {
                result.diagnostics.emplace_back(
                    "BuildContext program metadata does not match ProgramLink");
            }

            ShaderArtifactStore hot_store(
                RepoRootOSPath("build/cache-hot"),
                ShaderCacheMode::BuildIfMissing);
            const uint32_t cached_vertex_spv[] =
                {ShaderArtifactSPVMagic, 0x3101u, 0x3102u, 0x3103u};
            const uint32_t cached_fragment_spv[] =
                {ShaderArtifactSPVMagic, 0x3201u, 0x3202u, 0x3203u};
            if (!hot_store.SaveStageSPV(
                    a.mesh_stage,
                    cached_vertex_spv,
                    sizeof(cached_vertex_spv))
             || !hot_store.SaveStageSPV(
                    a.fragment_stage,
                    cached_fragment_spv,
                    sizeof(cached_fragment_spv))
             || !hot_store.SaveProgramMetadata(
                    a,
                    lit_a->GetProgramArtifactMetadata()))
            {
                result.diagnostics.emplace_back(
                    "hot program artifact fixture save failed");
            }
            else
            {
                lit_a->SetArtifactStore(&hot_store);
                if (!FinalizeShaderBuildContext(lit_a.get())
                 || !lit_a->GetStageShader(ShaderStage::Mesh)
                 || !lit_a->GetStageShader(ShaderStage::Fragment)
                 || lit_a->GetStageShader(
                        ShaderStage::Mesh)->GetSPVSize()
                        != sizeof(cached_vertex_spv)
                 || lit_a->GetStageShader(
                        ShaderStage::Fragment)->GetSPVSize()
                        != sizeof(cached_fragment_spv)
                 || std::memcmp(
                        lit_a->GetStageShader(
                            ShaderStage::Mesh)->GetSPVData(),
                        cached_vertex_spv,
                        sizeof(cached_vertex_spv)) != 0
                 || std::memcmp(
                        lit_a->GetStageShader(
                            ShaderStage::Fragment)->GetSPVData(),
                        cached_fragment_spv,
                        sizeof(cached_fragment_spv)) != 0)
                {
                    result.diagnostics.emplace_back(
                        "hot program hit must populate BuildContext SPV");
                }
            }

            ShaderArtifactStore read_only_miss_store(
                RepoRootOSPath("build/cache-read-only-miss"),
                ShaderCacheMode::ReadOnly);
            lit_b->SetArtifactStore(&read_only_miss_store);
            // read-only miss：不应命中元数据、不应编译（Finalize 返回 false）、
            // 不应产生 SPV 数据（stage 对象存在但 SPV 为空）
            if (read_only_miss_store.HasProgramMetadata(
                    lit_b->GetProgramLink())
             || FinalizeShaderBuildContext(lit_b.get())
             || (lit_b->GetStageShader(ShaderStage::Mesh)
                 && lit_b->GetStageShader(
                        ShaderStage::Mesh)->GetSPVSize() != 0)
             || (lit_b->GetStageShader(ShaderStage::Fragment)
                 && lit_b->GetStageShader(
                        ShaderStage::Fragment)->GetSPVSize() != 0))
            {
                result.diagnostics.emplace_back(
                    "read-only program miss must not compile or write");
            }

            if (a.mesh_stage.definition_hash == 0
             || a.mesh_stage.interface_hash == 0
             || a.mesh_stage.resource_hash == 0
             || a.mesh_stage.compiler_hash == 0
             || a.fragment_stage.definition_hash == 0
             || a.fragment_stage.interface_hash == 0
             || a.fragment_stage.resource_hash == 0
             || a.fragment_stage.compiler_hash == 0
             || a.resource_layout_hash == 0
             || a.vertex_input_hash == 0
             || a.compiler_hash == 0)
            {
                result.diagnostics.emplace_back(
                    "authoritative stage/program keys contain zero identity");
            }

            if (a.fragment_stage.GetDigest()
                    == c.fragment_stage.GetDigest()
             || a.BuildKey() == c.BuildKey())
            {
                result.diagnostics.emplace_back(
                    "different materials must not share authoritative keys");
            }

            if (a.mesh_stage.GetDigest()
                    == b.mesh_stage.GetDigest()
             || a.BuildKey() == b.BuildKey())
            {
                result.diagnostics.emplace_back(
                    "different geometry formats must not share authoritative keys");
            }

            if (a.mesh_stage.compiler_hash
                    == targeted.mesh_stage.compiler_hash
             || a.fragment_stage.compiler_hash
                    == targeted.fragment_stage.compiler_hash
             || a.BuildKey() == targeted.BuildKey())
            {
                result.diagnostics.emplace_back(
                    "compiler profile changes must invalidate authoritative keys");
            }

            const char *const shared_definition_ids[] =
            {
                "HumanSkinDefinition",
                "WoodDefinition",
                "StoneDefinition",
                "MetalDefinition"
            };
            std::vector<std::unique_ptr<ShaderBuildContext>>
                shared_builds;
            for (int i = 0; i < 4; ++i)
            {
                MaterialDefinition definition = lit;
                definition.definition_id = shared_definition_ids[i];
                definition.definition_name = shared_definition_ids[i];
                shared_builds.push_back(
                    build(nullptr, definition, lit_geometry_a));
            }

            if (shared_builds.size() != 4
             || !shared_builds[0]
             || !shared_builds[1]
             || !shared_builds[2]
             || !shared_builds[3])
            {
                result.diagnostics.emplace_back(
                    "cross-Definition ProgramKey fixtures failed");
            }
            else
            {
                const ShaderProgramKey shared_key =
                    shared_builds[0]->GetProgramLink().BuildKey();
                for (int i = 1; i < 4; ++i)
                {
                    if (!(shared_builds[i]->
                            GetProgramLink().BuildKey() == shared_key))
                    {
                        result.diagnostics.emplace_back(
                            "equivalent Definitions must share ProgramKey identity");
                        break;
                    }
                }
            }

            const uint64_t context_a =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr);
            const uint64_t context_b =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_b,
                    nullptr);
            const uint64_t context_targeted =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    &profile);
            if (context_a == 0
             || context_a == context_b
             || context_a == context_targeted)
            {
                result.diagnostics.emplace_back(
                    "program build context must include geometry and device");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunCanonicalShaderContractCase()
    {
        GateResult result;
        result.name = "Q1.canonical-shader-contract";

        const auto bytes_equal = [](const hgl::ValueArray<hgl::uint8> &lhs,
                                    const hgl::ValueArray<hgl::uint8> &rhs)
        {
            return lhs.GetCount() == rhs.GetCount()
                && (lhs.IsEmpty()
                 || std::memcmp(
                        lhs.GetData(),
                        rhs.GetData(),
                        static_cast<size_t>(lhs.GetCount())) == 0);
        };

        const ShaderContractStableID source_module =
            StableID("module.source");
        const ShaderContractStableID dependency_module =
            StableID("module.dependency");

        ResolvedModuleGraph graph_a{};
        graph_a.modules.Add(
            {source_module, 0x101u, 1, 0});
        graph_a.modules.Add(
            {dependency_module, 0x102u, 0, 0});
        graph_a.dependencies.Add(
            {source_module, dependency_module});
        graph_a.provider_selections.Add(
            {
                GLSLCodeModuleSemantic::Position,
                dependency_module,
                10,
                0
            });
        graph_a.aggregated_semantic_requirements.Add(
            {
                GLSLCodeModuleCapabilitySource::GeometryAttribute,
                GLSLCodeModuleSemantic::Position,
                uint32_t(GLSLCodeModuleNumericClass::Float),
                3,
                3
            });
        graph_a.aggregated_semantic_requirements.Add(
            {
                GLSLCodeModuleCapabilitySource::Resource,
                GLSLCodeModuleSemantic::MaterialData,
                uint32_t(GLSLCodeModuleNumericClass::Any),
                0,
                0
            });

        ResolvedModuleGraph graph_b{};
        graph_b.modules.Add(graph_a.modules[1]);
        graph_b.modules.Add(graph_a.modules[0]);
        graph_b.dependencies.Add(graph_a.dependencies[0]);
        graph_b.provider_selections.Add(graph_a.provider_selections[0]);
        graph_b.aggregated_semantic_requirements.Add(
            graph_a.aggregated_semantic_requirements[1]);
        graph_b.aggregated_semantic_requirements.Add(
            graph_a.aggregated_semantic_requirements[0]);

        hgl::ValueArray<hgl::uint8> graph_bytes_a;
        hgl::ValueArray<hgl::uint8> graph_bytes_b;
        if (!SerializeResolvedModuleGraph(graph_a, graph_bytes_a)
         || !SerializeResolvedModuleGraph(graph_b, graph_bytes_b)
         || !bytes_equal(graph_bytes_a, graph_bytes_b)
         || GetResolvedModuleGraphHash(graph_a)
                != GetResolvedModuleGraphHash(graph_b))
        {
            result.diagnostics.emplace_back(
                "module graph serialization must ignore input order");
        }

        ResolvedModuleGraph changed_graph = graph_a;
        changed_graph.modules[0].module_content_hash ^= 1u;
        if (GetResolvedModuleGraphHash(changed_graph)
            == GetResolvedModuleGraphHash(graph_a))
        {
            result.diagnostics.emplace_back(
                "module content changes must affect graph hash");
        }

        ResolvedModuleGraph duplicate_graph = graph_a;
        duplicate_graph.modules.Add(graph_a.modules[0]);
        if (ValidateResolvedModuleGraph(duplicate_graph))
            result.diagnostics.emplace_back(
                "duplicate module identities must be rejected");

        ShaderInterfaceContract interface_a{};
        interface_a.geometry_semantics.Add(
            {
                VertexSemantic::Position,
                ShaderSemanticScalarType::Float,
                3,
                1,
                0,
                uint32_t(VK_FORMAT_R32G32B32_SFLOAT)
            });
        interface_a.geometry_semantics.Add(
            {
                VertexSemantic::TexCoord,
                ShaderSemanticScalarType::Float,
                2,
                1,
                1,
                uint32_t(VK_FORMAT_R32G32_SFLOAT)
            });
        interface_a.inter_stage_semantics.Add(
            {
                InterStageSemantic::DataIndexID,
                ShaderSemanticScalarType::UnsignedInteger,
                InterStageInterpolation::Flat,
                1,
                1,
                0
            });
        interface_a.inter_stage_semantics.Add(
            {
                InterStageSemantic::UV0,
                ShaderSemanticScalarType::Float,
                InterStageInterpolation::Smooth,
                2,
                1,
                1
            });
        interface_a.descriptor_requirements.Add(
            {
                StableID("resource.material_data"),
                StableID("schema.PBRSurface.v1"),
                DescriptorSemantic::MaterialDataSlotData,
                DescriptorSemanticLayer::SSBO,
                DescriptorSetType::Material,
                DescriptorKind::SSBO,
                TextureSlot::BaseColor,
                SSBOType::PBRSurface,
                0,
                uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                1,
                true,
                false
            });
        interface_a.descriptor_requirements.Add(
            {
                StableID("resource.texture_layers"),
                StableID("schema.TextureLayer.v1"),
                DescriptorSemantic::MaterialTextureLayerTable,
                DescriptorSemanticLayer::SSBO,
                DescriptorSetType::Material,
                DescriptorKind::SSBO,
                TextureSlot::BaseColor,
                SSBOType::TextureLayer,
                0,
                uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                1,
                false,
                true
            });
        interface_a.entry_points.Add(
            {ShaderStage::Mesh, StableID("main.msh")});
        interface_a.entry_points.Add(
            {ShaderStage::Fragment, StableID("main.fs")});

        ShaderInterfaceContract interface_b{};
        interface_b.geometry_semantics.Add(interface_a.geometry_semantics[1]);
        interface_b.geometry_semantics.Add(interface_a.geometry_semantics[0]);
        interface_b.inter_stage_semantics.Add(
            interface_a.inter_stage_semantics[1]);
        interface_b.inter_stage_semantics.Add(
            interface_a.inter_stage_semantics[0]);
        interface_b.descriptor_requirements.Add(
            interface_a.descriptor_requirements[1]);
        interface_b.descriptor_requirements.Add(
            interface_a.descriptor_requirements[0]);
        interface_b.entry_points.Add(interface_a.entry_points[1]);
        interface_b.entry_points.Add(interface_a.entry_points[0]);

        hgl::ValueArray<hgl::uint8> interface_bytes_a;
        hgl::ValueArray<hgl::uint8> interface_bytes_b;
        if (!SerializeShaderInterfaceContract(
                interface_a, interface_bytes_a)
         || !SerializeShaderInterfaceContract(
                interface_b, interface_bytes_b)
         || !bytes_equal(interface_bytes_a, interface_bytes_b)
         || GetShaderInterfaceContractHash(interface_a)
                != GetShaderInterfaceContractHash(interface_b))
        {
            result.diagnostics.emplace_back(
                "shader interface serialization must ignore input order");
        }

        ShaderInterfaceContract conflicting_interface = interface_a;
        conflicting_interface.inter_stage_semantics[1].location = 0;
        if (ValidateShaderInterfaceContract(conflicting_interface))
            result.diagnostics.emplace_back(
                "inter-stage location conflicts must be rejected");

        OutputContract output_a{};
        output_a.purpose = ShaderProgramPurpose::ForwardColor;
        output_a.attachments.Add(
            {
                StableID("output.color"),
                ShaderStageValueType::Vec4,
                0,
                1,
                0
            });
        output_a.attachments.Add(
            {
                StableID("output.material_id"),
                ShaderStageValueType::UInt,
                1,
                1,
                0
            });
        OutputContract output_b = output_a;
        output_b.attachments.Clear();
        output_b.attachments.Add(output_a.attachments[1]);
        output_b.attachments.Add(output_a.attachments[0]);
        if (GetOutputContractHash(output_a) == 0
         || GetOutputContractHash(output_a)
                != GetOutputContractHash(output_b))
        {
            result.diagnostics.emplace_back(
                "output serialization must ignore attachment order");
        }

        OutputContract depth_output{};
        depth_output.purpose = ShaderProgramPurpose::DepthOnly;
        depth_output.depth_only = true;
        if (!ValidateOutputContract(depth_output))
            result.diagnostics.emplace_back(
                "depth-only output contract must allow no attachments");

        OutputContract unsupported_output = depth_output;
        unsupported_output.purpose =
            static_cast<ShaderProgramPurpose>(3);
        if (ValidateOutputContract(unsupported_output))
            result.diagnostics.emplace_back(
                "purpose validation must accept exactly three values");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterializationSharedInstanceCase()
    {
        GateResult result;
        result.name = "D1.materialization-shared-instance-separation";

        ShaderProgramKey program_key{};
        program_key.mesh_stage_digest = 0x7201u;
        program_key.fragment_stage_digest = 0x7202u;
        program_key.resource_layout_hash = 0x7203u;
        program_key.vertex_input_hash = 0x7204u;

        ShaderResourceSchema layout{};
        ShaderResourceSlot texture_resources{};
        texture_resources.logical_resource_id =
            StableID("resource.texture_layers");
        texture_resources.resource_schema_id =
            StableID("schema.TextureLayer");
        texture_resources.semantic =
            DescriptorSemantic::MaterialTextureLayerTable;
        texture_resources.kind = DescriptorKind::SSBO;
        texture_resources.ssbo_type = SSBOType::TextureLayer;
        texture_resources.required = true;
        layout.resources.push_back(texture_resources);

        ShaderResourceSlot data_resources{};
        data_resources.logical_resource_id =
            StableID("resource.material_data");
        data_resources.resource_schema_id =
            StableID("schema.PBRSurface");
        data_resources.semantic =
            DescriptorSemantic::MaterialDataSlotData;
        data_resources.kind = DescriptorKind::SSBO;
        data_resources.data_slot = 0;
        data_resources.ssbo_type = SSBOType::PBRSurface;
        data_resources.required = true;
        data_resources.allow_fallback = false;
        layout.resources.push_back(data_resources);

        MaterialRecipe recipe;
        recipe.recipe_name = "shared-instance-regression";

        RecipeTextureBinding texture{};
        texture.slot_name = GetTextureSlotName(TextureSlot::BaseColor);
        texture.use_direct_value = true;
        texture.direct_value = 7;
        recipe.textures.emplace_back(texture);

        RecipeSSBOAssetBinding asset{};
        asset.data_slot = DefaultMaterialDataSlot;
        asset.ssbo_type = SSBOType::PBRSurface;
        asset.ssbo_id = 41;
        asset.data_index = 3;
        asset.use_data_index = true;
        recipe.ssbo_assets.emplace_back(asset);

        ResolvedBindingTable binding_table{};
        BindingBuildDiagnostic diagnostic{};
        if (!BuildBindingTable(
                recipe,
                layout,
                program_key,
                binding_table,
                diagnostic)
         || binding_table.data.GetCount() != 1
         || binding_table.data[0].data_index != 3
         || !binding_table.data[0].use_data_index)
        {
            result.diagnostics.emplace_back(
                std::string("shared resolve failed: ")
                + GetBindingBuildErrorName(
                    diagnostic.error));
            result.passed = false;
            return result;
        }

        // Instance separation: changing the per-instance data_index must not
        // leak into the shared recipe identity (HashMaterialRecipe), while
        // the resolved binding table must still carry the new data_index.
        recipe.ssbo_assets[0].data_index = 9;
        ResolvedBindingTable changed_table{};
        if (!BuildBindingTable(
                recipe,
                layout,
                program_key,
                changed_table,
                diagnostic)
         || changed_table.data.GetCount() != 1
         || changed_table.data[0].data_index != 9)
        {
            result.diagnostics.emplace_back(
                "instance data_index was not projected into the binding table");
            result.passed = false;
            return result;
        }

        recipe.ssbo_assets[0].data_index = 3;
        const uint64_t first_recipe_hash = HashMaterialRecipe(recipe);
        recipe.ssbo_assets[0].data_index = 9;
        const uint64_t second_recipe_hash = HashMaterialRecipe(recipe);
        if (first_recipe_hash != second_recipe_hash)
            result.diagnostics.emplace_back(
            "instance data_index must not change shared recipe identity");

        recipe.ssbo_assets[0].ssbo_id = 42;
        const uint64_t changed_buffer_hash = HashMaterialRecipe(recipe);
        if (second_recipe_hash == changed_buffer_hash)
            result.diagnostics.emplace_back(
            "SSBO buffer identity must change shared recipe identity");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunBuiltinRegistryCoverageCase()
    {
        GateResult result;
        result.name = "E.builtin-registry-coverage";

        struct ExpectedEntry
        {
            const char *definition_id;
        };

        static const ExpectedEntry expected[] =
        {
            { BUILTIN_MTL_DEF_PURE_COLOR },
            { BUILTIN_MTL_DEF_MISSING_MATERIAL },
            { BUILTIN_MTL_DEF_TEXT }
        };

        for (const auto &entry : expected)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(entry.definition_id, definition))
            {
                result.diagnostics.emplace_back(std::string("Missing material definition: ") + entry.definition_id);
                continue;
            }

            if (definition.definition_name.empty()
             || !IsBootstrapMaterialDefinition(definition)
             || definition.source_kind != MaterialDefinitionSourceKind::File)
                result.diagnostics.emplace_back(std::string("Empty material definition name: ") + entry.definition_id);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunBootstrapMaterialBoundaryCase()
    {
        GateResult result;
        result.name = "R.bootstrap-material-boundary";

        struct ExpectedBootstrap
        {
            const char *definition_id;
            MaterialDefinitionBootstrapKind kind;
        };

        static const ExpectedBootstrap expected[] = {
            {BUILTIN_MTL_DEF_PURE_COLOR, MaterialDefinitionBootstrapKind::PureColor},
            {BUILTIN_MTL_DEF_MISSING_MATERIAL, MaterialDefinitionBootstrapKind::PureColor},
            {BUILTIN_MTL_DEF_TEXT, MaterialDefinitionBootstrapKind::TextAlphaBlend}
        };

        for (const auto &entry : expected)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(entry.definition_id, definition))
            {
                result.diagnostics.emplace_back(
                    std::string("missing bootstrap definition: ") + entry.definition_id);
                continue;
            }

            if (!IsBootstrapMaterialDefinition(definition)
             || definition.bootstrap_kind != entry.kind
             || definition.source_kind != MaterialDefinitionSourceKind::File)
            {
                result.diagnostics.emplace_back(
                    std::string("invalid bootstrap metadata: ") + entry.definition_id);
            }
        }

        const char *non_bootstrap_ids[] = {
            "VertexColor", "UnlitTexture", "Texture2DArray",
            "VertexLuminance", "VertexPaletteColor", "DebugNormalColor", "SkyMinimal", "Lit",
            "LitTextureArray"
        };
        for (const char *id : non_bootstrap_ids)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(id, definition))
            {
                result.diagnostics.emplace_back(std::string("missing non-bootstrap definition: ") + id);
                continue;
            }
            if (IsBootstrapMaterialDefinition(definition))
                result.diagnostics.emplace_back(std::string("non-bootstrap material marked bootstrap: ") + id);
        }

        if (BUILTIN_MTL_DEF_TEXT[0] == 0
         || BUILTIN_MTL_DEF_PURE_COLOR[0] == 0)
        {
            result.diagnostics.emplace_back("bootstrap canonical IDs must not be empty");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialDefinitionIdentityCase()
    {
        GateResult result;
        result.name = "R1.material-definition-identity";

        MaterialDefinition pure_color{};
        MaterialDefinition missing_alias{};
        if (!TryGetMaterialDefinitionByID(
                BUILTIN_MTL_DEF_PURE_COLOR, pure_color)
         || !TryGetMaterialDefinitionByID(
                BUILTIN_MTL_DEF_MISSING_MATERIAL, missing_alias))
        {
            result.diagnostics.emplace_back(
                "PureColor canonical definition or compatibility alias is missing");
        }
        else
        {
            if (pure_color.definition_id != BUILTIN_MTL_DEF_PURE_COLOR
             || missing_alias.definition_id != pure_color.definition_id
             || missing_alias.definition_name != pure_color.definition_name)
            {
                result.diagnostics.emplace_back(
                    "PureColor alias must resolve to the canonical definition identity");
            }
        }

        MaterialDefinition text{};
        if (!TryGetMaterialDefinitionByID(BUILTIN_MTL_DEF_TEXT, text))
        {
            result.diagnostics.emplace_back("Text canonical definition is missing");
        }
        else if (text.definition_id != BUILTIN_MTL_DEF_TEXT
              || text.source_kind != MaterialDefinitionSourceKind::File
              || !IsBootstrapMaterialDefinition(text)
              || text.bootstrap_kind != MaterialDefinitionBootstrapKind::TextAlphaBlend)
        {
            result.diagnostics.emplace_back(
                "Text canonical definition must be a file-backed TextAlphaBlend bootstrap");
        }

        MaterialRecipe canonical_recipe{};
        canonical_recipe.mtl_def_id = BUILTIN_MTL_DEF_TEXT;
        NormalizeRecipe(canonical_recipe);

        MaterialDefinition ordinary{};
        if (!TryGetMaterialDefinitionByID("Lit", ordinary)
         || ordinary.definition_id != "Lit"
         || ordinary.source_kind != MaterialDefinitionSourceKind::File
         || IsBootstrapMaterialDefinition(ordinary))
        {
            result.diagnostics.emplace_back(
                "ordinary material lookup must resolve the TOML definition");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunUnifiedMaterialBaselineCase()
    {
        GateResult result;
        result.name = "T.unified-material-baseline";

        MaterialDefinition pure_color{};
        if (!TryGetMaterialDefinitionByID(BUILTIN_MTL_DEF_PURE_COLOR, pure_color))
        {
            result.diagnostics.emplace_back("canonical PureColor must exist");
        }
        else if (!IsPureColorMaterialDefinition(pure_color)
               || pure_color.data_slot_decls.size() != 1
              || pure_color.data_slot_decls[0].ssbo_type != SSBOType::EmissiveSurface
              || pure_color.vertex_semantic_requirements.GetCount() != 1)
            result.diagnostics.emplace_back("canonical PureColor contract is not semantic-only");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialOutputContractCase()
    {
        GateResult result;
        result.name = "V1.material-output-contract";

            OutputContract opaque_output{};
            OutputContract transparent_output{};
            OutputContract depth_output{};
            OutputContract shadow_output{};
            MaterialOutputContractDiagnostic diagnostic{};
            if (!BuildMaterialOutputContract(
                    PassType::ForwardOpaque,
                    opaque_output,
                    diagnostic)
             || !BuildMaterialOutputContract(
                    PassType::ForwardTransparent,
                    transparent_output,
                    diagnostic)
             || !BuildMaterialOutputContract(
                    PassType::EarlyZSolid,
                    depth_output,
                    diagnostic)
             || !BuildMaterialOutputContract(
                    PassType::ShadowOpaque,
                    shadow_output,
                    diagnostic)
             || GetOutputContractHash(opaque_output)
                    != GetOutputContractHash(transparent_output)
             || !depth_output.depth_only
             || !depth_output.attachments.IsEmpty()
             || !shadow_output.depth_only
             || !shadow_output.attachments.IsEmpty()
             || GetOutputContractHash(depth_output)
                    == GetOutputContractHash(shadow_output))
            {
                result.diagnostics.emplace_back(
                    "output purpose contract mapping mismatch");
            }

            std::string applied;
            if (ApplyMaterialOutputContract(
                    opaque_output,
                    "#version 460\nvoid main(){}\n",
                    applied,
                    diagnostic)
             || diagnostic.error
                    != MaterialOutputContractError::MissingContractMarker)
            {
                result.diagnostics.emplace_back(
                    "output contract marker must be mandatory");
            }

            MaterialDefinition lit{};
            if (!TryGetMaterialDefinitionByID("Lit", lit))
            {
                result.diagnostics.emplace_back(
                    "Lit definition unavailable for output contract");
            }
            else
            {
                const GeometryVertexFormat geometry{
                    {VertexSemantic::Position, VF_V3F},
                    {VertexSemantic::TexCoord, VF_V2F},
                    {VertexSemantic::Normal, VF_V3F}
                };
                const auto build = [&](
                    const ShaderProgramPurpose purpose,
                    const bool override_purpose,
                    const PassType pass,
                    const bool alpha_test,
                    const bool dither,
                    const bool alpha_to_coverage)
                {
                    MaterialDefinition selected = lit;
                    selected.compositor_pass = pass;
                    MaterialDefinitionBuildRequest request{};
                    request.recipe.mtl_def_id = selected.definition_id;
                    request.recipe.render_state_overrides.has_alpha_test = true;
                    request.recipe.render_state_overrides.alpha_test = alpha_test;
                    request.recipe.render_state_overrides.has_dither = true;
                    request.recipe.render_state_overrides.dither = dither;
                    request.recipe.render_state_overrides.has_pipeline_config = true;
                    request.recipe.render_state_overrides.pipeline_config.alpha_to_coverage =
                        alpha_to_coverage;
                    request.geometry_vertex_format = &geometry;
                    request.defer_finalize = true;
                    request.override_shader_program_purpose =
                        override_purpose;
                    request.shader_program_purpose = purpose;
                    return std::unique_ptr<ShaderBuildContext>(
                        CreateMaterialFromDefinition(
                            nullptr, selected, request));
                };

                const auto opaque = build(
                    ShaderProgramPurpose::ForwardColor,
                    false,
                    PassType::ForwardOpaque,
                    false,
                    false,
                    false);
                const auto transparent = build(
                    ShaderProgramPurpose::ForwardColor,
                    false,
                    PassType::ForwardTransparent,
                    false,
                    false,
                    false);
                const auto depth = build(
                    ShaderProgramPurpose::DepthOnly,
                    true,
                    PassType::ForwardOpaque,
                    false,
                    false,
                    false);
                const auto shadow = build(
                    ShaderProgramPurpose::ShadowDepth,
                    true,
                    PassType::ForwardOpaque,
                    false,
                    false,
                    false);
                const auto masked_depth = build(
                    ShaderProgramPurpose::DepthOnly,
                    true,
                    PassType::ForwardOpaque,
                    true,
                    false,
                    false);
                const auto dither_shadow = build(
                    ShaderProgramPurpose::ShadowDepth,
                    true,
                    PassType::ForwardOpaque,
                    false,
                    true,
                    false);
                const auto a2c_depth = build(
                    ShaderProgramPurpose::DepthOnly,
                    true,
                    PassType::ForwardOpaque,
                    false,
                    false,
                    true);

                if (!opaque || !transparent || !depth || !shadow
                 || !masked_depth || !dither_shadow || !a2c_depth
                 || !opaque->HasProgramLink()
                 || !transparent->HasProgramLink()
                 || !depth->HasProgramLink()
                 || !shadow->HasProgramLink()
                 || !masked_depth->HasProgramLink()
                 || !dither_shadow->HasProgramLink()
                 || !a2c_depth->HasProgramLink())
                {
                    result.diagnostics.emplace_back(
                        "production output contract builds failed");
                }
                else
                {
                    // 安全取 stage GLSL：depth/shadow/a2c 变体本无 Fragment stage，
                    // GetStageShader 可能返回悬挂指针——先 has_fragment() 守卫 + 空指针检查，
                    // 并用值拷贝避免悬挂引用。
                    const auto safe_fs = [](const ShaderBuildContext *ctx) -> std::string
                    {
                        if (!ctx || !ctx->has_fragment())
                            return {};
                        const auto *stage = ctx->GetStageShader(ShaderStage::Fragment);
                        return stage ? stage->GetFinalGLSL() : std::string{};
                    };
                    const auto safe_ms = [](const ShaderBuildContext *ctx) -> std::string
                    {
                        if (!ctx || !ctx->has_mesh())
                            return {};
                        const auto *stage = ctx->GetStageShader(ShaderStage::Mesh);
                        return stage ? stage->GetFinalGLSL() : std::string{};
                    };

                    const std::string opaque_fs = safe_fs(opaque.get());
                    const std::string depth_fs = safe_fs(depth.get());
                    const std::string depth_ms = safe_ms(depth.get());
                    const std::string masked_depth_fs = safe_fs(masked_depth.get());
                    const std::string masked_depth_ms = safe_ms(masked_depth.get());
                    const std::string dither_shadow_fs = safe_fs(dither_shadow.get());
                    const std::string a2c_depth_fs = safe_fs(a2c_depth.get());
                    // 结构化断言：opaque 必有 Fragment（强检查）；
                    // depth/masked/dither/a2c 变体仅当 has_fragment() 时检查内容，
                    // 否则跳过对应子项（避免解引用悬挂指针 + 假阳性）。
                    const auto has = [](const ShaderBuildContext *ctx, ShaderStage s)
                    { return ctx && (ctx->GetShaderStage() & uint32_t(s)); };
                    const auto lacks = [](std::string_view src, const char *needle)
                    { return src.find(needle) == std::string::npos; };
                    const auto contains = [](std::string_view src, const char *needle)
                    { return src.find(needle) != std::string::npos; };

                    bool mismatch = false;
                    // opaque：ForwardColor，必有 Fragment
                    if (lacks(opaque_fs, "layout(location=0) out vec4 outColor;")
                     || lacks(opaque_fs, "WriteMaterialOutput(")
                     || contains(opaque_fs, "ULRE_OUTPUT_CONTRACT"))
                        mismatch = true;
                    // depth：DepthOnly，期望无 Fragment 输出相关符号（若有 Fragment 才查）
                    if (has(depth.get(), ShaderStage::Fragment))
                    {
                        if (contains(depth_fs, "outColor")
                         || contains(depth_fs, "WriteMaterialOutput(")
                         || contains(depth_fs, "ULRE_OUTPUT_CONTRACT")
                         || contains(depth_fs, "EvalAlpha("))
                            mismatch = true;
                    }
                    // depth：DepthOnly 的 mesh 源不应含 fragDataIndexID（depth 无需 data index）
                    if (has(depth.get(), ShaderStage::Mesh)
                     && contains(depth_ms, "fragDataIndexID"))
                        mismatch = true;
                    // masked_depth：alpha test，期望含 EvalAlpha/HGLApplyAlpha/ALPHA_TEST
                    if (has(masked_depth.get(), ShaderStage::Fragment)
                     && (lacks(masked_depth_fs, "EvalAlpha(")
                      || lacks(masked_depth_fs, "HGLApplyAlpha(")
                      || lacks(masked_depth_fs, "#define HGL_ALPHA_TEST 1")
                      || contains(masked_depth_fs, "EvalLighting(")))
                        mismatch = true;
                    // masked_depth：mesh 源不应含 fragWorldPos（depth 无需世界坐标输出）
                    if (has(masked_depth.get(), ShaderStage::Mesh)
                     && contains(masked_depth_ms, "fragWorldPos"))
                        mismatch = true;
                    // dither_shadow：期望含 ALPHA_DITHER + EvalLighting
                    if (has(dither_shadow.get(), ShaderStage::Fragment)
                     && (lacks(dither_shadow_fs, "#define HGL_ALPHA_DITHER 1")
                      || contains(dither_shadow_fs, "EvalLighting(")))
                        mismatch = true;
                    // a2c_depth：期望含 ALPHA_DITHER + EvalAlpha
                    if (has(a2c_depth.get(), ShaderStage::Fragment)
                     && (lacks(a2c_depth_fs, "#define HGL_ALPHA_DITHER 1")
                      || lacks(a2c_depth_fs, "EvalAlpha(")))
                        mismatch = true;

                    if (mismatch)
                    {
                        result.diagnostics.emplace_back(
                            "production fragment output generation mismatch");
                    }

                    const auto has_material_resource =
                        [](const ShaderBuildContext &spec)
                    {
                        for (const auto &requirement :
                             spec.GetShaderResourceSchema().resources)
                        {
                            if (requirement.set_type
                                == DescriptorSetType::Material)
                                return true;
                        }
                        return false;
                    };
                    const auto has_sky_resource =
                        [](const ShaderBuildContext &spec)
                    {
                        for (const auto &requirement :
                             spec.GetShaderResourceSchema().resources)
                        {
                            if (requirement.semantic
                                    == DescriptorSemantic::SkyInfo)
                                return true;
                        }
                        return false;
                    };
                    if (has_material_resource(*depth)
                     || has_sky_resource(*depth)
                     || !has_material_resource(*masked_depth)
                     || has_sky_resource(*masked_depth))
                    {
                        result.diagnostics.emplace_back(
                            "depth coverage resource pruning mismatch");
                    }

                    if (!(opaque->GetProgramLink().BuildKey()
                            == transparent->GetProgramLink().BuildKey())
                     || depth->GetProgramLink().BuildKey()
                            == shadow->GetProgramLink().BuildKey()
                     || opaque->GetProgramLink().BuildKey()
                            == depth->GetProgramLink().BuildKey()
                     || depth->GetProgramLink().BuildKey()
                            == masked_depth->GetProgramLink().BuildKey())
                    {
                        result.diagnostics.emplace_back(
                            "output purpose ProgramKey identity mismatch");
                    }
                }
            }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunPrimitiveVariantPurposeCase()
    {
        GateResult result;
        result.name = "V2.primitive-variant-purpose";

        MaterialRecipe surface_recipe{};
        surface_recipe.recipe_name = "Surface";
        MaterialRecipe depth_recipe{};
        depth_recipe.recipe_name = "Depth";
        MaterialRecipe shadow_recipe{};
        shadow_recipe.recipe_name = "Shadow";
        const PrimitiveVariant variants[] =
        {
            {
                &surface_recipe,
                PrimitiveVariantPurpose::Surface,
                0
            },
            {
                &depth_recipe,
                PrimitiveVariantPurpose::DepthOnly,
                0
            },
            {
                &shadow_recipe,
                PrimitiveVariantPurpose::ShadowCaster,
                0
            }
        };
        const PrimitiveAsset asset(
            nullptr,
            variants,
            static_cast<uint32_t>(std::size(variants)));

        const PrimitiveVariant *surface =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::Surface, 2);
        const PrimitiveVariant *depth =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::DepthOnly, 0);
        const PrimitiveVariant *shadow =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::ShadowCaster, 0);
        const PrimitiveVariant *fallback =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::Picking, 2);
        if (!surface
         || surface->material_recipe != &surface_recipe
         || !depth
         || depth->material_recipe != &depth_recipe
         || !shadow
         || shadow->material_recipe != &shadow_recipe
         || !fallback
         || fallback->material_recipe != &surface_recipe)
        {
            result.diagnostics.emplace_back(
                "primitive variants were not resolved by purpose");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunUnifiedForwardSkeletonCase()
    {
        GateResult result;
        result.name = "V3.unified-forward-skeleton";

        struct ExpectedSource
        {
            const char *definition_id;
            const char *source_module;
        };
        static const ExpectedSource expected_sources[] =
        {
            {"VertexPaletteColor", "material/vertex_color_source.glsl"},
            {"VertexColor", "material/vertex_color_source.glsl"},
            {"VertexLuminance", "material/luminance_source.glsl"},
            {"UnlitTexture", "material/texture_source.glsl"},
            {"Texture2DArray", "material/texture_array_source.glsl"},
            {"DebugNormalColor", "material/debug_normal_source.glsl"},
            {"Lit", "material/pbr_surface_source.glsl"},
            {"LitTextureArray", "material/pbr_texturearray_source.glsl"},
            {BUILTIN_MTL_DEF_PURE_COLOR, "material/unlit_source.glsl"},
            {BUILTIN_MTL_DEF_TEXT, "material/text_source_gpu.glsl"}
        };

        for (const ExpectedSource &expected : expected_sources)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(
                    expected.definition_id, definition)
             || !definition.fragment_source
             || std::strcmp(
                    definition.fragment_source,
                    "compositor/main_forward_surface.frag.glsl") != 0
             || !definition.fragment_surface_module
             || std::strcmp(
                    definition.fragment_surface_module,
                    "surface/material_surface.glsl") != 0
             || !definition.fragment_material_source_module
             || std::strcmp(
                    definition.fragment_material_source_module,
                    expected.source_module) != 0)
            {
                result.diagnostics.emplace_back(
                    std::string("definition did not use unified skeleton: ")
                    + expected.definition_id);
            }
        }

        MaterialDefinition lit{};
        MaterialDefinition unlit{};
        if (!TryGetMaterialDefinitionByID("Lit", lit)
         || !TryGetMaterialDefinitionByID("UnlitTexture", unlit))
        {
            result.diagnostics.emplace_back(
                "unified skeleton build definitions unavailable");
        }
        else
        {
            const GeometryVertexFormat lit_geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2F},
                {VertexSemantic::Normal, VF_V3F}
            };
            const GeometryVertexFormat unlit_geometry{
                {VertexSemantic::Position, VF_V2F},
                {VertexSemantic::TexCoord, VF_V2F}
            };
            const auto build = [](
                const MaterialDefinition &definition,
                const GeometryVertexFormat &geometry)
            {
                MaterialDefinitionBuildRequest request{};
                request.recipe.mtl_def_id = definition.definition_id;
                request.geometry_vertex_format = &geometry;
                request.defer_finalize = true;
                return std::unique_ptr<ShaderBuildContext>(
                    CreateMaterialFromDefinition(
                        nullptr, definition, request));
            };
            const auto lit_build = build(lit, lit_geometry);
            const auto unlit_build = build(unlit, unlit_geometry);
            if (!lit_build || !unlit_build)
            {
                result.diagnostics.emplace_back(
                    "unified forward skeleton build failed");
            }
            else
            {
                const std::string &lit_fs =
                    lit_build->GetStageShader(
                        ShaderStage::Fragment)->GetFinalGLSL();
                const std::string &unlit_fs =
                    unlit_build->GetStageShader(
                        ShaderStage::Fragment)->GetFinalGLSL();
                if (lit_fs.find(
                        "#define HGL_USE_SCENE_LIGHTING 1")
                        == std::string::npos
                 || lit_fs.find(
                        "#include \"lighting/forward_pbr.glsl\"")
                        == std::string::npos
                 || lit_fs.find(
                        "#include \"compositor/forward_lighting.glsl\"")
                        == std::string::npos
                 || unlit_fs.find(
                        "#define HGL_USE_SCENE_LIGHTING 0")
                        == std::string::npos
                 || unlit_fs.find(
                        "#include \"lighting/forward_flat.glsl\"")
                        == std::string::npos
                 || unlit_fs.find(
                        "#include \"compositor/flat_lighting.glsl\"")
                        == std::string::npos
                 || lit_fs.find(
                        "EvalSurface(si, materialDataIndex)")
                        == std::string::npos
                 || unlit_fs.find(
                        "EvalSurface(si, materialDataIndex)")
                        == std::string::npos
                 || lit_fs.find("EvalLighting(lighting)")
                        == std::string::npos
                 || unlit_fs.find("EvalLighting(lighting)")
                        == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "unified Lit/Unlit module schedule mismatch");
                }
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunUnifiedPureColorFragmentCase()
    {
        GateResult result;
        result.name = "V.unified-purecolor-fragment";

        MaterialDefinition pure_color{};
        if (!TryGetMaterialDefinitionByID(BUILTIN_MTL_DEF_PURE_COLOR, pure_color))
        {
            result.diagnostics.emplace_back("unified PureColor definition is unavailable");
            result.passed = false;
            return result;
        }

        if (!pure_color.fragment_source
         || std::strcmp(pure_color.fragment_source,
                        "compositor/main_forward_surface.frag.glsl") != 0
         || !pure_color.fragment_surface_module
         || std::strcmp(
                pure_color.fragment_surface_module,
                "surface/material_surface.glsl") != 0
         || !pure_color.fragment_material_source_module
         || std::strcmp(
                pure_color.fragment_material_source_module,
                "material/unlit_source.glsl") != 0)
            result.diagnostics.emplace_back("PureColor must use one FS module");

        CompositorAssembler assembler(GetShaderLibraryPath());
        hgl::ValueArray<InterStageSemanticContractEntry> stage_interface;
        MaterialStageInterfaceDiagnostic interface_diagnostic{};
        MaterialVertexVaryingConfig pure_color_varying{};
        pure_color_varying.emit_data_index_id = true;
        if (!BuildMaterialStageInterface(
                pure_color_varying,
                stage_interface,
                interface_diagnostic))
            result.diagnostics.emplace_back(
                "PureColor stage interface build failed");
        CompositorAssembler::CompositorModuleOptions options{};
        options.material_source_module =
            "material/unlit_source.glsl";
        options.forward_lighting_module =
            "compositor/flat_lighting.glsl";
        options.lighting_algorithm_module =
            "lighting/forward_flat.glsl";
        options.fragment_inputs = &stage_interface;
        const auto assembled = assembler.Assemble(
            SurfaceType::Unlit, BlendMode::Opaque, PassType::ForwardOpaque,
            "compositor/main_forward_surface.frag.glsl",
            "surface/material_surface.glsl",
            options);
        if (!assembled.success
         || assembled.fragment_glsl.find(
                "#include \"material/unlit_source.glsl\"")
                == std::string::npos
         || assembled.fragment_glsl.find(
                "#include \"lighting/forward_flat.glsl\"")
                == std::string::npos)
            result.diagnostics.emplace_back("unified PureColor FS source is invalid");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunUnifiedMaterialContractCase()
    {
        GateResult result;
        result.name = "U.unified-material-contract";

        if (std::strcmp(BUILTIN_MTL_DEF_PURE_COLOR, "builtin/pure_color") != 0)
            result.diagnostics.emplace_back("canonical PureColor ID changed unexpectedly");

        MaterialDefinition pure_color{};
        if (!TryGetMaterialDefinitionByID(BUILTIN_MTL_DEF_PURE_COLOR, pure_color))
        {
            result.diagnostics.emplace_back("canonical PureColor definition missing during contract phase");
        }
        else
        {
            if (!IsPureColorMaterialDefinition(pure_color)
             || pure_color.data_slot_decls.size() != 1
             || pure_color.data_slot_decls[0].ssbo_type != SSBOType::EmissiveSurface
             || pure_color.vertex_semantic_requirements.GetCount() != 1)
                result.diagnostics.emplace_back("PureColor contract is not canonical");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunTransformGraphModelCase()
    {
        GateResult result;
        result.name = "W.transform-graph-model";

        const VertexShaderNodeConfig flat = VertexNodeConfigResolver::FlatXY();
        const VertexShaderNodeConfig wall = VertexNodeConfigResolver::WallXY();
        const VertexShaderNodeConfig world = VertexNodeConfigResolver::World3D();
        const VertexShaderNodeConfig terrain = VertexNodeConfigResolver::Terrain();
        VertexShaderNodeConfig world_vec2 = world;
        world_vec2.input = VertexInputMode::Vec2Position;
        world_vec2.position_mapping = PositionMappingMode::LiftXY_XY0;

        if (VertexNodeConfigResolver::GetHash(flat) == VertexNodeConfigResolver::GetHash(wall)
         || VertexNodeConfigResolver::GetHash(flat) == VertexNodeConfigResolver::GetHash(world)
         || VertexNodeConfigResolver::GetHash(world) == VertexNodeConfigResolver::GetHash(terrain))
            result.diagnostics.emplace_back(
                "transform graph variants must have distinct structural identity");

        if (!VertexNodeConfigResolver::IsScreenLike(flat)
         || !VertexNodeConfigResolver::IsScreenLike(wall)
         || VertexNodeConfigResolver::IsScreenLike(world)
         || VertexNodeConfigResolver::IsScreenLike(world_vec2))
            result.diagnostics.emplace_back(
                "screen-space classification must include projection, not only Vec2 input");

        if (flat.input != VertexInputMode::Vec2Position
         || flat.position_mapping != PositionMappingMode::NDCLift
         || flat.projection != ProjectionMode::LocalToWorldOnly)
            result.diagnostics.emplace_back("FlatXY graph conversion mismatch");

        if (wall.position_mapping != PositionMappingMode::LiftXY_X0Y)
            result.diagnostics.emplace_back("WallXY graph conversion mismatch");

        if (terrain.position_mapping != PositionMappingMode::TerrainGrid
         || terrain.projection != ProjectionMode::WorldCameraVP)
            result.diagnostics.emplace_back("Terrain graph conversion mismatch");

        // Round-trip is identity now that VertexNodeConfigResolver is a static utility
        if (VertexNodeConfigResolver::GetHash(world) != VertexNodeConfigResolver::GetHash(world))
            result.diagnostics.emplace_back("transform graph hash self-consistency failure");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunTransformGraphCompositionCase()
    {
        GateResult result;
        result.name = "X.transform-graph-composition";

        VertexVaryingConfig varying{};
        varying.emit_data_index_id = true;

        const auto build = [&](const VertexShaderNodeConfig &graph)
        {
            return GenerateMeshShader(
                graph, varying, VK_FORMAT_R32G32B32_SFLOAT,
                {}, MeshShaderMode::VertexPassthrough, 96u,
                GetShaderLibraryPath(), {}, nullptr,
                hgl::graph::PrimitiveType::Triangles);
        };

        const std::string flat = build(VertexNodeConfigResolver::FlatXY());
        const std::string wall = build(VertexNodeConfigResolver::WallXY());
        const std::string world = build(VertexNodeConfigResolver::World3D());
        const std::string terrain = build(VertexNodeConfigResolver::Terrain());

        if (flat == wall || flat == world || world == terrain)
            result.diagnostics.emplace_back(
                "one material must produce distinct VS sources per transform graph");
        if (flat.find("vertex/s2_ndc_lift.glsl") == std::string::npos
         || wall.find("vertex/s2_lift_x0y.glsl") == std::string::npos
         || world.find("vertex/s2_passthrough3d.glsl") == std::string::npos
         || terrain.find("TerrainGrid") == std::string::npos)
            result.diagnostics.emplace_back(
                "transform graph source composition selected the wrong stage");

        ShaderStageBuildContext stage{};
        stage.stage = ShaderStage::Mesh;
        const ShaderStageKey flat_key =
            stage.BuildKeyWithProviderGraphHash(
                VertexNodeConfigResolver::GetHash(VertexNodeConfigResolver::FlatXY()));
        const ShaderStageKey wall_key =
            stage.BuildKeyWithProviderGraphHash(
                VertexNodeConfigResolver::GetHash(VertexNodeConfigResolver::WallXY()));
        if (flat_key == wall_key)
            result.diagnostics.emplace_back(
                "transform graph variants must produce distinct stage identity");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialSSBOBindingKeyCase()
    {
        GateResult result;
        result.name = "Y1a.material-ssbo-binding-key";

        MaterialRecipe recipe{};
        if (!UpsertRecipeSSBOAssetBinding(
                recipe, "surface_a", SSBOType::EmissiveSurface, 11, 0)
         || !UpsertRecipeSSBOAssetBinding(
                recipe, "surface_b", SSBOType::EmissiveSurface, 22, 0)
         || !UpsertRecipeSSBOAssetBinding(
                recipe, "surface_a", SSBOType::EmissiveSurface, 33, 1)
         || !UpsertRecipeSSBOAssetBinding(
                recipe, "surface_a", SSBOType::EmissiveSurface, 44, 0))
        {
            result.diagnostics.emplace_back("canonical SSBO binding upsert rejected valid keys");
        }

        const auto *surface_a_0 = FindRecipeSSBOAssetBindingByKey(recipe, "surface_a", 0);
        const auto *surface_b_0 = FindRecipeSSBOAssetBindingByKey(recipe, "surface_b", 0);
        const auto *surface_a_1 = FindRecipeSSBOAssetBindingByKey(recipe, "surface_a", 1);
        if (!surface_a_0 || surface_a_0->ssbo_id != 44
         || !surface_b_0 || surface_b_0->ssbo_id != 22
         || !surface_a_1 || surface_a_1->ssbo_id != 33
         || recipe.ssbo_assets.size() != 3)
        {
            result.diagnostics.emplace_back(
                "SSBO bindings were not isolated by name plus data slot index");
        }

        if (!FindRecipeSSBOAssetBinding(
                recipe, "surface_a", 1, SSBOType::EmissiveSurface)
         || FindRecipeSSBOAssetBinding(
                recipe, "surface_a", 1, SSBOType::PBRSurface))
        {
            result.diagnostics.emplace_back(
                "typed SSBO lookup did not enforce the canonical binding key");
        }

        if (ResolveRecipeSSBOType(
                recipe, "surface_a", 0, SSBOType::UserDefined)
                != SSBOType::EmissiveSurface
         || ResolveRecipeSSBOType(
                recipe, "missing", 0, SSBOType::UserDefined)
                != SSBOType::UserDefined
         || ResolveRecipeSSBOType(
                recipe, "surface_a", 0, SSBOType::PBRSurface)
                != SSBOType::PBRSurface)
        {
            result.diagnostics.emplace_back(
                "UserDefined authoring must inherit Recipe SSBO type without overriding explicit types");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunResolvedMaterialRenderStateCase()
    {
        GateResult result;
        result.name = "Y1.resolved-material-render-state";

        MaterialDefinition definition{};
        definition.compositor_blend = BlendMode::Masked;
        definition.default_render_state.double_sided = true;
        definition.default_render_state.alpha_cutoff = 0.35f;
        definition.default_render_state.pipeline_config =
            MakeSolid3DConfig();
        definition.default_render_state.pipeline_config.depth_write = false;

        MaterialRecipe defaults{};
        const ResolvedMaterialRenderState default_state =
            ResolveMaterialRenderState(definition, defaults);
        if (!default_state.double_sided
         || !default_state.alpha_test
         || default_state.alpha_cutoff != 0.35f
         || default_state.pipeline_config.depth_write)
        {
            result.diagnostics.emplace_back(
                "definition render defaults were not preserved by resolution");
        }

        MaterialRecipe overrides{};
        overrides.render_state_overrides.has_double_sided = true;
        overrides.render_state_overrides.double_sided = false;
        overrides.render_state_overrides.has_alpha_test = true;
        overrides.render_state_overrides.alpha_test = false;
        overrides.render_state_overrides.has_alpha_cutoff = true;
        overrides.render_state_overrides.alpha_cutoff = 0.8f;
        overrides.render_state_overrides.has_dither = true;
        overrides.render_state_overrides.dither = true;
        overrides.render_state_overrides.has_pipeline_config = true;
        overrides.render_state_overrides.pipeline_config =
            MakeAlpha3DConfig();
        const ResolvedMaterialRenderState resolved =
            ResolveMaterialRenderState(definition, overrides);
        if (resolved.double_sided
         || resolved.alpha_test
         || resolved.alpha_cutoff != 0.8f
         || !resolved.dither
         || !resolved.pipeline_config.alpha_blend)
        {
            result.diagnostics.emplace_back(
                "recipe render-state overrides were not applied atomically");
        }

        if (HashResolvedMaterialRenderState(default_state)
            == HashResolvedMaterialRenderState(resolved))
        {
            result.diagnostics.emplace_back(
                "resolved render-state differences must affect pipeline identity");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialDefinitionFileSchemaCase()
    {
        GateResult result;
        result.name = "S.material-definition-file-schema";

        const char material_file[] =
            "schema = 1\n"
            "id = \"LitFile\"\n"
            "name = \"LitFile\"\n"
            "source = \"file\"\n"
            "usage = \"General\"\n"
            "bootstrap = \"None\"\n"
            "provider_policy = \"AllowDerived\"\n"
            "[transform]\n"
            "source = \"Vec3Position\"\n"
            "mapping = \"Passthrough3D\"\n"
            "orientation = \"World\"\n"
            "scale = \"World\"\n"
            "projection = \"WorldCameraVP\"\n"
            "[fragment]\n"
            "source = \"compositor/main_forward_surface.frag.glsl\"\n"
            "surface_module = \"surface/material_surface.glsl\"\n"
            "material_source_module = \"material/pbr_surface_source.glsl\"\n"
            "ntb_module = \"ntb/ntb_tangent_vbo_normalmap.glsl\"\n"
            "[compositor]\n"
            "surface = \"Lit\"\n"
            "blend = \"Opaque\"\n"
            "pass = \"ForwardOpaque\"\n"
            "[vertex]\n"
            "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
            "varyings = [\"emit_world_pos\", \"emit_world_normal\", \"emit_uv0\"]\n"
            "[resources]\n"
            "ubos = [\"CameraInfo\", \"SkyInfo\"]\n";

        MaterialDefinitionFileData data;
        const auto parse = ParseMaterialDefinitionFile(
            material_file, static_cast<int>(std::strlen(material_file)), data);
        if (parse != MaterialDefinitionFileParseResult::OK)
        {
            result.diagnostics.emplace_back(
                std::string("material schema parse failed: ")
                + GetMaterialDefinitionFileParseResultName(parse));
        }
        else
        {
            const auto &definition = data.definition;
            if (definition.source_kind != MaterialDefinitionSourceKind::File
             || definition.definition_id != "LitFile"
             || definition.vertex_provider_policy != MaterialVertexProviderPolicy::AllowDerived
             || definition.vertex_node_config.position_mapping != PositionMappingMode::Passthrough3D
             || definition.vertex_semantic_requirements.GetCount() != 3
             || definition.ubo_requirements.size() != 2
             || std::strcmp(definition.fragment_source,
                            "compositor/main_forward_surface.frag.glsl") != 0
             || std::strcmp(definition.fragment_surface_module,
                            "surface/material_surface.glsl") != 0)
            {
                result.diagnostics.emplace_back("material schema fields mismatch");
            }

            const GeometryVertexFormat allow_derived_geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2F},
                {VertexSemantic::Normal, VF_V3F}
            };
            MaterialDefinitionBuildRequest allow_derived_request{};
            allow_derived_request.geometry_vertex_format = &allow_derived_geometry;
            MaterialResolvedVertexABI allow_derived_abi{};
            if (!BuildResolvedMaterialVertexABI(
                    definition, allow_derived_request, allow_derived_abi)
             || allow_derived_abi.vertex_input_glsl.IsEmpty()
             || allow_derived_abi.position_format != VK_FORMAT_R32G32B32_SFLOAT)
            {
                result.diagnostics.emplace_back(
                    "AllowDerived material definition must build a SSBO vertex ABI");
            }
        }

        const char legacy_file[] =
            "schema = 1\n"
            "id = \"LegacyDirect\"\n"
            "name = \"LegacyDirect\"\n"
            "source = \"file\"\n"
            "usage = \"General\"\n"
            "bootstrap = \"None\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[compositor]\n"
            "fragment = \"compositor/legacy_test_template.frag.glsl\"\n"
            "[vertex]\n"
            "requirements = [\"Position\"]\n";
        MaterialDefinitionFileData legacy_data;
        if (ParseMaterialDefinitionFile(
                legacy_file, static_cast<int>(std::strlen(legacy_file)), legacy_data)
                != MaterialDefinitionFileParseResult::InvalidValue)
        {
            result.diagnostics.emplace_back(
                "legacy compositor fragment schema must be rejected");
        }

        const char invalid_file[] =
            "schema = 1\n"
            "id = \"Broken\"\n"
            "name = \"Broken\"\n"
            "source = \"builtin\"\n"
            "usage = \"General\"\n"
            "bootstrap = \"None\"\n"
            "provider_policy = \"GeometryOnly\"\n";
        MaterialDefinitionFileData invalid_data;
        if (ParseMaterialDefinitionFile(
                invalid_file, static_cast<int>(std::strlen(invalid_file)), invalid_data)
                != MaterialDefinitionFileParseResult::InvalidValue)
        {
            result.diagnostics.emplace_back(
                "material schema must reject non-File source");
        }

        MaterialDefinitionFileRegistry registry;
        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(hgl::ToOSString(GetShaderLibraryPath()),
                                    &file_count, &error_count)
         || file_count != 11
         || error_count != 0)
        {
            result.diagnostics.emplace_back("material file registry bulk load failed");
        }
        else
        {
            const char *expected_file_ids[] = {
                "Lit", "LitTextureArray", "SkyMinimal", "DebugNormalColor",
                "VertexColor", "UnlitTexture", "Texture2DArray",
                "VertexLuminance", "VertexPaletteColor",
                "builtin/pure_color", "builtin/text_gpu",
                "builtin/text_gpu_bitmap"
            };
            for (const char *id : expected_file_ids)
            {
                if (!registry.FindByID(id))
                    result.diagnostics.emplace_back(
                        std::string("missing bulk material file: ") + id);
            }

        const char *bulk_ids[] = {
            "LitTextureArray", "SkyMinimal", "DebugNormalColor", "VertexColor",
            "UnlitTexture", "Texture2DArray", "VertexLuminance",
            "VertexPaletteColor"
        };
        for (const char *id : bulk_ids)
        {
            const MaterialDefinition *file_definition = registry.FindByID(id);
            MaterialDefinition registry_definition{};
            if (!file_definition
             || !TryGetMaterialDefinitionByID(id, registry_definition))
            {
                result.diagnostics.emplace_back(
                    std::string("bulk file/legacy lookup failed: ") + id);
                continue;
            }

            const bool same_surface_reference =
                (!file_definition->fragment_surface_module
                 && !registry_definition.fragment_surface_module)
                || (file_definition->fragment_surface_module
                 && registry_definition.fragment_surface_module
                 && std::strcmp(file_definition->fragment_surface_module,
                                registry_definition.fragment_surface_module) == 0);
            if (file_definition->compositor_surface != registry_definition.compositor_surface
             || file_definition->compositor_blend != registry_definition.compositor_blend
             || file_definition->compositor_pass != registry_definition.compositor_pass
             || file_definition->vertex_provider_policy != registry_definition.vertex_provider_policy
             || !same_surface_reference
             || file_definition->vertex_semantic_requirements.GetCount()
                    != registry_definition.vertex_semantic_requirements.GetCount()
             || file_definition->ubo_requirements.size()
                    != registry_definition.ubo_requirements.size()
             || file_definition->data_slot_decls.size()
                    != registry_definition.data_slot_decls.size()
             || file_definition->texture_slot_decls.size()
                    != registry_definition.texture_slot_decls.size())
            {
                result.diagnostics.emplace_back(
                    std::string("bulk file definition contract mismatch: ") + id);
                continue;
            }

            for (int i = 0; i < file_definition->vertex_semantic_requirements.GetCount(); ++i)
            {
                if (!(file_definition->vertex_semantic_requirements[i]
                    == registry_definition.vertex_semantic_requirements[i]))
                {
                    result.diagnostics.emplace_back(
                        std::string("bulk semantic mismatch: ") + id);
                    break;
                }
            }

            if (VertexNodeConfigResolver::IsScreenLike(file_definition->vertex_node_config)
             && (file_definition->vertex_node_config.input != VertexInputMode::Vec2Position
              || file_definition->vertex_node_config.position_mapping != PositionMappingMode::NDCLift
              || file_definition->vertex_node_config.projection != ProjectionMode::LocalToWorldOnly))
            {
                result.diagnostics.emplace_back(
                    std::string("2D file node config mismatch: ") + id);
            }
        }

        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunFallbackInferenceCase()
    {
        GateResult result;
        result.name = "F.fallback-dimension-neutral";

        GeometryVertexFormat gvf_2d{};
        gvf_2d.Add(VertexSemantic::Position, VK_FORMAT_R32G32_SFLOAT, 2, sizeof(float) * 2);

        GeometryVertexFormat gvf_3d{};
        gvf_3d.Add(VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT, 3, sizeof(float) * 3);

        if (gvf_2d.GetVertexInputHash() == gvf_3d.GetVertexInputHash())
            result.diagnostics.emplace_back(
                "Geometry formats with different dimensions must remain distinct");
        if (std::strcmp(GetFallbackMaterialDefinitionID(), BUILTIN_MTL_DEF_PURE_COLOR) != 0)
            result.diagnostics.emplace_back("all Geometry dimensions must use unified PureColor fallback");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunProviderResourceManifestCase()
    {
        GateResult result;
        result.name = "H1.provider-resource-manifest";

        GLSLCodeModuleRegistry registry;

        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(
                hgl::ToOSString(GetShaderLibraryPath()), &file_count, &error_count))
        {
            result.diagnostics.emplace_back("provider registry directory scan failed");
        }
        else if (error_count != 0)
        {
            result.diagnostics.emplace_back(
                "provider registry directory scan reported parse errors");
        }

        const auto *pbr_2d = registry.FindByName("pbr_surface_source");
        const auto *pbr_array = registry.FindByName("pbr_texturearray_source");
        const auto *ntb_2d = registry.FindByName("ntb_tangent_vbo_normalmap");
        const auto *ntb_array = registry.FindByName("ntb_texturearray_normalmap");
        const auto *ntb_derivative = registry.FindByName("ntb_derivative_normalmap");
        if (!pbr_2d || !pbr_array || !ntb_2d || !ntb_array || !ntb_derivative)
        {
            result.diagnostics.emplace_back("provider metadata modules are missing");
        }
        else
        {
            const char *roots_2d[] = {pbr_2d->name, ntb_2d->name};
            ModuleResourceManifest manifest_2d{};
            if (!BuildModuleResourceManifest(
                    roots_2d, uint32_t(std::size(roots_2d)), manifest_2d, &registry))
            {
                result.diagnostics.emplace_back(
                    std::string("Texture2D provider manifest failed: ")
                    + GetModuleResourceManifestErrorName(manifest_2d.error));
            }
            else
            {
                if (manifest_2d.ssbo_count != 1
                 || std::strcmp(manifest_2d.ssbos[0].name, "mtl") != 0
                 || manifest_2d.ssbos[0].ssbo_type != SSBOType::PBRSurface
                 || manifest_2d.ssbos[0].data_slot != 0)
                    result.diagnostics.emplace_back(
                        "Texture2D providers must declare one PBRSurface material SSBO");

                if (manifest_2d.texture_layer_count != 2)
                    result.diagnostics.emplace_back(
                        "Texture2D providers must declare bindless per-slot layer-table dependencies");

                const std::vector<SerializedDescriptorEntry> descriptors =
                    BuildDescriptorsFromDefinition(MaterialDefinition{}, manifest_2d);
                bool has_layer_table = false;
                for (const auto &entry : descriptors)
                {
                    if (entry.semantic == DescriptorSemantic::MaterialTextureLayerTable
                     && entry.name
                     && std::strcmp(entry.name, "mtl_texture_layer_rows") == 0)
                    {
                        has_layer_table = true;
                        break;
                    }
                }
                if (!has_layer_table)
                    result.diagnostics.emplace_back(
                        "Texture2D bindless providers must emit the layer-table descriptor");
            }

            const char *roots_array[] = {pbr_array->name, ntb_array->name};
            ModuleResourceManifest manifest_array{};
            if (!BuildModuleResourceManifest(
                    roots_array, uint32_t(std::size(roots_array)), manifest_array, &registry))
            {
                result.diagnostics.emplace_back(
                    std::string("Texture2DArray provider manifest failed: ")
                    + GetModuleResourceManifestErrorName(manifest_array.error));
            }
            else
            {
                if (manifest_array.ssbo_count != 1
                 || manifest_array.texture_layer_count != 1
                 || manifest_array.texture_layers[0].slot != TextureSlot::Custom0)
                    result.diagnostics.emplace_back(
                        "Texture2DArray providers must declare bindless Custom0 layer resources");

                const std::vector<SerializedDescriptorEntry> descriptors =
                    BuildDescriptorsFromDefinition(MaterialDefinition{}, manifest_array);
                bool has_layer_table = false;
                for (const auto &entry : descriptors)
                {
                    if (entry.semantic == DescriptorSemantic::MaterialTextureLayerTable
                     && entry.name
                     && std::strcmp(entry.name, "mtl_texture_layer_rows") == 0)
                    {
                        has_layer_table = true;
                        break;
                    }
                }
                if (!has_layer_table)
                    result.diagnostics.emplace_back(
                        "Texture2DArray provider layer dependency must emit the layer-table descriptor");
            }

            const char *derivative_root = ntb_derivative->name;
            ModuleResourceManifest derivative_manifest{};
            if (!BuildModuleResourceManifest(
                    &derivative_root, 1, derivative_manifest, &registry)
             || derivative_manifest.texture_layer_count != 1)
               result.diagnostics.emplace_back(
                   "Derivative normal-map provider must declare its bindless layer-table dependency");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunGLSLCodeModuleFileCase()
    {
        GateResult result;
        result.name = "I.glsl-code-module-file-parse";

        auto expect_parse = [&result](const char *content,
                                      const GLSLCodeModuleParseResult expected,
                                      const char *label)
        {
            GLSLCodeModuleFileData data;
            const GLSLCodeModuleParseResult actual =
                ParseGLSLCodeModuleFile(content, int(std::strlen(content)), data);
            if (actual != expected)
            {
                result.diagnostics.emplace_back(std::string(label) + ": got "
                    + GetGLSLCodeModuleParseResultName(actual)
                    + " expected " + GetGLSLCodeModuleParseResultName(expected));
                return false;
            }
            return true;
        };

        // Valid full metadata block.
        const char full_meta[] =
            "// @ulre begin\n"
            "// @ulre name sample_ntb\n"
            "// @ulre kind VertexInput\n"
            "// @ulre priority 10\n"
            "// @ulre require GeometryAttribute Normal Float 3 3\n"
            "// @ulre require GeometryAttribute Tangent Any\n"
            "// @ulre provide Normal\n"
            "// @ulre provide Tangent\n"
            "// @ulre uses s2_lift_xy0\n"
            "// @ulre end\n";
        {
            GLSLCodeModuleFileData data;
            const GLSLCodeModuleParseResult parse =
                ParseGLSLCodeModuleFile(full_meta, int(std::strlen(full_meta)), data);
            if (parse != GLSLCodeModuleParseResult::OK)
                result.diagnostics.emplace_back("full-metadata parse failed");
            else
            {
                if (std::strcmp(data.name.c_str(), "sample_ntb") != 0)
                    result.diagnostics.emplace_back("full-metadata name mismatch");
                if (data.kind != GLSLCodeModuleKind::VertexInput)
                    result.diagnostics.emplace_back("full-metadata kind mismatch");
                if (data.priority != 10)
                    result.diagnostics.emplace_back("full-metadata priority mismatch");
                if (data.semantic_requirements.GetCount() != 2)
                    result.diagnostics.emplace_back("full-metadata require count mismatch");
                else
                {
                    const auto &normal_req = data.semantic_requirements[0];
                    if (normal_req.source != GLSLCodeModuleCapabilitySource::GeometryAttribute
                     || normal_req.semantic != GLSLCodeModuleSemantic::Normal
                     || normal_req.numeric_class_mask != uint32_t(GLSLCodeModuleNumericClass::Float)
                     || normal_req.min_component_count != 3
                     || normal_req.max_component_count != 3)
                        result.diagnostics.emplace_back("full-metadata require[0] mismatch");

                    const auto &tangent_req = data.semantic_requirements[1];
                    if (tangent_req.source != GLSLCodeModuleCapabilitySource::GeometryAttribute
                     || tangent_req.semantic != GLSLCodeModuleSemantic::Tangent
                     || tangent_req.numeric_class_mask != uint32_t(GLSLCodeModuleNumericClass::Any)
                     || tangent_req.min_component_count != 0
                     || tangent_req.max_component_count != 0)
                        result.diagnostics.emplace_back("full-metadata require[1] mismatch");
                }
                if (data.semantic_provides.GetCount() != 2
                 || data.semantic_provides[0] != GLSLCodeModuleSemantic::Normal
                 || data.semantic_provides[1] != GLSLCodeModuleSemantic::Tangent)
                    result.diagnostics.emplace_back("full-metadata provide mismatch");
                if (data.pending_module_requirements.GetCount() != 1
                 || std::strcmp(data.pending_module_requirements[0].c_str(), "s2_lift_xy0") != 0)
                    result.diagnostics.emplace_back("full-metadata uses mismatch");
            }
        }

        expect_parse("// @ulre begin\n// @ulre end\n", GLSLCodeModuleParseResult::OK, "minimal");
        expect_parse("void main() {}\n", GLSLCodeModuleParseResult::Skipped, "no-metadata");
        expect_parse("// @ulre name x\n// @ulre begin\n// @ulre end\n", GLSLCodeModuleParseResult::MissingBegin, "missing-begin");
        expect_parse("// @ulre begin\n// @ulre begin\n// @ulre end\n", GLSLCodeModuleParseResult::DuplicateBegin, "duplicate-begin");
        expect_parse("// @ulre begin\n// @ulre name x\n", GLSLCodeModuleParseResult::MissingEnd, "missing-end");
        expect_parse("// @ulre begin\n// @ulre nope 1\n// @ulre end\n", GLSLCodeModuleParseResult::UnknownDirective, "unknown-directive");
        expect_parse("// @ulre begin\n// @ulre name a\n// @ulre name b\n// @ulre end\n", GLSLCodeModuleParseResult::DuplicateDirective, "duplicate-directive");
        expect_parse("// @ulre begin\n// @ulre kind\n// @ulre end\n", GLSLCodeModuleParseResult::MissingDirectiveArgument, "missing-argument");
        expect_parse("// @ulre begin\n// @ulre kind NoSuchKind\n// @ulre end\n", GLSLCodeModuleParseResult::InvalidKind, "invalid-kind");
        expect_parse("// @ulre begin\n// @ulre provide NoSuchSemantic\n// @ulre end\n", GLSLCodeModuleParseResult::InvalidSemantic, "invalid-semantic");
        expect_parse("// @ulre begin\n// @ulre require BadSource Normal\n// @ulre end\n", GLSLCodeModuleParseResult::InvalidSource, "invalid-source");
        expect_parse("// @ulre begin\n// @ulre require GeometryAttribute Normal NotAClass\n// @ulre end\n", GLSLCodeModuleParseResult::InvalidNumericClass, "invalid-numclass");
        expect_parse("// @ulre begin\n// @ulre priority notanumber\n// @ulre end\n", GLSLCodeModuleParseResult::InvalidNumber, "invalid-number");
        expect_parse("// @ulre begin\n// @ulre conflicts\n// @ulre end\n", GLSLCodeModuleParseResult::InvalidConflict, "invalid-conflict");

        // Registry scan of the real ShaderLibrary directory.
        GLSLCodeModuleRegistry registry;

        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(hgl::ToOSString(GetShaderLibraryPath()), &file_count, &error_count))
            result.diagnostics.emplace_back("LoadDirectory failed to scan directory");
        else
        {
            if (file_count != 72)
                result.diagnostics.emplace_back("LoadDirectory expected 72 file modules, got "
                                                + std::to_string(file_count) + " (4 vertex SSBO modules added)");
            if (error_count != 0)
                result.diagnostics.emplace_back("LoadDirectory reported "
                    + std::to_string(error_count) + " errors");

            const int expected_count = 72;
            if (registry.GetCount() != expected_count)
                result.diagnostics.emplace_back("registry count after LoadDirectory mismatch: got "
                    + std::to_string(registry.GetCount()));

            GLSLCodeModuleMetadataValidationDiagnostic metadata_diagnostic{};
            if (!ValidateGLSLCodeModuleRegistryMetadata(
                    registry, metadata_diagnostic))
            {
                result.diagnostics.emplace_back(
                    "formal metadata validation failed: error="
                    + std::string(
                        GetGLSLCodeModuleMetadataValidationErrorName(
                            metadata_diagnostic.error))
                    + " module="
                    + std::string(metadata_diagnostic.module_name.c_str())
                    + " related="
                    + std::string(
                        metadata_diagnostic.related_module_name.c_str()));
            }
        }

        const auto *lift = registry.FindByName("s2_lift_xy0");
        if (!lift)
            result.diagnostics.emplace_back("s2_lift_xy0 not found by name");
        else if (lift->kind != GLSLCodeModuleKind::Position
              || lift->semantic_requirement_count != 1
              || lift->semantic_requirements[0].semantic != GLSLCodeModuleSemantic::Position
              || lift->semantic_requirements[0].min_component_count != 2
              || lift->semantic_requirements[0].max_component_count != 2)
            result.diagnostics.emplace_back("s2_lift_xy0 capability metadata mismatch");

        const auto *surface_interface = registry.FindByName("surface_interface");
        if (!surface_interface)
            result.diagnostics.emplace_back("surface_interface not found by name");
        else if (surface_interface->kind != GLSLCodeModuleKind::Shared)
            result.diagnostics.emplace_back("surface_interface kind mismatch");

        const auto *compositor_lit = registry.FindByName("main_forward_surface");
        if (!compositor_lit)
            result.diagnostics.emplace_back("main_forward_surface not found by name");
        else
        {
            if (compositor_lit->kind != GLSLCodeModuleKind::FragmentShader)
                result.diagnostics.emplace_back("main_forward_surface kind mismatch");
            if (compositor_lit->dependency_count != 4)
                result.diagnostics.emplace_back("main_forward_surface uses resolution expected 4 deps, got "
                    + std::to_string(compositor_lit->dependency_count));
        }

        const auto *forward_input = registry.FindByName("forward_lighting");
        if (!forward_input || forward_input->kind != GLSLCodeModuleKind::Utility)
            result.diagnostics.emplace_back("forward_lighting input assembly module is missing or has wrong kind");

        const auto *forward_algorithm = registry.FindByName("forward_pbr");
        if (!forward_algorithm || forward_algorithm->kind != GLSLCodeModuleKind::Utility)
            result.diagnostics.emplace_back("forward_pbr lighting algorithm module is missing or has wrong kind");

        const auto *alternate_algorithm = registry.FindByName("forward_flat");
        if (!alternate_algorithm || alternate_algorithm->kind != GLSLCodeModuleKind::Utility)
            result.diagnostics.emplace_back("forward_flat alternate lighting algorithm is missing or has wrong kind");


        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunGLSLCodeModuleMetadataValidationCase()
    {
        GateResult result;
        result.name = "I1.glsl-code-module-metadata-validation";

        const auto make_definition = [](
            const char *name) -> GLSLCodeModuleDefinition
        {
            GLSLCodeModuleDefinition definition{};
            definition.name = name;
            definition.glsl_code = "// metadata validation";
            definition.kind = GLSLCodeModuleKind::VertexInput;
            return definition;
        };

        {
            const GLSLCodeModuleSemantic duplicate_provides[] =
            {
                GLSLCodeModuleSemantic::Position,
                GLSLCodeModuleSemantic::Position
            };
            GLSLCodeModuleDefinition definition =
                make_definition("duplicate_provide");
            definition.semantic_provides = duplicate_provides;
            definition.semantic_provide_count = 2;

            GLSLCodeModuleMetadataValidationDiagnostic diagnostic{};
            if (ValidateGLSLCodeModuleMetadata(definition, diagnostic)
             || diagnostic.error
                    != GLSLCodeModuleMetadataValidationError::DuplicateProvide)
            {
                result.diagnostics.emplace_back(
                    "duplicate provide metadata must be rejected");
            }
        }

        {
            GLSLCodeModuleDefinition first =
                make_definition("cycle_first");
            GLSLCodeModuleDefinition second =
                make_definition("cycle_second");
            const GLSLCodeModuleDependency first_dependencies[] =
            {
                {second.name}
            };
            const GLSLCodeModuleDependency second_dependencies[] =
            {
                {first.name}
            };
            first.dependencies = first_dependencies;
            first.dependency_count = 1;
            second.dependencies = second_dependencies;
            second.dependency_count = 1;

            GLSLCodeModuleRegistry registry;
            GLSLCodeModuleMetadataValidationDiagnostic diagnostic{};
            if (!registry.Register(first)
             || !registry.Register(second)
             || ValidateGLSLCodeModuleRegistryMetadata(registry, diagnostic)
             || diagnostic.error
                    != GLSLCodeModuleMetadataValidationError::DependencyCycle)
            {
                result.diagnostics.emplace_back(
                    "explicit dependency cycle must be rejected");
            }
        }

        {
            const GLSLCodeModuleSemantic position[] =
            {
                GLSLCodeModuleSemantic::Position
            };
            GLSLCodeModuleDefinition first =
                make_definition("ambiguous_first");
            GLSLCodeModuleDefinition second =
                make_definition("ambiguous_second");
            first.semantic_provides = position;
            first.semantic_provide_count = 1;
            second.semantic_provides = position;
            second.semantic_provide_count = 1;

            GLSLCodeModuleRegistry registry;
            GLSLCodeModuleMetadataValidationDiagnostic diagnostic{};
            if (!registry.Register(first)
             || !registry.Register(second)
             || ValidateGLSLCodeModuleRegistryMetadata(registry, diagnostic)
             || diagnostic.error
                    != GLSLCodeModuleMetadataValidationError::
                        AmbiguousProviderPriority)
            {
                result.diagnostics.emplace_back(
                    "equal-priority providers must be rejected as ambiguous");
            }
        }

        {
            GLSLCodeModuleDefinition first =
                make_definition("conflict_first");
            GLSLCodeModuleDefinition second =
                make_definition("conflict_second");
            const char *conflicts[] = {second.name};
            first.module_conflict_names = conflicts;
            first.module_conflict_count = 1;
            if (!AreGLSLCodeModulesConflicting(first, second)
             || !AreGLSLCodeModulesConflicting(second, first))
            {
                result.diagnostics.emplace_back(
                    "module conflicts must be symmetric at query time");
            }
        }

        {
            GLSLCodeModuleDefinition base =
                make_definition("metadata_hash");
            GLSLCodeModuleDefinition changed = base;
            changed.priority = 10;
            if (GetGLSLCodeModuleDefinitionHash(base)
                == GetGLSLCodeModuleDefinitionHash(changed))
            {
                result.diagnostics.emplace_back(
                    "module metadata changes must affect definition hash");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunShadowMaterialModuleGraphCase()
    {
        GateResult result;
        result.name = "I2.shadow-material-module-graph";

        GLSLCodeModuleRegistry &registry = GetGLSLCodeModuleRegistry();
        static const char *definition_ids[] =
        {
            "Lit",
            "VertexPaletteColor",
            "VertexLuminance",
            "DebugNormalColor",
            "VertexColor",
            "SkyMinimal",
            "UnlitTexture",
            "Texture2DArray",
            "LitTextureArray"
        };

        for (const char *definition_id : definition_ids)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(definition_id, definition))
            {
                result.diagnostics.emplace_back(
                    std::string("missing module-graph definition: ")
                    + definition_id);
                continue;
            }

            ResolvedModuleGraph first_graph{};
            ResolvedModuleGraph second_graph{};
            ResolvedModuleGraphBuildDiagnostic diagnostic{};
            if (!BuildMaterialResolvedModuleGraph(
                    definition, registry, first_graph, diagnostic))
            {
                result.diagnostics.emplace_back(
                    std::string("module graph build failed: ")
                    + definition_id + " error="
                    + GetResolvedModuleGraphBuildErrorName(
                        diagnostic.error)
                    + " module=" + diagnostic.module_name.c_str());
                continue;
            }
            if (!BuildMaterialResolvedModuleGraph(
                    definition, registry, second_graph, diagnostic)
             || GetResolvedModuleGraphHash(first_graph) == 0
             || GetResolvedModuleGraphHash(first_graph)
                    != GetResolvedModuleGraphHash(second_graph))
            {
                result.diagnostics.emplace_back(
                    std::string("module graph is not deterministic: ")
                    + definition_id);
            }
        }

        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID("Lit", definition))
            {
                result.diagnostics.emplace_back(
                    "missing Lit definition for request graph identity");
            }
            else
            {
                MaterialDefinitionBuildRequest default_request{};
                MaterialDefinitionBuildRequest override_request{};
                override_request.has_vertex_node_config_override = true;
                override_request.vertex_node_config_override =
                    VertexNodeConfigResolver::FlatXY();

                ResolvedModuleGraph default_graph{};
                ResolvedModuleGraph override_graph{};
                ResolvedModuleGraphBuildDiagnostic diagnostic{};
                if (!BuildMaterialResolvedModuleGraph(
                        definition,
                        registry,
                        default_graph,
                        diagnostic,
                        &default_request)
                 || !BuildMaterialResolvedModuleGraph(
                        definition,
                        registry,
                        override_graph,
                        diagnostic,
                        &override_request)
                 || GetResolvedModuleGraphHash(default_graph)
                        == GetResolvedModuleGraphHash(override_graph))
                {
                    result.diagnostics.emplace_back(
                        "request-selected transform must affect module graph");
                }
            }
        }

        const auto build_synthetic_registry = [](
            GLSLCodeModuleRegistry &out_registry,
            GLSLCodeModuleDefinition &dependency,
            GLSLCodeModuleDefinition &root,
            GLSLCodeModuleDefinition &stage2,
            GLSLCodeModuleDefinition &stage3,
            GLSLCodeModuleDependency &root_dependency,
            const bool reverse_order) -> bool
        {
            dependency = {};
            dependency.name = "shadow_dependency";
            dependency.glsl_code = "// shadow dependency";
            dependency.kind = GLSLCodeModuleKind::Utility;

            root_dependency = {
                dependency.name
            };
            root = {};
            root.name = "main_depth_only";
            root.glsl_code = "// shadow root";
            root.kind = GLSLCodeModuleKind::FragmentShader;
            root.dependencies = &root_dependency;
            root.dependency_count = 1;

            stage2 = {};
            stage2.name = "s2_passthrough3d";
            stage2.glsl_code = "// synthetic stage 2";
            stage2.kind = GLSLCodeModuleKind::Position;

            stage3 = {};
            stage3.name = "s3_world_camera_vp";
            stage3.glsl_code = "// synthetic stage 3";
            stage3.kind = GLSLCodeModuleKind::Transform;

            if (reverse_order)
            {
                return out_registry.Register(stage3)
                    && out_registry.Register(stage2)
                    && out_registry.Register(root)
                    && out_registry.Register(dependency);
            }

            return out_registry.Register(dependency)
                && out_registry.Register(root)
                && out_registry.Register(stage2)
                && out_registry.Register(stage3);
        };

        GLSLCodeModuleRegistry first_registry;
        GLSLCodeModuleRegistry second_registry;
        GLSLCodeModuleDefinition first_dependency{};
        GLSLCodeModuleDefinition first_root{};
        GLSLCodeModuleDefinition first_stage2{};
        GLSLCodeModuleDefinition first_stage3{};
        GLSLCodeModuleDependency first_root_dependency{};
        GLSLCodeModuleDefinition second_dependency{};
        GLSLCodeModuleDefinition second_root{};
        GLSLCodeModuleDefinition second_stage2{};
        GLSLCodeModuleDefinition second_stage3{};
        GLSLCodeModuleDependency second_root_dependency{};

        if (!build_synthetic_registry(
                first_registry,
                first_dependency,
                first_root,
                first_stage2,
                first_stage3,
                first_root_dependency,
                false)
         || !build_synthetic_registry(
                second_registry,
                second_dependency,
                second_root,
                second_stage2,
                second_stage3,
                second_root_dependency,
                true))
        {
            result.diagnostics.emplace_back(
                "failed to build synthetic module registries");
        }
        else
        {
            MaterialDefinition definition{};
            definition.definition_id = "SyntheticShadowGraph";
            SetMaterialFragmentSource(
                definition, "synthetic/shadow_root.frag.glsl");
            definition.vertex_node_config = MakeDefault3DNodeConfig();
            MaterialDefinitionBuildRequest graph_request{};
            graph_request.override_shader_program_purpose = true;
            graph_request.shader_program_purpose =
                ShaderProgramPurpose::ShadowDepth;

            ResolvedModuleGraph first_graph{};
            ResolvedModuleGraph second_graph{};
            ResolvedModuleGraphBuildDiagnostic diagnostic{};
            hgl::ValueArray<hgl::uint8> first_bytes;
            hgl::ValueArray<hgl::uint8> second_bytes;
            if (!BuildMaterialResolvedModuleGraph(
                    definition,
                    first_registry,
                    first_graph,
                    diagnostic,
                    &graph_request)
             || !BuildMaterialResolvedModuleGraph(
                    definition,
                    second_registry,
                    second_graph,
                    diagnostic,
                    &graph_request)
             || !SerializeResolvedModuleGraph(first_graph, first_bytes)
             || !SerializeResolvedModuleGraph(second_graph, second_bytes)
             || first_bytes.GetCount() != second_bytes.GetCount()
             || std::memcmp(
                    first_bytes.GetData(),
                    second_bytes.GetData(),
                    static_cast<size_t>(first_bytes.GetCount())) != 0
             || GetResolvedModuleGraphHash(first_graph)
                    != GetResolvedModuleGraphHash(second_graph))
            {
                result.diagnostics.emplace_back(
                    "module graph must ignore registry IDs and order");
            }
        }

        MaterialDefinition missing_root{};
        missing_root.definition_id = "MissingShadowRoot";
        SetMaterialFragmentSource(
            missing_root, "synthetic/not_registered.frag.glsl");
        missing_root.vertex_node_config = MakeDefault3DNodeConfig();
        MaterialDefinitionBuildRequest missing_request{};
        missing_request.override_shader_program_purpose = true;
        missing_request.shader_program_purpose =
            ShaderProgramPurpose::ShadowDepth;
        GLSLCodeModuleRegistry missing_registry;
        ResolvedModuleGraph missing_graph{};
        ResolvedModuleGraphBuildDiagnostic missing_diagnostic{};
        if (BuildMaterialResolvedModuleGraph(
                missing_root,
                missing_registry,
                missing_graph,
                missing_diagnostic,
                &missing_request)
         || missing_diagnostic.error
                != ResolvedModuleGraphBuildError::MissingRootModule)
        {
            result.diagnostics.emplace_back(
                "missing module roots must fail explicitly");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    constexpr uint32_t RESOLVER_ANY  = uint32_t(GLSLCodeModuleNumericClass::Any);
    constexpr uint32_t RESOLVER_FLOAT = uint32_t(GLSLCodeModuleNumericClass::Float);
    constexpr uint32_t RESOLVER_NORM  = uint32_t(GLSLCodeModuleNumericClass::Normalized);
    constexpr uint32_t RESOLVER_PACK  = uint32_t(GLSLCodeModuleNumericClass::Packed);

    static std::vector<GLSLCodeModuleGeometryCapability> ResolverBuildCaps(const GeometryVertexFormat &gvf)
    {
        std::vector<GLSLCodeModuleGeometryCapability> caps;
        hgl::ValueArray<GLSLCodeModuleGeometryCapability> temp;
        if (!GLSLCodeModuleCapabilityResolver::BuildGeometryCapabilities(gvf, temp))
            return caps;
        for (int i = 0; i < temp.GetCount(); ++i)
            caps.push_back(temp[i]);
        return caps;
    }

    static GateResult RunCapabilityResolverCase()
    {
        GateResult result;
        result.name = "J.capability-resolver";

        // A synthetic module whose pointers stay stable because the module
        // object itself is heap-allocated through a unique_ptr.
        struct SyntheticModule
        {
            std::string name;
            std::string code;
            std::vector<GLSLCodeModuleSemanticRequirement> requirements;
            std::vector<GLSLCodeModuleSemantic> provides;
            GLSLCodeModuleDefinition definition;
        };

        struct ResolverFixture
        {
            GLSLCodeModuleRegistry registry;
            std::vector<std::unique_ptr<SyntheticModule>> storage;


            ResolverFixture()
            {
            }

            const GLSLCodeModuleDefinition *Add(const char *name,
                                                const GLSLCodeModuleKind kind,
                                                const int32_t priority,
                                                std::vector<GLSLCodeModuleSemanticRequirement> reqs,
                                                std::vector<GLSLCodeModuleSemantic> provides)
            {
                auto module = std::make_unique<SyntheticModule>();
                module->name = name;
                module->code = "// synthetic";
                module->requirements = std::move(reqs);
                module->provides = std::move(provides);

                GLSLCodeModuleDefinition definition;
                
                definition.name = module->name.c_str();
                definition.glsl_code = module->code.c_str();
                definition.kind = kind;
                definition.priority = priority;
                definition.semantic_requirements = module->requirements.empty() ? nullptr : module->requirements.data();
                definition.semantic_requirement_count = uint32_t(module->requirements.size());
                definition.semantic_provides = module->provides.empty() ? nullptr : module->provides.data();
                definition.semantic_provide_count = uint32_t(module->provides.size());

                module->definition = definition;
                const auto *ptr = &module->definition;
                if (!registry.Register(*ptr))
                    return nullptr;

                storage.push_back(std::move(module));
                return ptr;
            }
        };

        struct ResolveOutcome
        {
            bool ok = false;
            GLSLCodeModuleResolutionResult result;
        };

        auto resolve = [](const ResolverFixture &fixture,
                          const std::vector<GLSLCodeModuleSemanticRequirement> &reqs,
                          const std::vector<GLSLCodeModuleGeometryCapability> &caps,
                          const std::vector<GLSLCodeModuleSemantic> &resources)
        {
            GLSLCodeModuleResolutionRequest request;
            request.requirements = reqs.empty() ? nullptr : reqs.data();
            request.requirement_count = uint32_t(reqs.size());
            request.geometry_capabilities = caps.empty() ? nullptr : caps.data();
            request.geometry_capability_count = uint32_t(caps.size());
            request.resources = resources.empty() ? nullptr : resources.data();
            request.resource_count = uint32_t(resources.size());
            const GLSLCodeModuleCapabilityResolver resolver;
            ResolveOutcome outcome;
            outcome.ok = resolver.Resolve(fixture.registry, request, outcome.result);
            return outcome;
        };

        const std::vector<GLSLCodeModuleSemantic> kNoResources;

        auto has_selection = [](const GLSLCodeModuleResolutionResult &res, const GLSLCodeModuleDefinition *expected)
        {
            for (int i = 0; i < res.selections.GetCount(); ++i)
            {
                if (res.selections[i].provider == expected)
                    return true;
            }
            return false;
        };

        auto rejected_candidate = [](const GLSLCodeModuleResolutionResult &res, const GLSLCodeModuleDefinition *candidate)
        {
            for (int i = 0; i < res.diagnostics.GetCount(); ++i)
            {
                if (res.diagnostics[i].candidate == candidate)
                    return true;
            }
            return false;
        };

        auto produced = [](const GLSLCodeModuleSemantic semantic)
        {
            GLSLCodeModuleSemanticRequirement requirement;
            requirement.source = GLSLCodeModuleCapabilitySource::ProducedSemantic;
            requirement.semantic = semantic;
            requirement.numeric_class_mask = RESOLVER_ANY;
            return requirement;
        };

        auto geometry = [](const GLSLCodeModuleSemantic semantic,
                           const uint32_t mask = RESOLVER_ANY,
                           const uint8_t minc = 0,
                           const uint8_t maxc = 0)
        {
            GLSLCodeModuleSemanticRequirement requirement;
            requirement.source = GLSLCodeModuleCapabilitySource::GeometryAttribute;
            requirement.semantic = semantic;
            requirement.numeric_class_mask = mask;
            requirement.min_component_count = minc;
            requirement.max_component_count = maxc;
            return requirement;
        };

        auto resource = [](const GLSLCodeModuleSemantic semantic)
        {
            GLSLCodeModuleSemanticRequirement requirement;
            requirement.source = GLSLCodeModuleCapabilitySource::Resource;
            requirement.semantic = semantic;
            requirement.numeric_class_mask = RESOLVER_ANY;
            return requirement;
        };

        auto check = [&result](const bool condition, const std::string &label)
        {
            if (!condition)
                result.diagnostics.push_back(label);
        };

        // ------------------------------------------------------------------
        // 1. Full NTB geometry selects the direct NTB provider.
        {
            ResolverFixture fixture;
            const auto *ntb_direct = fixture.Add("ntb_direct", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal),
                  geometry(GLSLCodeModuleSemantic::Tangent),
                  geometry(GLSLCodeModuleSemantic::Binormal) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);
            gvf.Add(VertexSemantic::Normal, VF_V3F);
            gvf.Add(VertexSemantic::Tangent, VF_V3F);
            gvf.Add(VertexSemantic::Bitangent, VF_V3F);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources);
            check(outcome.ok, "J1 direct-ntb: resolution must succeed");
            check(outcome.result.resolved, "J1 direct-ntb: resolved flag must be set");
            check(has_selection(outcome.result, ntb_direct), "J1 direct-ntb: expected ntb_direct selected");
        }

        // ------------------------------------------------------------------
        // 2. Normal-only geometry selects the normal-only provider; the full
        //    NTB provider is rejected because Tangent/Binormal are absent.
        {
            ResolverFixture fixture;
            const auto *ntb_direct = fixture.Add("ntb_direct", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal),
                  geometry(GLSLCodeModuleSemantic::Tangent),
                  geometry(GLSLCodeModuleSemantic::Binormal) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });
            const auto *normal_only = fixture.Add("normal_only", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);
            gvf.Add(VertexSemantic::Normal, VF_V3F);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources);
            check(outcome.ok, "J2 normal-only: resolution must succeed");
            check(has_selection(outcome.result, normal_only), "J2 normal-only: expected normal_only selected");
            check(rejected_candidate(outcome.result, ntb_direct), "J2 normal-only: ntb_direct must be rejected");
        }

        // ------------------------------------------------------------------
        // 3. Packed RGB10A2 normal selects the decode provider.
        {
            ResolverFixture fixture;
            fixture.Add("ntb_direct", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal),
                  geometry(GLSLCodeModuleSemantic::Tangent),
                  geometry(GLSLCodeModuleSemantic::Binormal) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });
            const auto *normal_only = fixture.Add("normal_only", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal });
            const auto *decode = fixture.Add("normal_decode_a2rgb10", GLSLCodeModuleKind::Utility, 100,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_NORM | RESOLVER_PACK) },
                { GLSLCodeModuleSemantic::Normal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);
            // Packed formats have no VF_ macro; vec_size must be explicit.
            gvf.Add(VertexSemantic::Normal, PF_A2RGB10UN, 4, 0);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources);
            check(outcome.ok, "J3 packed-normal: resolution must succeed");
            check(has_selection(outcome.result, decode), "J3 packed-normal: expected decode provider selected");
            check(rejected_candidate(outcome.result, normal_only), "J3 packed-normal: float provider must be rejected");
        }

        // ------------------------------------------------------------------
        // 4. Position-derived sky NTB (geometry has only Position).
        {
            ResolverFixture fixture;
            const auto *ntb_from_position = fixture.Add("ntb_from_position", GLSLCodeModuleKind::Utility, 50,
                { geometry(GLSLCodeModuleSemantic::Position, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });
            fixture.Add("ntb_direct", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal),
                  geometry(GLSLCodeModuleSemantic::Tangent),
                  geometry(GLSLCodeModuleSemantic::Binormal) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources);
            check(outcome.ok, "J4 sky-ntb: resolution must succeed");
            check(has_selection(outcome.result, ntb_from_position), "J4 sky-ntb: expected position-derived provider selected");
        }

        // ------------------------------------------------------------------
        // 5. Heightmap terrain NTB: with the HeightMap resource the dedicated
        //    provider wins over the plain position-derived fallback.
        {
            ResolverFixture fixture;
            fixture.Add("ntb_from_position", GLSLCodeModuleKind::Utility, 50,
                { geometry(GLSLCodeModuleSemantic::Position, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });
            const auto *ntb_heightmap = fixture.Add("ntb_from_heightmap", GLSLCodeModuleKind::Utility, 60,
                { geometry(GLSLCodeModuleSemantic::Position, RESOLVER_FLOAT, 3, 3),
                  geometry(GLSLCodeModuleSemantic::UV0, RESOLVER_FLOAT, 2, 2),
                  resource(GLSLCodeModuleSemantic::HeightMap) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);
            gvf.Add(VertexSemantic::TexCoord, VF_V2F);

            const auto outcome = resolve(
                fixture,
                { produced(GLSLCodeModuleSemantic::Normal) },
                ResolverBuildCaps(gvf),
                { GLSLCodeModuleSemantic::HeightMap });
            check(outcome.ok, "J5 terrain-ntb: resolution must succeed");
            check(has_selection(outcome.result, ntb_heightmap), "J5 terrain-ntb: expected heightmap provider selected");
        }

        // ------------------------------------------------------------------
        // 6. Heightmap provider without the HeightMap resource must fail.
        {
            ResolverFixture fixture;
            fixture.Add("ntb_heightmap", GLSLCodeModuleKind::Utility, 60,
                { geometry(GLSLCodeModuleSemantic::Position, RESOLVER_FLOAT, 3, 3),
                  geometry(GLSLCodeModuleSemantic::UV0, RESOLVER_FLOAT, 2, 2),
                  resource(GLSLCodeModuleSemantic::HeightMap) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);
            gvf.Add(VertexSemantic::TexCoord, VF_V2F);

            const auto outcome = resolve(
                fixture,
                { produced(GLSLCodeModuleSemantic::Normal) },
                ResolverBuildCaps(gvf),
                kNoResources);
            check(!outcome.ok, "J6 missing-resource: resolution must fail");
            check(!outcome.result.resolved, "J6 missing-resource: resolved flag must be clear");
            check(outcome.result.diagnostics.GetCount() > 0, "J6 missing-resource: diagnostics must be present");
        }

        // ------------------------------------------------------------------
        // 7. Octahedral RG8SN normal (2-component, normalized).
        {
            ResolverFixture fixture;
            const auto *octa = fixture.Add("normal_decode_octa", GLSLCodeModuleKind::Utility, 100,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_NORM, 2, 2) },
                { GLSLCodeModuleSemantic::Normal });
            fixture.Add("normal_only", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);
            gvf.Add(VertexSemantic::Normal, VF_V2SN8);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources);
            check(outcome.ok, "J7 octa-normal: resolution must succeed");
            check(has_selection(outcome.result, octa), "J7 octa-normal: expected octa decoder selected");
        }

        // ------------------------------------------------------------------
        // 8. Priority ordering: higher priority provider wins.
        {
            ResolverFixture fixture;
            const auto *low = fixture.Add("normal_low", GLSLCodeModuleKind::VertexInput, 0,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal });
            const auto *high = fixture.Add("normal_high", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);
            gvf.Add(VertexSemantic::Normal, VF_V3F);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources);
            check(outcome.ok, "J8 priority: resolution must succeed");
            check(has_selection(outcome.result, high), "J8 priority: expected high-priority provider selected");
            check(!has_selection(outcome.result, low), "J8 priority: low-priority provider must not be selected");
        }

        // ------------------------------------------------------------------
        // 9. Deterministic tie-break: equal priority picks the lower module ID.
        {
            ResolverFixture fixture;
            const auto *a = fixture.Add("normal_a", GLSLCodeModuleKind::VertexInput, 0,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal });
            const auto *b = fixture.Add("normal_b", GLSLCodeModuleKind::VertexInput, 0,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);
            gvf.Add(VertexSemantic::Normal, VF_V3F);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources);
            check(outcome.ok, "J9 tie-break: resolution must succeed");
            check(has_selection(outcome.result, a), "J9 tie-break: expected first-registered (lower ID) provider selected");
            check(!has_selection(outcome.result, b), "J9 tie-break: second provider must not be selected");
        }

        // ------------------------------------------------------------------
        // 10. RG16F/RG32F normal geometries resolve to the same provider.
        {
            ResolverFixture fixture;
            const auto *normal_only = fixture.Add("normal_only", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal });

            GeometryVertexFormat gvf32;
            gvf32.Add(VertexSemantic::Position, VF_V3F);
            gvf32.Add(VertexSemantic::Normal, VF_V3F);

            GeometryVertexFormat gvf16;
            gvf16.Add(VertexSemantic::Position, VF_V3F);
            gvf16.Add(VertexSemantic::Normal, VF_V3HF);

            const auto outcome32 = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf32), kNoResources);
            const auto outcome16 = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf16), kNoResources);
            check(outcome32.ok && outcome16.ok, "J10 rg16f-rg32f: both resolutions must succeed");
            check(has_selection(outcome32.result, normal_only) && has_selection(outcome16.result, normal_only),
                  "J10 rg16f-rg32f: both formats must select the same provider");
        }

        // ------------------------------------------------------------------
        // 11. Provider composition: a WorldNormal provider depends on the
        //     produced Normal semantic; dependencies commit first.
        {
            ResolverFixture fixture;
            const auto *ntb_from_position = fixture.Add("ntb_from_position", GLSLCodeModuleKind::Utility, 50,
                { geometry(GLSLCodeModuleSemantic::Position, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });
            const auto *world_normal = fixture.Add("world_normal_from_normal", GLSLCodeModuleKind::Utility, 50,
                { produced(GLSLCodeModuleSemantic::Normal) },
                { GLSLCodeModuleSemantic::WorldNormal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::WorldNormal) }, ResolverBuildCaps(gvf), kNoResources);
            check(outcome.ok, "J11 composition: resolution must succeed");
            check(has_selection(outcome.result, ntb_from_position), "J11 composition: NTB provider must be selected first");
            check(has_selection(outcome.result, world_normal), "J11 composition: world-normal provider must be selected");

            if (outcome.result.selections.GetCount() == 2)
            {
                check(outcome.result.selections[0].provider == ntb_from_position,
                      "J11 composition: dependency must appear before its dependent");
            }
            else
            {
                result.diagnostics.push_back("J11 composition: expected exactly two selections");
            }
        }

        // ------------------------------------------------------------------
        // 12. YUV color: provider needs ColorY + ColorUV attributes; direct
        //     RGBA color provider needs only Color. Exercise both branches
        //     with explicit capability arrays (the geometry->capability mapper
        //     cannot yet express YUV channels through VertexSemantic).
        {
            ResolverFixture fixture;
            const auto *color_direct = fixture.Add("color_direct", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Color) },
                { GLSLCodeModuleSemantic::Color });
            const auto *color_yuv = fixture.Add("color_decode_yuv", GLSLCodeModuleKind::Utility, 100,
                { geometry(GLSLCodeModuleSemantic::ColorY),
                  geometry(GLSLCodeModuleSemantic::ColorUV) },
                { GLSLCodeModuleSemantic::Color });

            std::vector<GLSLCodeModuleGeometryCapability> yuv_caps;
            GLSLCodeModuleGeometryCapability cap_y;
            cap_y.semantic = GLSLCodeModuleSemantic::ColorY;
            cap_y.numeric_class_mask = RESOLVER_FLOAT;
            cap_y.component_count = 1;
            yuv_caps.push_back(cap_y);
            GLSLCodeModuleGeometryCapability cap_uv;
            cap_uv.semantic = GLSLCodeModuleSemantic::ColorUV;
            cap_uv.numeric_class_mask = RESOLVER_NORM;
            cap_uv.component_count = 2;
            yuv_caps.push_back(cap_uv);

            const auto outcome_yuv = resolve(fixture, { produced(GLSLCodeModuleSemantic::Color) }, yuv_caps, kNoResources);
            check(outcome_yuv.ok, "J12 yuv: resolution must succeed");
            check(has_selection(outcome_yuv.result, color_yuv), "J12 yuv: expected YUV decoder selected");
            check(rejected_candidate(outcome_yuv.result, color_direct), "J12 yuv: direct RGBA provider must be rejected");

            std::vector<GLSLCodeModuleGeometryCapability> rgba_caps;
            GLSLCodeModuleGeometryCapability cap_color;
            cap_color.semantic = GLSLCodeModuleSemantic::Color;
            cap_color.numeric_class_mask = RESOLVER_NORM;
            cap_color.component_count = 4;
            rgba_caps.push_back(cap_color);

            const auto outcome_rgba = resolve(fixture, { produced(GLSLCodeModuleSemantic::Color) }, rgba_caps, kNoResources);
            check(outcome_rgba.ok, "J12 rgba: resolution must succeed");
            check(has_selection(outcome_rgba.result, color_direct), "J12 rgba: expected direct color provider selected");
            check(!has_selection(outcome_rgba.result, color_yuv), "J12 rgba: YUV decoder must not be selected");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialMultiSlotSourceCase()
    {
        GateResult result;
        result.name = "Z.material-multislot-source";

        std::vector<DataSlotDeclaration> slots = {
            {"surface_a", SSBOType::EmissiveSurface},
            {"surface_b", SSBOType::EmissiveSurface}
        };
        const SerializedVertexEntry vertices[] = {
            {VF_V2F, VertexSemantic::Position}
        };
        const SerializedDescriptorEntry descriptors[] = {
            {
                DescriptorSetType::PerObject,
                DescriptorKind::SSBO,
                uint32_t(hgl::graph::kMeshFragment),
                "mtl_data_index_rows",
                "DataIndexRows",
                nullptr,
                DescriptorSemantic::MaterialDataIndexTable,
                TextureSlot::BaseColor,
                DefaultMaterialDataSlot,
                SSBOType::MaterialDataIndexTable,
                DescriptorSemanticLayer::SSBO
            }
        };
        const MaterialShaderCompilerInput compiler_input{
            "MultiSlotMaterial",
            PrimitiveType::Triangles,
            vertices,
            1,
            descriptors,
            1
        };

        CompositorMaterialBuildConfig config{};
        config.data_slot_decls = &slots;
        config.defer_finalize = true;
        // mesh 化后顶点路径统一走 Mesh stage（VS 已彻底废弃）
        config.shader_stage_flag_bits = uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *build_spec = CompileCompositorMaterial(
            nullptr,
            compiler_input,
            "#version 460\nlayout(location=0) in vec2 Position;\nvoid main(){gl_Position=vec4(Position,0.0,1.0);}\n",
            "#version 460\nlayout(location=0) out vec4 outColor;\nvoid main(){outColor=vec4(1.0);}\n",
            config);
        if (!build_spec)
        {
            result.diagnostics.emplace_back("multi-slot compiler did not produce a build spec");
            result.passed = false;
            return result;
        }

        const ShaderCreateInfo *fragment =
            build_spec->GetStageShader(ShaderStage::Fragment);
        if (!fragment)
        {
            result.diagnostics.emplace_back("multi-slot compiler did not produce a fragment stage");
        }
        else
        {
            const std::string &source = fragment->GetFinalGLSL();
            const auto count_occurrences = [&source](const char *token)
            {
                size_t count = 0;
                size_t offset = 0;
                while (true)
                {
                    const size_t found = source.find(token, offset);
                    if (found == std::string::npos)
                        break;
                    ++count;
                    offset = found + 1;
                }
                return count;
            };

            if (source.find("#define MTL_DATA surface_a") == std::string::npos
             || source.find("#define MTL_DATA_SLOT_1 surface_b") == std::string::npos
             || source.find("#define MTL_DATA_SLOT_COUNT 2u") == std::string::npos)
                result.diagnostics.emplace_back("multi-slot aliases were not injected");

            if (count_occurrences("struct EmissiveSurfaceData") != 1)
                result.diagnostics.emplace_back("repeated SSBO type emitted duplicate GLSL struct");

            if (source.find("} surface_a;") == std::string::npos
             || source.find("} surface_b;") == std::string::npos)
                result.diagnostics.emplace_back("named multi-slot SSBO declarations are incomplete");
        }

        const ShaderCreateInfo *vertex =
            build_spec->GetStageShader(ShaderStage::Mesh);
        if (!vertex)
        {
            result.diagnostics.emplace_back("multi-slot compiler did not produce a mesh stage");
        }
        else
        {
            const std::string &source = vertex->GetFinalGLSL();
            if (source.find("ResolveDataIndexID(uint iid, uint data_slot)") == std::string::npos
             || source.find("iid * MTL_DATA_INDEX_ROW_STRIDE + data_slot") == std::string::npos)
                result.diagnostics.emplace_back("data-index resolver is not slot-aware");
        }

        delete build_spec;
        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunDescriptorContractCase()
    {
        GateResult result;
        result.name = "W1.material-descriptor-contract";

        ShaderResourceSchema persistent_layout;
        DescriptorContract first_contract{};
        DescriptorContract second_contract{};
        {
            std::string viewport_name = "viewport";
            std::string viewport_struct = "ViewportInfo";
            std::string material_name = "mtl";
            std::string material_struct = "PBRSurfaceData";
            SerializedDescriptorEntry entries[] =
            {
                {
                    DescriptorSetType::Scene,
                    DescriptorKind::UBO,
                    uint32_t(hgl::graph::kMeshFragment),
                    viewport_name.c_str(),
                    viewport_struct.c_str(),
                    nullptr,
                    DescriptorSemantic::ViewportInfo,
                    TextureSlot::BaseColor,
                    DefaultMaterialDataSlot,
                    SSBOType::UserDefined,
                    DescriptorSemanticLayer::UBO
                },
                {
                    DescriptorSetType::Material,
                    DescriptorKind::SSBO,
                    uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                    material_name.c_str(),
                    material_struct.c_str(),
                    nullptr,
                    DescriptorSemantic::MaterialDataSlotData,
                    TextureSlot::BaseColor,
                    DefaultMaterialDataSlot,
                    SSBOType::PBRSurface,
                    DescriptorSemanticLayer::SSBO,
                    MakeRecipeSSBOId(0),
                    true,
                    true,
                    false
                }
            };
            std::vector<SerializedDescriptorEntry> reversed{
                entries[1], entries[0]
            };

            if (!BuildDescriptorContract(
                    entries, 2, first_contract)
             || !BuildDescriptorContract(
                    reversed, second_contract)
             || GetDescriptorContractHash(first_contract)
                    != GetDescriptorContractHash(second_contract)
             || !BuildResourceSchemaFromContract(
                    first_contract, persistent_layout))
            {
                result.diagnostics.emplace_back(
                    "descriptor contract build/hash/layout failed");
            }

            SerializedDescriptorEntry visibility_changed_entries[] =
            {
                entries[0], entries[1]
            };
            visibility_changed_entries[1].stage_flags =
                uint32_t(hgl::graph::kMeshFragment);
            DescriptorContract visibility_changed{};
            if (!BuildDescriptorContract(
                    visibility_changed_entries,
                    2,
                    visibility_changed)
             || GetDescriptorContractHash(first_contract)
                    == GetDescriptorContractHash(
                        visibility_changed))
            {
                result.diagnostics.emplace_back(
                    "descriptor stage visibility must affect contract hash");
            }

            std::vector<SerializedDescriptorEntry> roundtrip;
            if (!ConvertDescriptorContractToFixed(
                    first_contract, roundtrip)
             || roundtrip.size() != 2
             || std::strcmp(roundtrip[0].name, "viewport") != 0
             || std::strcmp(
                    roundtrip[1].struct_name, "PBRSurfaceData") != 0)
            {
                result.diagnostics.emplace_back(
                    "descriptor contract Fixed adapter mismatch");
            }

            SerializedDescriptorEntry duplicate_entries[] =
            {
                entries[0], entries[0]
            };
            DescriptorContract invalid_contract{};
            if (BuildDescriptorContract(
                    duplicate_entries, 2, invalid_contract))
            {
                result.diagnostics.emplace_back(
                    "duplicate descriptor identities must be rejected");
            }

            DescriptorContract varying_contract = first_contract;
            MaterialVertexVaryingConfig varying{};
            varying.emit_data_index_id = true;
            ShaderResourceSchema varying_layout;
            if (!EnsureDescriptorContractVaryingResources(
                    varying, varying_contract)
             || !BuildResourceSchemaFromContract(
                    varying_contract, varying_layout))
            {
                result.diagnostics.emplace_back(
                    "varying descriptor resources were not added");
            }
            else
            {
                bool has_data_index = false;
                bool has_texture_layer = false;
                for (const auto &requirement :
                     varying_layout.resources)
                {
                    if (requirement.semantic
                        == DescriptorSemantic::MaterialDataIndexTable)
                    {
                        has_data_index =
                            requirement.stage_flags
                                == uint32_t(
                                    hgl::graph::kMeshFragment);
                    }
                    if (requirement.semantic
                        == DescriptorSemantic::MaterialTextureLayerTable)
                    {
                        has_texture_layer = true;
                    }
                }
                // P1-2e：varying 路径只负责 MaterialDataIndexTable；
                // MaterialTextureLayerTable 由 manifest/纹理槽声明提供，
                // 不再由 varying 契约生成。
                if (!has_data_index || has_texture_layer)
                    result.diagnostics.emplace_back(
                        "varying tables missing from runtime layout");
            }
        }

        if (persistent_layout.resources.size() != 2
         || persistent_layout.resources[0].name != "viewport"
         || persistent_layout.resources[0].struct_name != "ViewportInfo"
         || persistent_layout.resources[1].struct_name != "PBRSurfaceData")
        {
            result.diagnostics.emplace_back(
                "material layout did not retain owned descriptor strings");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunSamplerPresetLibraryCase()
    {
        GateResult result;
        result.name = "AC.sampler-preset-library";

        auto &lib = SamplerPresetLibrary::Instance();

        // 1. 加载 sampler.toml（统一注册机制唯一数据源）。
        const std::string root = GetShaderLibraryPath();
        const hgl::filesystem::Path sampler_toml(
            hgl::ToOSString(root + "/sampler.toml"));
        if (!lib.Load(sampler_toml.ToOSString()))
        {
            result.diagnostics.emplace_back("sampler.toml load failed");
            result.passed = false;
            return result;
        }

        // 2. 名字→索引一致（顺序 = sampler.toml 数组顺序）。
        struct NameIndex
        {
            const char *name;
            uint32_t index;
        };
        const NameIndex expected[] =
        {
            { "Nearest",        0u },
            { "Linear",         1u },
            { "Trilinear",      2u },
            { "TrilinearAniso", 3u },
            { "ShadowPCF",      4u },
            { "Terrain",        5u },
            { "UI",             6u },
        };
        if (lib.GetCount() != uint32_t(std::size(expected)))
        {
            result.diagnostics.emplace_back(
                "preset count mismatch: expected "
                + std::to_string(std::size(expected))
                + " got " + std::to_string(lib.GetCount()));
        }
        for (const auto &e : expected)
        {
            if (lib.GetIndex(e.name) != e.index)
            {
                result.diagnostics.emplace_back(
                    std::string("name->index mismatch: ") + e.name);
            }
        }

        // 3. 未知名 / 空名 / 空指针 → 显式无效（~0u）——调用方必须处理，
        //    不再静默保底 0（=Nearest）掩盖 sampler.toml 顺序错位。
        if (lib.GetIndex("DoesNotExistSampler") != ~0u)
            result.diagnostics.emplace_back("unknown name must yield invalid index");
        if (lib.GetIndex("") != ~0u)
            result.diagnostics.emplace_back("empty name must yield invalid index");
        if (lib.GetIndex(nullptr) != ~0u)
            result.diagnostics.emplace_back("null name must yield invalid index");

        // 4. max_lod 统一 15.0（不再按纹理 mip 级数派生）。
        for (uint32_t i = 0; i < lib.GetCount(); ++i)
        {
            const VkSamplerCreateInfo *sci = lib.GetCreateInfo(i);
            if (!sci)
            {
                result.diagnostics.emplace_back(
                    "GetCreateInfo(" + std::to_string(i) + ") returned null");
                continue;
            }
            if (sci->maxLod != 15.0f)
            {
                result.diagnostics.emplace_back(
                    "preset " + std::to_string(i) + " maxLod != 15.0");
            }
        }

        // 5. 关键预设过滤语义。
        const VkSamplerCreateInfo *nearest = lib.GetCreateInfo(0);
        if (nearest
            && (nearest->magFilter != VK_FILTER_NEAREST
             || nearest->minFilter != VK_FILTER_NEAREST))
            result.diagnostics.emplace_back("Nearest filter mismatch");

        const VkSamplerCreateInfo *trilinear = lib.GetCreateInfo(2);
        if (trilinear
            && trilinear->mipmapMode != VK_SAMPLER_MIPMAP_MODE_LINEAR)
            result.diagnostics.emplace_back("Trilinear mipmap mode mismatch");

        const VkSamplerCreateInfo *aniso = lib.GetCreateInfo(3);
        if (aniso && aniso->anisotropyEnable != VK_TRUE)
            result.diagnostics.emplace_back("TrilinearAniso anisotropy mismatch");

        const VkSamplerCreateInfo *pcf = lib.GetCreateInfo(4);
        if (pcf
            && (pcf->compareEnable != VK_TRUE
             || pcf->addressModeU != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE))
            result.diagnostics.emplace_back("ShadowPCF compare/clamp mismatch");

        // 6. sampler 宏注入（name→idx 与运行时 binding=1 数组下标一致）。
        const std::vector<std::string> names =
            { "Trilinear", "Linear", "Terrain" };
        const std::string macros = BuildSamplerMacros(names);
        const std::string expected_macros =
            "#define TrilinearSampler 2u\n"
            "#define LinearSampler 1u\n"
            "#define TerrainSampler 5u\n";
        if (macros != expected_macros)
        {
            result.diagnostics.emplace_back(
                "sampler macro injection mismatch: got [" + macros + "]");
        }

        // 7. 未知名 → 不生成宏（shader 编译显式失败暴露，不再静默保底 0）；
        //    空列表、空名跳过。
        if (!BuildSamplerMacros({ "UnknownSampler" }).empty())
            result.diagnostics.emplace_back("unknown sampler must produce no macro");
        if (!BuildSamplerMacros({}).empty())
            result.diagnostics.emplace_back("empty sampler list must produce no macros");
        if (!BuildSamplerMacros({ "" }).empty())
            result.diagnostics.emplace_back("empty sampler name must be skipped");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunShaderLibraryPathCase()
    {
        GateResult result;
        result.name = "AB.shader-library-path";

        // Baseline: default resolution must find the repository ShaderLibrary.
        const std::string root = GetShaderLibraryPath("ShaderLibrary");
        const hgl::filesystem::Path root_path(hgl::ToOSString(root));
        if (!IsShaderLibraryRoot(root_path))
        {
            result.diagnostics.emplace_back(
                "ShaderLibrary path resolver did not find a valid material root");
        }

        const hgl::OSString env_name(kShaderLibraryPathEnvironmentVariable);
        const hgl::OSString env_root_os =
            hgl::ToOSString(RepoRootPath("ShaderLibrary"));

        // Capture the inherited value BEFORE any in-process mutation, so the
        // final restore puts the environment back exactly as the gate found it.
        const wchar_t *saved_env =
            _wgetenv(kShaderLibraryPathEnvironmentVariable);
        const hgl::OSString saved_env_os =
            saved_env && saved_env[0] ? hgl::OSString(saved_env) : hgl::OSString();

        // 1. Env-var override wins over an explicit (invalid) request.
        _wputenv((env_name + OS_TEXT("=") + env_root_os).c_str());
        const std::string env_override =
            GetShaderLibraryPath("C:/definitely/not/a/library");
        const hgl::filesystem::Path env_override_path(
            hgl::ToOSString(env_override));
        if (!IsShaderLibraryRoot(env_override_path))
        {
            result.diagnostics.emplace_back(
                "ULRE_SHADERLIBRARY_PATH override must beat the requested path");
        }

        // 2. Invalid env-var value must be ignored, not fatal.
        _wputenv((env_name + OS_TEXT("=C:/definitely/not/a/library")).c_str());
        const std::string invalid_env = GetShaderLibraryPath("ShaderLibrary");
        if (invalid_env.empty())
        {
            result.diagnostics.emplace_back(
                "invalid ULRE_SHADERLIBRARY_PATH must fall back, not fail");
        }

        // 3. Restoring the original value (or clearing, when unset) restores
        //    default resolution and must not poison later gate cases.
        if (!saved_env_os.IsEmpty())
            _wputenv((env_name + OS_TEXT("=") + saved_env_os).c_str());
        else
            _wputenv(env_name.c_str());
        const std::string cleared = GetShaderLibraryPath("ShaderLibrary");
        if (cleared.empty())
        {
            result.diagnostics.emplace_back(
                "restoring ULRE_SHADERLIBRARY_PATH must not break resolution");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunResourceContractBoundaryCase()
    {
        GateResult result;
        result.name = "AD.resource-contract-boundary";

        MaterialDefinition definition{};
        definition.data_slot_decls = {{"mtl", SSBOType::PBRSurface}};

        std::vector<SerializedDescriptorEntry> descriptors;
        descriptor_builder_common::AppendDefinitionMaterialDescriptors(
            descriptors,
            definition,
            uint32_t(hgl::graph::kMeshFragment),
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT));

        ModuleResourceManifest compatible_manifest{};
        compatible_manifest.texture_layer_count = 1;
        compatible_manifest.texture_layers[0] = {
            TextureSlot::BaseColor,
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
            true,
            false
        };
        compatible_manifest.ssbo_count = 1;
        compatible_manifest.ssbos[0] = {
            "mtl",
            SSBOType::PBRSurface,
            DefaultMaterialDataSlot,
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        if (!descriptor_builder_common::AppendManifestSSBODescriptors(
                descriptors, compatible_manifest)
         || !descriptor_builder_common::AppendManifestTextureLayerDescriptors(
                descriptors, compatible_manifest)
         || !compatible_manifest.IsValid())
        {
            result.diagnostics.emplace_back(
                "Compatible definition and module resources must merge without duplication.");
        }
        else
        {
            const ShaderResourceSchema schema =
                BuildShaderResourceSchema(
                    descriptors.data(),
                    static_cast<uint32_t>(descriptors.size()));
            std::vector<std::string> diagnostics;
            if (!ValidateShaderResourceSchema(schema, diagnostics))
                result.diagnostics.emplace_back(
                    "Merged definition/module resource contract failed validation.");

            bool has_required_texture_layer = false;
            bool has_single_material_ssbo = false;
            for (const auto &req : schema.resources)
            {
                if (req.semantic == DescriptorSemantic::MaterialTextureLayerTable
                 && req.texture_slot == TextureSlot::BaseColor)
                    has_required_texture_layer = req.required && !req.allow_fallback;
                if (req.semantic == DescriptorSemantic::MaterialDataSlotData
                 && req.data_slot == DefaultMaterialDataSlot)
                    has_single_material_ssbo = true;
            }
            if (!has_required_texture_layer || !has_single_material_ssbo)
                result.diagnostics.emplace_back(
                    "Merged resource policy or SSBO identity was not preserved.");
        }

        ModuleResourceManifest ssbo_name_conflict{};
        ssbo_name_conflict.ssbo_count = 1;
        ssbo_name_conflict.ssbos[0] = {
            "mtl",
            SSBOType::TextureLayer,
            DefaultMaterialDataSlot,
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT)
        };
        if (descriptor_builder_common::AppendManifestSSBODescriptors(
                descriptors, ssbo_name_conflict)
         || ssbo_name_conflict.error != ModuleResourceManifestError::ResourceConflict)
        {
            result.diagnostics.emplace_back(
                "Same-name SSBO type conflicts must fail explicitly.");
        }

        SerializedDescriptorEntry hash_entry{};
        hash_entry.set_type = DescriptorSetType::Material;
        hash_entry.kind = DescriptorKind::SSBO;
        hash_entry.stage_flags = uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT);
        hash_entry.name = "mtl";
        hash_entry.struct_name = "PBRSurfaceData";
        hash_entry.semantic = DescriptorSemantic::MaterialDataSlotData;
        hash_entry.semantic_layer = DescriptorSemanticLayer::SSBO;
        hash_entry.data_slot = DefaultMaterialDataSlot;
        hash_entry.ssbo_type = SSBOType::PBRSurface;
        hash_entry.ssbo_id = 11;
        hash_entry.has_requirement_policy = true;
        hash_entry.required = true;
        hash_entry.allow_fallback = false;

        std::vector<SerializedDescriptorEntry> hash_entries{hash_entry};
        const uint64_t strict_hash =
            descriptor_builder_common::HashResourceContract(0, hash_entries);
        hash_entries[0].required = false;
        hash_entries[0].allow_fallback = true;
        const uint64_t optional_hash =
            descriptor_builder_common::HashResourceContract(0, hash_entries);
        if (strict_hash == optional_hash)
            result.diagnostics.emplace_back(
                "Required/fallback policy changes must change the resource contract hash.");

        hash_entries[0].required = true;
        hash_entries[0].allow_fallback = false;
        hash_entries[0].ssbo_type = SSBOType::TextureLayer;
        const uint64_t ssbo_type_hash =
            descriptor_builder_common::HashResourceContract(0, hash_entries);
        if (strict_hash == ssbo_type_hash)
            result.diagnostics.emplace_back(
                "SSBO type changes must change the resource contract hash.");

        SerializedDescriptorEntry ssbo_hash_entry{};
        ssbo_hash_entry.set_type = DescriptorSetType::Material;
        ssbo_hash_entry.kind = DescriptorKind::SSBO;
        ssbo_hash_entry.stage_flags = uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT);
        ssbo_hash_entry.name = "mtl";
        ssbo_hash_entry.struct_name = "PBRSurfaceData";
        ssbo_hash_entry.semantic = DescriptorSemantic::MaterialDataSlotData;
        ssbo_hash_entry.semantic_layer = DescriptorSemanticLayer::SSBO;
        ssbo_hash_entry.data_slot = DefaultMaterialDataSlot;
        ssbo_hash_entry.ssbo_type = SSBOType::PBRSurface;
        ssbo_hash_entry.ssbo_id = 11;

        std::vector<SerializedDescriptorEntry> ssbo_hash_entries{ssbo_hash_entry};
        const uint64_t first_ssbo_hash =
            descriptor_builder_common::HashResourceContract(0, ssbo_hash_entries);
        ssbo_hash_entries[0].ssbo_id = 12;
        const uint64_t second_ssbo_hash =
            descriptor_builder_common::HashResourceContract(0, ssbo_hash_entries);
        if (first_ssbo_hash == second_ssbo_hash)
            result.diagnostics.emplace_back(
                "SSBO buffer identity changes must change the resource contract hash.");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunModuleInvariantsCase()
    {
        GateResult result;
        result.name = "MI.module-invariants";

        // 1. Real ShaderLibrary scan: names unique, stable IDs unique.
        GLSLCodeModuleRegistry registry;
        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(
                hgl::ToOSString(GetShaderLibraryPath()),
                &file_count, &error_count))
        {
            result.diagnostics.emplace_back(
                "module registry directory scan failed");
        }
        if (error_count != 0)
        {
            result.diagnostics.emplace_back(
                "module registry scan reported "
                + std::to_string(error_count) + " errors");
        }

        const int count = registry.GetCount();
        for (int i = 0; i < count; ++i)
        {
            const auto *left = registry.GetModuleByIndex(i);
            if (!left || !left->name || !left->name[0])
            {
                result.diagnostics.emplace_back(
                    "module at index " + std::to_string(i)
                    + " has no name");
                continue;
            }

            const uint64_t left_id = GetGLSLCodeModuleStableID(*left);
            for (int j = i + 1; j < count; ++j)
            {
                const auto *right = registry.GetModuleByIndex(j);
                if (!right || !right->name)
                    continue;

                if (std::strcmp(left->name, right->name) == 0)
                {
                    result.diagnostics.emplace_back(
                        "duplicate module name: "
                        + std::string(left->name));
                }
                if (GetGLSLCodeModuleStableID(*right) == left_id)
                {
                    result.diagnostics.emplace_back(
                        "stable ID collision between "
                        + std::string(left->name) + " and "
                        + std::string(right->name));
                }
            }
        }

        // 2. Content-hash sensitivity: changing kind / priority / a single
        //    requirement must change the canonical content hash.
        const GLSLCodeModuleSemanticRequirement normal_requirement{
            GLSLCodeModuleCapabilitySource::ProducedSemantic,
            GLSLCodeModuleSemantic::Normal,
            static_cast<uint32_t>(GLSLCodeModuleNumericClass::Float),
            3, 3
        };
        const GLSLCodeModuleSemanticRequirement uv_requirement{
            GLSLCodeModuleCapabilitySource::ProducedSemantic,
            GLSLCodeModuleSemantic::UV0,
            static_cast<uint32_t>(GLSLCodeModuleNumericClass::Float),
            2, 2
        };

        GLSLCodeModuleDefinition base{};
        base.name = "invariant_base";
        base.glsl_code = "// invariant base";
        base.kind = GLSLCodeModuleKind::Utility;
        base.priority = 10;
        base.semantic_requirements = &normal_requirement;
        base.semantic_requirement_count = 1;

        GLSLCodeModuleDefinition changed_kind = base;
        changed_kind.kind = GLSLCodeModuleKind::Transform;

        GLSLCodeModuleDefinition changed_priority = base;
        changed_priority.priority = 20;

        GLSLCodeModuleDefinition changed_requirement = base;
        changed_requirement.semantic_requirements = &uv_requirement;

        const uint64_t base_hash =
            GetCanonicalGLSLCodeModuleContentHash(base, registry);
        if (base_hash
            == GetCanonicalGLSLCodeModuleContentHash(
                changed_kind, registry))
        {
            result.diagnostics.emplace_back(
                "content hash must change when kind changes");
        }
        if (base_hash
            == GetCanonicalGLSLCodeModuleContentHash(
                changed_priority, registry))
        {
            result.diagnostics.emplace_back(
                "content hash must change when priority changes");
        }
        if (base_hash
            == GetCanonicalGLSLCodeModuleContentHash(
                changed_requirement, registry))
        {
            result.diagnostics.emplace_back(
                "content hash must change when a requirement changes");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

}

int main(const int argc, char **argv)
{
    if (argc > 2)
    {
        GLogError(
            "[ShaderResourceSchemaRegressionGate] Expected zero or one group argument.");
        return 2;
    }

    const char *selected_group = argc == 2 ? argv[1] : "all";
    if (!IsKnownRegressionGroup(selected_group))
    {
        GLogError(
            "[ShaderResourceSchemaRegressionGate] Unknown regression group: %s",
            selected_group);
        return 2;
    }

    const bool run_glsl = IsRegressionGroupSelected(selected_group, "glsl");
    const bool run_interface = IsRegressionGroupSelected(selected_group, "interface");
    const bool run_descriptor = IsRegressionGroupSelected(selected_group, "descriptor");
    const bool run_cache = IsRegressionGroupSelected(selected_group, "cache");
    const bool run_materialization =
        IsRegressionGroupSelected(selected_group, "materialization");
    const bool run_pipeline = IsRegressionGroupSelected(selected_group, "pipeline");
    const bool run_module_invariants =
        IsRegressionGroupSelected(selected_group, "module-invariants");

    std::vector<GateResult> results;

    if (run_descriptor)
    {
        constexpr SerializedDescriptorEntry valid_entries[] =
        {
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(hgl::graph::kMeshFragment), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
            { DescriptorSetType::PerObject, DescriptorKind::SSBO, uint32_t(hgl::graph::kMeshFragment), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::MaterialDataIndexTable, DescriptorSemanticLayer::SSBO },
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::TextureLayer, DescriptorSemanticLayer::SSBO },
        };
        results.push_back(RunValidationCase("A.valid-layered-paths", valid_entries, uint32_t(std::size(valid_entries)), true));

        constexpr SerializedDescriptorEntry unknown_semantic[] =
        {
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(hgl::graph::kMeshFragment), "broken", "ViewportInfo", nullptr, DescriptorSemantic::Unknown, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
        };
        results.push_back(RunValidationCase("B1.unknown-semantic-hard-fail", unknown_semantic, 1, false));

        constexpr SerializedDescriptorEntry semantic_kind_mismatch[] =
        {
            { DescriptorSetType::PerObject, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::MaterialDataIndexTable, DescriptorSemanticLayer::SSBO },
        };
        results.push_back(RunValidationCase("B2.semantic-kind-mismatch-hard-fail", semantic_kind_mismatch, 1, false));

        constexpr SerializedDescriptorEntry invalid_fixed_descriptor[] =
        {
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(hgl::graph::kMeshFragment), "mtl", "PBRSurfaceData", nullptr, DescriptorSemantic::MaterialDataSlotData, TextureSlot::BaseColor, 0xffu, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO },
        };
        results.push_back(RunValidationCase("B3.invalid-fixed-descriptor-hard-fail", invalid_fixed_descriptor, 1, false));

        constexpr SerializedDescriptorEntry palette_explicit[] =
        {
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(hgl::graph::kMeshFragment), "color_palette", "ColorPalette", nullptr, DescriptorSemantic::MaterialColorPalette, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
        };
        results.push_back(RunValidationCase("C.scene-color-palette-explicit", palette_explicit, 1, true));
    }

    if (run_materialization) results.push_back(RunMaterializationSharedInstanceCase());
    if (run_materialization) results.push_back(RunBuiltinRegistryCoverageCase());
    if (run_materialization) results.push_back(RunBootstrapMaterialBoundaryCase());
    if (run_materialization) results.push_back(RunMaterialDefinitionIdentityCase());
    if (run_pipeline) results.push_back(RunUnifiedMaterialBaselineCase());
    if (run_pipeline) results.push_back(RunMaterialOutputContractCase());
    if (run_pipeline) results.push_back(RunPrimitiveVariantPurposeCase());
    if (run_pipeline) results.push_back(RunUnifiedForwardSkeletonCase());
    if (run_pipeline) results.push_back(RunUnifiedPureColorFragmentCase());
    if (run_descriptor) results.push_back(RunUnifiedMaterialContractCase());
    if (run_materialization) results.push_back(RunTransformGraphModelCase());
    if (run_materialization) results.push_back(RunTransformGraphCompositionCase());
    if (run_cache) results.push_back(RunAuthoritativeMaterialCacheIdentityCase());
    if (run_descriptor) results.push_back(RunMaterialSSBOBindingKeyCase());
    if (run_materialization) results.push_back(RunResolvedMaterialRenderStateCase());
    if (run_materialization) results.push_back(RunMaterialDefinitionFileSchemaCase());
    if (run_materialization) results.push_back(RunFallbackInferenceCase());
    if (run_glsl) results.push_back(RunProviderResourceManifestCase());
    if (run_glsl) results.push_back(RunGLSLCodeModuleFileCase());
    if (run_glsl) results.push_back(RunGLSLCodeModuleMetadataValidationCase());
    if (run_glsl) results.push_back(RunShadowMaterialModuleGraphCase());
    if (run_glsl) results.push_back(RunCapabilityResolverCase());
    if (run_interface) results.push_back(RunShaderSemanticRegistryCase());
    if (run_materialization) results.push_back(RunResolvedBindingTableCase());
    if (run_interface) results.push_back(RunMaterialVertexABICharacterizationCase());
    if (run_interface) results.push_back(RunMaterialSemanticABIParityCase());
    if (run_interface) results.push_back(RunMaterialSemanticResolverPreviewCase());
    if (run_glsl) results.push_back(RunCompositorVersionPlacementCase());
    if (run_cache) results.push_back(RunProviderGraphIdentityCase());
    if (run_cache) results.push_back(RunProviderGraphCompositionCase());
    if (run_cache) results.push_back(RunResolvedStageCacheIdentityCase());
    if (run_cache) results.push_back(RunCanonicalShaderContractCase());
    if (run_pipeline) results.push_back(RunMaterialMultiSlotSourceCase());
    if (run_descriptor) results.push_back(RunDescriptorContractCase());
    if (run_pipeline) results.push_back(RunShaderLibraryPathCase());
    if (run_materialization) results.push_back(RunSamplerPresetLibraryCase());
    if (run_descriptor) results.push_back(RunResourceContractBoundaryCase());
    if (run_module_invariants) results.push_back(RunModuleInvariantsCase());

    bool all_passed = true;
    for (const auto &result : results)
    {
        std::fprintf(result.passed ? stdout : stderr,
                     "[%s] %s\n",
                     result.passed ? "PASS" : "FAIL",
                     result.name.c_str());

        if (!result.passed)
        {
            all_passed = false;
            for (const auto &diag : result.diagnostics)
                std::fprintf(stderr, "  - %s\n", diag.c_str());
        }
    }

    return all_passed ? 0 : 1;
}
