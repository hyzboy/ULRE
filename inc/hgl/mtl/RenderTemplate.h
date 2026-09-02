#pragma once

#include <hgl/common/ShaderStageDef.h>
#include <hgl/type/String.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    class ShaderCodeModuleRegistry;
    enum class RenderTemplateID : uint8
    {
        Unknown = 0,
        ForwardLitShadowedAO,
        ForwardLitShadowedIdentityAO,
        ForwardLitUnshadowedAO,
        ForwardUnlit,
        ShadowCasterOpaque,
        ShadowCasterMasked,
        Sky,
        Decal,
        PostProcessSSAO,
        PostProcessDOF
    };

    enum class ShaderModuleSlotRole : uint8
    {
        Unknown = 0,
        SurfaceProvider,
        DirectLightProvider,
        ShadowProvider,
        AmbientLightProvider,
        AmbientOcclusionProvider,
        LightingModel,
        OutputPolicy
    };

    constexpr uint32 MaxRenderTemplateModuleRoots = 8;

    struct RenderTemplateModuleRoot
    {
        ShaderModuleSlotRole role = ShaderModuleSlotRole::Unknown;
        AnsiString module_name;
    };

    struct RenderTemplateRequest
    {
        RenderTemplateID template_id = RenderTemplateID::Unknown;
        ShaderStage stage = ShaderStage::Fragment;
        uint32 template_version = 0;
        RenderTemplateModuleRoot module_roots[
            MaxRenderTemplateModuleRoots]{};
        uint32 module_root_count = 0;

        bool AddModuleRoot(
            ShaderModuleSlotRole role,
            const AnsiString &module_name) noexcept;

        const RenderTemplateModuleRoot *FindModuleRoot(
            ShaderModuleSlotRole role) const noexcept;

        uint64 GetHash() const noexcept;
    };

    struct RenderTemplateSlot
    {
        ShaderModuleSlotRole role = ShaderModuleSlotRole::Unknown;
        bool required = true;
    };

    struct RenderTemplateDefinition
    {
        RenderTemplateID id = RenderTemplateID::Unknown;
        const char *name = nullptr;
        ShaderStage stage = ShaderStage::Fragment;
        uint32 version = 0;
        const RenderTemplateSlot *slots = nullptr;
        uint32 slot_count = 0;
    };

    enum class RenderTemplateValidationError : uint8
    {
        None = 0,
        UnknownTemplate,
        StageMismatch,
        VersionMismatch,
        EmptyModuleRoot,
        UnknownSlotRole,
        UnexpectedSlotRole,
        DuplicateSlotRole,
        MissingRequiredSlot
        ,ModuleNotFound
        ,ModuleSlotMismatch
        ,MissingModuleCapability
    };

    struct RenderTemplateValidationDiagnostic
    {
        RenderTemplateValidationError error =
            RenderTemplateValidationError::None;
        RenderTemplateID template_id = RenderTemplateID::Unknown;
        ShaderModuleSlotRole role = ShaderModuleSlotRole::Unknown;
        AnsiString module_name;
    };

    const char *GetRenderTemplateName(RenderTemplateID id) noexcept;
    const char *GetShaderModuleSlotRoleName(ShaderModuleSlotRole role) noexcept;
    const char *GetRenderTemplateValidationErrorName(
        RenderTemplateValidationError error) noexcept;

    const RenderTemplateDefinition *FindRenderTemplate(
        RenderTemplateID id) noexcept;

    bool ValidateRenderTemplateRequest(
        const RenderTemplateRequest &request,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept;

    bool ValidateRenderTemplateRequest(
        const RenderTemplateRequest &request,
        const ShaderCodeModuleRegistry &module_registry,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept;
}
