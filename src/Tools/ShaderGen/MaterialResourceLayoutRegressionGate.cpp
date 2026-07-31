#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/common/RenderOptions.h>

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
            row += std::to_string(req.slot_index);
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
            { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld },
            { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable },
            { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialInstance, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::PBRSurface },
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
}

int main()
{
    std::vector<GateResult> results;

    constexpr FixedDescriptorEntry valid_entries[] =
    {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::DataIndex, DescriptorSemanticLayer::SSBO },
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
        { DescriptorSetType::Material, DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "mtl_data_index_rows", "DataIndexRows", "sampler2D", DescriptorSemantic::MaterialDataIndexTable, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::DataIndex, DescriptorSemanticLayer::SSBO },
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
