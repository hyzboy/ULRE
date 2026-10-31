#include <hgl/mtl/ScenePassProfileResolver.h>

namespace hgl::graph::mtl
{
    ScenePassProfile MakeIdentityForwardLitProfile() noexcept
    {
        ScenePassProfile profile;
        profile.AddModule(
            ShaderModuleSlotRole::SkyProvider,
            "sky_atmosphere", "module/provider/sky/sky_atmosphere.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider,
            "surface_material", "module/provider/surface/surface_material.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::DiffuseLightProvider,
            "diffuse_light_lambert", "module/provider/diffuse_light/diffuse_light_lambert.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::SpecularLightProvider,
            "specular_light_cook_torrance",
            "module/provider/specular_light/specular_light_cook_torrance.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::ShadowProvider,
            "shadow_pcf", "module/provider/shadow/shadow_pcf.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::AmbientLightProvider,
            "ambient_light_sky",
            "module/provider/ambient_light/ambient_light_sky.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::AmbientOcclusionProvider,
            "ambient_occlusion_identity", "module/provider/ambient_occlusion/ambient_occlusion_identity.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::LightingModel,
            "lighting_model_pbr", "module/provider/lighting_model/lighting_model_pbr.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::OutputPolicy,
            "output_policy_forward", "module/provider/output_policy/output_policy_forward.glsl");
        return profile;
    }

    ScenePassProfile MakeForwardUnlitProfile() noexcept
    {
        ScenePassProfile profile;
        profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider,
            "surface_material", "module/provider/surface/surface_material.glsl");
        // material_surface 无条件调用 GetNTB，故 NTB 是必需槽；unlit 不需要法线
        // 扰动，用空对象补位（返回以世界法线为 N 的正交基）。
        profile.AddModule(
            ShaderModuleSlotRole::NTBProvider,
            "ntb_none", "module/provider/ntb/ntb_none.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::OutputPolicy,
            "output_policy_flat", "module/provider/output_policy/output_policy_flat.glsl");
        return profile;
    }

    ScenePassProfile MakeSkyProfile() noexcept
    {
        ScenePassProfile profile;
        profile.AddModule(
            ShaderModuleSlotRole::SkyProvider,
            "sky_atmosphere", "module/provider/sky/sky_atmosphere.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider,
            "surface_sky_minimal", "module/provider/surface/surface_sky_minimal.glsl");
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
            "surface_material", "module/provider/surface/surface_material.glsl");
        // 阴影 pass 只用 EvalAlpha，从不读法线；但 material_surface.glsl 的定义里
        // 无条件调用 GetNTB（GLSL 定义处即解析符号），所以仍需一个实现补位。
        profile.AddModule(
            ShaderModuleSlotRole::NTBProvider,
            "ntb_none", "module/provider/ntb/ntb_none.glsl");
        profile.AddModule(
            ShaderModuleSlotRole::OutputPolicy,
            "output_policy_forward",
            "module/provider/output_policy/output_policy_forward.glsl");
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
