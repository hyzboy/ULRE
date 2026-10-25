#include <hgl/mtl/RenderTemplate.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/mtl/ShaderLibraryPath.h>

// 渲染模板的「生成期面」：只有这里才需要读 ShaderLibrary 里的 GLSL 模块清单，
// 用来核对 module root 的 include 路径、构建整张模块图、闭合能力声明。
//
// 发行形态（ULRE_ENABLE_SHADERGEN=0）里 ShaderLibrary 根本不存在，所以这一侧
// 跟着 ULRE.ShaderGen 一起退出构建；运行时的结构校验在
// src/ShaderMaterialData/RenderTemplate.cpp（同名重载，不吃 registry）。

namespace hgl::graph::mtl
{
    namespace
    {
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
    }

    bool ValidateRenderTemplateRequest(
        const RenderTemplateRequest &request,
        const ShaderCodeModuleRegistry &module_registry,
        RenderTemplateValidationDiagnostic &out_diagnostic,
        ShaderCodeResourceManifest *out_manifest) noexcept
    {
        // 先做结构校验（数据面），再核 GLSL 模块清单。
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
                RenderTemplateValidationError::ModuleGraphInvalid,
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
