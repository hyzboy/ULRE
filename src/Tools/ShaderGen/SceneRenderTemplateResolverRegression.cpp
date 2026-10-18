#include <hgl/mtl/SceneRenderTemplateResolver.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/ShaderCompilerProfileAPI.h>
#include <hgl/ShaderCompilerAPI.h>
#include <hgl/mtl/MaterialDefinitionFile.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/time/Time.h>

using namespace hgl;
using namespace hgl::graph::mtl;

namespace
{
    bool Check(const bool success, const char *message)
    {
        if (!success)
            GLogError(u8"[ProviderIdentityRegression] %s", message);
        return success;
    }

    class FixtureDirectory
    {
        ManagedArray<OSString> files;

    public:
        OSString root;

        bool Init()
        {
            OSString current;
            if (!filesystem::GetCurrentPath(current))
                return false;
            root = (filesystem::Path(current) / ToOSString(
                (AnsiString("provider-identity-")
                    + AnsiString::numberOf(GetTimeUs())).c_str())).ToOSString();
            return filesystem::MakePath(root);
        }

        bool Write(const AnsiString &relative, const AnsiString &code)
        {
            const filesystem::Path path =
                filesystem::Path(root) / ToOSString(relative.c_str());
            if (!filesystem::MakePath(path.GetParent().ToOSString()))
                return false;
            *files.Create() = path.ToOSString();
            return filesystem::SaveMemoryToFile(
                path.ToOSString(), code.c_str(), code.Length()) == code.Length();
        }

        ~FixtureDirectory()
        {
            for (int i = 0; i < files.GetCount(); ++i)
            {
                const OSString &file = *files[i];
                if (filesystem::FileExist(file) && !filesystem::FileDelete(file))
                    GLogWarning(u8"Cannot remove provider fixture: %ls", file.c_str());
            }
            if (!root.IsEmpty())
            {
                const OSString common =
                    (filesystem::Path(root) / OSString(OS_TEXT("common"))).ToOSString();
                if (filesystem::IsDirectory(common) && !filesystem::DeletePath(common))
                    GLogWarning(u8"Cannot remove fixture directory: %ls", common.c_str());
                if (!filesystem::DeletePath(root))
                    GLogWarning(u8"Cannot remove fixture directory: %ls", root.c_str());
            }
        }
    };

    AnsiString ModuleText(const AnsiString &name, const ShaderModuleSlotRole role)
    {
        return AnsiString("// @ulre begin\n// @ulre name ") + name
            + "\n// @ulre kind Utility\n// @ulre slot "
            + GetShaderModuleSlotRoleName(role) + "\n// @ulre end\n";
    }

    bool RunProviderIdentityRegression()
    {
        FixtureDirectory fixtures;
        if (!Check(fixtures.Init(), "create fixture directory"))
            return false;
        RenderTemplateRequest request;
        request.template_id = RenderTemplateID::ForwardLitShadowedAO;
        request.template_version = 1;
        const auto *definition = FindRenderTemplate(request.template_id);
        for (uint32 i = 0; i < definition->slot_count; ++i)
        {
            const ShaderModuleSlotRole role = definition->slots[i].role;
            const AnsiString name =
                AnsiString("provider_") + GetShaderModuleSlotRoleName(role);
            if (!Check(fixtures.Write(
                    AnsiString("module_") + AnsiString::numberOf(i) + ".glsl",
                    ModuleText(name, role)), "write named provider")
             || !Check(request.AddModuleRoot(role, name), "add name-only root"))
                return false;
        }
        if (!Check(fixtures.Write(
                "common/shared.glsl",
                ModuleText("common_source", ShaderModuleSlotRole::MaterialSourceProvider)),
                "write common search fixture"))
            return false;

        ShaderCodeModuleRegistry registry;
        int count = 0, errors = 0;
        if (!Check(registry.LoadDirectory(
                filesystem::Path(fixtures.root).GetFilename(), &count, &errors)
                && errors == 0 && count == 11, "load file-backed providers"))
            return false;
        ResolvedRenderTemplate canonical;
        RenderTemplateValidationDiagnostic diagnostic;
        if (!Check(ResolveRenderTemplate(request, registry, canonical, diagnostic),
                "resolve name-only source/NTB/surface"))
            return false;
        for (uint32 i = 0; i < request.module_root_count; ++i)
        {
            const auto *source = registry.FindFileByName(
                request.module_roots[i].module_name.c_str());
            if (!Check(source && canonical.request.module_roots[i].include_path
                    == source->include_path, "canonical include from registry"))
                return false;
        }
        FixtureDirectory relocated;
        if (!Check(relocated.Init(), "create relocated library"))
            return false;
        for (uint32 i = 0; i < request.module_root_count; ++i)
        {
            const auto *source = registry.FindFileByName(
                request.module_roots[i].module_name.c_str());
            if (!Check(relocated.Write(source->include_path, source->glsl_code),
                    "copy relocated provider"))
                return false;
        }
        ShaderCodeModuleRegistry relocated_registry;
        ResolvedRenderTemplate relocated_template;
        if (!Check(relocated_registry.LoadDirectory(relocated.root, &count, &errors)
                && errors == 0
                && ResolveRenderTemplate(
                    request, relocated_registry, relocated_template, diagnostic)
                && relocated_template.stable_hash == canonical.stable_hash,
                "physical library root does not affect stable identity"))
            return false;

        RenderTemplateRequest alias = canonical.request;
        alias.module_roots[0].include_path = "common\\..\\module_0.glsl";
        ResolvedRenderTemplate resolved;
        if (!Check(ResolveRenderTemplate(alias, registry, resolved, diagnostic)
                && resolved.stable_hash == canonical.stable_hash,
                "dot/backslash aliases preserve canonical identity"))
            return false;
        alias.module_roots[0].include_path = "../module_0.glsl";
        if (!Check(!ResolveRenderTemplate(alias, registry, resolved, diagnostic)
                && diagnostic.error == RenderTemplateValidationError::IncludePathNotFound,
                "reject escape above the include root"))
            return false;
        alias.module_roots[0].include_path = "module_1.glsl";
        if (!Check(!ResolveRenderTemplate(alias, registry, resolved, diagnostic)
                && diagnostic.error == RenderTemplateValidationError::IncludePathNameMismatch
                && diagnostic.expected_include_path == "module_0.glsl",
                "reject name/path mismatch with source context"))
            return false;
        alias.module_roots[0].include_path = "missing.glsl";
        if (!Check(!ResolveRenderTemplate(alias, registry, resolved, diagnostic)
                && diagnostic.error == RenderTemplateValidationError::IncludePathNotFound,
                "reject missing include"))
            return false;
        alias = request;
        alias.module_roots[0].module_name = request.module_roots[1].module_name;
        if (!Check(!ResolveRenderTemplate(alias, registry, resolved, diagnostic)
                && diagnostic.error == RenderTemplateValidationError::ModuleSlotMismatch,
                "reject wrong slot"))
            return false;
        alias.module_roots[0].module_name = "unknown_provider";
        if (!Check(!ResolveRenderTemplate(alias, registry, resolved, diagnostic)
                && diagnostic.error == RenderTemplateValidationError::ModuleNotFound,
                "reject unknown provider"))
            return false;

        const ShaderCodeModuleFileData *first = registry.FindFileByName(
            request.module_roots[0].module_name.c_str());
        if (!Check(registry.LoadDirectory(fixtures.root, &count, &errors)
                && count == 0 && errors == 11
                && registry.FindFileByName(first->name.c_str()) == first,
                "duplicate scan preserves original provenance"))
            return false;
        const ShaderCodeModuleDefinition *original = &first->definition;
        ShaderCodeModuleDefinition metadata_only = *original;
        if (!Check(registry.RemoveByName(original->name)
                && !registry.FindFileByName(original->name)
                && registry.Register(metadata_only)
                && !registry.FindFileByName(original->name),
                "removed/re-registered names cannot reuse stale provenance"))
            return false;
        if (!Check(!ResolveRenderTemplate(request, registry, resolved, diagnostic)
                && diagnostic.error == RenderTemplateValidationError::ModuleSourceMissing,
                "metadata-only roots have no include source"))
            return false;
        if (!Check(registry.RemoveByName(original->name) && registry.Register(*original),
                "restore file-backed provider"))
            return false;

        alias = request;
        auto &source_root = alias.module_roots[6];
        if (!Check(source_root.role == ShaderModuleSlotRole::MaterialSourceProvider,
                "fixture source slot"))
            return false;
        source_root.module_name = "common_source";
        source_root.include_path = "shared.glsl";
        if (!Check(!ResolveRenderTemplate(alias, registry, resolved, diagnostic)
                && diagnostic.error == RenderTemplateValidationError::IncludePathNotFound,
                "common requires an explicit root-relative prefix"))
            return false;
        source_root.include_path = "common/shared.glsl";
        if (!Check(ResolveRenderTemplate(alias, registry, resolved, diagnostic)
                && resolved.request.module_roots[6].include_path == "common/shared.glsl",
                "explicit common path resolves to its registered source"))
            return false;
        if (!Check(fixtures.Write("shared.glsl",
                ModuleText("direct_source", ShaderModuleSlotRole::MaterialSourceProvider)),
                "write root search collision"))
            return false;
        if (!Check(ResolveRenderTemplate(alias, registry, resolved, diagnostic),
                "explicit common path is not shadowed by a root basename"))
            return false;
        source_root.include_path = "shared.glsl";
        if (!Check(!ResolveRenderTemplate(alias, registry, resolved, diagnostic)
                && diagnostic.error == RenderTemplateValidationError::IncludePathNameMismatch,
                "root basename cannot substitute for a different registered source"))
            return false;

        FixtureDirectory invalid;
        if (!Check(invalid.Init() && invalid.Write("broken.glsl",
                "// @ulre begin\n// @ulre name broken\n// @ulre kind Utility\n"
                "// @ulre slot material_source_provider\n"
                "// @ulre uses missing_dependency\n// @ulre end\n"),
                "write invalid dependency fixture"))
            return false;
        if (!Check(registry.LoadDirectory(invalid.root, &count, &errors)
                && count == 0 && errors > 0 && !registry.FindFileByName("broken"),
                "failed dependency removes source lookup"))
            return false;
        if (!Check(filesystem::FileDelete(first->source_path)
                && !ResolveRenderTemplate(request, registry, resolved, diagnostic)
                && diagnostic.error == RenderTemplateValidationError::IncludePathNotFound,
                "removed physical source is rejected"))
            return false;
        registry.Clear();
        return Check(!registry.FindFileByName("common_source")
            && registry.GetCount() == 0, "clear removes provenance");
    }

    bool RunCompilerRootRegression()
    {
        FixtureDirectory fixtures;
        if (!Check(fixtures.Init(), "create compiler root fixture"))
            return false;
        const AnsiString value = "vec4 FixtureValue() { return vec4(1.0); }\n";
        const OSString previous_root = GetShaderIncludeRoot();
        struct RestoreCompilerRoot
        {
            OSString root;
            ~RestoreCompilerRoot()
            {
                hgl::graph::CloseShaderCompiler();
                const U8String utf8 = ToU8String(root);
                if (!SetShaderIncludeRoot(reinterpret_cast<const char *>(utf8.c_str())))
                    GLogError(u8"Cannot restore shader include root");
            }
        } restore{previous_root};
        const auto compile = [](const AnsiString &include, const bool expected)
        {
            const AnsiString source = AnsiString("#version 460\n#include ") + include
                + "\nlayout(location=0) out vec4 color;\n"
                  "void main() { color = FixtureValue(); }\n";
            auto *spv = hgl::graph::CompileShader(
                uint32(hgl::graph::ShaderStage::Fragment), source.c_str());
            const bool success = spv != nullptr;
            if (spv)
                hgl::graph::FreeSPVData(spv);
            return Check(success == expected, include.c_str());
        };
        const AnsiString quoted_shared = "\"shared.glsl\"";
        if (!Check(fixtures.Write("shared.glsl", value)
                && fixtures.Write("common/shared.glsl", "#error COMMON_MUST_NOT_SHADOW_ROOT\n")
                && fixtures.Write("common/entry.glsl", "#include \"shared.glsl\"\n"),
                "write nested include collision"))
            return false;
        const U8String fixture_utf8 = ToU8String(fixtures.root);
        if (!Check(SetShaderIncludeRoot(
                    reinterpret_cast<const char *>(fixture_utf8.c_str()))
                && hgl::graph::InitShaderCompiler()
                && hgl::graph::InitShaderCompiler(),
                "initialize root-only compiler idempotently"))
            return false;
        if (!compile("\"common/entry.glsl\"", true)
         || !compile("<common/entry.glsl>", true)
         || !compile("\"common/../shared.glsl\"", true)
         || !compile("\"common\\\\..\\\\shared.glsl\"", true)
         || !Check(ResolveShaderLibraryModulePath(fixtures.root, "shared.glsl")
                == filesystem::NormalizeFilename(
                    (filesystem::Path(fixtures.root) / OSString(OS_TEXT("shared.glsl"))).ToOSString()),
                "validator and real compiler choose the root file"))
            return false;

        const U8String fixture_name =
            ToU8String(filesystem::Path(fixtures.root).GetFilename());
        const AnsiString cwd_name = AnsiString(
            reinterpret_cast<const char *>(fixture_name.c_str()), fixture_name.Length()) + ".glsl";
        const AnsiString quoted_cwd = AnsiString("\"") + cwd_name + "\"";
        if (!Check(fixtures.Write(cwd_name, value)
                && fixtures.Write(AnsiString("../") + cwd_name, "#error CWD_MUST_NOT_SHADOW_ROOT\n"),
                "write current-directory collision")
         || !compile(quoted_cwd, true)
         || !Check(filesystem::FileDelete(
                (filesystem::Path(fixtures.root) / ToOSString(cwd_name.c_str())).ToOSString())
                && fixtures.Write(AnsiString("../") + cwd_name, value),
                "leave only a current-directory include")
         || !compile(quoted_cwd, false))
            return false;

        const AnsiString absolute = AnsiString("\"")
            + AnsiString(reinterpret_cast<const char *>(fixture_utf8.c_str()), fixture_utf8.Length())
            + "/shared.glsl\"";
        if (!compile(absolute, false)
         || !compile("\"/shared.glsl\"", false)
         || !compile(AnsiString("\"../") + cwd_name + "\"", false)
         || !compile("\"common/../../shared.glsl\"", false))
            return false;
        if (!Check(filesystem::FileDelete(
                (filesystem::Path(fixtures.root) / OSString(OS_TEXT("shared.glsl"))).ToOSString())
                && fixtures.Write("common/shared.glsl", value),
                "leave only the common-directory include")
         || !Check(ResolveShaderLibraryModulePath(fixtures.root, "shared.glsl").IsEmpty(),
                "validator has no common fallback")
         || !compile(quoted_shared, false)
         || !compile("\"common/entry.glsl\"", false)
         || !compile("\"common/shared.glsl\"", true))
            return false;
        return true;
    }

    bool RunMaterialSchemaRegression()
    {
        const AnsiString header =
            "id = \"identity_fixture\"\nname = \"Identity\"\n"
            "provider_policy = \"GeometryOnly\"\n[vertex]\nrequirements = [\"Position\"]\n";
        const char *invalid_fragments[] = {
            "material_source_module = \"material/unlit_source.glsl\"\n",
            "ntb_module = \"ntb/normal.glsl\"\n",
            "surface_module_name = \"sky_cube_surface\"\nsurface_module_include = \"sky/cube.glsl\"\n",
            "ambient_module_name = \"indirect_ibl\"\n",
            "material_source_module_name = \"\"\n",
            "ntb_module_name = \"\"\n",
            "surface_module_name = \"\"\n",
            "material_source_module_name = 42\n",
            "ntb_module_name = true\n",
            "surface_module_name = []\n",
            "material_source_module_name = \"unlit_source\"\nmaterial_source_module_include = \"x.glsl\"\n"
        };
        for (const char *fragment : invalid_fragments)
        {
            MaterialDefinitionFileData parsed;
            const AnsiString text =
                AnsiString("schema = 4\n") + header + "[fragment]\n" + fragment;
            if (!Check(ParseMaterialDefinitionFile(text.c_str(), text.Length(), parsed)
                    != MaterialDefinitionFileParseResult::OK
                    && !parsed.parse_error.IsEmpty()
                    && !parsed.parse_error.key.empty(),
                    "invalid provider fields report the key"))
                return false;
        }
        const char *invalid_schemas[] = { "", "schema = 3\n", "schema = 2\n",
            "schema = \"4\"\n", "schema = true\n" };
        for (const char *schema : invalid_schemas)
        {
            MaterialDefinitionFileData parsed;
            const AnsiString text = AnsiString(schema) + header;
            if (!Check(ParseMaterialDefinitionFile(text.c_str(), text.Length(), parsed)
                    != MaterialDefinitionFileParseResult::OK, "reject non-schema-4 material"))
                return false;
        }
        MaterialDefinitionFileData parsed;
        const AnsiString valid = AnsiString("schema = 4\n") + header
            + "[fragment]\nmaterial_source_module_name = \"source_identity\"\n"
            "ntb_module_name = \"ntb_identity\"\nsurface_module_name = \"surface_identity\"\n";
        return Check(ParseMaterialDefinitionFile(valid.c_str(), valid.Length(), parsed)
                == MaterialDefinitionFileParseResult::OK
            && AnsiString(parsed.definition.material_source_module_name) == "source_identity"
            && AnsiString(parsed.definition.ntb_module_name) == "ntb_identity"
            && AnsiString(parsed.definition.surface_module_name) == "surface_identity",
            "schema 4 provider names");
    }

    bool RunSurfaceBindingRegression(const ShaderCodeModuleRegistry &registry)
    {
        const char *ids[] = { "SkyCube", "SkyMinimal" };
        const char *names[] = { "sky_cube_surface", "sky_minimal_surface" };
        for (int i = 0; i < 2; ++i)
        {
            MaterialDefinition definition;
            RenderTemplateRequest request;
            RenderTemplateValidationDiagnostic diagnostic;
            ResolvedRenderTemplate resolved;
            if (!Check(TryGetMaterialDefinitionByID(ids[i], definition)
                    && ResolveSceneRenderTemplateRequest(
                        RenderTemplateID::Sky, hgl::graph::ShaderStage::Fragment,
                        MakeSkyProfile(), request, diagnostic)
                    && AppendMaterialRenderTemplateRoots(definition, request)
                    && ResolveRenderTemplate(request, registry, resolved, diagnostic),
                    "shared Sky surface binding"))
                return false;
            const auto *root =
                resolved.request.FindModuleRoot(ShaderModuleSlotRole::SurfaceProvider);
            const auto *source = registry.FindFileByName(names[i]);
            if (!Check(root && source && root->module_name == names[i]
                    && root->include_path == source->include_path,
                    "SkyCube override and SkyMinimal default use canonical sources"))
                return false;
        }
        return true;
    }
}

int main(int argc, char **argv)
{
    if (!logger::InitLogger(OS_TEXT("SceneRenderTemplateResolverRegression")))
        return 20;
    if (argc == 2 && AnsiString(argv[1]) == "--compiler-root")
        return RunCompilerRootRegression() ? 0 : 22;
    SceneRenderTemplateProfile profile;
    if (!profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "material_surface",
            "surface/material_surface.glsl")
     || !profile.AddModule(
            ShaderModuleSlotRole::OutputPolicy, "flat_lighting",
            "compositor/flat_lighting.glsl")
     || !profile.AddModule(
            ShaderModuleSlotRole::MaterialSourceProvider, "unlit_source",
            "material/unlit_source.glsl"))
        return 1;

    RenderTemplateRequest request;
    RenderTemplateValidationDiagnostic diagnostic{};
    if (!ResolveSceneRenderTemplateRequest(
            RenderTemplateID::ForwardUnlit, hgl::graph::ShaderStage::Fragment,
            profile, request, diagnostic))
        return 2;
    if (request.template_id != RenderTemplateID::ForwardUnlit
     || request.module_root_count != 3)
        return 3;
    ShaderCodeModuleRegistry registry;
    int load_errors = 0;
    if (!registry.LoadDirectory(ToOSString(GetShaderLibraryPath()), nullptr, &load_errors)
     || load_errors != 0)
        return 5;
    ResolvedRenderTemplate resolved{};
    if (!ResolveRenderTemplate(request, registry, resolved, diagnostic)
     || !resolved.IsValid()
     || resolved.request.module_roots[0].include_path
            != "surface/material_surface.glsl")
        return 6;
    FragmentTemplateComposer composer;
    FragmentTemplateComposer::ComposeInput compose_input{};
    compose_input.resolved_template = &resolved;
    ShaderDocument document;
    ShaderDocumentDiagnostics document_diagnostics;
    if (!composer.Compose(
            compose_input, document, document_diagnostics)
     || document.GetBlockCount() == 0)
        return 7;
    const SceneRenderTemplateProfile sky_profile = MakeSkyProfile();
    if (sky_profile.module_count != 3
     || sky_profile.roles[0] != ShaderModuleSlotRole::SkyProvider)
        return 9;
    const SceneRenderTemplateProfile shadow_profile =
        MakeShadowCasterProfile();
    if (shadow_profile.module_count != 2
     || shadow_profile.roles[0] != ShaderModuleSlotRole::SurfaceProvider)
        return 10;
    RenderTemplateRequest shadow_request;
    if (!ResolveSceneRenderTemplateRequest(
            RenderTemplateID::ShadowCasterMasked,
            hgl::graph::ShaderStage::Fragment, shadow_profile,
            shadow_request, diagnostic)
     || shadow_request.template_id != RenderTemplateID::ShadowCasterMasked)
        return 11;
    RenderTemplateRequest opaque_request;
    if (!ResolveSceneRenderTemplateRequest(
            RenderTemplateID::ShadowCasterOpaque,
            hgl::graph::ShaderStage::Fragment,
            MakeShadowCasterProfile(), opaque_request, diagnostic)
     || opaque_request.template_id != RenderTemplateID::ShadowCasterOpaque)
        return 12;

    SceneRenderTemplateProfile duplicate;
    SceneRenderTemplateProfile missing_path;
    if (missing_path.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "material_surface"))
        return 8;
    if (!duplicate.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "surface/a",
            "surface/a.glsl")
     || duplicate.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "surface/b",
            "surface/b.glsl"))
        return 4;
    return RunProviderIdentityRegression()
        && RunMaterialSchemaRegression()
        && RunSurfaceBindingRegression(registry) ? 0 : 21;
}
