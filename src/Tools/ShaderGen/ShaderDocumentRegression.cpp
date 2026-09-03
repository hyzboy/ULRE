#include <hgl/mtl/ShaderDocument.h>
#include <hgl/mtl/FixedPipelineVariant.h>
#include <hgl/mtl/RenderTemplate.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/ShaderCodeModuleFile.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/mtl/ShaderLegacyAuditShell.h>
#include <hgl/mtl/ShaderRuntimeReadOnlyValidationShell.h>
#include "../../ShaderGen/document/DocumentFragmentBuilder.h"

using namespace hgl::graph::mtl;
using hgl::AnsiString;

int main()
{
    ShaderDocument document;
    document.Add(ShaderDocumentBlockKind::Version, "#version 460");
    document.Add(ShaderDocumentBlockKind::Define, "#define TEST_DEFINE 1");
    document.Add(ShaderDocumentBlockKind::Raw, "void main() {}");

    AnsiString serialized;
    ShaderDocumentDiagnostics diagnostics;
    if (!document.Serialize(serialized, diagnostics))
        return 1;
    if (serialized.IsEmpty() || diagnostics.GetCount() != 0)
        return 6;
    ShaderDocumentDiagnostics fragment_diagnostics;
    AnsiString fragment;
    if (!document.SerializeFragment(fragment, fragment_diagnostics)
     || fragment != serialized)
        return 9;
    const hgl::uint64 first_hash = document.GetSerializedHash(diagnostics);
    const hgl::uint64 second_hash = document.GetSerializedHash(diagnostics);
    if (first_hash != second_hash)
        return 5;

    ShaderDocument invalid;
    ShaderDocumentSource invalid_source;
    invalid_source.stage = "fragment";
    invalid.Add(ShaderDocumentBlockKind::Raw, "raw");
    invalid.Add(ShaderDocumentBlockKind::Version, "#version 460", invalid_source);
    invalid.Add(ShaderDocumentBlockKind::Version, "#version 460", invalid_source);
    if (invalid.Serialize(serialized, diagnostics)
     || diagnostics.GetCount() < 2)
        return 2;
    if (diagnostics[0]->source.stage.IsEmpty())
        return 10;

    const AnsiString legacy = "#version 460\nvoid main() {}\n";
    ShaderDocument raw_document;
    ShaderDocumentSource raw_source;
    raw_source.stage = "fragment";
    raw_document.Add(ShaderDocumentBlockKind::Raw, legacy, raw_source);
    AnsiString raw_serialized;
    ShaderDocumentDiagnostics raw_diagnostics;
    if (!raw_document.Serialize(raw_serialized, raw_diagnostics)
     || raw_serialized != legacy
     || raw_document.GetBlockCount() != 1
     || raw_document.GetBlock(0).source.stage != "fragment")
        return 3;

    ShaderDocument injected;
    ShaderDocumentSource injected_source;
    injected_source.stage = "mesh";
    injected.Add(
        ShaderDocumentBlockKind::Raw,
        "#version 460\n",
        injected_source);
    injected.Add(
        ShaderDocumentBlockKind::Define,
        "#define INJECTED 1\n",
        injected_source);
    injected.Add(
        ShaderDocumentBlockKind::Raw,
        "void main() {}\n",
        injected_source);
    AnsiString injected_serialized;
    ShaderDocumentDiagnostics injected_diagnostics;
    if (!injected.Serialize(injected_serialized, injected_diagnostics)
     || injected_serialized !=
            "#version 460\n#define INJECTED 1\nvoid main() {}\n")
        return 7;

    ShaderDocument builder_document;
    ShaderDocumentDiagnostics builder_diagnostics;
    DocumentFragmentBuilder builder(builder_document, builder_diagnostics);
    if (!builder.Add(ShaderDocumentBlockKind::Version, "#version 460\n")
     || !builder.Add(ShaderDocumentBlockKind::Define, "#define TEST 1\n")
     || !builder.Add(ShaderDocumentBlockKind::MainBody, "void main() {}\n")
     || builder.Add(ShaderDocumentBlockKind::Resource, "layout(set=0) uniform X {};\n")
     || builder_diagnostics.GetCount() != 1)
        return 11;

    ShaderLegacyAuditShell audit;
    audit.BeginAudit();
    audit.CompleteAudit();
    if (audit.GetState() != ShaderLegacyAuditState::ReadyForCleanup
     || !audit.IsCleanupSafe())
        return 12;

    ShaderRuntimeReadOnlyValidationShell validation;
    validation.BeginValidation();
    validation.SetCacheState(false);
    validation.SetArtifactReadable(false);
    validation.SetSchemaState(false);
    validation.SetModuleReady(false);
    validation.CompleteValidation();
    if (validation.GetStage() != ShaderRuntimeValidationStage::Complete
     || !validation.GetSummary().dry_run
     || validation.GetSummary().cache_valid
     || validation.GetSummary().artifact_readable)
        return 13;

    RenderTemplateRequest template_request{};
    template_request.template_id = RenderTemplateID::ForwardLitShadowedAO;
    template_request.template_version = 1;
    if (!template_request.AddModuleRoot(
            ShaderModuleSlotRole::SurfaceProvider, "surface/pbr_texture")
     || !template_request.AddModuleRoot(
            ShaderModuleSlotRole::DirectLightProvider, "direct/sun")
     || !template_request.AddModuleRoot(
            ShaderModuleSlotRole::ShadowProvider, "shadow/pcf")
     || !template_request.AddModuleRoot(
            ShaderModuleSlotRole::AmbientLightProvider, "ambient/ibl")
     || !template_request.AddModuleRoot(
            ShaderModuleSlotRole::AmbientOcclusionProvider, "ao/identity")
     || !template_request.AddModuleRoot(
            ShaderModuleSlotRole::LightingModel, "lighting/pbr")
     || !template_request.AddModuleRoot(
            ShaderModuleSlotRole::OutputPolicy, "output/forward_hdr"))
        return 14;

    RenderTemplateValidationDiagnostic template_diagnostic{};
    if (!ValidateRenderTemplateRequest(template_request, template_diagnostic)
     || template_request.GetHash() == 0)
        return 15;

    RenderTemplateRequest incomplete_request{};
    incomplete_request.template_id = RenderTemplateID::ForwardLitShadowedAO;
    incomplete_request.template_version = 1;
    if (ValidateRenderTemplateRequest(incomplete_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::MissingRequiredSlot)
        return 16;

    RenderTemplateRequest unknown_template_request{};
    unknown_template_request.template_version = 1;
    if (ValidateRenderTemplateRequest(
            unknown_template_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::UnknownTemplate)
        return 17;

    RenderTemplateRequest wrong_stage_request = template_request;
    wrong_stage_request.stage = hgl::graph::ShaderStage::Mesh;
    if (ValidateRenderTemplateRequest(wrong_stage_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::StageMismatch)
        return 18;

    RenderTemplateRequest wrong_version_request = template_request;
    ++wrong_version_request.template_version;
    if (ValidateRenderTemplateRequest(wrong_version_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::VersionMismatch)
        return 19;

    RenderTemplateRequest unexpected_slot_request{};
    unexpected_slot_request.template_id = RenderTemplateID::ForwardUnlit;
    unexpected_slot_request.template_version = 1;
    if (!unexpected_slot_request.AddModuleRoot(
            ShaderModuleSlotRole::SurfaceProvider, "surface/pbr_texture")
     || !unexpected_slot_request.AddModuleRoot(
            ShaderModuleSlotRole::ShadowProvider, "shadow/pcf")
     || ValidateRenderTemplateRequest(
            unexpected_slot_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::UnexpectedSlotRole)
        return 20;

    RenderTemplateRequest duplicate_slot_request = template_request;
    duplicate_slot_request.module_roots[duplicate_slot_request.module_root_count] =
        duplicate_slot_request.module_roots[0];
    ++duplicate_slot_request.module_root_count;
    if (ValidateRenderTemplateRequest(duplicate_slot_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::DuplicateSlotRole)
        return 21;

    const FixedShaderVariantKey variant_key{
        FixedPipelineFamily::ForwardLit,
        FixedShaderProfile::ForwardLitPBRIBLRGBA16F2,
        FixedShaderQualityTier::High
    };
    const FixedPipelineVariant *variant =
        ResolveFixedPipelineVariant(variant_key);
    if (!variant
     || variant->fragment_template != RenderTemplateID::ForwardLitShadowedAO
     || variant->template_version != 1
     || !IsFixedShaderProfileAllowed(
            GetFixedShaderProfileMask(
                FixedShaderProfile::ForwardLitPBRIBLRGBA16F2),
            FixedShaderProfile::ForwardLitPBRIBLRGBA16F2)
     || IsFixedShaderProfileAllowed(
            GetFixedShaderProfileMask(
                FixedShaderProfile::ForwardLitPBRIBLRGBA16F2),
            FixedShaderProfile::ForwardLitFakePBRSH)
     || ResolveFixedPipelineVariant({
            FixedPipelineFamily::ForwardLit,
            FixedShaderProfile::ForwardLitPBRIBLRGBA16F2,
            FixedShaderQualityTier::Low }))
        return 22;

    const char slot_module[] =
        "// @ulre begin\n"
        "// @ulre name test_ambient\n"
        "// @ulre kind Utility\n"
        "// @ulre slot ambient_light_provider\n"
        "// @ulre provides_capability ambient_diffuse|ambient_specular\n"
        "// @ulre requires_capability surface_normal\n"
        "// @ulre end\n"
        "void TestAmbient() {}\n";
    ShaderCodeModuleFileData module_file_data{};
    if (ParseShaderCodeModuleFile(
            slot_module,
            int(sizeof(slot_module) - 1),
            module_file_data) != ShaderCodeModuleParseResult::OK
     || module_file_data.slot_role
            != ShaderModuleSlotRole::AmbientLightProvider
     || module_file_data.provided_capabilities
            != (hgl::uint32(ShaderModuleCapability::AmbientDiffuse)
              | hgl::uint32(ShaderModuleCapability::AmbientSpecular))
     || module_file_data.required_capabilities
            != hgl::uint32(ShaderModuleCapability::SurfaceNormal))
        return 23;

    const ShaderModuleSlotRole root_roles[] =
    {
        ShaderModuleSlotRole::SurfaceProvider,
        ShaderModuleSlotRole::DirectLightProvider,
        ShaderModuleSlotRole::ShadowProvider,
        ShaderModuleSlotRole::AmbientLightProvider,
        ShaderModuleSlotRole::AmbientOcclusionProvider,
        ShaderModuleSlotRole::LightingModel,
        ShaderModuleSlotRole::OutputPolicy
    };
    const char *const root_names[] =
    {
        "surface/pbr_texture", "direct/sun", "shadow/pcf", "ambient/ibl",
        "ao/identity", "lighting/pbr", "output/forward_hdr"
    };
    ShaderCodeModuleDefinition root_modules[7]{};
    ShaderCodeModuleRegistry root_registry;
    for (int index = 0; index < 7; ++index)
    {
        root_modules[index].name = root_names[index];
        root_modules[index].glsl_code = "";
        root_modules[index].slot_role = root_roles[index];
        if (!root_registry.Register(root_modules[index]))
            return 24;
    }
    root_modules[1].provided_capabilities =
        static_cast<hgl::uint32>(ShaderModuleCapability::DirectLight);
    root_modules[5].required_capabilities =
        static_cast<hgl::uint32>(ShaderModuleCapability::DirectLight);
    if (!ValidateRenderTemplateRequest(
            template_request, root_registry, template_diagnostic))
        return 25;
    ResolvedRenderTemplate resolved_template{};
    if (!ResolveRenderTemplate(
            template_request, root_registry, resolved_template,
            template_diagnostic)
     || !resolved_template.IsValid()
     || resolved_template.definition->id != template_request.template_id
     || resolved_template.module_root_count != template_request.module_root_count
     || resolved_template.manifest.code_module_count != 7
     || resolved_template.stable_hash == 0)
        return 29;
    root_modules[1].provided_capabilities = 0;
    if (ValidateRenderTemplateRequest(
            template_request, root_registry, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::MissingModuleCapability)
        return 26;
    root_modules[1].provided_capabilities =
        static_cast<hgl::uint32>(ShaderModuleCapability::DirectLight);
    root_modules[0].slot_role = ShaderModuleSlotRole::LightingModel;
    if (ValidateRenderTemplateRequest(
            template_request, root_registry, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::ModuleSlotMismatch)
        return 27;
    root_modules[0].slot_role = root_roles[0];
    const char *surface_conflicts[] = {"direct/sun"};
    root_modules[0].module_conflict_names = surface_conflicts;
    root_modules[0].module_conflict_count = 1;
    if (ValidateRenderTemplateRequest(
            template_request, root_registry, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::ModuleConflict)
        return 28;

    return 0;
}
