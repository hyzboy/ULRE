#include <hgl/mtl/RenderTemplate.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>

namespace hgl::graph::mtl
{
    namespace
    {
        constexpr RenderTemplateSlot ForwardLitSlots[] =
        {
            { ShaderModuleSlotRole::SurfaceProvider },
            { ShaderModuleSlotRole::DirectLightProvider },
            { ShaderModuleSlotRole::ShadowProvider },
            { ShaderModuleSlotRole::AmbientLightProvider },
            { ShaderModuleSlotRole::AmbientOcclusionProvider },
            { ShaderModuleSlotRole::LightingModel },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr RenderTemplateSlot ForwardLitUnshadowedSlots[] =
        {
            { ShaderModuleSlotRole::SurfaceProvider },
            { ShaderModuleSlotRole::DirectLightProvider },
            { ShaderModuleSlotRole::AmbientLightProvider },
            { ShaderModuleSlotRole::AmbientOcclusionProvider },
            { ShaderModuleSlotRole::LightingModel },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr RenderTemplateSlot ForwardUnlitSlots[] =
        {
            { ShaderModuleSlotRole::SurfaceProvider },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr RenderTemplateSlot ShadowCasterSlots[] =
        {
            { ShaderModuleSlotRole::SurfaceProvider },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr RenderTemplateSlot SkySlots[] =
        {
            { ShaderModuleSlotRole::AmbientLightProvider },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr RenderTemplateSlot DecalSlots[] =
        {
            { ShaderModuleSlotRole::SurfaceProvider },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr RenderTemplateSlot PostProcessSlots[] =
        {
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr RenderTemplateDefinition Templates[] =
        {
            { RenderTemplateID::ForwardLitShadowedAO,
              "forward_lit_shadowed_ao", ShaderStage::Fragment, 1,
              ForwardLitSlots, uint32(sizeof(ForwardLitSlots) / sizeof(ForwardLitSlots[0])) },
            { RenderTemplateID::ForwardLitShadowedIdentityAO,
              "forward_lit_shadowed_identity_ao", ShaderStage::Fragment, 1,
              ForwardLitSlots, uint32(sizeof(ForwardLitSlots) / sizeof(ForwardLitSlots[0])) },
            { RenderTemplateID::ForwardLitUnshadowedAO,
              "forward_lit_unshadowed_ao", ShaderStage::Fragment, 1,
              ForwardLitUnshadowedSlots, uint32(sizeof(ForwardLitUnshadowedSlots) / sizeof(ForwardLitUnshadowedSlots[0])) },
            { RenderTemplateID::ForwardUnlit,
              "forward_unlit", ShaderStage::Fragment, 1,
              ForwardUnlitSlots, uint32(sizeof(ForwardUnlitSlots) / sizeof(ForwardUnlitSlots[0])) },
            { RenderTemplateID::ShadowCasterOpaque,
              "shadow_caster_opaque", ShaderStage::Fragment, 1,
              ShadowCasterSlots, uint32(sizeof(ShadowCasterSlots) / sizeof(ShadowCasterSlots[0])) },
            { RenderTemplateID::ShadowCasterMasked,
              "shadow_caster_masked", ShaderStage::Fragment, 1,
              ShadowCasterSlots, uint32(sizeof(ShadowCasterSlots) / sizeof(ShadowCasterSlots[0])) },
            { RenderTemplateID::Sky,
              "sky", ShaderStage::Fragment, 1,
              SkySlots, uint32(sizeof(SkySlots) / sizeof(SkySlots[0])) },
            { RenderTemplateID::Decal,
              "decal", ShaderStage::Fragment, 1,
              DecalSlots, uint32(sizeof(DecalSlots) / sizeof(DecalSlots[0])) },
            { RenderTemplateID::PostProcessSSAO,
              "postprocess_ssao", ShaderStage::Fragment, 1,
              PostProcessSlots, uint32(sizeof(PostProcessSlots) / sizeof(PostProcessSlots[0])) },
            { RenderTemplateID::PostProcessDOF,
              "postprocess_dof", ShaderStage::Fragment, 1,
              PostProcessSlots, uint32(sizeof(PostProcessSlots) / sizeof(PostProcessSlots[0])) }
        };

        bool HasSlot(
            const RenderTemplateDefinition &definition,
            const ShaderModuleSlotRole role) noexcept
        {
            for (uint32 index = 0; index < definition.slot_count; ++index)
            {
                if (definition.slots[index].role == role)
                    return true;
            }
            return false;
        }

        bool SetFailure(
            RenderTemplateValidationDiagnostic &diagnostic,
            const RenderTemplateValidationError error,
            const RenderTemplateRequest &request,
            const ShaderModuleSlotRole role = ShaderModuleSlotRole::Unknown,
            const AnsiString &module_name = {}) noexcept
        {
            diagnostic.error = error;
            diagnostic.template_id = request.template_id;
            diagnostic.role = role;
            diagnostic.module_name = module_name;
            return false;
        }
    }

    bool RenderTemplateRequest::AddModuleRoot(
        const ShaderModuleSlotRole role,
        const AnsiString &module_name) noexcept
    {
        if (role == ShaderModuleSlotRole::Unknown
         || module_name.IsEmpty()
         || module_root_count >= MaxRenderTemplateModuleRoots
         || FindModuleRoot(role))
            return false;

        module_roots[module_root_count].role = role;
        module_roots[module_root_count].module_name = module_name;
        ++module_root_count;
        return true;
    }

    const RenderTemplateModuleRoot *RenderTemplateRequest::FindModuleRoot(
        const ShaderModuleSlotRole role) const noexcept
    {
        for (uint32 index = 0; index < module_root_count; ++index)
        {
            if (module_roots[index].role == role)
                return module_roots + index;
        }
        return nullptr;
    }

    uint64 RenderTemplateRequest::GetHash() const noexcept
    {
        hgl::hash::FNV1aHasher64 hash;
        hash << template_id << stage << template_version << module_root_count;
        for (uint32 index = 0; index < module_root_count; ++index)
        {
            hash << module_roots[index].role;
            hash.AppendBytes(
                module_roots[index].module_name.c_str(),
                module_roots[index].module_name.Length());
        }
        return hash;
    }

    const char *GetRenderTemplateName(const RenderTemplateID id) noexcept
    {
        const RenderTemplateDefinition *definition = FindRenderTemplate(id);
        return definition ? definition->name : "unknown";
    }

    const char *GetShaderModuleSlotRoleName(
        const ShaderModuleSlotRole role) noexcept
    {
        switch (role)
        {
        case ShaderModuleSlotRole::SurfaceProvider: return "surface_provider";
        case ShaderModuleSlotRole::DirectLightProvider: return "direct_light_provider";
        case ShaderModuleSlotRole::ShadowProvider: return "shadow_provider";
        case ShaderModuleSlotRole::AmbientLightProvider: return "ambient_light_provider";
        case ShaderModuleSlotRole::AmbientOcclusionProvider: return "ambient_occlusion_provider";
        case ShaderModuleSlotRole::LightingModel: return "lighting_model";
        case ShaderModuleSlotRole::OutputPolicy: return "output_policy";
        default: return "unknown";
        }
    }

    const char *GetRenderTemplateValidationErrorName(
        const RenderTemplateValidationError error) noexcept
    {
        switch (error)
        {
        case RenderTemplateValidationError::None: return "None";
        case RenderTemplateValidationError::UnknownTemplate: return "UnknownTemplate";
        case RenderTemplateValidationError::StageMismatch: return "StageMismatch";
        case RenderTemplateValidationError::VersionMismatch: return "VersionMismatch";
        case RenderTemplateValidationError::EmptyModuleRoot: return "EmptyModuleRoot";
        case RenderTemplateValidationError::UnknownSlotRole: return "UnknownSlotRole";
        case RenderTemplateValidationError::UnexpectedSlotRole: return "UnexpectedSlotRole";
        case RenderTemplateValidationError::DuplicateSlotRole: return "DuplicateSlotRole";
        case RenderTemplateValidationError::MissingRequiredSlot: return "MissingRequiredSlot";
        case RenderTemplateValidationError::ModuleNotFound: return "ModuleNotFound";
        case RenderTemplateValidationError::ModuleSlotMismatch: return "ModuleSlotMismatch";
        case RenderTemplateValidationError::MissingModuleCapability: return "MissingModuleCapability";
        }
        return "Unknown";
    }

    const RenderTemplateDefinition *FindRenderTemplate(
        const RenderTemplateID id) noexcept
    {
        for (const RenderTemplateDefinition &definition : Templates)
        {
            if (definition.id == id)
                return &definition;
        }
        return nullptr;
    }

    bool ValidateRenderTemplateRequest(
        const RenderTemplateRequest &request,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};
        const RenderTemplateDefinition *definition =
            FindRenderTemplate(request.template_id);
        if (!definition)
            return SetFailure(
                out_diagnostic,
                RenderTemplateValidationError::UnknownTemplate,
                request);

        if (request.stage != definition->stage)
            return SetFailure(
                out_diagnostic,
                RenderTemplateValidationError::StageMismatch,
                request);

        if (request.template_version != definition->version)
            return SetFailure(
                out_diagnostic,
                RenderTemplateValidationError::VersionMismatch,
                request);

        for (uint32 index = 0; index < request.module_root_count; ++index)
        {
            const RenderTemplateModuleRoot &root =
                request.module_roots[index];
            if (root.role == ShaderModuleSlotRole::Unknown)
                return SetFailure(
                    out_diagnostic,
                    RenderTemplateValidationError::UnknownSlotRole,
                    request,
                    root.role,
                    root.module_name);
            if (root.module_name.IsEmpty())
                return SetFailure(
                    out_diagnostic,
                    RenderTemplateValidationError::EmptyModuleRoot,
                    request,
                    root.role);
            if (!HasSlot(*definition, root.role))
                return SetFailure(
                    out_diagnostic,
                    RenderTemplateValidationError::UnexpectedSlotRole,
                    request,
                    root.role,
                    root.module_name);
            for (uint32 previous = 0; previous < index; ++previous)
            {
                if (request.module_roots[previous].role == root.role)
                    return SetFailure(
                        out_diagnostic,
                        RenderTemplateValidationError::DuplicateSlotRole,
                        request,
                        root.role,
                        root.module_name);
            }
        }

        for (uint32 index = 0; index < definition->slot_count; ++index)
        {
            const RenderTemplateSlot &slot = definition->slots[index];
            if (slot.required && !request.FindModuleRoot(slot.role))
                return SetFailure(
                    out_diagnostic,
                    RenderTemplateValidationError::MissingRequiredSlot,
                    request,
                    slot.role);
        }

        return true;
    }

    bool ValidateRenderTemplateRequest(
        const RenderTemplateRequest &request,
        const ShaderCodeModuleRegistry &module_registry,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept
    {
        if (!ValidateRenderTemplateRequest(request, out_diagnostic))
            return false;

        uint32 provided_capabilities = 0;
        uint32 required_capabilities = 0;
        for (uint32 index = 0; index < request.module_root_count; ++index)
        {
            const RenderTemplateModuleRoot &root = request.module_roots[index];
            const ShaderCodeModuleDefinition *definition =
                module_registry.FindByName(root.module_name.c_str());
            if (!definition)
                return SetFailure(
                    out_diagnostic,
                    RenderTemplateValidationError::ModuleNotFound,
                    request,
                    root.role,
                    root.module_name);
            if (definition->slot_role != root.role)
                return SetFailure(
                    out_diagnostic,
                    RenderTemplateValidationError::ModuleSlotMismatch,
                    request,
                    root.role,
                    root.module_name);
            provided_capabilities |= definition->provided_capabilities;
            required_capabilities |= definition->required_capabilities;
        }
        if ((required_capabilities & ~provided_capabilities) != 0)
            return SetFailure(
                out_diagnostic,
                RenderTemplateValidationError::MissingModuleCapability,
                request,
                ShaderModuleSlotRole::Unknown,
                "");
        return true;
    }
}
