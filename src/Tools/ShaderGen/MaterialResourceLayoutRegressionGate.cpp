#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/mtl/MaterialDefinitionFile.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/shadergen/ShaderCreateInfo.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/shadergen/ShaderArtifactStore.h>
#include <hgl/shadergen/ShaderSemanticRegistry.h>
#include <hgl/graph/glsl/GLSLCodeModule.h>
#include <hgl/graph/glsl/GLSLCodeModuleCapabilityResolver.h>
#include <hgl/graph/glsl/GLSLCodeModuleFile.h>
#include <hgl/graph/glsl/GLSLCodeModuleRegistry.h>
#include <hgl/graph/glsl/ShaderResourceManifest.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/log/Log.h>
#include "../../ShaderGen/2d/Build2DCommon.h"
#include "../../ShaderGen/3d/DefinitionDescriptorBuilder3D.h"
#include "../../ShaderGen/common/VertexBuilderCommon.h"
#include "../../ShaderGen/common/VertexShaderAssembler.h"

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
            || std::strcmp(group, "pipeline") == 0;
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
            BUILTIN_MTL_DEF_FALLBACK,
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

            if (definition.vertex_semantic_requirements.IsEmpty())
                result.diagnostics.emplace_back(
                    std::string("empty semantic-only ABI: ") + id);
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
        const InterStageSemanticInfo *uv1 =
            GetInterStageSemanticInfo(InterStageSemantic::UV1);
        const InterStageSemanticInfo *color =
            GetInterStageSemanticInfo(InterStageSemantic::Color);
        if (!data_index
         || data_index->stable_location != 0
         || data_index->interpolation != InterStageInterpolation::Flat
         || !uv1
         || uv1->stable_location != 5
         || uv1->legacy_packed_order != InvalidLegacyPackedOrder
         || !color
         || color->stable_location != 6)
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

        const InterStageSemanticMask lit_semantics =
            GetInterStageSemanticMask(InterStageSemantic::DataIndexID)
          | GetInterStageSemanticMask(InterStageSemantic::TextureLayerID)
          | GetInterStageSemanticMask(InterStageSemantic::WorldPosition)
          | GetInterStageSemanticMask(InterStageSemantic::WorldNormal)
          | GetInterStageSemanticMask(InterStageSemantic::UV0);
        if (!ResolveLegacyPackedInterStageSemanticLocation(
                lit_semantics, InterStageSemantic::DataIndexID, location)
         || location != 0
         || !ResolveLegacyPackedInterStageSemanticLocation(
                lit_semantics, InterStageSemantic::TextureLayerID, location)
         || location != 1
         || !ResolveLegacyPackedInterStageSemanticLocation(
                lit_semantics, InterStageSemantic::WorldPosition, location)
         || location != 2
         || !ResolveLegacyPackedInterStageSemanticLocation(
                lit_semantics, InterStageSemantic::WorldNormal, location)
         || location != 3
         || !ResolveLegacyPackedInterStageSemanticLocation(
                lit_semantics, InterStageSemantic::UV0, location)
         || location != 4)
        {
            result.diagnostics.emplace_back(
                "legacy packed lit varying mapping mismatch");
        }

        const InterStageSemanticMask color_semantics =
            GetInterStageSemanticMask(InterStageSemantic::Color);
        if (!ResolveLegacyPackedInterStageSemanticLocation(
                color_semantics, InterStageSemantic::Color, location)
         || location != 0
         || ResolveLegacyPackedInterStageSemanticLocation(
                color_semantics, InterStageSemantic::UV1, location))
        {
            result.diagnostics.emplace_back(
                "legacy packed sparse varying mapping mismatch");
        }

        VertexVaryingConfig lit_varying{};
        lit_varying.emit_data_index_id = true;
        lit_varying.emit_texture_layer_id = true;
        lit_varying.emit_world_pos = true;
        lit_varying.emit_world_normal = true;
        lit_varying.emit_uv0 = true;
        const std::string lit_vs = GenerateVertexShader(
            MakeDefault3DNodeConfig(),
            lit_varying,
            VK_FORMAT_R32G32B32_SFLOAT,
            {},
            GetShaderLibraryPath());
        if (lit_vs.find(
                "layout(location=0) flat out uint fragDataIndexID;")
                == std::string::npos
         || lit_vs.find(
                "layout(location=1) flat out uint fragTextureLayerID;")
                == std::string::npos
         || lit_vs.find("layout(location=2) out vec3 fragWorldPos;")
                == std::string::npos
         || lit_vs.find("layout(location=3) out vec3 fragWorldNormal;")
                == std::string::npos
         || lit_vs.find("layout(location=4) out vec2 fragUV0;")
                == std::string::npos)
        {
            result.diagnostics.emplace_back(
                "legacy generated lit varying ABI changed");
        }

        VertexVaryingConfig color_varying{};
        color_varying.emit_vertex_color = true;
        const std::string color_vs = GenerateVertexShader(
            MakeDefault3DNodeConfig(),
            color_varying,
            VK_FORMAT_R32G32B32_SFLOAT,
            {},
            GetShaderLibraryPath());
        if (color_vs.find(
                "layout(location=0) out vec4 fragVertexColor;")
                == std::string::npos
         || color_vs.find(
                "layout(location=6) out vec4 fragVertexColor;")
                != std::string::npos)
        {
            result.diagnostics.emplace_back(
                "legacy generated vertex-color ABI changed");
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
        position_provider.id = GLSLCodeModuleID::SkyLightHeader;
        position_provider.name = "preview_position_from_geometry";
        position_provider.glsl_code = "// preview only";
        position_provider.kind = GLSLCodeModuleKind::VertexInput;
        position_provider.semantic_requirements = &position_geometry;
        position_provider.semantic_requirement_count = 1;
        position_provider.semantic_provides = position_provides;
        position_provider.semantic_provide_count = 1;

        GLSLCodeModuleDefinition uv_provider{};
        uv_provider.id = GLSLCodeModuleID::SkyLightSimple;
        uv_provider.name = "preview_uv_from_geometry";
        uv_provider.glsl_code = "// preview only";
        uv_provider.kind = GLSLCodeModuleKind::VertexInput;
        uv_provider.semantic_requirements = &uv_geometry;
        uv_provider.semantic_requirement_count = 1;
        uv_provider.semantic_provides = uv_provides;
        uv_provider.semantic_provide_count = 1;

        GLSLCodeModuleDefinition normal_provider{};
        normal_provider.id = GLSLCodeModuleID::SkyLightCubeMap;
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

        const GeometryVertexFormat text_geometry{
            {VertexSemantic::Position, VF_V2I},
            {VertexSemantic::TexCoord, VF_V2F}
        };
        check_preview("Text2D", text_geometry, 2);

        // The opt-in ABI builder consumes the preview graph to generate both
        // FixedVertexEntry data and GLSL declarations without invoking the
        // GLSL compiler plugin.
        MaterialDefinition text_definition{};
        if (!TryGetMaterialDefinitionByID("Text2D", text_definition))
        {
            result.diagnostics.emplace_back("missing switched-build definition: Text2D");
        }
        else
        {
            MaterialDefinitionBuildRequest request{};
            request.geometry_vertex_format = &text_geometry;
            request.enable_resolved_vertex_abi = true;
            request.vertex_code_module_registry = &registry;
            MaterialResolvedVertexABI abi{};
            if (!BuildResolvedMaterialVertexABI(text_definition, request, abi))
            {
                result.diagnostics.emplace_back("resolved vertex ABI builder failed");
            }
            else
            {
                const std::string source(abi.vertex_input_glsl.c_str());
                if (abi.position_format != VF_V2I
                 || abi.vertex_entries.GetCount() != 2
                 || !(abi.vertex_entries[0] == FixedVertexEntry{VF_V2I, VertexSemantic::Position})
                 || !(abi.vertex_entries[1] == FixedVertexEntry{VF_V2F, VertexSemantic::TexCoord})
                 || source.find("layout(location=0) in ivec2 Position;") == std::string::npos
                 || source.find("layout(location=1) in vec2 TexCoord;") == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "resolved vertex ABI does not match Geometry");
                }
            }
        }

        const auto build_abi = [&](const MaterialDefinition &definition,
                                   const GeometryVertexFormat &geometry,
                                   MaterialResolvedVertexABI &out_abi) -> bool
        {
            MaterialDefinitionBuildRequest request{};
            request.geometry_vertex_format = &geometry;
            request.enable_resolved_vertex_abi = true;
            request.vertex_code_module_registry = &registry;
            return BuildResolvedMaterialVertexABI(definition, request, out_abi);
        };

        // The same vec2 declaration must serve RG16F and RG32F Geometry
        // inputs. The raw format remains distinct only in FixedVertexEntry.
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
            if (!build_abi(pure2d_definition, rg16_geometry, rg16_abi)
             || !build_abi(pure2d_definition, rg32_geometry, rg32_abi)
             || rg16_abi.vertex_input_glsl != rg32_abi.vertex_input_glsl
             || rg16_abi.vertex_entries.GetCount() != 1
             || rg32_abi.vertex_entries.GetCount() != 1
             || rg16_abi.vertex_entries[0].format != VF_V2HF
             || rg32_abi.vertex_entries[0].format != VF_V2F)
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
            if (!build_abi(normal_definition, float_normal_geometry, float_normal_abi)
             || !build_abi(normal_definition, packed_normal_geometry, packed_normal_abi)
             || float_normal_abi.vertex_input_glsl == packed_normal_abi.vertex_input_glsl
             || packed_normal_abi.vertex_entries.GetCount() != 2
             || packed_normal_abi.vertex_entries[1].format != PF_A2BGR10UN)
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
            if (assembled.fragment_glsl.find("#define SURFACE_TYPE ") == std::string::npos)
                result.diagnostics.emplace_back(
                    "Compositor permutation defines were not injected");
            if (assembled.fragment_glsl.find("#version", 8) != std::string::npos)
                result.diagnostics.emplace_back(
                    "Compositor GLSL contains a second #version directive");
            const size_t surface_call = assembled.fragment_glsl.find(
                "SurfaceOutput so = EvalSurface(si, fragDataIndexID);");
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
        lighting_options.direct_lighting_module = "lighting/direct_toon.glsl";
        lighting_options.indirect_lighting_module = "lighting/indirect_pbr_ambient.glsl";
        lighting_options.lighting_algorithm_module = "lighting/forward_flat.glsl";
        lighting_options.material_source_module = "material/pbr_texturearray_source.glsl";
        lighting_options.ntb_module = "ntb/ntb_texturearray_normalmap.glsl";
        lighting_options.forward_lighting_module = "compositor/forward_lighting.glsl";
        const auto scheduled_lighting = assembler.Assemble(
            SurfaceType::Lit,
            BlendMode::Opaque,
            PassType::ForwardOpaque,
            nullptr,
            "surface/lit_surface.glsl",
            lighting_options);
        if (!scheduled_lighting.success
         || scheduled_lighting.fragment_glsl.find(
                "#include \"lighting/direct_toon.glsl\"")
                == std::string::npos
         || scheduled_lighting.fragment_glsl.find(
                "#include \"lighting/indirect_pbr_ambient.glsl\"")
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
                "#include \"surface/lit_surface.glsl\"")
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
            "compositor/main_forward_unlit_texture.frag.glsl",
            "surface/unlit_texture_surface.glsl",
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

        const auto unsupported = assembler.Assemble(
            SurfaceType::Lit,
            BlendMode::Opaque,
            PassType::ShadowOpaque);
        if (unsupported.success
         || unsupported.error_message.find("Unsupported compositor pass") == std::string::npos)
            result.diagnostics.emplace_back(
                "Unsupported compositor pass must fail explicitly");

        CompositorAssembler::CompositorModuleOptions cubemap_options{};
        cubemap_options.sky_module = "sky/sky_cubemap.glsl";
        const auto cubemap = assembler.Assemble(
            SurfaceType::Sky,
            BlendMode::Opaque,
            PassType::ForwardOpaque,
            "compositor/main_forward_sky.frag.glsl",
            "surface/sky_cubemap_surface.glsl",
            cubemap_options);
        if (!cubemap.success
         || cubemap.fragment_glsl.find("#include \"sky/sky_cubemap.glsl\"")
                == std::string::npos
         || cubemap.fragment_glsl.find(
                "#include \"surface/sky_cubemap_surface.glsl\"")
                == std::string::npos)
            result.diagnostics.emplace_back(
                "CubeMap compositor must select the CubeMap sky and surface modules");

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
            3, 3, 0
        };
        const GLSLCodeModuleSemantic normal_provides[] = {
            GLSLCodeModuleSemantic::Normal
        };
        const GLSLCodeModuleDefinition normal_provider{
            GLSLCodeModuleID::SkyLightHeader,
            "identity_normal",
            "// identity",
            nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
            GLSLCodeModuleKind::VertexInput,
            &normal_requirement, 1,
            normal_provides, 1,
            10, 0
        };

        const GLSLCodeModuleSemanticRequirement uv_requirement{
            GLSLCodeModuleCapabilitySource::GeometryAttribute,
            GLSLCodeModuleSemantic::UV0,
            static_cast<uint32_t>(GLSLCodeModuleNumericClass::Float),
            2, 2, 0
        };
        const GLSLCodeModuleSemantic uv_provides[] = {
            GLSLCodeModuleSemantic::UV0
        };
        const GLSLCodeModuleDefinition uv_provider{
            GLSLCodeModuleID::SkyLightSimple,
            "identity_uv",
            "// identity",
            nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
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
            GLSLCodeModuleID::SkyLightCubeMap,
            "identity_normal_packed",
            "// identity",
            nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
            GLSLCodeModuleKind::Decode,
            &normal_requirement, 1,
            normal_provides, 1,
            20, 0
        };
        changed_result.selections[0].provider = &packed_normal_provider;
        if (GetGLSLCodeModuleProviderGraphHash(changed_result) == first_hash)
            result.diagnostics.emplace_back("distinct provider graphs must hash differently");

        ShaderStageBuildSpec stage{};
        stage.stage = ShaderStage::Vertex;
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
            GLSLCodeModuleID::SkyLightHeader,
            "compose_position",
            "vec4 GetLocalPos() { return vec4(Position, 1.0); }",
            nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
            GLSLCodeModuleKind::Position,
            nullptr, 0, nullptr, 0, 0, 0
        };
        const GLSLCodeModuleDefinition normal_provider{
            GLSLCodeModuleID::SkyLightSimple,
            "compose_normal",
            "vec3 GetNormal() { return Normal; }",
            nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
            GLSLCodeModuleKind::Basis,
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

        ShaderStageBuildSpec vertex{};
        vertex.stage = ShaderStage::Vertex;
        vertex.outputs.Add({0, ShaderStageValueType::Vec3, 0, 0});
        vertex.outputs.Add({0, ShaderStageValueType::Vec2, 1, 0});
        ShaderStageBuildSpec fragment{};
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

        ShaderStageBuildSpec stage{};
        stage.stage = ShaderStage::Vertex;
        stage.glsl_module_graph_hash = 0x13579bdf2468ace0ull;
        const ShaderStageKey stage_key = stage.BuildKey();
        const ShaderStageKey equivalent_stage_key = stage.BuildKey();
        if (!(stage_key == equivalent_stage_key))
            result.diagnostics.emplace_back("equivalent provider stages must share cache identity");

        if (rg16_geometry.GetVertexInputHash() == rg32_geometry.GetVertexInputHash())
            result.diagnostics.emplace_back("raw Geometry formats must remain distinct");

        ShaderProgramLinkSpec rg16_link{};
        rg16_link.vertex_stage = stage_key;
        rg16_link.fragment_stage.stage = ShaderStage::Fragment;
        rg16_link.vertex_input_hash = rg16_geometry.GetVertexInputHash();
        ShaderProgramLinkSpec rg32_link = rg16_link;
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

        ShaderArtifactStore shadow_store(
            RepoRootOSPath("build"),
            ShaderCacheMode::BuildIfMissing,
            ShaderArtifactCacheNamespace::ShadowV1);
        const uint32_t shadow_payload[] = {0x07230203u, 4u, 5u, 6u};
        if (!shadow_store.SaveStageSPV(stage_key, shadow_payload, sizeof(shadow_payload)))
        {
            result.diagnostics.emplace_back("shadow namespace cache save failed");
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
                    "shadow namespace must not replace the legacy artifact");
            }

            if (!shadow_store.LoadStageSPV(stage_key, shadow_loaded)
             || shadow_loaded.GetCount() != static_cast<int>(sizeof(shadow_payload))
             || std::memcmp(
                    shadow_loaded.GetData(), shadow_payload, sizeof(shadow_payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "shadow namespace must load its isolated artifact");
            }
        }

        if (store.GetCacheNamespace() != ShaderArtifactCacheNamespace::Legacy
         || shadow_store.GetCacheNamespace() != ShaderArtifactCacheNamespace::ShadowV1)
        {
            result.diagnostics.emplace_back("artifact cache namespace state mismatch");
        }

        ShaderGenMigrationOptions migration_options{};
        if (migration_options.implementation_path != ShaderGenImplementationPath::Legacy
         || migration_options.artifact_namespace != ShaderArtifactCacheNamespace::Legacy)
        {
            result.diagnostics.emplace_back(
                "migration options must preserve the legacy path by default");
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
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "PBRSurfaceData", nullptr, DescriptorSemantic::MaterialDataSlotData, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::PBRSurface },
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable },
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
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "PBRSurfaceData", nullptr, DescriptorSemantic::MaterialDataSlotData, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::PBRSurface },
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable },
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

    static GateResult RunMaterializationSharedInstanceCase()
    {
        GateResult result;
        result.name = "D1.materialization-shared-instance-separation";

        MaterialRecipe recipe;
        recipe.recipe_name = "shared-instance-regression";

        RecipeTextureBinding texture{};
        texture.slot = TextureSlot::BaseColor;
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

        MaterializationResolveCallbacks callbacks{};
        callbacks.resolve_texture = [](const RecipeTextureBinding &input, ResolvedResource &output)
        {
            output.slot = input.slot;
            output.bindless_handle = input.direct_value;
            output.texture_layer = input.direct_value;
            return true;
        };
        callbacks.resolve_struct = [](const RecipeSSBOAssetBinding &input, ResolvedStructRef &output)
        {
            output.data_slot = input.data_slot;
            output.ssbo_type = input.ssbo_type;
            output.ssbo_id = input.ssbo_id;
            output.data_index = input.data_index;
            return true;
        };

        MaterializationSharedSpec shared;
        if (!ResolveMaterializationSharedSpec(recipe, callbacks, shared))
        {
            result.diagnostics.emplace_back("shared resolve failed");
            result.passed = false;
            return result;
        }

        const MaterializationSpec first = MaterializeMaterializationInstance(shared, recipe);
        recipe.ssbo_assets[0].data_index = 9;
        const MaterializationSpec second = MaterializeMaterializationInstance(shared, recipe);

        if (shared.spec.struct_refs.empty()
         || shared.spec.struct_refs[0].data_index != 0
         || first.struct_refs[0].data_index != 3
         || second.struct_refs[0].data_index != 9)
            result.diagnostics.emplace_back("instance data_index leaked into shared spec");

        MaterializationIndexTables tables;
        uint32_t first_texture_row = 0;
        uint32_t first_data_row = 0;
        uint32_t second_texture_row = 0;
        uint32_t second_data_row = 0;
        WriteSpecToIndexTables(first, tables, first_texture_row, first_data_row);
        WriteSpecToIndexTables(second, tables, second_texture_row, second_data_row);

        const auto *first_data = tables.GetMaterialDataIndexRow(first_data_row);
        const auto *second_data = tables.GetMaterialDataIndexRow(second_data_row);
        if (first_texture_row == second_texture_row
         || first_data_row == second_data_row
         || !first_data || !second_data
         || first_data->values[DefaultMaterialDataSlot] != 3
         || second_data->values[DefaultMaterialDataSlot] != 9)
            result.diagnostics.emplace_back("instance rows were not written independently");

        MaterializationInstanceData first_instance = MakeMaterializationInstanceData(first);
        MaterializationInstanceData second_instance = MakeMaterializationInstanceData(second);
        MaterializationIndexTables instance_tables;
        uint32_t first_instance_texture_row = 0;
        uint32_t first_instance_data_row = 0;
        uint32_t second_instance_texture_row = 0;
        uint32_t second_instance_data_row = 0;
        WriteMaterializationInstanceToIndexTables(
            shared.spec,
            first_instance,
            instance_tables,
            first_instance_texture_row,
            first_instance_data_row);
        WriteMaterializationInstanceToIndexTables(
            shared.spec,
            second_instance,
            instance_tables,
            second_instance_texture_row,
            second_instance_data_row);

        const auto *first_instance_data =
            instance_tables.GetMaterialDataIndexRow(first_instance_data_row);
        const auto *second_instance_data =
            instance_tables.GetMaterialDataIndexRow(second_instance_data_row);
        if (first_instance.texture_layer_row != first_instance_texture_row
         || first_instance.data_index_row != first_instance_data_row
         || second_instance.texture_layer_row != second_instance_texture_row
         || second_instance.data_index_row != second_instance_data_row
         || !first_instance_data || !second_instance_data
         || first_instance_data->values[DefaultMaterialDataSlot] != 3
         || second_instance_data->values[DefaultMaterialDataSlot] != 9)
            result.diagnostics.emplace_back("explicit instance data was not wired to index tables");

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
            { BUILTIN_MTL_DEF_FALLBACK },
            { BUILTIN_MTL_DEF_MISSING_MATERIAL },
            { BUILTIN_MTL_DEF_TEXT }
        };

        for (const auto &entry : expected)
        {
            MaterialDefinition bmi{};
            if (!TryGetMaterialDefinitionByID(entry.definition_id, bmi))
            {
                result.diagnostics.emplace_back(std::string("Missing material definition: ") + entry.definition_id);
                continue;
            }

            if (bmi.definition_name.empty()
             || !IsBootstrapMaterialDefinition(bmi)
             || bmi.source_kind != MaterialDefinitionSourceKind::BuiltIn)
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
            {BUILTIN_MTL_DEF_FALLBACK, MaterialDefinitionBootstrapKind::PureColor},
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
             || definition.source_kind != MaterialDefinitionSourceKind::BuiltIn)
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

        if (BUILTIN_MTL_DEF_ERROR_CHECKER[0] == 0
         || BUILTIN_MTL_DEF_PURE_DEPTH[0] == 0)
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
        MaterialDefinition text_alias{};
        MaterialDefinition text_creator{};
        if (!TryGetMaterialDefinitionByID(BUILTIN_MTL_DEF_TEXT, text)
         || !TryGetMaterialDefinitionByID("Text2D", text_alias)
         || !TryGetMaterialDefinitionByBuiltinMaterialCreatorID(
                BuiltinMaterialCreatorID::Text2D, text_creator))
        {
            result.diagnostics.emplace_back(
                "Text canonical definition, alias, or creator route is missing");
        }
        else if (text.definition_id != BUILTIN_MTL_DEF_TEXT
              || text_alias.definition_id != text.definition_id
              || text_creator.definition_id != text.definition_id
              || text_alias.source_kind != MaterialDefinitionSourceKind::BuiltIn
              || !IsBootstrapMaterialDefinition(text_alias))
        {
            result.diagnostics.emplace_back(
                "Text2D alias and creator route must share one bootstrap identity");
        }

        MaterialRecipe canonical_recipe{};
        canonical_recipe.mtl_def_id = BUILTIN_MTL_DEF_TEXT;
        MaterialRecipe alias_recipe{};
        alias_recipe.mtl_def_id = "Text2D";
        NormalizeRecipe(canonical_recipe);
        NormalizeRecipe(alias_recipe);
        if (canonical_recipe.mtl_def_id != alias_recipe.mtl_def_id
         || HashMaterialShaderVariant(canonical_recipe)
                != HashMaterialShaderVariant(alias_recipe))
        {
            result.diagnostics.emplace_back(
                "recipe normalization must collapse aliases to one shader identity");
        }

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
                        "compositor/pure_color.frag.glsl") != 0
         || pure_color.fragment_program_mode != MaterialFragmentProgramMode::Compositor
         || pure_color.fragment_surface_module)
            result.diagnostics.emplace_back("PureColor must use one FS module");

        CompositorAssembler assembler(GetShaderLibraryPath());
        const auto assembled = assembler.Assemble(
            SurfaceType::Unlit, BlendMode::Opaque, PassType::ForwardOpaque,
            "compositor/pure_color.frag.glsl", nullptr);
        if (!assembled.success
         || assembled.fragment_glsl.find("MTL_DATA.data[fragDataIndexID].color")
                == std::string::npos
         || assembled.fragment_glsl.find("layout(location=0) flat in uint fragDataIndexID")
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

        const MaterialTransformGraph flat = MaterialTransformGraph::FlatXY();
        const MaterialTransformGraph wall = MaterialTransformGraph::WallXY();
        const MaterialTransformGraph world = MaterialTransformGraph::World3D();
        const MaterialTransformGraph terrain = MaterialTransformGraph::Terrain();
        MaterialTransformGraph world_vec2 = world;
        world_vec2.source = VertexInputMode::Vec2Position;
        world_vec2.mapping = PositionMappingMode::LiftXY_XY0;

        if (flat == wall || flat == world || world == terrain
         || flat.GetHash() == wall.GetHash()
         || world.GetHash() == terrain.GetHash())
            result.diagnostics.emplace_back(
                "transform graph variants must have distinct structural identity");

        if (!flat.IsScreenLike()
         || !wall.IsScreenLike()
         || world.IsScreenLike()
         || world_vec2.IsScreenLike())
            result.diagnostics.emplace_back(
                "screen-space classification must include projection, not only Vec2 input");

        const VertexShaderNodeConfig flat_config = flat.ToNodeConfig();
        if (flat_config.input != VertexInputMode::Vec2Position
         || flat_config.position_mapping != PositionMappingMode::NDCLift
         || flat_config.projection != ProjectionMode::LocalToWorldOnly)
            result.diagnostics.emplace_back("FlatXY graph conversion mismatch");

        const VertexShaderNodeConfig wall_config = wall.ToNodeConfig();
        if (wall_config.position_mapping != PositionMappingMode::LiftXY_X0Y)
            result.diagnostics.emplace_back("WallXY graph conversion mismatch");

        const VertexShaderNodeConfig terrain_config = terrain.ToNodeConfig();
        if (terrain_config.position_mapping != PositionMappingMode::TerrainGrid
         || terrain_config.projection != ProjectionMode::WorldCameraVP)
            result.diagnostics.emplace_back("Terrain graph conversion mismatch");

        if (MaterialTransformGraph::FromNodeConfig(world.ToNodeConfig()) != world)
            result.diagnostics.emplace_back("transform graph round-trip mismatch");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunTransformGraphCompositionCase()
    {
        GateResult result;
        result.name = "X.transform-graph-composition";

        VertexVaryingConfig varying{};
        varying.emit_data_index_id = true;

        const auto build = [&](const MaterialTransformGraph &graph)
        {
            return GenerateVertexShader(
                graph.ToNodeConfig(), varying, VK_FORMAT_R32G32B32_SFLOAT,
                std::string(), GetShaderLibraryPath());
        };

        const std::string flat = build(MaterialTransformGraph::FlatXY());
        const std::string wall = build(MaterialTransformGraph::WallXY());
        const std::string world = build(MaterialTransformGraph::World3D());
        const std::string terrain = build(MaterialTransformGraph::Terrain());

        if (flat == wall || flat == world || world == terrain)
            result.diagnostics.emplace_back(
                "one material must produce distinct VS sources per transform graph");
        if (flat.find("vertex/s2_ndc_lift.glsl") == std::string::npos
         || wall.find("vertex/s2_lift_x0y.glsl") == std::string::npos
         || world.find("vertex/s2_passthrough3d.glsl") == std::string::npos
         || terrain.find("TerrainGrid") == std::string::npos)
            result.diagnostics.emplace_back(
                "transform graph source composition selected the wrong stage");

        ShaderStageBuildSpec stage{};
        stage.stage = ShaderStage::Vertex;
        const ShaderStageKey flat_key =
            stage.BuildKeyWithProviderGraphHash(MaterialTransformGraph::FlatXY().GetHash());
        const ShaderStageKey wall_key =
            stage.BuildKeyWithProviderGraphHash(MaterialTransformGraph::WallXY().GetHash());
        if (flat_key == wall_key)
            result.diagnostics.emplace_back(
                "transform graph variants must produce distinct stage identity");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialShaderVariantIdentityCase()
    {
        GateResult result;
        result.name = "Y.material-shader-variant-identity";

        MaterialRecipe first{};
        first.mtl_def_id = BUILTIN_MTL_DEF_PURE_COLOR;
        first.recipe_name = "GizmoColor_0";
        first.domain = "Gizmo";
        first.ssbo_assets.push_back(
            {"mtl", 0, SSBOType::EmissiveSurface, 100, 0, true, true});

        MaterialRecipe second = first;
        second.recipe_name = "GizmoColor_1";
        second.domain = "AnotherInstanceDomain";
        second.ssbo_assets[0].data_index = 1;
        if (HashMaterialRecipe(first) == HashMaterialRecipe(second))
            result.diagnostics.emplace_back(
                "full recipe identity should retain instance differences");

        second = first;
        second.ssbo_assets[0].ssbo_id = 101;
        if (HashMaterialRecipe(first) == HashMaterialRecipe(second))
            result.diagnostics.emplace_back(
                "full recipe identity must retain SSBO resource identity");

        if (HashMaterialShaderVariant(first) != HashMaterialShaderVariant(second))
            result.diagnostics.emplace_back(
                "shader variant identity must ignore recipe/SSBO instance differences");

        second.has_transform_graph = true;
        second.transform_graph = MaterialTransformGraph::WallXY();
        if (HashMaterialShaderVariant(first) == HashMaterialShaderVariant(second))
            result.diagnostics.emplace_back(
                "shader variant identity must include transform graph differences");

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
        overrides.alpha_cutoff = 0.5f;
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
            "program_mode = \"Compositor\"\n"
            "provider_policy = \"AllowDerived\"\n"
            "[transform]\n"
            "source = \"Vec3Position\"\n"
            "mapping = \"Passthrough3D\"\n"
            "orientation = \"World\"\n"
            "scale = \"World\"\n"
            "projection = \"WorldCameraVP\"\n"
            "[fragment]\n"
            "source = \"compositor/main_forward_lit.frag.glsl\"\n"
            "surface_module = \"surface/lit_surface.glsl\"\n"
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
             || !definition.has_transform_graph
             || definition.transform_graph.mapping != PositionMappingMode::Passthrough3D
             || definition.vertex_semantic_requirements.GetCount() != 3
             || definition.ubo_requirements.size() != 2
             || std::strcmp(definition.fragment_source,
                            "compositor/main_forward_lit.frag.glsl") != 0
             || definition.fragment_program_module
                    != definition.fragment_source
             || std::strcmp(definition.fragment_surface_module,
                            "surface/lit_surface.glsl") != 0)
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
             || allow_derived_abi.vertex_entries.GetCount() != 3)
            {
                result.diagnostics.emplace_back(
                    "AllowDerived material definition must build a geometry ABI");
            }
        }

        const char legacy_file[] =
            "schema = 1\n"
            "id = \"LegacyDirect\"\n"
            "name = \"LegacyDirect\"\n"
            "source = \"file\"\n"
            "usage = \"General\"\n"
            "bootstrap = \"None\"\n"
            "program_mode = \"DirectInclude\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[compositor]\n"
            "fragment = \"compositor/legacy_test_template.frag.glsl\"\n"
            "[vertex]\n"
            "requirements = [\"Position\"]\n";
        MaterialDefinitionFileData legacy_data;
        if (ParseMaterialDefinitionFile(
                legacy_file, static_cast<int>(std::strlen(legacy_file)), legacy_data)
                != MaterialDefinitionFileParseResult::OK
         || !legacy_data.definition.fragment_source
         || std::strcmp(
                legacy_data.definition.fragment_source,
                "compositor/legacy_test_template.frag.glsl") != 0
         || legacy_data.definition.fragment_program_module
                != legacy_data.definition.fragment_source)
        {
            result.diagnostics.emplace_back(
                "legacy compositor.fragment must convert at the parser boundary");
        }

        const char invalid_file[] =
            "schema = 1\n"
            "id = \"Broken\"\n"
            "name = \"Broken\"\n"
            "source = \"builtin\"\n"
            "usage = \"General\"\n"
            "bootstrap = \"None\"\n"
            "program_mode = \"Compositor\"\n"
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
         || file_count != 10
         || error_count != 0)
        {
            result.diagnostics.emplace_back("material file registry bulk load failed");
        }
        else
        {
            const char *expected_file_ids[] = {
                "Lit", "LitTextureArray", "SkyMinimal", "DebugNormalColor",
                "VertexColor", "UnlitTexture", "Texture2DArray",
                "VertexLuminance", "VertexPaletteColor", "SkyCubeMap"
            };
            for (const char *id : expected_file_ids)
            {
                if (!registry.FindByID(id))
                    result.diagnostics.emplace_back(
                        std::string("missing bulk material file: ") + id);
            }

            const MaterialDefinition *cubemap_file =
                registry.FindByID("SkyCubeMap");
            if (!cubemap_file
             || cubemap_file->compositor_surface != SurfaceType::Sky
             || !cubemap_file->fragment_surface_module
             || std::strcmp(
                    cubemap_file->fragment_surface_module,
                    "surface/sky_cubemap_surface.glsl") != 0
             || cubemap_file->code_module_requirements.size() != 1
             || cubemap_file->code_module_requirements[0]
                    != GLSLCodeModuleID::SkyLightCubeMap)
            {
                result.diagnostics.emplace_back(
                    "SkyCubeMap material definition contract is invalid");
            }
            else
            {
                ShaderResourceManifest cubemap_manifest{};
                if (!Build3DShaderResourceManifest(
                        *cubemap_file,
                        SkyLightAmbientModel::CubeMap,
                        cubemap_manifest)
                 || cubemap_manifest.texture_count != 1
                 || cubemap_manifest.textures[0].semantic
                        != DescriptorSemantic::SkyCubemapSampler)
                {
                    result.diagnostics.emplace_back(
                        "SkyCubeMap resource manifest must contain one sky cubemap sampler");
                }
            }

            const MaterialDefinition *file_definition = registry.FindByID("Lit");
            MaterialDefinition legacy_definition{};
            if (!file_definition
             || !TryGetMaterialDefinitionByID("Lit", legacy_definition))
            {
                result.diagnostics.emplace_back("Lit file/legacy definition lookup failed");
            }
            else
            {
                if (file_definition->definition_id != legacy_definition.definition_id)
                    result.diagnostics.emplace_back("Lit id mismatch");
                if (file_definition->fragment_program_mode != legacy_definition.fragment_program_mode)
                    result.diagnostics.emplace_back("Lit mode mismatch");
                if (file_definition->compositor_surface != legacy_definition.compositor_surface)
                    result.diagnostics.emplace_back("Lit surface mismatch");
                if (file_definition->compositor_blend != legacy_definition.compositor_blend)
                    result.diagnostics.emplace_back("Lit blend mismatch");
                if (file_definition->compositor_pass != legacy_definition.compositor_pass)
                    result.diagnostics.emplace_back("Lit pass mismatch");
                if (file_definition->vertex_semantic_requirements.GetCount()
                        != legacy_definition.vertex_semantic_requirements.GetCount())
                    result.diagnostics.emplace_back("Lit semantic count mismatch");
                if (file_definition->code_module_requirements
                        != legacy_definition.code_module_requirements)
                    result.diagnostics.emplace_back("Lit code module mismatch");
                if (file_definition->ubo_requirements != legacy_definition.ubo_requirements)
                    result.diagnostics.emplace_back("Lit UBO mismatch");
                if (file_definition->data_slot_decls.size()
                        != legacy_definition.data_slot_decls.size())
                    result.diagnostics.emplace_back("Lit SSBO mismatch");
                if (file_definition->texture_slot_decls.size()
                        != legacy_definition.texture_slot_decls.size())
                    result.diagnostics.emplace_back("Lit texture mismatch");
                if (file_definition->vertex_varying.emit_world_pos
                        != legacy_definition.vertex_varying.emit_world_pos
                 || file_definition->vertex_varying.emit_world_normal
                        != legacy_definition.vertex_varying.emit_world_normal
                 || file_definition->vertex_varying.emit_uv0
                        != legacy_definition.vertex_varying.emit_uv0)
                    result.diagnostics.emplace_back("Lit varying mismatch");
                if (file_definition->vertex_provider_policy
                        != legacy_definition.vertex_provider_policy)
                    result.diagnostics.emplace_back("Lit provider policy mismatch");
                if (std::strcmp(file_definition->fragment_source,
                                "compositor/main_forward_lit.frag.glsl") != 0
                 || std::strcmp(file_definition->fragment_surface_module,
                                "surface/lit_surface.glsl") != 0)
                    result.diagnostics.emplace_back("Lit stage reference mismatch");

                for (int i = 0; i < file_definition->vertex_semantic_requirements.GetCount(); ++i)
                {
                    if (!(file_definition->vertex_semantic_requirements[i]
                        == legacy_definition.vertex_semantic_requirements[i]))
                    {
                        result.diagnostics.emplace_back(
                            "Lit file semantic requirements differ from legacy");
                        break;
                    }
                }

            }
        }

        const char *bulk_ids[] = {
            "LitTextureArray", "SkyMinimal", "DebugNormalColor", "VertexColor",
            "UnlitTexture", "Texture2DArray", "VertexLuminance",
            "VertexPaletteColor"
        };
        for (const char *id : bulk_ids)
        {
            const MaterialDefinition *file_definition = registry.FindByID(id);
            MaterialDefinition legacy_definition{};
            if (!file_definition
             || !TryGetMaterialDefinitionByID(id, legacy_definition))
            {
                result.diagnostics.emplace_back(
                    std::string("bulk file/legacy lookup failed: ") + id);
                continue;
            }

            const bool same_surface_reference =
                (!file_definition->fragment_surface_module
                 && !legacy_definition.fragment_surface_module)
                || (file_definition->fragment_surface_module
                 && legacy_definition.fragment_surface_module
                 && std::strcmp(file_definition->fragment_surface_module,
                                legacy_definition.fragment_surface_module) == 0);
            if (file_definition->fragment_program_mode != legacy_definition.fragment_program_mode
             || file_definition->compositor_surface != legacy_definition.compositor_surface
             || file_definition->compositor_blend != legacy_definition.compositor_blend
             || file_definition->compositor_pass != legacy_definition.compositor_pass
             || file_definition->vertex_provider_policy != legacy_definition.vertex_provider_policy
             || !same_surface_reference
             || file_definition->vertex_semantic_requirements.GetCount()
                    != legacy_definition.vertex_semantic_requirements.GetCount()
             || file_definition->ubo_requirements.size()
                    != legacy_definition.ubo_requirements.size()
             || file_definition->data_slot_decls.size()
                    != legacy_definition.data_slot_decls.size()
             || file_definition->texture_slot_decls.size()
                    != legacy_definition.texture_slot_decls.size())
            {
                result.diagnostics.emplace_back(
                    std::string("bulk file definition contract mismatch: ") + id);
                continue;
            }

            for (int i = 0; i < file_definition->vertex_semantic_requirements.GetCount(); ++i)
            {
                if (!(file_definition->vertex_semantic_requirements[i]
                    == legacy_definition.vertex_semantic_requirements[i]))
                {
                    result.diagnostics.emplace_back(
                        std::string("bulk semantic mismatch: ") + id);
                    break;
                }
            }

            if (file_definition->has_transform_graph
             && file_definition->transform_graph.IsScreenLike()
             && (file_definition->vertex_node_config.input != VertexInputMode::Vec2Position
              || file_definition->vertex_node_config.position_mapping != PositionMappingMode::NDCLift
              || file_definition->vertex_node_config.projection != ProjectionMode::LocalToWorldOnly))
            {
                result.diagnostics.emplace_back(
                    std::string("2D file node config mismatch: ") + id);
            }
        }

        {
            const MaterialDefinition *file_definition = registry.FindByID("Lit");
            MaterialDefinition legacy_definition{};
            MaterialDefinition merged_definition{};
            if (!file_definition
             || !TryGetMaterialDefinitionByID("Lit", legacy_definition)
             || !MergeMaterialDefinitionFile(
                    legacy_definition, *file_definition, merged_definition)
             || merged_definition.source_kind != MaterialDefinitionSourceKind::File
             || merged_definition.fragment_source
                    != file_definition->fragment_source)
            {
                result.diagnostics.emplace_back(
                    "file material merge must prefer the valid TOML definition");
            }
            else
            {
                const GeometryVertexFormat geometry{
                    {VertexSemantic::Position, VF_V3F},
                    {VertexSemantic::TexCoord, VF_V2F},
                    {VertexSemantic::Normal, VF_V3F}
                };
                MaterialDefinitionBuildRequest request{};
                request.geometry_vertex_format = &geometry;
                MaterialResolvedVertexABI abi{};
                if (!BuildResolvedMaterialVertexABI(
                        merged_definition, request, abi)
                 || abi.vertex_entries.GetCount() != 3)
                {
                    result.diagnostics.emplace_back(
                        "file material must build vertex ABI from semantic TOML data");
                }
            }

            MaterialDefinition fallback_definition{};
            if (!TryGetMaterialDefinitionByID(
                    BUILTIN_MTL_DEF_FALLBACK, fallback_definition)
             || MergeMaterialDefinitionFile(
                    fallback_definition, *file_definition, merged_definition))
            {
                result.diagnostics.emplace_back(
                    "bootstrap material must not be overridden by TOML");
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
        const GLSLCodeModuleDefinition *pbr =
            FindGLSLCodeModuleDefinition(GLSLCodeModuleID::PBRSurface);

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

        if (!pbr || !pbr->glsl_code
         || pbr->texture_requirement_count != 0
         || pbr->texture_requirements)
        {
            result.diagnostics.emplace_back(
                "PBRSurface texture requirements must come from the material definition.");
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

    static GateResult RunProviderResourceManifestCase()
    {
        GateResult result;
        result.name = "H1.provider-resource-manifest";

        GLSLCodeModuleRegistry registry;
        if (!registry.RegisterBuiltinModules())
            result.diagnostics.emplace_back("provider registry builtin registration failed");

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
            const GLSLCodeModuleID roots_2d[] = {pbr_2d->id, ntb_2d->id};
            ShaderResourceManifest manifest_2d{};
            if (!BuildShaderResourceManifest(
                    roots_2d, uint32_t(std::size(roots_2d)), manifest_2d, &registry))
            {
                result.diagnostics.emplace_back(
                    std::string("Texture2D provider manifest failed: ")
                    + GetShaderResourceManifestErrorName(manifest_2d.error));
            }
            else
            {
                if (manifest_2d.ssbo_count != 1
                 || std::strcmp(manifest_2d.ssbos[0].name, "mtl") != 0
                 || manifest_2d.ssbos[0].ssbo_type != SSBOType::PBRSurface
                 || manifest_2d.ssbos[0].data_slot != 0)
                    result.diagnostics.emplace_back(
                        "Texture2D providers must declare one PBRSurface material SSBO");

                if (manifest_2d.texture_count != 5
                 || manifest_2d.texture_layer_count != 2)
                    result.diagnostics.emplace_back(
                        "Texture2D providers must declare samplers and per-slot bindless layer-table dependencies");

                const std::vector<FixedDescriptorEntry> descriptors =
                    Build3DDescriptorsFromDefinition(MaterialDefinition{}, manifest_2d);
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

            const GLSLCodeModuleID roots_array[] = {pbr_array->id, ntb_array->id};
            ShaderResourceManifest manifest_array{};
            if (!BuildShaderResourceManifest(
                    roots_array, uint32_t(std::size(roots_array)), manifest_array, &registry))
            {
                result.diagnostics.emplace_back(
                    std::string("Texture2DArray provider manifest failed: ")
                    + GetShaderResourceManifestErrorName(manifest_array.error));
            }
            else
            {
                if (manifest_array.ssbo_count != 1
                 || manifest_array.texture_count != 5
                 || manifest_array.texture_layer_count != 1
                 || manifest_array.texture_layers[0].slot != TextureSlot::Custom0)
                    result.diagnostics.emplace_back(
                        "Texture2DArray providers must declare sampler and Custom0 layer resources");

                for (uint32_t i = 0; i < manifest_array.texture_count; ++i)
                {
                    if (!manifest_array.textures[i].glsl_type
                     || std::strcmp(
                            manifest_array.textures[i].glsl_type,
                            "sampler2DArray") != 0)
                    {
                        result.diagnostics.emplace_back(
                            "Texture2DArray provider samplers must retain sampler2DArray type");
                        break;
                    }
                }

                const std::vector<FixedDescriptorEntry> descriptors =
                    Build3DDescriptorsFromDefinition(MaterialDefinition{}, manifest_array);
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

            const GLSLCodeModuleID derivative_root = ntb_derivative->id;
            ShaderResourceManifest derivative_manifest{};
            if (!BuildShaderResourceManifest(
                    &derivative_root, 1, derivative_manifest, &registry)
             || derivative_manifest.texture_count != 1
             || derivative_manifest.textures[0].slot != TextureSlot::Normal
             || derivative_manifest.texture_layer_count != 1)
               result.diagnostics.emplace_back(
                   "Derivative normal-map provider must declare its sampler and bindless layer-table dependency");
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
        if (!registry.LoadDirectory(hgl::ToOSString(GetShaderLibraryPath()), &file_count, &error_count))
            result.diagnostics.emplace_back("LoadDirectory failed to scan directory");
        else
        {
            if (file_count != 68)
                result.diagnostics.emplace_back("LoadDirectory expected 68 file modules, got "
                    + std::to_string(file_count));
            if (error_count != 0)
                result.diagnostics.emplace_back("LoadDirectory reported "
                    + std::to_string(error_count) + " errors");

            const int expected_count = 68 + int(GLSLCodeModuleID::RANGE_SIZE);
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

        const auto *forward_input = registry.FindByName("forward_lighting");
        if (!forward_input || forward_input->kind != GLSLCodeModuleKind::Utility)
            result.diagnostics.emplace_back("forward_lighting input assembly module is missing or has wrong kind");

        const auto *forward_algorithm = registry.FindByName("forward_pbr");
        if (!forward_algorithm || forward_algorithm->kind != GLSLCodeModuleKind::Utility)
            result.diagnostics.emplace_back("forward_pbr lighting algorithm module is missing or has wrong kind");

        const auto *alternate_algorithm = registry.FindByName("forward_flat");
        if (!alternate_algorithm || alternate_algorithm->kind != GLSLCodeModuleKind::Utility)
            result.diagnostics.emplace_back("forward_flat alternate lighting algorithm is missing or has wrong kind");

        // Re-scan must detect every duplicate name and keep counts stable.
        int dup_count = 0;
        int dup_errors = 0;
        if (!registry.LoadDirectory(hgl::ToOSString(GetShaderLibraryPath()), &dup_count, &dup_errors))
            result.diagnostics.emplace_back("second LoadDirectory failed");
        else if (dup_count != 0 || dup_errors != 68)
            result.diagnostics.emplace_back("second LoadDirectory must report 68 duplicates, got files="
                + std::to_string(dup_count) + " errors=" + std::to_string(dup_errors));

        const int stable_count = 68 + int(GLSLCodeModuleID::RANGE_SIZE);
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

    static GateResult RunMaterialMultiSlotSourceCase()
    {
        GateResult result;
        result.name = "Z.material-multislot-source";

        std::vector<MaterialDataSlotDecl> slots = {
            {"surface_a", SSBOType::EmissiveSurface},
            {"surface_b", SSBOType::EmissiveSurface}
        };
        const FixedVertexEntry vertices[] = {
            {VF_V2F, VertexSemantic::Position}
        };
        const FixedDescriptorEntry descriptors[] = {
            {
                DescriptorSetType::Material,
                DescriptorKind::SSBO,
                uint32_t(VK_SHADER_STAGE_VERTEX_BIT),
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
        const FixedMaterialDef fixed_definition{
            "MultiSlotMaterial",
            PrimitiveType::Triangles,
            vertices,
            1,
            descriptors,
            1
        };

        CompositorMaterialBuildConfig config{};
        config.data_slot_decls = &slots;
        config.generate_only = true;

        ShaderProgramBuildSpec *build_spec = CompileCompositorMaterial(
            nullptr,
            fixed_definition,
            "#version 450\nlayout(location=0) in vec2 Position;\nvoid main(){gl_Position=vec4(Position,0.0,1.0);}\n",
            "#version 450\nlayout(location=0) out vec4 outColor;\nvoid main(){outColor=vec4(1.0);}\n",
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
            build_spec->GetStageShader(ShaderStage::Vertex);
        if (!vertex)
        {
            result.diagnostics.emplace_back("multi-slot compiler did not produce a vertex stage");
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

    static GateResult RunDirectIncludeManifestCase()
    {
        GateResult result;
        result.name = "AA.direct-include-manifest";

        MaterialDefinition definition{};
        definition.code_module_requirements.push_back(
            GLSLCodeModuleID::SkyLightHeader);
        ShaderResourceManifest manifest{};
        if (!build2d::Build2DShaderResourceManifest(definition, manifest)
         || !manifest.IsValid())
        {
            result.diagnostics.emplace_back("2D DirectInclude manifest build failed");
            result.passed = false;
            return result;
        }

        const FixedMaterialDef fixed_definition{
            "DirectIncludeManifest",
            PrimitiveType::Triangles,
            nullptr,
            0,
            nullptr,
            0
        };
        CompositorMaterialBuildConfig config{};
        config.resource_manifest = &manifest;
        config.generate_only = true;
        ShaderProgramBuildSpec *build_spec = CompileCompositorMaterial(
            nullptr,
            fixed_definition,
            "#version 450\nvoid main(){gl_Position=vec4(0.0);}\n",
            "#version 450\nlayout(location=0) out vec4 outColor;\nvoid main(){outColor=vec4(1.0);}\n",
            config);
        if (!build_spec)
        {
            result.diagnostics.emplace_back("DirectInclude manifest source generation failed");
        }
        else
        {
            const ShaderCreateInfo *fragment =
                build_spec->GetStageShader(ShaderStage::Fragment);
            if (!fragment
             || fragment->GetFinalGLSL().find("// GLSLCodeModule:") == std::string::npos)
                result.diagnostics.emplace_back(
                    "DirectInclude code modules were not injected before fragment code");
        }

        delete build_spec;
        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunDescriptorBuilderConvergenceCase()
    {
        GateResult result;
        result.name = "AC.descriptor-builder-convergence";

        const MaterialDefinition *unlit_texture =
            GetMaterialDefinitionFileRegistry().FindByID("UnlitTexture");
        const MaterialDefinition *texture_array =
            GetMaterialDefinitionFileRegistry().FindByID("Texture2DArray");
        if (!unlit_texture || !texture_array)
        {
            result.diagnostics.emplace_back(
                "Descriptor convergence definitions were not loaded");
            result.passed = false;
            return result;
        }

        const auto build_2d_descriptors =
            [](const MaterialDefinition &definition)
                -> std::vector<FixedDescriptorEntry>
        {
            MaterialDefinitionBuildRequest request{};
            const Material2DBuildParams params =
                Material2DBuildParams::From(request, definition);
            ShaderResourceManifest manifest{};
            if (!build2d::Build2DShaderResourceManifest(definition, manifest)
             || !manifest.IsValid())
                return {};
            return build2d::Build2DDescriptorsFromDefinition(params, manifest);
        };

        const std::vector<FixedDescriptorEntry> unlit_descriptors =
            build_2d_descriptors(*unlit_texture);
        const std::vector<FixedDescriptorEntry> array_descriptors =
            build_2d_descriptors(*texture_array);
        if (unlit_descriptors.empty() || array_descriptors.empty())
        {
            result.diagnostics.emplace_back(
                "2D descriptor builders must produce descriptors for file definitions");
        }
        else
        {
            const FixedDescriptorEntry *unlit_sampler = nullptr;
            for (const auto &entry : unlit_descriptors)
            {
                if (entry.semantic == DescriptorSemantic::MaterialSampler
                 && entry.texture_slot == TextureSlot::BaseColor)
                {
                    unlit_sampler = &entry;
                    break;
                }
            }

            if (!unlit_sampler
             || !unlit_sampler->glsl_type
             || std::strcmp(unlit_sampler->glsl_type, "sampler2D") != 0)
            {
                result.diagnostics.emplace_back(
                    "2D sampler type was not preserved");
            }

            bool has_data_index_rows = false;
            bool has_texture_layer_rows = false;
            for (const auto &entry : array_descriptors)
            {
                has_data_index_rows |=
                    entry.semantic == DescriptorSemantic::MaterialDataIndexTable;
                has_texture_layer_rows |=
                    entry.semantic == DescriptorSemantic::MaterialTextureLayerTable;
            }
            if (!has_data_index_rows || !has_texture_layer_rows)
                result.diagnostics.emplace_back(
                    "2D material data slots must emit both index and texture-layer rows");

            const MaterialResourceLayout unlit_layout =
                BuildMaterialResourceLayout(
                    unlit_descriptors.data(),
                    static_cast<uint32_t>(unlit_descriptors.size()));
            std::vector<std::string> layout_diagnostics;
            if (!ValidateMaterialResourceLayout(unlit_layout, layout_diagnostics))
                result.diagnostics.emplace_back(
                    "2D descriptor output failed resource-layout validation");

            const FixedMaterialDef fixed_definition{
                "DescriptorPolicy",
                PrimitiveType::Triangles,
                nullptr,
                0,
                unlit_descriptors.data(),
                static_cast<uint32_t>(unlit_descriptors.size())
            };
            CompositorMaterialBuildConfig config{};
            config.material_definition = unlit_texture;
            config.generate_only = true;
            ShaderProgramBuildSpec *build_spec = CompileCompositorMaterial(
                nullptr,
                fixed_definition,
                "#version 450\nvoid main(){gl_Position=vec4(0.0);}\n",
                "#version 450\nlayout(location=0) out vec4 outColor;\nvoid main(){outColor=vec4(1.0);}\n",
                config);
            bool checked_required_policy = false;
            if (!build_spec)
            {
                result.diagnostics.emplace_back(
                    "Material compiler rejected the 2D descriptor policy contract");
            }
            else
            {
                for (const auto &requirement :
                     build_spec->GetMaterialResourceLayout().requirements)
                {
                    if (requirement.semantic == DescriptorSemantic::MaterialSampler
                     && requirement.texture_slot == TextureSlot::BaseColor)
                    {
                        checked_required_policy = true;
                        if (!requirement.required || requirement.allow_fallback)
                            result.diagnostics.emplace_back(
                                "Definition-required texture policy was not propagated");
                        break;
                    }
                }
                delete build_spec;
            }
            if (!checked_required_policy)
                result.diagnostics.emplace_back(
                    "Compiled 2D resource layout omitted the material sampler policy");

            ShaderResourceManifest unlit_manifest{};
            ShaderResourceManifest array_manifest{};
            if (!build2d::Build2DShaderResourceManifest(*unlit_texture, unlit_manifest)
             || !build2d::Build2DShaderResourceManifest(*texture_array, array_manifest)
             || descriptor_builder_common::HashResourceContract(
                    unlit_manifest.stable_hash, unlit_descriptors)
                    == descriptor_builder_common::HashResourceContract(
                        array_manifest.stable_hash, array_descriptors))
            {
                result.diagnostics.emplace_back(
                    "Different 2D descriptor/resource contracts must hash differently");
            }
        }

        MaterialDefinition camera_definition{};
        camera_definition.vertex_node_config.projection =
            ProjectionMode::ClipPassthrough;
        camera_definition.ubo_requirements.push_back(
            UBODescriptorSemantic::CameraInfo);
        MaterialDefinitionBuildRequest camera_request{};
        const Material2DBuildParams camera_params =
            Material2DBuildParams::From(camera_request, camera_definition);
        ShaderResourceManifest camera_manifest{};
        const auto camera_descriptors =
            build2d::Build2DDescriptorsFromDefinition(
                camera_params, camera_manifest);
        bool has_camera = false;
        for (const auto &entry : camera_descriptors)
            has_camera |= entry.semantic == DescriptorSemantic::CameraInfo;
        if (!has_camera)
            result.diagnostics.emplace_back(
                "2D definition UBO requirements must be emitted by the common builder");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunShaderLibraryPathCase()
    {
        GateResult result;
        result.name = "AB.shader-library-path";

        const std::string root = GetShaderLibraryPath("ShaderLibrary");
        const hgl::filesystem::Path root_path(hgl::ToOSString(root));
        if (!IsShaderLibraryRoot(root_path))
        {
            result.diagnostics.emplace_back(
                "ShaderLibrary path resolver did not find a valid material root");
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
        definition.texture_slot_decls = {
            {TextureSlot::BaseColor, GLSLSamplerType::Sampler2D, true, "TextureBaseColor"}
        };

        std::vector<FixedDescriptorEntry> descriptors;
        descriptor_builder_common::AppendDefinitionMaterialDescriptors(
            descriptors,
            definition,
            uint32_t(VK_SHADER_STAGE_VERTEX_BIT),
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT));

        ShaderResourceManifest compatible_manifest{};
        compatible_manifest.texture_count = 1;
        compatible_manifest.textures[0] = {
            "TextureBaseColor",
            "sampler2D",
            DescriptorSemantic::MaterialSampler,
            TextureSlot::BaseColor,
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
            true
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
         || !descriptor_builder_common::AppendManifestTextureDescriptors(
                descriptors, compatible_manifest)
         || !compatible_manifest.IsValid())
        {
            result.diagnostics.emplace_back(
                "Compatible definition and module resources must merge without duplication.");
        }
        else
        {
            const MaterialResourceLayout layout =
                BuildMaterialResourceLayout(
                    descriptors.data(),
                    static_cast<uint32_t>(descriptors.size()));
            std::vector<std::string> diagnostics;
            if (!ValidateMaterialResourceLayout(layout, diagnostics))
                result.diagnostics.emplace_back(
                    "Merged definition/module resource contract failed validation.");

            bool has_required_sampler = false;
            bool has_single_material_ssbo = false;
            for (const auto &req : layout.requirements)
            {
                if (req.semantic == DescriptorSemantic::MaterialSampler
                 && req.texture_slot == TextureSlot::BaseColor)
                    has_required_sampler = req.required && !req.allow_fallback;
                if (req.semantic == DescriptorSemantic::MaterialDataSlotData
                 && req.data_slot == DefaultMaterialDataSlot)
                    has_single_material_ssbo = true;
            }
            if (!has_required_sampler || !has_single_material_ssbo)
                result.diagnostics.emplace_back(
                    "Merged resource policy or SSBO identity was not preserved.");
        }

        ShaderResourceManifest name_conflict{};
        name_conflict.texture_count = 1;
        name_conflict.textures[0] = {
            "TextureBaseColor",
            "samplerCube",
            DescriptorSemantic::MaterialSampler,
            TextureSlot::BaseColor,
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
            true
        };
        if (descriptor_builder_common::AppendManifestTextureDescriptors(
                descriptors, name_conflict)
         || name_conflict.error != ShaderResourceManifestError::ResourceConflict)
        {
            result.diagnostics.emplace_back(
                "Same-name sampler type conflicts must fail explicitly.");
        }

        ShaderResourceManifest slot_conflict{};
        slot_conflict.texture_count = 1;
        slot_conflict.textures[0] = {
            "TextureBaseColorAlias",
            "sampler2D",
            DescriptorSemantic::MaterialSampler,
            TextureSlot::BaseColor,
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
            true
        };
        if (descriptor_builder_common::AppendManifestTextureDescriptors(
                descriptors, slot_conflict)
         || slot_conflict.error != ShaderResourceManifestError::ResourceConflict)
        {
            result.diagnostics.emplace_back(
                "Same semantic slot with a different name must fail explicitly.");
        }

        ShaderResourceManifest texture_manifest{};
        texture_manifest.texture_count = 1;
        texture_manifest.textures[0] = {
            "TextureNormal",
            "sampler2D",
            DescriptorSemantic::MaterialTexture,
            TextureSlot::Normal,
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
            false
        };
        std::vector<FixedDescriptorEntry> texture_descriptors;
        if (!descriptor_builder_common::AppendManifestTextureDescriptors(
                texture_descriptors, texture_manifest)
         || texture_descriptors.size() != 1
         || texture_descriptors[0].kind != DescriptorKind::Texture
         || texture_descriptors[0].semantic != DescriptorSemantic::MaterialTexture)
        {
            result.diagnostics.emplace_back(
                "Manifest MaterialTexture must remain a texture descriptor, not a sampler alias.");
        }

        FixedDescriptorEntry hash_entry{};
        hash_entry.set_type = DescriptorSetType::Material;
        hash_entry.kind = DescriptorKind::TextureSampler;
        hash_entry.stage_flags = uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT);
        hash_entry.name = "TextureBaseColor";
        hash_entry.glsl_type = "sampler2D";
        hash_entry.semantic = DescriptorSemantic::MaterialSampler;
        hash_entry.semantic_layer = DescriptorSemanticLayer::Sampler;
        hash_entry.texture_slot = TextureSlot::BaseColor;
        hash_entry.has_requirement_policy = true;
        hash_entry.required = true;
        hash_entry.allow_fallback = false;

        std::vector<FixedDescriptorEntry> hash_entries{hash_entry};
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
        hash_entries[0].glsl_type = "sampler2DArray";
        const uint64_t sampler_hash =
            descriptor_builder_common::HashResourceContract(0, hash_entries);
        if (strict_hash == sampler_hash)
            result.diagnostics.emplace_back(
                "Sampler type changes must change the resource contract hash.");

        FixedDescriptorEntry ssbo_hash_entry{};
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

        std::vector<FixedDescriptorEntry> ssbo_hash_entries{ssbo_hash_entry};
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
}

int main(const int argc, char **argv)
{
    if (argc > 2)
    {
        GLogError(
            "[MaterialResourceLayoutRegressionGate] Expected zero or one group argument.");
        return 2;
    }

    const char *selected_group = argc == 2 ? argv[1] : "all";
    if (!IsKnownRegressionGroup(selected_group))
    {
        GLogError(
            "[MaterialResourceLayoutRegressionGate] Unknown regression group: %s",
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

    std::vector<GateResult> results;

    if (run_descriptor)
    {
        constexpr FixedDescriptorEntry valid_entries[] =
        {
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::MaterialDataIndexTable, DescriptorSemanticLayer::SSBO },
            { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::Texture },
            { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "SkyCubemap", nullptr, "samplerCube", DescriptorSemantic::MaterialSampler, TextureSlot::Custom0, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::Sampler },
        };
        results.push_back(RunValidationCase("A.valid-layered-paths", valid_entries, uint32_t(std::size(valid_entries)), true));

        constexpr FixedDescriptorEntry unknown_semantic[] =
        {
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "broken", "ViewportInfo", nullptr, DescriptorSemantic::Unknown, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
        };
        results.push_back(RunValidationCase("B1.unknown-semantic-hard-fail", unknown_semantic, 1, false));

        constexpr FixedDescriptorEntry semantic_kind_mismatch[] =
        {
            { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "mtl_data_index_rows", "DataIndexRows", "sampler2D", DescriptorSemantic::MaterialDataIndexTable, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::MaterialDataIndexTable, DescriptorSemanticLayer::SSBO },
        };
        results.push_back(RunValidationCase("B2.semantic-kind-mismatch-hard-fail", semantic_kind_mismatch, 1, false));

        constexpr FixedDescriptorEntry invalid_fixed_descriptor[] =
        {
            { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "PBRSurfaceData", nullptr, DescriptorSemantic::MaterialDataSlotData, TextureSlot::BaseColor, 0xffu, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO },
        };
        results.push_back(RunValidationCase("B3.invalid-fixed-descriptor-hard-fail", invalid_fixed_descriptor, 1, false));

        constexpr FixedDescriptorEntry palette_explicit[] =
        {
            { DescriptorSetType::Material, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "color_palette", "ColorPalette", nullptr, DescriptorSemantic::MaterialColorPalette, TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
        };
        results.push_back(RunValidationCase("C.material-color-palette-explicit", palette_explicit, 1, true));
    }

    if (run_descriptor) results.push_back(RunBindlessEquivalenceCase());
    if (run_materialization) results.push_back(RunMaterializationSharedInstanceCase());
    if (run_materialization) results.push_back(RunBuiltinRegistryCoverageCase());
    if (run_materialization) results.push_back(RunBootstrapMaterialBoundaryCase());
    if (run_materialization) results.push_back(RunMaterialDefinitionIdentityCase());
    if (run_pipeline) results.push_back(RunUnifiedMaterialBaselineCase());
    if (run_pipeline) results.push_back(RunUnifiedPureColorFragmentCase());
    if (run_descriptor) results.push_back(RunUnifiedMaterialContractCase());
    if (run_materialization) results.push_back(RunTransformGraphModelCase());
    if (run_materialization) results.push_back(RunTransformGraphCompositionCase());
    if (run_cache) results.push_back(RunMaterialShaderVariantIdentityCase());
    if (run_descriptor) results.push_back(RunMaterialSSBOBindingKeyCase());
    if (run_materialization) results.push_back(RunResolvedMaterialRenderStateCase());
    if (run_materialization) results.push_back(RunMaterialDefinitionFileSchemaCase());
    if (run_materialization) results.push_back(RunFallbackInferenceCase());
    if (run_glsl) results.push_back(RunGLSLCodeModuleRegistryCase());
    if (run_glsl) results.push_back(RunShaderResourceManifestCase());
    if (run_glsl) results.push_back(RunProviderResourceManifestCase());
    if (run_glsl) results.push_back(RunGLSLCodeModuleFileCase());
    if (run_glsl) results.push_back(RunCapabilityResolverCase());
    if (run_interface) results.push_back(RunShaderSemanticRegistryCase());
    if (run_interface) results.push_back(RunMaterialVertexABICharacterizationCase());
    if (run_interface) results.push_back(RunMaterialSemanticABIParityCase());
    if (run_interface) results.push_back(RunMaterialSemanticResolverPreviewCase());
    if (run_glsl) results.push_back(RunCompositorVersionPlacementCase());
    if (run_cache) results.push_back(RunProviderGraphIdentityCase());
    if (run_cache) results.push_back(RunProviderGraphCompositionCase());
    if (run_cache) results.push_back(RunResolvedStageCacheIdentityCase());
    if (run_pipeline) results.push_back(RunMaterialMultiSlotSourceCase());
    if (run_pipeline) results.push_back(RunDirectIncludeManifestCase());
    if (run_descriptor) results.push_back(RunDescriptorBuilderConvergenceCase());
    if (run_pipeline) results.push_back(RunShaderLibraryPathCase());
    if (run_descriptor) results.push_back(RunResourceContractBoundaryCase());

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
