#include <hgl/mtl/RenderTemplate.h>

// 渲染模板的「数据面」：静态模板表、名字表、请求容器、以及不依赖模块清单的
// 结构校验。发行形态（ULRE_ENABLE_SHADERGEN=0）同样需要这些能力——选模板、
// 装 module root、算 stable_hash 都要在运行时完成。
//
// 与 ULRE.ShaderGen 的分界线：任何需要读 ShaderLibrary 里的 GLSL 模块清单
// （ShaderCodeModuleRegistry / ShaderCodeResourceManifest）才能做的校验与解析，
// 都留在 src/ShaderGen/template/RenderTemplateResolve.cpp。

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
            { ShaderModuleSlotRole::DiffuseLightProvider },
            { ShaderModuleSlotRole::SpecularLightProvider },
            { ShaderModuleSlotRole::AmbientLightProvider },
            { ShaderModuleSlotRole::ShadowProvider },
            { ShaderModuleSlotRole::AmbientOcclusionProvider },
            { ShaderModuleSlotRole::LightingModel },
            { ShaderModuleSlotRole::MaterialSourceProvider },
            { ShaderModuleSlotRole::NTBProvider },
            { ShaderModuleSlotRole::OutputPolicy },
            { ShaderModuleSlotRole::SurfaceProvider }
        };

        // ForwardLit 的 defines 恒为 "HGL_USE_MATERIAL_SOURCE_PROVIDER 1"（
        // FragmentTemplateComposer::BuildFragmentDefines 按模板分支发射），故表中该槽
        // 必须保持 required=true：置 false 会让定义里承诺恒为 1 的宏指向缺失模块。
        // 必需性判定的唯一执行点 = BuildAllSlotIncludeText。
        //
        // AmbientOcclusionProvider 在所有 forward 光照模板里都是必需槽：
        // forward_lighting.glsl 无条件接线 `GetAmbientOcclusion`，“本模板不做 AO”
        // 由恒等实现 identity_ao 表达，而不是条件编译。
        constexpr RenderTemplateSlot ForwardLitUnshadowedSlots[] =
        {
            { ShaderModuleSlotRole::SkyProvider },
            { ShaderModuleSlotRole::DiffuseLightProvider },
            { ShaderModuleSlotRole::SpecularLightProvider },
            { ShaderModuleSlotRole::AmbientLightProvider },
            { ShaderModuleSlotRole::AmbientOcclusionProvider },
            { ShaderModuleSlotRole::LightingModel },
            { ShaderModuleSlotRole::MaterialSourceProvider },
            { ShaderModuleSlotRole::NTBProvider },
            { ShaderModuleSlotRole::OutputPolicy },
            { ShaderModuleSlotRole::SurfaceProvider }
        };

        // NTBProvider 是必需槽：material_surface.glsl 无条件调用 GetNTB（GLSL 在
        // 函数定义处就要解析符号，与调用点是否可达无关）。“不需要法线扰动”由空对象
        // ntb/ntb_none.glsl 表达。
        constexpr RenderTemplateSlot ForwardUnlitSlots[] =
        {
            { ShaderModuleSlotRole::OutputPolicy },
            { ShaderModuleSlotRole::MaterialSourceProvider },
            { ShaderModuleSlotRole::NTBProvider },
            { ShaderModuleSlotRole::SurfaceProvider }
        };

        // OutputPolicy 仅参与校验、不发射：WriteMaterialOutput 由
        // FragmentTemplateComposer::AppendOutputDeclarations 内联生成。
        // NTBProvider 必需的理由同上（surface 模块要解析 GetNTB）。
        constexpr RenderTemplateSlot ShadowCasterOpaqueSlots[] =
        {
            { ShaderModuleSlotRole::MaterialSourceProvider, false },
            { ShaderModuleSlotRole::NTBProvider },
            { ShaderModuleSlotRole::SurfaceProvider },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr RenderTemplateSlot ShadowCasterMaskedSlots[] =
        {
            { ShaderModuleSlotRole::MaterialSourceProvider },
            { ShaderModuleSlotRole::NTBProvider },
            { ShaderModuleSlotRole::SurfaceProvider },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        // 刻意不含 OutputPolicy：Sky 的 WriteMaterialOutput 由
        // AppendOutputDeclarations 内联生成，不存在可 #include 的输出策略模块。
        // 保留该 slot 会强制每个 profile 提供一个永不发射的模块，声明闭包随之
        // 把 forward_lighting 与 lighting_interface 算进去（过近似）。
        constexpr RenderTemplateSlot SkySlots[] =
        {
            { ShaderModuleSlotRole::SkyProvider },
            { ShaderModuleSlotRole::SurfaceProvider }
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

    const char *GetRenderTemplateFragmentSourceFile(
        const RenderTemplateID id) noexcept
    {
        switch (id)
        {
        case RenderTemplateID::ForwardLitShadowedAO:
        case RenderTemplateID::ForwardLitShadowedIdentityAO:
        case RenderTemplateID::ForwardLitUnshadowedAO:
            return "forward_lit.glsl.tmpl";
        case RenderTemplateID::ForwardUnlit:
            return "forward_unlit.glsl.tmpl";
        case RenderTemplateID::Sky:
            return "sky.glsl.tmpl";
        default:
            return nullptr;
        }
    }

    const char *GetShaderModuleSlotRoleName(
        const ShaderModuleSlotRole role) noexcept
    {
        switch (role)
        {
        case ShaderModuleSlotRole::SurfaceProvider: return "surface_provider";
        case ShaderModuleSlotRole::DiffuseLightProvider: return "diffuse_light_provider";
        case ShaderModuleSlotRole::SpecularLightProvider: return "specular_light_provider";
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

    // 只做「请求 vs 模板表」的结构校验：不碰 GLSL 模块清单，发行形态可用。
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
}
