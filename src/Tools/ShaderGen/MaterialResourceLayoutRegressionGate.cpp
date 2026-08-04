#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/graph/glsl/GLSLCodeModule.h>
#include <hgl/graph/glsl/GLSLCodeModuleCapabilityResolver.h>
#include <hgl/graph/glsl/GLSLCodeModuleFile.h>
#include <hgl/graph/glsl/GLSLCodeModuleRegistry.h>
#include <hgl/graph/glsl/ShaderResourceManifest.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include "../../ShaderGen/common/VertexBuilderCommon.h"

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

using namespace hgl::graph;
using namespace hgl::graph::mtl;

namespace
{
    struct GateResult
    {
        std::string name;
        bool passed = false;
        std::vector<std::string> diagnostics;
    };

    static std::vector<std::string> SummarizeConstraintShape(const MaterialResourceLayout &contract)
    {
        std::vector<std::string> rows;
        rows.reserve(contract.requirements.size());

        for (const auto &req : contract.requirements)
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
            row += std::to_string(req.ssbo_slot);
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
                                        const FixedDescriptorEntry *entries,
                                        const uint32_t count,
                                        const bool expected_pass)
    {
        GateResult result;
        result.name = name ? name : "<unnamed>";

        const MaterialResourceLayout contract = BuildMaterialResourceLayout(entries, count);
        result.passed = (ValidateMaterialResourceLayout(contract, result.diagnostics) == expected_pass);
        return result;
    }

    static bool CheckVertexEntries(const std::vector<FixedVertexEntry> &actual,
                                   const std::vector<FixedVertexEntry> &expected,
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
                                       const std::vector<FixedVertexEntry> &expected)
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
            BUILTIN_MTL_DEF_FALLBACK_2D,
            BUILTIN_MTL_DEF_FALLBACK_3D,
            BUILTIN_MTL_DEF_MISSING_MATERIAL,
            BUILTIN_MTL_DEF_TEXT,
            BUILTIN_MTL_DEF_SKY,
            "Standard",
            "StandardTextureArray",
            "Gizmo3D",
            "RectTexture2D",
            "RectTexture2DArray"
        };
        for (const char *id : builtin_ids)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(id, definition))
            {
                result.diagnostics.emplace_back(std::string("missing built-in definition: ") + id);
                continue;
            }
            if (definition.vertex_attributes.IsEmpty()
             || definition.vertex_attributes[0].semantic != VertexSemantic::Position
             || definition.vertex_stage.stage != ShaderStage::Vertex
             || definition.fragment_stage.stage != ShaderStage::Fragment)
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
            "PureColor2D",
            "PureColor3D",
            "Text2D"
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

            if (definition.vertex_semantic_requirements.GetCount()
                    != definition.vertex_attributes.GetCount()
             || definition.vertex_stage.inputs.GetCount()
                    != definition.vertex_attributes.GetCount())
            {
                result.diagnostics.emplace_back(std::string("semantic/legacy input count mismatch: ") + id);
                continue;
            }

            GeometryVertexFormat geometry;
            std::vector<vertex_builder_common::VertexSemanticDecl> semantic_decls;
            std::vector<FixedVertexEntry> expected_entries;
            bool valid = true;
            for (int i = 0; i < definition.vertex_attributes.GetCount(); ++i)
            {
                const auto &attribute = definition.vertex_attributes[i];
                const auto &requirement = definition.vertex_semantic_requirements[i];
                if (requirement.source != GLSLCodeModuleCapabilitySource::ProducedSemantic
                 || requirement.semantic != GetGLSLCodeModuleSemanticFromVertexSemantic(attribute.semantic)
                 || requirement.semantic == GLSLCodeModuleSemantic::Unknown
                 || !geometry.Add(attribute.semantic, attribute.format))
                {
                    valid = false;
                    break;
                }

                semantic_decls.push_back({attribute.semantic, attribute.format});
                expected_entries.push_back({attribute.format, attribute.semantic});
            }

            if (!valid)
            {
                result.diagnostics.emplace_back(std::string("invalid semantic contract: ") + id);
                continue;
            }

            const vertex_builder_common::VertexBuildInput input{
                PrimitiveType::Triangles, &geometry, semantic_decls.data(),
                static_cast<uint32_t>(semantic_decls.size())
            };
            const auto entries = vertex_builder_common::BuildVertexEntries(input);
            std::string diagnostic;
            if (!CheckVertexEntries(entries, expected_entries, diagnostic))
                result.diagnostics.emplace_back(std::string("legacy ABI parity failed for ")
                    + id + ": " + diagnostic);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunBindlessEquivalenceCase()
    {
        GateResult result;
        result.name = "D.bindless-dual-form-equivalence";

        constexpr FixedDescriptorEntry standard_entries[] =
        {
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo },
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo },
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr, DescriptorSemantic::SkyInfo },
            { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld },
            { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable },
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "PBRSurfaceData", nullptr, DescriptorSemantic::MaterialSSBOSlotData, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::PBRSurface },
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialSSBOIndexTable },
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable },
            { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::BaseColor },
            { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::Normal },
            { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::Roughness },
        };

        constexpr FixedDescriptorEntry array_entries[] =
        {
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo },
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo },
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr, DescriptorSemantic::SkyInfo },
            { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld },
            { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable },
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "PBRSurfaceData", nullptr, DescriptorSemantic::MaterialSSBOSlotData, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::PBRSurface },
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialSSBOIndexTable },
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable },
            { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2DArray", DescriptorSemantic::MaterialTexture, TextureSlot::BaseColor },
            { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2DArray", DescriptorSemantic::MaterialTexture, TextureSlot::Normal },
            { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2DArray", DescriptorSemantic::MaterialTexture, TextureSlot::Roughness },
        };

        std::vector<std::string> diagnostics_standard;
        std::vector<std::string> diagnostics_array;
        const MaterialResourceLayout standard_contract = BuildMaterialResourceLayout(standard_entries, uint32_t(std::size(standard_entries)));
        const MaterialResourceLayout array_contract = BuildMaterialResourceLayout(array_entries, uint32_t(std::size(array_entries)));

        const bool standard_ok = ValidateMaterialResourceLayout(standard_contract, diagnostics_standard);
        const bool array_ok = ValidateMaterialResourceLayout(array_contract, diagnostics_array);
        if (!standard_ok || !array_ok)
        {
            result.passed = false;
            result.diagnostics = !standard_ok ? diagnostics_standard : diagnostics_array;
            return result;
        }

        const auto standard_shape = SummarizeConstraintShape(standard_contract);
        const auto array_shape = SummarizeConstraintShape(array_contract);
        if (standard_shape != array_shape)
        {
            result.passed = false;
            result.diagnostics.emplace_back("Bindless dual forms produced different normalized constraint shapes.");
            return result;
        }

        result.passed = true;
        return result;
    }

    static GateResult RunBuiltinRegistryCoverageCase()
    {
        GateResult result;
        result.name = "E.builtin-registry-coverage";

        struct ExpectedEntry
        {
            const char *definition_id;
            BuiltinMaterialCreatorID builtin_id;
        };

        static const ExpectedEntry expected[] =
        {
            { BUILTIN_MTL_DEF_FALLBACK_2D, BuiltinMaterialCreatorID::PureColor2D },
            { BUILTIN_MTL_DEF_FALLBACK_3D, BuiltinMaterialCreatorID::PureColor3D },
            { BUILTIN_MTL_DEF_MISSING_MATERIAL, BuiltinMaterialCreatorID::PureColor3D },
            { BUILTIN_MTL_DEF_TEXT, BuiltinMaterialCreatorID::Text2D },
            { BUILTIN_MTL_DEF_SKY, BuiltinMaterialCreatorID::SkyMinimal },
            { "Standard", BuiltinMaterialCreatorID::Standard },
            { "StandardTextureArray", BuiltinMaterialCreatorID::StandardTextureArray },
            { "Gizmo3D", BuiltinMaterialCreatorID::Gizmo3D },
            { "RectTexture2D", BuiltinMaterialCreatorID::RectTexture2D },
            { "RectTexture2DArray", BuiltinMaterialCreatorID::RectTexture2DArray },
        };

        for (const auto &entry : expected)
        {
            MaterialDefinition bmi{};
            if (!TryGetMaterialDefinitionByID(entry.definition_id, bmi))
            {
                result.diagnostics.emplace_back(std::string("Missing material definition: ") + entry.definition_id);
                continue;
            }

            if (bmi.builtin_creator_id != static_cast<uint32_t>(entry.builtin_id))
            {
                result.diagnostics.emplace_back(std::string("Builtin creator mismatch: ") + entry.definition_id);
                continue;
            }

            if (bmi.definition_name.empty())
                result.diagnostics.emplace_back(std::string("Empty material definition name: ") + entry.definition_id);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunFallbackInferenceCase()
    {
        GateResult result;
        result.name = "F.fallback-dimension-inference";

        GeometryVertexFormat gvf_2d{};
        gvf_2d.Add(VertexSemantic::Position, VK_FORMAT_R32G32_SFLOAT, 2, sizeof(float) * 2);

        GeometryVertexFormat gvf_3d{};
        gvf_3d.Add(VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT, 3, sizeof(float) * 3);

        MaterialDefinitionBuildRequest request_2d{};
        request_2d.geometry_vertex_format = &gvf_2d;

        MaterialDefinitionBuildRequest request_3d{};
        request_3d.geometry_vertex_format = &gvf_3d;

        MaterialDefinitionBuildRequest request_unknown{};

        if (!ShouldUse2DFallbackMaterial(request_2d))
            result.diagnostics.emplace_back("2D position format must select fallback_2d.");

        if (ShouldUse2DFallbackMaterial(request_3d))
            result.diagnostics.emplace_back("3D position format must not select fallback_2d.");

        if (ShouldUse2DFallbackMaterial(request_unknown))
            result.diagnostics.emplace_back("Missing geometry hint must default to 3D fallback.");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunGLSLCodeModuleRegistryCase()
    {
        GateResult result;
        result.name = "G.glsl-code-module-registry";

        const GLSLCodeModuleDefinition *header =
            FindGLSLCodeModuleDefinition(GLSLCodeModuleID::SkyLightHeader);
        const GLSLCodeModuleDefinition *simple =
            FindGLSLCodeModuleDefinition(GLSLCodeModuleID::SkyLightSimple);
        const GLSLCodeModuleDefinition *cubemap =
            FindGLSLCodeModuleDefinition(GLSLCodeModuleID::SkyLightCubeMap);

        if (!header || !header->glsl_code || !header->glsl_code[0])
            result.diagnostics.emplace_back("SkyLightHeader module is not registered with GLSL code.");

        if (!simple || !simple->glsl_code || !simple->glsl_code[0])
            result.diagnostics.emplace_back("SkyLightSimple module is not registered with GLSL code.");
        else
        {
            if (simple->ubo_requirement_count != 1
             || !simple->ubo_requirements
             || simple->ubo_requirements[0].semantic != UBODescriptorSemantic::SkyInfo)
                result.diagnostics.emplace_back("SkyLightSimple must require exactly SkyInfo.");

            if (simple->texture_requirement_count != 0)
                result.diagnostics.emplace_back("SkyLightSimple must not require a cubemap texture.");

            if (simple->code_module_requirement_count != 1
             || !simple->code_module_requirements
             || simple->code_module_requirements[0] != GLSLCodeModuleID::SkyLightHeader)
                result.diagnostics.emplace_back("SkyLightSimple must depend on SkyLightHeader.");
        }

        if (!cubemap || !cubemap->glsl_code || !cubemap->glsl_code[0])
            result.diagnostics.emplace_back("SkyLightCubeMap module is not registered with GLSL code.");
        else
        {
            if (cubemap->ubo_requirement_count != 1
             || !cubemap->ubo_requirements
             || cubemap->ubo_requirements[0].semantic != UBODescriptorSemantic::SkyInfo)
                result.diagnostics.emplace_back("SkyLightCubeMap must require exactly SkyInfo.");

            if (cubemap->texture_requirement_count != 1
             || !cubemap->texture_requirements
             || cubemap->texture_requirements[0].semantic != DescriptorSemantic::SkyCubemapSampler
             || cubemap->texture_requirements[0].slot != TextureSlot::Custom0
             || !cubemap->texture_requirements[0].glsl_type
             || std::string(cubemap->texture_requirements[0].glsl_type) != "samplerCube")
                result.diagnostics.emplace_back("SkyLightCubeMap must require SkyCubemap samplerCube.");

            if (cubemap->code_module_requirement_count != 1
             || !cubemap->code_module_requirements
             || cubemap->code_module_requirements[0] != GLSLCodeModuleID::SkyLightHeader)
                result.diagnostics.emplace_back("SkyLightCubeMap must depend on SkyLightHeader.");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunShaderResourceManifestCase()
    {
        GateResult result;
        result.name = "H.shader-resource-manifest";

        constexpr GLSLCodeModuleID roots[] =
        {
            GLSLCodeModuleID::SkyLightSimple,
            GLSLCodeModuleID::SkyLightCubeMap
        };

        ShaderResourceManifest manifest{};
        if (!BuildShaderResourceManifest(manifest.code_modules, 0, manifest))
            result.diagnostics.emplace_back("Empty root list must produce a valid manifest.");

        if (!BuildShaderResourceManifest(roots, uint32_t(std::size(roots)), manifest))
        {
            result.diagnostics.emplace_back(
                std::string("Combined SkyLight manifest failed: ")
                + GetShaderResourceManifestErrorName(manifest.error));
        }
        else
        {
            if (manifest.code_module_count != 3)
                result.diagnostics.emplace_back("Shared SkyLightHeader must be deduplicated.");

            if (manifest.ubo_count != 1
             || manifest.ubos[0].semantic != UBODescriptorSemantic::SkyInfo)
                result.diagnostics.emplace_back("Combined SkyLight modules must produce one SkyInfo UBO.");

            if (manifest.texture_count != 1
             || manifest.textures[0].semantic != DescriptorSemantic::SkyCubemapSampler)
                result.diagnostics.emplace_back("Combined SkyLight modules must produce one SkyCubemap texture.");

            if (manifest.stable_hash == 0)
                result.diagnostics.emplace_back("Valid manifest must produce a non-zero stable hash.");
        }

        ShaderResourceManifest simple_manifest{};
        ShaderResourceManifest cubemap_manifest{};
        const GLSLCodeModuleID simple_root = GLSLCodeModuleID::SkyLightSimple;
        const GLSLCodeModuleID cubemap_root = GLSLCodeModuleID::SkyLightCubeMap;
        if (!BuildShaderResourceManifest(&simple_root, 1, simple_manifest)
         || !BuildShaderResourceManifest(&cubemap_root, 1, cubemap_manifest))
        {
            result.diagnostics.emplace_back("Individual SkyLight manifests must resolve.");
        }
        else if (simple_manifest.stable_hash == cubemap_manifest.stable_hash)
        {
            result.diagnostics.emplace_back("Different SkyLight resource closures must hash differently.");
        }

        if (BuildShaderResourceManifest(nullptr, 1, manifest)
         || manifest.error != ShaderResourceManifestError::NullRootList)
            result.diagnostics.emplace_back("Null root list must fail explicitly.");

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
            "// @ulre flags 0x3\n"
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
                if (data.flags != 0x3u)
                    result.diagnostics.emplace_back("full-metadata flags mismatch");
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

        // Registry scan of the real ShaderLibrary directory.
        GLSLCodeModuleRegistry registry;
        if (!registry.RegisterBuiltinModules())
            result.diagnostics.emplace_back("RegisterBuiltinModules failed");

        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(OS_TEXT("E:/ULRE/ShaderLibrary"), &file_count, &error_count))
            result.diagnostics.emplace_back("LoadDirectory failed to scan directory");
        else
        {
            if (file_count != 54)
                result.diagnostics.emplace_back("LoadDirectory expected 54 file modules, got "
                    + std::to_string(file_count));
            if (error_count != 0)
                result.diagnostics.emplace_back("LoadDirectory reported "
                    + std::to_string(error_count) + " errors");

            const int expected_count = 54 + int(GLSLCodeModuleID::RANGE_SIZE);
            if (registry.GetCount() != expected_count)
                result.diagnostics.emplace_back("registry count after LoadDirectory mismatch: got "
                    + std::to_string(registry.GetCount()));
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

        const auto *compositor_lit = registry.FindByName("main_forward_lit");
        if (!compositor_lit)
            result.diagnostics.emplace_back("main_forward_lit not found by name");
        else
        {
            if (compositor_lit->kind != GLSLCodeModuleKind::FragmentShader)
                result.diagnostics.emplace_back("main_forward_lit kind mismatch");
            if (compositor_lit->code_module_requirement_count != 4)
                result.diagnostics.emplace_back("main_forward_lit uses resolution expected 4 deps, got "
                    + std::to_string(compositor_lit->code_module_requirement_count));
        }

        // Re-scan must detect every duplicate name and keep counts stable.
        int dup_count = 0;
        int dup_errors = 0;
        if (!registry.LoadDirectory(OS_TEXT("E:/ULRE/ShaderLibrary"), &dup_count, &dup_errors))
            result.diagnostics.emplace_back("second LoadDirectory failed");
        else if (dup_count != 0 || dup_errors != 54)
            result.diagnostics.emplace_back("second LoadDirectory must report 54 duplicates, got files="
                + std::to_string(dup_count) + " errors=" + std::to_string(dup_errors));

        const int stable_count = 54 + int(GLSLCodeModuleID::RANGE_SIZE);
        if (registry.GetCount() != stable_count)
            result.diagnostics.emplace_back("registry count changed after duplicate re-scan: got "
                + std::to_string(registry.GetCount()));

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
            uint16_t next_id = 100;

            ResolverFixture()
            {
                if (!registry.RegisterBuiltinModules())
                    throw std::runtime_error("RegisterBuiltinModules failed");
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
                definition.id = static_cast<GLSLCodeModuleID>(next_id++);
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
                          const std::vector<GLSLCodeModuleSemantic> &resources,
                          const std::vector<GLSLCodeModuleSemantic> &options)
        {
            GLSLCodeModuleResolutionRequest request;
            request.requirements = reqs.empty() ? nullptr : reqs.data();
            request.requirement_count = uint32_t(reqs.size());
            request.geometry_capabilities = caps.empty() ? nullptr : caps.data();
            request.geometry_capability_count = uint32_t(caps.size());
            request.resources = resources.empty() ? nullptr : resources.data();
            request.resource_count = uint32_t(resources.size());
            request.options = options.empty() ? nullptr : options.data();
            request.option_count = uint32_t(options.size());

            const GLSLCodeModuleCapabilityResolver resolver;
            ResolveOutcome outcome;
            outcome.ok = resolver.Resolve(fixture.registry, request, outcome.result);
            return outcome;
        };

        const std::vector<GLSLCodeModuleSemantic> kNoResources;
        const std::vector<GLSLCodeModuleSemantic> kNoOptions;

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

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources, kNoOptions);
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

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources, kNoOptions);
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
            const auto *decode = fixture.Add("normal_decode_a2rgb10", GLSLCodeModuleKind::Decode, 100,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_NORM | RESOLVER_PACK) },
                { GLSLCodeModuleSemantic::Normal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);
            // Packed formats have no VF_ macro; vec_size must be explicit.
            gvf.Add(VertexSemantic::Normal, PF_A2RGB10UN, 4, 0);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources, kNoOptions);
            check(outcome.ok, "J3 packed-normal: resolution must succeed");
            check(has_selection(outcome.result, decode), "J3 packed-normal: expected decode provider selected");
            check(rejected_candidate(outcome.result, normal_only), "J3 packed-normal: float provider must be rejected");
        }

        // ------------------------------------------------------------------
        // 4. Position-derived sky NTB (geometry has only Position).
        {
            ResolverFixture fixture;
            const auto *ntb_from_position = fixture.Add("ntb_from_position", GLSLCodeModuleKind::Basis, 50,
                { geometry(GLSLCodeModuleSemantic::Position, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });
            fixture.Add("ntb_direct", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal),
                  geometry(GLSLCodeModuleSemantic::Tangent),
                  geometry(GLSLCodeModuleSemantic::Binormal) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources, kNoOptions);
            check(outcome.ok, "J4 sky-ntb: resolution must succeed");
            check(has_selection(outcome.result, ntb_from_position), "J4 sky-ntb: expected position-derived provider selected");
        }

        // ------------------------------------------------------------------
        // 5. Heightmap terrain NTB: with the HeightMap resource the dedicated
        //    provider wins over the plain position-derived fallback.
        {
            ResolverFixture fixture;
            fixture.Add("ntb_from_position", GLSLCodeModuleKind::Basis, 50,
                { geometry(GLSLCodeModuleSemantic::Position, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });
            const auto *ntb_heightmap = fixture.Add("ntb_from_heightmap", GLSLCodeModuleKind::Basis, 60,
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
                { GLSLCodeModuleSemantic::HeightMap },
                kNoOptions);
            check(outcome.ok, "J5 terrain-ntb: resolution must succeed");
            check(has_selection(outcome.result, ntb_heightmap), "J5 terrain-ntb: expected heightmap provider selected");
        }

        // ------------------------------------------------------------------
        // 6. Heightmap provider without the HeightMap resource must fail.
        {
            ResolverFixture fixture;
            fixture.Add("ntb_heightmap", GLSLCodeModuleKind::Basis, 60,
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
                kNoResources,
                kNoOptions);
            check(!outcome.ok, "J6 missing-resource: resolution must fail");
            check(!outcome.result.resolved, "J6 missing-resource: resolved flag must be clear");
            check(outcome.result.diagnostics.GetCount() > 0, "J6 missing-resource: diagnostics must be present");
        }

        // ------------------------------------------------------------------
        // 7. Octahedral RG8SN normal (2-component, normalized).
        {
            ResolverFixture fixture;
            const auto *octa = fixture.Add("normal_decode_octa", GLSLCodeModuleKind::Decode, 100,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_NORM, 2, 2) },
                { GLSLCodeModuleSemantic::Normal });
            fixture.Add("normal_only", GLSLCodeModuleKind::VertexInput, 100,
                { geometry(GLSLCodeModuleSemantic::Normal, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);
            gvf.Add(VertexSemantic::Normal, VF_V2SN8);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources, kNoOptions);
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

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources, kNoOptions);
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

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf), kNoResources, kNoOptions);
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

            const auto outcome32 = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf32), kNoResources, kNoOptions);
            const auto outcome16 = resolve(fixture, { produced(GLSLCodeModuleSemantic::Normal) }, ResolverBuildCaps(gvf16), kNoResources, kNoOptions);
            check(outcome32.ok && outcome16.ok, "J10 rg16f-rg32f: both resolutions must succeed");
            check(has_selection(outcome32.result, normal_only) && has_selection(outcome16.result, normal_only),
                  "J10 rg16f-rg32f: both formats must select the same provider");
        }

        // ------------------------------------------------------------------
        // 11. Provider composition: a WorldNormal provider depends on the
        //     produced Normal semantic; dependencies commit first.
        {
            ResolverFixture fixture;
            const auto *ntb_from_position = fixture.Add("ntb_from_position", GLSLCodeModuleKind::Basis, 50,
                { geometry(GLSLCodeModuleSemantic::Position, RESOLVER_FLOAT, 3, 3) },
                { GLSLCodeModuleSemantic::Normal, GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal });
            const auto *world_normal = fixture.Add("world_normal_from_normal", GLSLCodeModuleKind::Basis, 50,
                { produced(GLSLCodeModuleSemantic::Normal) },
                { GLSLCodeModuleSemantic::WorldNormal });

            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VF_V3F);

            const auto outcome = resolve(fixture, { produced(GLSLCodeModuleSemantic::WorldNormal) }, ResolverBuildCaps(gvf), kNoResources, kNoOptions);
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
            const auto *color_yuv = fixture.Add("color_decode_yuv", GLSLCodeModuleKind::Decode, 100,
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

            const auto outcome_yuv = resolve(fixture, { produced(GLSLCodeModuleSemantic::Color) }, yuv_caps, kNoResources, kNoOptions);
            check(outcome_yuv.ok, "J12 yuv: resolution must succeed");
            check(has_selection(outcome_yuv.result, color_yuv), "J12 yuv: expected YUV decoder selected");
            check(rejected_candidate(outcome_yuv.result, color_direct), "J12 yuv: direct RGBA provider must be rejected");

            std::vector<GLSLCodeModuleGeometryCapability> rgba_caps;
            GLSLCodeModuleGeometryCapability cap_color;
            cap_color.semantic = GLSLCodeModuleSemantic::Color;
            cap_color.numeric_class_mask = RESOLVER_NORM;
            cap_color.component_count = 4;
            rgba_caps.push_back(cap_color);

            const auto outcome_rgba = resolve(fixture, { produced(GLSLCodeModuleSemantic::Color) }, rgba_caps, kNoResources, kNoOptions);
            check(outcome_rgba.ok, "J12 rgba: resolution must succeed");
            check(has_selection(outcome_rgba.result, color_direct), "J12 rgba: expected direct color provider selected");
            check(!has_selection(outcome_rgba.result, color_yuv), "J12 rgba: YUV decoder must not be selected");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }
}

int main()
{
    std::vector<GateResult> results;

    constexpr FixedDescriptorEntry valid_entries[] =
    {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialSSBOIndexTable, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::MaterialSSBOIndexTable, DescriptorSemanticLayer::SSBO },
        { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::Texture },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "SkyCubemap", nullptr, "samplerCube", DescriptorSemantic::MaterialSampler, TextureSlot::Custom0, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::Sampler },
    };
    results.push_back(RunValidationCase("A.valid-layered-paths", valid_entries, uint32_t(std::size(valid_entries)), true));

    constexpr FixedDescriptorEntry unknown_semantic[] =
    {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "broken", "ViewportInfo", nullptr, DescriptorSemantic::Unknown, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
    };
    results.push_back(RunValidationCase("B1.unknown-semantic-hard-fail", unknown_semantic, 1, false));

    constexpr FixedDescriptorEntry semantic_kind_mismatch[] =
    {
        { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "mtl_data_index_rows", "DataIndexRows", "sampler2D", DescriptorSemantic::MaterialSSBOIndexTable, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::MaterialSSBOIndexTable, DescriptorSemanticLayer::SSBO },
    };
    results.push_back(RunValidationCase("B2.semantic-kind-mismatch-hard-fail", semantic_kind_mismatch, 1, false));

    constexpr FixedDescriptorEntry invalid_fixed_descriptor[] =
    {
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "PBRSurfaceData", nullptr, DescriptorSemantic::MaterialSSBOSlotData, TextureSlot::BaseColor, 0xffu, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO },
    };
    results.push_back(RunValidationCase("B3.invalid-fixed-descriptor-hard-fail", invalid_fixed_descriptor, 1, false));

    constexpr FixedDescriptorEntry palette_explicit[] =
    {
        { DescriptorSetType::Material, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "color_pattle", "ColorPattle", nullptr, DescriptorSemantic::MaterialColorPalette, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
    };
    results.push_back(RunValidationCase("C.material-color-palette-explicit", palette_explicit, 1, true));

    results.push_back(RunBindlessEquivalenceCase());
    results.push_back(RunBuiltinRegistryCoverageCase());
    results.push_back(RunFallbackInferenceCase());
    results.push_back(RunGLSLCodeModuleRegistryCase());
    results.push_back(RunShaderResourceManifestCase());
    results.push_back(RunGLSLCodeModuleFileCase());
    results.push_back(RunCapabilityResolverCase());
    results.push_back(RunMaterialVertexABICharacterizationCase());
    results.push_back(RunMaterialSemanticABIParityCase());

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
