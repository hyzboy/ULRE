#include <hgl/mtl/RenderTemplate.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/mtl/ShaderLibraryPath.h>

namespace hgl::graph::mtl
{
    namespace
    {
        // 数组顺序 = fragment #include 发射顺序（契约见 RenderTemplate.h）。
        // SurfaceProvider 恒排末位：material_surface.glsl 调用
        // EvalMaterialSource / EvalMaterialAlpha / GetNTB，必须后于
        // MaterialSourceProvider 与 NTBProvider 声明。

        constexpr RenderTemplateSlot ForwardLitSlots[] =
        {
            { ShaderModuleSlotRole::SkyProvider },
            { ShaderModuleSlotRole::DirectLightProvider },
            { ShaderModuleSlotRole::AmbientLightProvider },
            { ShaderModuleSlotRole::ShadowProvider },
            { ShaderModuleSlotRole::AmbientOcclusionProvider },
            { ShaderModuleSlotRole::LightingModel },
            { ShaderModuleSlotRole::MaterialSourceProvider },
            { ShaderModuleSlotRole::NTBProvider },
            { ShaderModuleSlotRole::OutputPolicy },
            { ShaderModuleSlotRole::SurfaceProvider }
        };

        // ForwardLit 的 defines 恒为 "HGL_USE_MATERIAL_SOURCE_PROVIDER 1" /
        // "HGL_USE_NTB_PROVIDER 1"（FragmentTemplateComposer::BuildFragmentDefines 按模板
        // 分支发射），故表中这两个槽必须保持 required=true：置 false 会让定义里承诺
        // 恒为 1 的宏指向缺失模块。必需性判定的唯一执行点 = BuildAllSlotIncludeText。
        constexpr RenderTemplateSlot ForwardLitUnshadowedSlots[] =
        {
            { ShaderModuleSlotRole::SkyProvider },
            { ShaderModuleSlotRole::DirectLightProvider },
            { ShaderModuleSlotRole::AmbientLightProvider },
            { ShaderModuleSlotRole::LightingModel },
            { ShaderModuleSlotRole::MaterialSourceProvider },
            { ShaderModuleSlotRole::NTBProvider },
            { ShaderModuleSlotRole::OutputPolicy },
            { ShaderModuleSlotRole::SurfaceProvider }
        };

        constexpr RenderTemplateSlot ForwardUnlitSlots[] =
        {
            { ShaderModuleSlotRole::OutputPolicy },
            { ShaderModuleSlotRole::MaterialSourceProvider },
            { ShaderModuleSlotRole::SurfaceProvider }
        };

        // OutputPolicy 仅参与校验、不发射：WriteMaterialOutput 由
        // FragmentTemplateComposer::AppendOutputDeclarations 内联生成。
        constexpr RenderTemplateSlot ShadowCasterOpaqueSlots[] =
        {
            { ShaderModuleSlotRole::MaterialSourceProvider, false },
            { ShaderModuleSlotRole::SurfaceProvider },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr RenderTemplateSlot ShadowCasterMaskedSlots[] =
        {
            { ShaderModuleSlotRole::MaterialSourceProvider },
            { ShaderModuleSlotRole::SurfaceProvider },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr RenderTemplateSlot SkySlots[] =
        {
            { ShaderModuleSlotRole::SkyProvider },
            { ShaderModuleSlotRole::SurfaceProvider },
            // 同 ShadowCaster：只校验、不发射。
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
              ShadowCasterOpaqueSlots, uint32(sizeof(ShadowCasterOpaqueSlots) / sizeof(ShadowCasterOpaqueSlots[0])) },
            { RenderTemplateID::ShadowCasterMasked,
              "shadow_caster_masked", ShaderStage::Fragment, 1,
              ShadowCasterMaskedSlots, uint32(sizeof(ShadowCasterMaskedSlots) / sizeof(ShadowCasterMaskedSlots[0])) },
            { RenderTemplateID::Sky,
              "sky", ShaderStage::Fragment, 1,
              SkySlots, uint32(sizeof(SkySlots) / sizeof(SkySlots[0])) }
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
            RenderTemplateValidationDiagnostic &,
            RenderTemplateValidationError,
            const RenderTemplateRequest &,
            ShaderModuleSlotRole,
            const AnsiString &) noexcept;

        bool ResolveModuleRoot(
            const RenderTemplateRequest &request,
            const RenderTemplateModuleRoot &root,
            const ShaderCodeModuleRegistry &registry,
            RenderTemplateModuleRoot &out_root,
            RenderTemplateValidationDiagnostic &diagnostic)
        {
            const ShaderCodeModuleDefinition *module =
                registry.FindByName(root.module_name.c_str());
            const ShaderCodeModuleFileData *source =
                registry.FindFileByName(root.module_name.c_str());
            const auto fail = [&](const RenderTemplateValidationError error)
            {
                SetFailure(diagnostic, error, request, root.role, root.module_name);
                diagnostic.include_path = root.include_path;
                diagnostic.expected_include_path = source
                    ? source->include_path : AnsiString();
                return false;
            };
            if (!module)
                return fail(RenderTemplateValidationError::ModuleNotFound);
            if (module->slot_role != root.role)
                return fail(RenderTemplateValidationError::ModuleSlotMismatch);
            if (!source)
                return fail(RenderTemplateValidationError::ModuleSourceMissing);

            const OSString expected = ResolveShaderLibraryModulePath(
                source->source_root, source->include_path);
            const OSString actual = ResolveShaderLibraryModulePath(
                source->source_root,
                root.include_path.IsEmpty() ? source->include_path : root.include_path);
            if (expected.IsEmpty() || actual.IsEmpty())
                return fail(RenderTemplateValidationError::IncludePathNotFound);
#if HGL_OS == HGL_OS_Windows
            const bool same_file = actual.CaseComp(source->source_path) == 0
                && expected.CaseComp(source->source_path) == 0;
#else
            const bool same_file = actual == source->source_path
                && expected == source->source_path;
#endif
            if (!same_file)
                return fail(RenderTemplateValidationError::IncludePathNameMismatch);

            out_root = root;
            out_root.include_path = source->include_path;
            return true;
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
            hash.AppendBytes(
                module_roots[index].include_path.c_str(),
                module_roots[index].include_path.Length());
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
        case ShaderModuleSlotRole::MaterialSourceProvider: return "material_source_provider";
        case ShaderModuleSlotRole::NTBProvider: return "ntb_provider";
        case ShaderModuleSlotRole::SkyProvider: return "sky_provider";
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
        case RenderTemplateValidationError::ModuleGraphInvalid: return "ModuleGraphInvalid";
        case RenderTemplateValidationError::ModuleConflict: return "ModuleConflict";
        case RenderTemplateValidationError::ModuleSourceMissing: return "ModuleSourceMissing";
        case RenderTemplateValidationError::IncludePathNotFound: return "IncludePathNotFound";
        case RenderTemplateValidationError::IncludePathNameMismatch: return "IncludePathNameMismatch";
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
        RenderTemplateValidationDiagnostic &out_diagnostic,
        ShaderCodeResourceManifest *out_manifest) noexcept
    {
        if (!ValidateRenderTemplateRequest(request, out_diagnostic))
            return false;

        const char *root_names[MaxRenderTemplateModuleRoots]{};
        for (uint32 index = 0; index < request.module_root_count; ++index)
        {
            const RenderTemplateModuleRoot &root = request.module_roots[index];
            RenderTemplateModuleRoot resolved_root;
            if (!ResolveModuleRoot(
                    request, root, module_registry, resolved_root, out_diagnostic))
                return false;
            root_names[index] =
                module_registry.FindByName(root.module_name.c_str())->name;
        }

        // 校验过程必须构建整张模块图；调用方可复用该结果（见 out_manifest）。
        ShaderCodeResourceManifest local_manifest;
        ShaderCodeResourceManifest *manifest =
            out_manifest ? out_manifest : &local_manifest;

        if (!BuildShaderCodeResourceManifest(
                root_names,
                request.module_root_count,
                *manifest,
                &module_registry))
            return SetFailure(
                out_diagnostic,
                manifest->error == ShaderCodeResourceManifestError::ResourceConflict
                    ? RenderTemplateValidationError::ModuleConflict
                    : RenderTemplateValidationError::ModuleGraphInvalid,
                request);

        // 能力闭合必须基于整张依赖图（含传递依赖），不能只看 roots——
        // 仅统计 roots 会漏掉依赖模块声明的 required_capabilities。
        uint32 provided_capabilities = 0;
        uint32 required_capabilities = 0;
        for (uint32 index = 0; index < manifest->code_module_count; ++index)
        {
            const ShaderCodeModuleDefinition *definition =
                module_registry.FindByName(manifest->code_module_names[index]);
            if (!definition)
                return SetFailure(
                    out_diagnostic,
                    RenderTemplateValidationError::ModuleGraphInvalid,
                    request);
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

    bool ResolveRenderTemplate(
        const RenderTemplateRequest &request,
        const ShaderCodeModuleRegistry &module_registry,
        ResolvedRenderTemplate &out_template,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept
    {
        out_template = {};
        // 校验内部已构建整张模块图，此处直接复用——不必再构建一次。
        if (!ValidateRenderTemplateRequest(
                request, module_registry, out_diagnostic,
                &out_template.manifest))
            return false;

        const RenderTemplateDefinition *definition =
            FindRenderTemplate(request.template_id);
        out_template.definition = definition;
        out_template.request = request;
        out_template.module_root_count = request.module_root_count;
        for (uint32 index = 0; index < request.module_root_count; ++index)
        {
            if (!ResolveModuleRoot(
                    request, request.module_roots[index], module_registry,
                    out_template.request.module_roots[index], out_diagnostic))
                return false;
            const ShaderCodeModuleDefinition *module =
                module_registry.FindByName(
                    request.module_roots[index].module_name.c_str());
            if (!module)
                return SetFailure(
                    out_diagnostic,
                    RenderTemplateValidationError::ModuleNotFound,
                    request,
                    request.module_roots[index].role,
                    request.module_roots[index].module_name);
            out_template.module_roots[index] = module;
        }
        hgl::hash::FNV1aHasher64 hash;
        hash << out_template.request.GetHash() << out_template.manifest.stable_hash;
        out_template.stable_hash = hash;
        return out_template.IsValid();
    }
}
