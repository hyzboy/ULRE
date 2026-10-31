#include <hgl/mtl/ScenePassProfileResolver.h>

namespace hgl::graph::mtl
{
    ScenePassProfile MakeIdentityForwardLitProfile() noexcept
    {
        ScenePassProfile profile;
        profile.AddModule(
            ShaderModuleSlotRole::SkyProvider,
            "sky_atmosphere", "sky/sky_atmosphere.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider,
            "material_surface", "surface/material_surface.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::DiffuseLightProvider,
            "diffuse_lambert", "lighting/diffuse_lambert.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::SpecularLightProvider,
            "specular_cook_torrance",
            "lighting/specular_cook_torrance.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::ShadowProvider,
            "pcf_shadow", "shadow/pcf_shadow.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::AmbientLightProvider,
            "indirect_sky_ambient",
            "lighting/indirect_sky_ambient.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::AmbientOcclusionProvider,
            "identity_ao", "ao/identity.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::LightingModel,
            "forward_pbr", "lighting/forward_pbr.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::OutputPolicy,
            "forward_lighting", "compositor/forward_lighting.glsl");
        return profile;
    }

    ScenePassProfile MakeForwardUnlitProfile() noexcept
    {
        ScenePassProfile profile;
        profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider,
            "material_surface", "surface/material_surface.glsl");
        // material_surface 无条件调用 GetNTB，故 NTB 是必需槽；unlit 不需要法线
        // 扰动，用空对象补位（返回以世界法线为 N 的正交基）。
        profile.AddModule(
            ShaderModuleSlotRole::NTBProvider,
            "ntb_none", "ntb/ntb_none.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::OutputPolicy,
            "flat_lighting", "compositor/flat_lighting.glsl");
        return profile;
    }

    ScenePassProfile MakeSkyProfile() noexcept
    {
        ScenePassProfile profile;
        profile.AddModule(
            ShaderModuleSlotRole::SkyProvider,
            "sky_atmosphere", "sky/sky_atmosphere.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider,
            "sky_minimal_surface", "surface/sky_minimal_surface.glsl");
        // 刻意不提供 OutputPolicy：Sky 骨架的 WriteMaterialOutput 由领域主
        // 着色器直接书写，
        // forward_lighting 一个字节都不会出现在生成的 GLSL 里。给它一个 root
        // 只会让声明闭包把永不发射的模块（连同其 lighting_interface 依赖）
        // 算进去——纯粹的过近似，缓存键跟着无关文件失效。
        return profile;
    }

    ScenePassProfile MakeShadowCasterProfile() noexcept
    {
        ScenePassProfile profile;
        profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider,
            "material_surface", "surface/material_surface.glsl");
        // 阴影 pass 只用 EvalAlpha，从不读法线；但 material_surface.glsl 的定义里
        // 无条件调用 GetNTB（GLSL 定义处即解析符号），所以仍需一个实现补位。
        profile.AddModule(
            ShaderModuleSlotRole::NTBProvider,
            "ntb_none", "ntb/ntb_none.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::OutputPolicy,
            "forward_lighting",
            "compositor/forward_lighting.glsl");
        return profile;
    }

    bool ScenePassProfile::AddModule(
        const ShaderModuleSlotRole role,
        const AnsiString &module_name,
        const AnsiString &include_path) noexcept
    {
        if (role == ShaderModuleSlotRole::Unknown
         || module_name.IsEmpty()
         || include_path.IsEmpty()
         || module_count >= MaxPassProfileModuleRoots)
            return false;
        for (uint32 index = 0; index < module_count; ++index)
        {
            if (roles[index] == role)
                return false;
        }
        roles[module_count] = role;
        module_names[module_count] = module_name;
        include_paths[module_count] = include_path;
        ++module_count;
        return true;
    }

    bool ResolveScenePassProfileRequest(
        const PassProfileID profile_id,
        const ShaderStage stage,
        const ScenePassProfile &profile,
        PassProfileRequest &out_request,
        PassProfileValidationDiagnostic &out_diagnostic) noexcept
    {
        out_request = {};
        out_request.profile_id = profile_id;
        out_request.stage = stage;
        out_diagnostic = {};
        out_diagnostic.profile_id = profile_id;
        const PassProfileDefinition *definition =
            FindPassProfile(profile_id);
        if (!definition)
        {
            out_diagnostic.error =
                PassProfileValidationError::UnknownTemplate;
            return false;
        }
        if (stage != definition->stage)
        {
            out_diagnostic.error =
                PassProfileValidationError::StageMismatch;
            return false;
        }
        out_request.profile_version = definition->version;
        for (uint32 index = 0; index < profile.module_count; ++index)
        {
            if (!out_request.AddModuleRoot(
                    profile.roles[index], profile.module_names[index]))
            {
                out_diagnostic = {};
                out_diagnostic.error =
                    PassProfileValidationError::DuplicateSlotRole;
                out_diagnostic.profile_id = out_request.profile_id;
                out_diagnostic.role = profile.roles[index];
                out_diagnostic.module_name = profile.module_names[index];
                return false;
            }
            out_request.module_roots[index].include_path =
                profile.include_paths[index];
        }
        // Material capability roots are appended by the caller after selecting
        // this scene profile. Full template validation runs once that request
        // is complete in the material build path.
        return true;
    }

}
