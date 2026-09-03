#include <hgl/mtl/SceneRenderTemplateResolver.h>

namespace hgl::graph::mtl
{
    bool SceneRenderTemplateProfile::AddModule(
        const ShaderModuleSlotRole role,
        const AnsiString &module_name,
        const AnsiString &include_path) noexcept
    {
        if (role == ShaderModuleSlotRole::Unknown
         || module_name.IsEmpty()
         || include_path.IsEmpty()
         || module_count >= MaxRenderTemplateModuleRoots)
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

    bool ResolveSceneRenderTemplateRequest(
        const FixedPipelineVariant &variant,
        const ShaderStage stage,
        const SceneRenderTemplateProfile &profile,
        RenderTemplateRequest &out_request,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept
    {
        out_request = {};
        out_request.template_id = variant.fragment_template;
        out_request.stage = stage;
        out_request.template_version = variant.template_version;
        for (uint32 index = 0; index < profile.module_count; ++index)
        {
            if (!out_request.AddModuleRoot(
                    profile.roles[index], profile.module_names[index]))
            {
                out_diagnostic = {};
                out_diagnostic.error =
                    RenderTemplateValidationError::DuplicateSlotRole;
                out_diagnostic.template_id = out_request.template_id;
                out_diagnostic.role = profile.roles[index];
                out_diagnostic.module_name = profile.module_names[index];
                return false;
            }
            out_request.module_roots[index].include_path =
                profile.include_paths[index];
        }
        return ValidateRenderTemplateRequest(out_request, out_diagnostic);
    }
}
