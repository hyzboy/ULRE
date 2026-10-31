#include <hgl/mtl/PassProfile.h>

// 渲染模板的「数据面」：静态模板表、名字表、请求容器、以及不依赖模块清单的
// 结构校验。发行形态（ULRE_ENABLE_SHADERGEN=0）同样需要这些能力——选模板、
// 装 module root、算 stable_hash 都要在运行时完成。
//
// 与 ULRE.ShaderProgramBuild 的分界线：任何需要读 ShaderLibrary 里的 GLSL 模块清单
// （ShaderCodeModuleRegistry / ShaderCodeResourceManifest）才能做的校验与解析，
// 都留在 src/ShaderProgramBuild/template/PassProfileResolve.cpp。

namespace hgl::graph::mtl
{
    namespace
    {
        // 数组顺序 = fragment #include 发射顺序（契约见 PassProfile.h）。
        // SurfaceProvider 恒排末位：material_surface.glsl 调用
        // EvalMaterialSource / EvalMaterialAlpha / GetNTB，必须后于
        // MaterialSourceProvider 与 NTBProvider 声明。

        constexpr PassProfileSlot ForwardLitSlots[] =
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

        // MaterialSourceProvider 在 ForwardLit 的固定 include 表里是必需槽：
        // 主着色器已物理 include material_surface.glsl 并调用其 EvalSurface，
        // 缺了 provider 就会在链接期才炸，所以表里保持 required=true。
        // 必需性判定的唯一执行点 = ValidateScenePassProfileRequest。
        //
        // AmbientOcclusionProvider 在所有 forward 光照模板里都是必需槽：
        // forward_lighting.glsl 无条件接线 `GetAmbientOcclusion`，“本模板不做 AO”
        // 由恒等实现 identity_ao 表达，而不是条件编译。
        constexpr PassProfileSlot ForwardLitUnshadowedSlots[] =
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
        constexpr PassProfileSlot ForwardUnlitSlots[] =
        {
            { ShaderModuleSlotRole::OutputPolicy },
            { ShaderModuleSlotRole::MaterialSourceProvider },
            { ShaderModuleSlotRole::NTBProvider },
            { ShaderModuleSlotRole::SurfaceProvider }
        };

        // OutputPolicy 仅参与校验、不发射：WriteMaterialOutput 由领域主着色器
        // 直接书写，不经任何拼装。
        // NTBProvider 必需的理由同上（surface 模块要解析 GetNTB）。
        constexpr PassProfileSlot ShadowCasterOpaqueSlots[] =
        {
            { ShaderModuleSlotRole::MaterialSourceProvider, false },
            { ShaderModuleSlotRole::NTBProvider },
            { ShaderModuleSlotRole::SurfaceProvider },
            { ShaderModuleSlotRole::OutputPolicy }
        };

        constexpr PassProfileSlot ShadowCasterMaskedSlots[] =
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
        constexpr PassProfileSlot SkySlots[] =
        {
            { ShaderModuleSlotRole::SkyProvider },
            { ShaderModuleSlotRole::SurfaceProvider }
        };

        constexpr PassProfileDefinition Templates[] =
        {
            { PassProfileID::ForwardLitShadowedAO,
              "forward_lit_shadowed_ao", ShaderStage::Fragment, 1,
              ForwardLitSlots, uint32(sizeof(ForwardLitSlots) / sizeof(ForwardLitSlots[0])) },
            { PassProfileID::ForwardLitShadowedIdentityAO,
              "forward_lit_shadowed_identity_ao", ShaderStage::Fragment, 1,
              ForwardLitSlots, uint32(sizeof(ForwardLitSlots) / sizeof(ForwardLitSlots[0])) },
            { PassProfileID::ForwardLitUnshadowedAO,
              "forward_lit_unshadowed_ao", ShaderStage::Fragment, 1,
              ForwardLitUnshadowedSlots, uint32(sizeof(ForwardLitUnshadowedSlots) / sizeof(ForwardLitUnshadowedSlots[0])) },
            { PassProfileID::ForwardUnlit,
              "forward_unlit", ShaderStage::Fragment, 1,
              ForwardUnlitSlots, uint32(sizeof(ForwardUnlitSlots) / sizeof(ForwardUnlitSlots[0])) },
            { PassProfileID::ShadowCasterOpaque,
              "shadow_caster_opaque", ShaderStage::Fragment, 1,
              ShadowCasterOpaqueSlots, uint32(sizeof(ShadowCasterOpaqueSlots) / sizeof(ShadowCasterOpaqueSlots[0])) },
            { PassProfileID::ShadowCasterMasked,
              "shadow_caster_masked", ShaderStage::Fragment, 1,
              ShadowCasterMaskedSlots, uint32(sizeof(ShadowCasterMaskedSlots) / sizeof(ShadowCasterMaskedSlots[0])) },
            { PassProfileID::Sky,
              "sky", ShaderStage::Fragment, 1,
              SkySlots, uint32(sizeof(SkySlots) / sizeof(SkySlots[0])) }
        };

        bool HasSlot(
            const PassProfileDefinition &definition,
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
            PassProfileValidationDiagnostic &diagnostic,
            const PassProfileValidationError error,
            const PassProfileRequest &request,
            const ShaderModuleSlotRole role = ShaderModuleSlotRole::Unknown,
            const AnsiString &module_name = {}) noexcept
        {
            diagnostic.error = error;
            diagnostic.profile_id = request.profile_id;
            diagnostic.role = role;
            diagnostic.module_name = module_name;
            return false;
        }
    }

    bool PassProfileRequest::AddModuleRoot(
        const ShaderModuleSlotRole role,
        const AnsiString &module_name) noexcept
    {
        if (role == ShaderModuleSlotRole::Unknown
         || module_name.IsEmpty()
         || module_root_count >= MaxPassProfileModuleRoots
         || FindModuleRoot(role))
            return false;

        module_roots[module_root_count].role = role;
        module_roots[module_root_count].module_name = module_name;
        ++module_root_count;
        return true;
    }

    const PassProfileModuleRoot *PassProfileRequest::FindModuleRoot(
        const ShaderModuleSlotRole role) const noexcept
    {
        for (uint32 index = 0; index < module_root_count; ++index)
        {
            if (module_roots[index].role == role)
                return module_roots + index;
        }
        return nullptr;
    }

    uint64 PassProfileRequest::GetHash() const noexcept
    {
        hgl::hash::FNV1aHasher64 hash;
        hash << profile_id << stage << profile_version << module_root_count;
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

    const char *GetPassProfileName(const PassProfileID id) noexcept
    {
        const PassProfileDefinition *definition = FindPassProfile(id);
        return definition ? definition->name : "unknown";
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

    const char *GetPassProfileValidationErrorName(
        const PassProfileValidationError error) noexcept
    {
        switch (error)
        {
        case PassProfileValidationError::None: return "None";
        case PassProfileValidationError::UnknownTemplate: return "UnknownTemplate";
        case PassProfileValidationError::StageMismatch: return "StageMismatch";
        case PassProfileValidationError::VersionMismatch: return "VersionMismatch";
        case PassProfileValidationError::EmptyModuleRoot: return "EmptyModuleRoot";
        case PassProfileValidationError::UnknownSlotRole: return "UnknownSlotRole";
        case PassProfileValidationError::UnexpectedSlotRole: return "UnexpectedSlotRole";
        case PassProfileValidationError::DuplicateSlotRole: return "DuplicateSlotRole";
        case PassProfileValidationError::MissingRequiredSlot: return "MissingRequiredSlot";
        case PassProfileValidationError::ModuleNotFound: return "ModuleNotFound";
        case PassProfileValidationError::ModuleSlotMismatch: return "ModuleSlotMismatch";
        case PassProfileValidationError::MissingModuleCapability: return "MissingModuleCapability";
        case PassProfileValidationError::ModuleGraphInvalid: return "ModuleGraphInvalid";
        case PassProfileValidationError::ModuleSourceMissing: return "ModuleSourceMissing";
        case PassProfileValidationError::IncludePathNotFound: return "IncludePathNotFound";
        case PassProfileValidationError::IncludePathNameMismatch: return "IncludePathNameMismatch";
        }
        return "Unknown";
    }

    const PassProfileDefinition *FindPassProfile(
        const PassProfileID id) noexcept
    {
        for (const PassProfileDefinition &definition : Templates)
        {
            if (definition.id == id)
                return &definition;
        }
        return nullptr;
    }

    // 只做「请求 vs 模板表」的结构校验：不碰 GLSL 模块清单，发行形态可用。
    bool ValidatePassProfileRequest(
        const PassProfileRequest &request,
        PassProfileValidationDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};
        const PassProfileDefinition *definition =
            FindPassProfile(request.profile_id);
        if (!definition)
            return SetFailure(
                out_diagnostic,
                PassProfileValidationError::UnknownTemplate,
                request);

        if (request.stage != definition->stage)
            return SetFailure(
                out_diagnostic,
                PassProfileValidationError::StageMismatch,
                request);

        if (request.profile_version != definition->version)
            return SetFailure(
                out_diagnostic,
                PassProfileValidationError::VersionMismatch,
                request);

        for (uint32 index = 0; index < request.module_root_count; ++index)
        {
            const PassProfileModuleRoot &root =
                request.module_roots[index];
            if (root.role == ShaderModuleSlotRole::Unknown)
                return SetFailure(
                    out_diagnostic,
                    PassProfileValidationError::UnknownSlotRole,
                    request,
                    root.role,
                    root.module_name);
            if (root.module_name.IsEmpty())
                return SetFailure(
                    out_diagnostic,
                    PassProfileValidationError::EmptyModuleRoot,
                    request,
                    root.role);
            if (!HasSlot(*definition, root.role))
                return SetFailure(
                    out_diagnostic,
                    PassProfileValidationError::UnexpectedSlotRole,
                    request,
                    root.role,
                    root.module_name);
            for (uint32 previous = 0; previous < index; ++previous)
            {
                if (request.module_roots[previous].role == root.role)
                    return SetFailure(
                        out_diagnostic,
                        PassProfileValidationError::DuplicateSlotRole,
                        request,
                        root.role,
                        root.module_name);
            }
        }

        for (uint32 index = 0; index < definition->slot_count; ++index)
        {
            const PassProfileSlot &slot = definition->slots[index];
            if (slot.required && !request.FindModuleRoot(slot.role))
                return SetFailure(
                    out_diagnostic,
                    PassProfileValidationError::MissingRequiredSlot,
                    request,
                    slot.role);
        }

        return true;
    }
}
