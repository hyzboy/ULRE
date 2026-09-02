#pragma once

#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    enum class ShaderRuntimeValidationStage
    {
        Idle,
        CacheLookup,
        ArtifactRead,
        DescriptorValidate,
        VulkanModuleCreate,
        Complete
    };

    struct ShaderRuntimeReadOnlyValidationSummary
    {
        bool dry_run = true;
        bool cache_valid = false;
        bool artifact_readable = false;
        bool schema_valid = false;
        bool module_ready = false;
        ShaderRuntimeValidationStage stage = ShaderRuntimeValidationStage::Idle;
    };

    class ShaderRuntimeReadOnlyValidationShell
    {
        ShaderRuntimeValidationStage stage = ShaderRuntimeValidationStage::Idle;
        ShaderRuntimeReadOnlyValidationSummary summary;

    public:
        ShaderRuntimeReadOnlyValidationShell();

        void BeginValidation();
        void SetCacheState(bool cache_valid);
        void SetArtifactReadable(bool readable);
        void SetSchemaState(bool schema_valid);
        void SetModuleReady(bool module_ready);
        void CompleteValidation();

        ShaderRuntimeValidationStage GetStage() const;
        const ShaderRuntimeReadOnlyValidationSummary &GetSummary() const;

        AnsiString GetStatusText() const;
        AnsiString GetValidationChecklist() const;
    };
}
