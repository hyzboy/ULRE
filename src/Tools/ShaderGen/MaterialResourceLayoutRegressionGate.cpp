#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>

#include <algorithm>
#include <cstdio>
#include <iterator>
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

    static GateResult RunBindlessEquivalenceCase()
    {
        GateResult result;
        result.name = "D.bindless-dual-form-equivalence";

        constexpr FixedDescriptorEntry standard_entries[] =
        {
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo },
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo },
            { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr, DescriptorSemantic::SkyInfo },
            { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld },
            { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable },
            { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialInstance, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::PBRSurface },
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
            { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld },
            { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable },
            { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialInstance, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::PBRSurface },
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
            bool is_2d = false;
            bool is_text = false;
            bool with_sky = false;
        };

        static const ExpectedEntry expected[] =
        {
            { BUILTIN_MTL_DEF_FALLBACK_2D, BuiltinMaterialCreatorID::PureColor2D, true,  false, false },
            { BUILTIN_MTL_DEF_FALLBACK_3D, BuiltinMaterialCreatorID::PureColor3D, false, false, false },
            { BUILTIN_MTL_DEF_MISSING_MATERIAL, BuiltinMaterialCreatorID::PureColor3D, false, false, false },
            { BUILTIN_MTL_DEF_TEXT, BuiltinMaterialCreatorID::Text2D, true, true, false },
            { BUILTIN_MTL_DEF_SKY, BuiltinMaterialCreatorID::SkyMinimal, false, false, true },
            { "Standard", BuiltinMaterialCreatorID::Standard, false, false, true },
            { "StandardTextureArray", BuiltinMaterialCreatorID::StandardTextureArray, false, false, true },
            { "PBRColor3D", BuiltinMaterialCreatorID::PBRColor3D, false, false, true },
            { "Gizmo3D", BuiltinMaterialCreatorID::Gizmo3D, false, false, false },
            { "RectTexture2D", BuiltinMaterialCreatorID::RectTexture2D, true, false, false },
            { "RectTexture2DArray", BuiltinMaterialCreatorID::RectTexture2DArray, true, false, false },
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

            if (bmi.is_2d != entry.is_2d)
            {
                result.diagnostics.emplace_back(std::string("is_2d mismatch: ") + entry.definition_id);
                continue;
            }

            if (bmi.is_text != entry.is_text)
            {
                result.diagnostics.emplace_back(std::string("is_text mismatch: ") + entry.definition_id);
                continue;
            }

            if (bmi.with_sky != entry.with_sky)
                result.diagnostics.emplace_back(std::string("with_sky mismatch: ") + entry.definition_id);
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
}

int main()
{
    std::vector<GateResult> results;

    constexpr FixedDescriptorEntry valid_entries[] =
    {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialSSBOIndexTable, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::MaterialSSBOIndexTable, DescriptorSemanticLayer::SSBO },
        { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::Texture },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "SkyCubemap", nullptr, "samplerCube", DescriptorSemantic::MaterialSampler, TextureSlot::Custom0, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::Sampler },
    };
    results.push_back(RunValidationCase("A.valid-layered-paths", valid_entries, uint32_t(std::size(valid_entries)), true));

    constexpr FixedDescriptorEntry unknown_semantic[] =
    {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "broken", "ViewportInfo", nullptr, DescriptorSemantic::Unknown, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
    };
    results.push_back(RunValidationCase("B1.unknown-semantic-hard-fail", unknown_semantic, 1, false));

    constexpr FixedDescriptorEntry semantic_kind_mismatch[] =
    {
        { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "mtl_data_index_rows", "DataIndexRows", "sampler2D", DescriptorSemantic::MaterialSSBOIndexTable, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::MaterialSSBOIndexTable, DescriptorSemanticLayer::SSBO },
    };
    results.push_back(RunValidationCase("B2.semantic-kind-mismatch-hard-fail", semantic_kind_mismatch, 1, false));

    constexpr FixedDescriptorEntry invalid_fixed_descriptor[] =
    {
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialInstance, TextureSlot::BaseColor, 0xffu, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO },
    };
    results.push_back(RunValidationCase("B3.invalid-fixed-descriptor-hard-fail", invalid_fixed_descriptor, 1, false));

    constexpr FixedDescriptorEntry palette_explicit[] =
    {
        { DescriptorSetType::Material, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "color_pattle", "ColorPattle", nullptr, DescriptorSemantic::MaterialColorPalette, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
    };
    results.push_back(RunValidationCase("C.material-color-palette-explicit", palette_explicit, 1, true));

    results.push_back(RunBindlessEquivalenceCase());
    results.push_back(RunBuiltinRegistryCoverageCase());
    results.push_back(RunFallbackInferenceCase());

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
