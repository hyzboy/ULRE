#include <hgl/mtl/ShaderRuntimeReadOnlyValidationShell.h>

namespace hgl::graph::mtl
{
    ShaderRuntimeReadOnlyValidationShell::ShaderRuntimeReadOnlyValidationShell()
    {
        summary.dry_run = true;
        summary.cache_valid = false;
        summary.artifact_readable = false;
        summary.schema_valid = false;
        summary.module_ready = false;
        summary.stage = ShaderRuntimeValidationStage::Idle;
    }

    void ShaderRuntimeReadOnlyValidationShell::BeginValidation()
    {
        stage = ShaderRuntimeValidationStage::CacheLookup;
        summary.stage = stage;
        summary.dry_run = true;
    }

    void ShaderRuntimeReadOnlyValidationShell::SetCacheState(bool cache_valid)
    {
        summary.cache_valid = cache_valid;
        if(stage == ShaderRuntimeValidationStage::CacheLookup)
        {
            stage = ShaderRuntimeValidationStage::ArtifactRead;
            summary.stage = stage;
        }
    }

    void ShaderRuntimeReadOnlyValidationShell::SetArtifactReadable(bool readable)
    {
        summary.artifact_readable = readable;
        if(stage == ShaderRuntimeValidationStage::ArtifactRead)
        {
            stage = ShaderRuntimeValidationStage::DescriptorValidate;
            summary.stage = stage;
        }
    }

    void ShaderRuntimeReadOnlyValidationShell::SetSchemaState(bool schema_valid)
    {
        summary.schema_valid = schema_valid;
    }

    void ShaderRuntimeReadOnlyValidationShell::SetModuleReady(bool module_ready)
    {
        summary.module_ready = module_ready;
        if(module_ready && stage == ShaderRuntimeValidationStage::DescriptorValidate)
        {
            stage = ShaderRuntimeValidationStage::VulkanModuleCreate;
            summary.stage = stage;
        }
    }

    void ShaderRuntimeReadOnlyValidationShell::CompleteValidation()
    {
        stage = ShaderRuntimeValidationStage::Complete;
        summary.stage = stage;
        summary.dry_run = true;
    }

    bool ShaderRuntimeReadOnlyValidationShell::ValidateProgramArtifacts(
        const ShaderArtifactStore &store,
        const ShaderLinkSpec &link,
        const ShaderProgramArtifactMetadata &expected_metadata)
    {
        BeginValidation();

        const bool cache_valid = store.HasProgramMetadata(link);
        SetCacheState(cache_valid);

        ValueArray<uint8> vertex_spv;
        ValueArray<uint8> fragment_spv;
        const bool artifacts_readable =
            store.LoadProgramArtifacts(
                link,
                expected_metadata,
                vertex_spv,
                fragment_spv);
        SetArtifactReadable(artifacts_readable);

        const bool schema_valid =
            artifacts_readable
            && IsValidShaderProgramArtifactMetadata(expected_metadata);
        SetSchemaState(schema_valid);

        const bool module_ready =
            schema_valid
            && vertex_spv.GetCount() > 0
            && fragment_spv.GetCount() > 0;
        SetModuleReady(module_ready);
        CompleteValidation();
        return module_ready;
    }

    ShaderRuntimeValidationStage ShaderRuntimeReadOnlyValidationShell::GetStage() const
    {
        return stage;
    }

    const ShaderRuntimeReadOnlyValidationSummary &ShaderRuntimeReadOnlyValidationShell::GetSummary() const
    {
        return summary;
    }

    AnsiString ShaderRuntimeReadOnlyValidationShell::GetStatusText() const
    {
        switch(stage)
        {
        case ShaderRuntimeValidationStage::Idle:
            return "Runtime validation shell idle; no read-only artifact validation started.";
        case ShaderRuntimeValidationStage::CacheLookup:
            return "Runtime validation shell: evaluating cache hit/miss and artifact identity.";
        case ShaderRuntimeValidationStage::ArtifactRead:
            return "Runtime validation shell: verifying artifact payload is readable and metadata is valid.";
        case ShaderRuntimeValidationStage::DescriptorValidate:
            return "Runtime validation shell: validating schema and descriptor contract before Vulkan module creation.";
        case ShaderRuntimeValidationStage::VulkanModuleCreate:
            return "Runtime validation shell: Vulkan module creation path is ready for final validation.";
        case ShaderRuntimeValidationStage::Complete:
            return "Runtime validation shell: dry-run validation complete; final release cut is still deferred to the release freeze.";
        }

        return "Runtime validation shell: unknown stage.";
    }

    AnsiString ShaderRuntimeReadOnlyValidationShell::GetValidationChecklist() const
    {
        return AnsiString("1. Validate cache identity and artifact hash\n")
             + "2. Confirm SPV artifact is readable without GLSL generation\n"
             + "3. Verify schema/descriptor contract compatibility\n"
             + "4. Check Vulkan module creation readiness\n"
             + "5. Keep this as a dry-run gate until the final release freeze";
    }
}
