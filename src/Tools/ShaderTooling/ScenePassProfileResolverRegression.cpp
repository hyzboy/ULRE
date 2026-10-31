#include <hgl/mtl/ScenePassProfileResolver.h>
#include <hgl/mtl/ResolvedPassProfile.h>
#include <hgl/mtl/GlslModuleRegistry.h>
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
        PassProfileRequest request;
        request.profile_id = PassProfileID::ForwardLitShadowedAO;
        request.profile_version = 1;
        const auto *definition = FindPassProfile(request.profile_id);
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

        GlslModuleRegistry registry;
        int count = 0, errors = 0;
        if (!Check(registry.LoadDirectory(
                filesystem::Path(fixtures.root).GetFilename(), &count, &errors)
                && errors == 0 && count == 12, "load file-backed providers"))
            return false;
        ResolvedPassProfile canonical;
        PassProfileValidationDiagnostic diagnostic;
        if (!Check(ResolvePassProfile(request, registry, canonical, diagnostic),
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
        GlslModuleRegistry relocated_registry;
        ResolvedPassProfile relocated_template;
        if (!Check(relocated_registry.LoadDirectory(relocated.root, &count, &errors)
                && errors == 0
                && ResolvePassProfile(
                    request, relocated_registry, relocated_template, diagnostic)
                && relocated_template.stable_hash == canonical.stable_hash,
                "physical library root does not affect stable identity"))
            return false;

        PassProfileRequest alias = canonical.request;
        alias.module_roots[0].include_path = "common\\..\\module_0.glsl";
        ResolvedPassProfile resolved;
        if (!Check(ResolvePassProfile(alias, registry, resolved, diagnostic)
                && resolved.stable_hash == canonical.stable_hash,
                "dot/backslash aliases preserve canonical identity"))
            return false;
        alias.module_roots[0].include_path = "../module_0.glsl";
        if (!Check(!ResolvePassProfile(alias, registry, resolved, diagnostic)
                && diagnostic.error == PassProfileValidationError::IncludePathNotFound,
                "reject escape above the include root"))
            return false;
        alias.module_roots[0].include_path = "module_1.glsl";
        if (!Check(!ResolvePassProfile(alias, registry, resolved, diagnostic)
                && diagnostic.error == PassProfileValidationError::IncludePathNameMismatch
                && diagnostic.expected_include_path == "module_0.glsl",
                "reject name/path mismatch with source context"))
            return false;
        alias.module_roots[0].include_path = "missing.glsl";
        if (!Check(!ResolvePassProfile(alias, registry, resolved, diagnostic)
                && diagnostic.error == PassProfileValidationError::IncludePathNotFound,
                "reject missing include"))
            return false;
        alias = request;
        alias.module_roots[0].module_name = request.module_roots[1].module_name;
        if (!Check(!ResolvePassProfile(alias, registry, resolved, diagnostic)
                && diagnostic.error == PassProfileValidationError::ModuleSlotMismatch,
                "reject wrong slot"))
            return false;
        alias.module_roots[0].module_name = "unknown_provider";
        if (!Check(!ResolvePassProfile(alias, registry, resolved, diagnostic)
                && diagnostic.error == PassProfileValidationError::ModuleNotFound,
                "reject unknown provider"))
            return false;

        const GlslModuleFileData *first = registry.FindFileByName(
            request.module_roots[0].module_name.c_str());
        if (!Check(registry.LoadDirectory(fixtures.root, &count, &errors)
                && count == 0 && errors == 12
                && registry.FindFileByName(first->name.c_str()) == first,
                "duplicate scan preserves original provenance"))
            return false;
        const GlslModuleDefinition *original = &first->definition;
        GlslModuleDefinition metadata_only = *original;
        if (!Check(registry.RemoveByName(original->name)
                && !registry.FindFileByName(original->name)
                && registry.Register(metadata_only)
                && !registry.FindFileByName(original->name),
                "removed/re-registered names cannot reuse stale provenance"))
            return false;
        if (!Check(!ResolvePassProfile(request, registry, resolved, diagnostic)
                && diagnostic.error == PassProfileValidationError::ModuleSourceMissing,
                "metadata-only roots have no include source"))
            return false;
        if (!Check(registry.RemoveByName(original->name) && registry.Register(*original),
                "restore file-backed provider"))
            return false;

        alias = request;
        // 槽表会演进（直接光已拆成 diffuse/specular 两槽），所以按角色查索引，
        // 不把 MaterialSourceProvider 的位置写死。
        uint32 source_index = UINT32_MAX;
        for (uint32 i = 0; i < alias.module_root_count; ++i)
            if (alias.module_roots[i].role == ShaderModuleSlotRole::MaterialSourceProvider)
            {
                source_index = i;
                break;
            }
        if (!Check(source_index != UINT32_MAX, "fixture source slot"))
            return false;
        const uint32 source_slot_index = source_index;
        auto &source_root = alias.module_roots[source_index];
        source_root.module_name = "common_source";
        source_root.include_path = "shared.glsl";
        if (!Check(!ResolvePassProfile(alias, registry, resolved, diagnostic)
                && diagnostic.error == PassProfileValidationError::IncludePathNotFound,
                "common requires an explicit root-relative prefix"))
            return false;
        source_root.include_path = "common/shared.glsl";
        if (!Check(ResolvePassProfile(alias, registry, resolved, diagnostic)
                && resolved.request.module_roots[source_slot_index].include_path
                    == "common/shared.glsl",
                "explicit common path resolves to its registered source"))
            return false;
        if (!Check(fixtures.Write("shared.glsl",
                ModuleText("direct_source", ShaderModuleSlotRole::MaterialSourceProvider)),
                "write root search collision"))
            return false;
        if (!Check(ResolvePassProfile(alias, registry, resolved, diagnostic),
                "explicit common path is not shadowed by a root basename"))
            return false;
        source_root.include_path = "shared.glsl";
        if (!Check(!ResolvePassProfile(alias, registry, resolved, diagnostic)
                && diagnostic.error == PassProfileValidationError::IncludePathNameMismatch,
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
                && !ResolvePassProfile(request, registry, resolved, diagnostic)
                && diagnostic.error == PassProfileValidationError::IncludePathNotFound,
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
            "material_source_module = \"module/provider/material_source/material_source_unlit.glsl\"\n",
            "ntb_module = \"ntb/normal.glsl\"\n",
            "surface_module_name = \"sky_cube_surface\"\nsurface_module_include = \"sky/cube.glsl\"\n",
            "ambient_module_name = \"indirect_ibl\"\n",
            "material_source_module_name = \"\"\n",
            "ntb_module_name = \"\"\n",
            "surface_module_name = \"\"\n",
            "material_source_module_name = 42\n",
            "ntb_module_name = true\n",
            "surface_module_name = []\n",
            "material_source_module_name = \"material_source_unlit\"\nmaterial_source_module_include = \"x.glsl\"\n"
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

    bool RunSurfaceBindingRegression(const GlslModuleRegistry &registry)
    {
        const char *ids[] = { "sky_cube", "sky_minimal" };
        const char *names[] = { "surface_sky_cube", "surface_sky_minimal" };
        for (int i = 0; i < 2; ++i)
        {
            MaterialDefinition definition;
            PassProfileRequest request;
            PassProfileValidationDiagnostic diagnostic;
            ResolvedPassProfile resolved;
            if (!Check(TryGetMaterialDefinitionByID(ids[i], definition)
                    && ResolveScenePassProfileRequest(
                        PassProfileID::Sky, hgl::graph::ShaderStage::Fragment,
                        MakeSkyProfile(), request, diagnostic)
                    && AppendMaterialPassProfileRoots(definition, request)
                    && ResolvePassProfile(request, registry, resolved, diagnostic),
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
    if (!logger::InitLogger(OS_TEXT("ScenePassProfileResolverRegression")))
        return 20;
    if (argc == 2 && AnsiString(argv[1]) == "--compiler-root")
        return RunCompilerRootRegression() ? 0 : 22;
    ScenePassProfile profile;
    if (!profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "surface_material",
            "module/provider/surface/surface_material.glsl")
     || !profile.AddModule(
            ShaderModuleSlotRole::OutputPolicy, "output_policy_flat",
            "module/provider/output_policy/output_policy_flat.glsl")
     || !profile.AddModule(
            ShaderModuleSlotRole::MaterialSourceProvider, "material_source_unlit",
            "module/provider/material_source/material_source_unlit.glsl")
     // NTBProvider 是 ForwardUnlit 的必需槽（material_surface.glsl 无条件调用
     // GetNTB），不需要法线扰动的模板用空对象 ntb_none 补位。
     || !profile.AddModule(
            ShaderModuleSlotRole::NTBProvider, "ntb_none",
            "module/provider/ntb/ntb_none.glsl"))
        return 1;

    PassProfileRequest request;
    PassProfileValidationDiagnostic diagnostic{};
    if (!ResolveScenePassProfileRequest(
            PassProfileID::ForwardUnlit, hgl::graph::ShaderStage::Fragment,
            profile, request, diagnostic))
        return 2;
    if (request.profile_id != PassProfileID::ForwardUnlit
     || request.module_root_count != 4)
        return 3;
    GlslModuleRegistry registry;
    int load_errors = 0;
    if (!registry.LoadDirectory(ToOSString(GetShaderLibraryPath()), nullptr, &load_errors)
     || load_errors != 0)
        return 5;
    ResolvedPassProfile resolved{};
    if (!ResolvePassProfile(request, registry, resolved, diagnostic)
     || !resolved.IsValid()
     || resolved.request.module_roots[0].include_path
            != "module/provider/surface/surface_material.glsl")
        return 6;
    // 片段模板组装（FragmentTemplateComposer）已随「无生成器」终态删除：
    // 片段阶段的编排不再由本工具验证，改由领域固定主着色器文件本体承担。
    const ScenePassProfile sky_profile = MakeSkyProfile();
    if (sky_profile.module_count != 2
     || sky_profile.roles[0] != ShaderModuleSlotRole::SkyProvider)
        return 9;
    const ScenePassProfile shadow_profile =
        MakeShadowCasterProfile();
    if (shadow_profile.module_count != 3
     || shadow_profile.roles[0] != ShaderModuleSlotRole::SurfaceProvider)
        return 10;
    PassProfileRequest shadow_request;
    if (!ResolveScenePassProfileRequest(
            PassProfileID::ShadowCasterMasked,
            hgl::graph::ShaderStage::Fragment, shadow_profile,
            shadow_request, diagnostic)
     || shadow_request.profile_id != PassProfileID::ShadowCasterMasked)
        return 11;
    PassProfileRequest opaque_request;
    if (!ResolveScenePassProfileRequest(
            PassProfileID::ShadowCasterOpaque,
            hgl::graph::ShaderStage::Fragment,
            MakeShadowCasterProfile(), opaque_request, diagnostic)
     || opaque_request.profile_id != PassProfileID::ShadowCasterOpaque)
        return 12;

    ScenePassProfile duplicate;
    ScenePassProfile missing_path;
    if (missing_path.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "surface_material"))
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
