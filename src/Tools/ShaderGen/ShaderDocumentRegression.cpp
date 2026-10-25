#include <hgl/mtl/ShaderDocument.h>
#include <hgl/mtl/RenderTemplate.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/ShaderCodeModuleFile.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/filesystem/Path.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/log/Log.h>
#include <hgl/time/Time.h>

using namespace hgl;
using namespace hgl::graph::mtl;
using hgl::AnsiString;

namespace
{
    struct RootModuleSpec
    {
        ShaderModuleSlotRole role;
        const char *name;
        const char *relative_path;
    };

    const RootModuleSpec root_specs[] =
    {
        { ShaderModuleSlotRole::SurfaceProvider, "surface/pbr_texture", "surface/pbr_texture.glsl" },
        { ShaderModuleSlotRole::DiffuseLightProvider, "diffuse/lambert", "diffuse/lambert.glsl" },
        { ShaderModuleSlotRole::SpecularLightProvider, "specular/ggx", "specular/ggx.glsl" },
        { ShaderModuleSlotRole::ShadowProvider, "shadow/pcf", "shadow/pcf.glsl" },
        { ShaderModuleSlotRole::AmbientLightProvider, "ambient/ibl", "ambient/ibl.glsl" },
        { ShaderModuleSlotRole::AmbientOcclusionProvider, "ao/identity", "ao/identity.glsl" },
        { ShaderModuleSlotRole::LightingModel, "lighting/pbr", "lighting/pbr.glsl" },
        { ShaderModuleSlotRole::OutputPolicy, "output/forward_hdr", "output/forward_hdr.glsl" },
        { ShaderModuleSlotRole::MaterialSourceProvider, "material/source", "material/source.glsl" },
        { ShaderModuleSlotRole::NTBProvider, "ntb/provider", "ntb/provider.glsl" },
        { ShaderModuleSlotRole::SkyProvider, "sky/atmosphere", "sky/atmosphere.glsl" }
    };

    const int root_spec_count = int(sizeof(root_specs) / sizeof(root_specs[0]));

    // 模板根模块必须能解析到真实文件来源（ResolveModuleRoot 校验
    // include_path 与 source_path 一致），故回归用例改为落盘真实模块文件。
    enum class FixtureVariant
    {
        Baseline,
        MissingDirectLightCapability,
        SurfaceDeclaresLightingModelSlot
    };

    struct FixtureVariantSpec
    {
        FixtureVariant variant;
        const char *directory;
    };

    const FixtureVariantSpec fixture_variants[] =
    {
        { FixtureVariant::Baseline, "baseline" },
        { FixtureVariant::MissingDirectLightCapability, "missing_capability" },
        { FixtureVariant::SurfaceDeclaresLightingModelSlot, "slot_mismatch" }
    };

    const int fixture_variant_count =
        int(sizeof(fixture_variants) / sizeof(fixture_variants[0]));

    class FixtureDirectory
    {
    public:
        hgl::OSString root;

        bool Init()
        {
            hgl::OSString current;
            if (!hgl::filesystem::GetCurrentPath(current))
                return false;
            root = (hgl::filesystem::Path(current) / hgl::ToOSString(
                (AnsiString("shader-document-")
                    + AnsiString::numberOf(hgl::GetTimeUs())).c_str())).ToOSString();
            return hgl::filesystem::MakePath(root);
        }

        /// <root>/<directory>/<relative>
        hgl::OSString Resolve(const char *directory, const char *relative) const
        {
            AnsiString tail(directory);
            if (relative)
            {
                tail += "/";
                tail += relative;
            }
            tail.Replace('/', HGL_DIRECTORY_SEPARATOR);
            return (hgl::filesystem::Path(root) / hgl::ToOSString(tail.c_str()))
                .ToOSString();
        }

        bool Write(const char *directory, const char *relative, const AnsiString &code)
        {
            const hgl::OSString file = Resolve(directory, relative);
            const hgl::OSString parent =
                hgl::filesystem::Path(file).GetParent().ToOSString();
            if (!hgl::filesystem::MakePath(parent))
                return false;
            return hgl::filesystem::SaveMemoryToFile(
                file, code.c_str(), code.Length()) == code.Length();
        }

        ~FixtureDirectory()
        {
            if (root.IsEmpty())
                return;

            // DeletePath 只删空目录，故按「文件 → 叶目录 → 变体目录 → 根」顺序清理
            for (int variant_index = 0;
                 variant_index < fixture_variant_count;
                 ++variant_index)
            {
                const char *directory = fixture_variants[variant_index].directory;
                for (int index = 0; index < root_spec_count; ++index)
                {
                    const hgl::OSString file =
                        Resolve(directory, root_specs[index].relative_path);
                    if (hgl::filesystem::FileExist(file)
                     && !hgl::filesystem::FileDelete(file))
                        GLogWarning(u8"Cannot remove shader-document fixture: %ls",
                                    file.c_str());
                }
                for (int index = 0; index < root_spec_count; ++index)
                {
                    const hgl::OSString leaf = hgl::filesystem::Path(
                        Resolve(directory, root_specs[index].relative_path))
                        .GetParent().ToOSString();
                    if (hgl::filesystem::IsDirectory(leaf)
                     && !hgl::filesystem::DeletePath(leaf))
                        GLogWarning(u8"Cannot remove fixture directory: %ls",
                                    leaf.c_str());
                }
                const hgl::OSString variant_root = Resolve(directory, nullptr);
                if (hgl::filesystem::IsDirectory(variant_root)
                 && !hgl::filesystem::DeletePath(variant_root))
                    GLogWarning(u8"Cannot remove fixture directory: %ls",
                                variant_root.c_str());
            }
            if (!hgl::filesystem::DeletePath(root))
                GLogWarning(u8"Cannot remove fixture directory: %ls", root.c_str());
        }
    };

    AnsiString ModuleText(const RootModuleSpec &spec, const FixtureVariant variant)
    {
        const char *slot = GetShaderModuleSlotRoleName(spec.role);
        const char *provides = nullptr;
        const char *requires_capability = nullptr;

        switch (spec.role)
        {
            case ShaderModuleSlotRole::DiffuseLightProvider:
                if (variant != FixtureVariant::MissingDirectLightCapability)
                    provides = "direct_diffuse";
                break;
            case ShaderModuleSlotRole::SpecularLightProvider:
                if (variant != FixtureVariant::MissingDirectLightCapability)
                    provides = "direct_specular";
                break;
            case ShaderModuleSlotRole::LightingModel:
                requires_capability = "direct_diffuse|direct_specular";
                break;
            case ShaderModuleSlotRole::SurfaceProvider:
                if (variant == FixtureVariant::SurfaceDeclaresLightingModelSlot)
                    slot = "lighting_model";
                break;
            default:
                break;
        }

        AnsiString text = AnsiString("// @ulre begin\n// @ulre name ") + spec.name
            + "\n// @ulre kind Utility\n// @ulre slot " + slot + "\n";
        if (provides)
            text += AnsiString("// @ulre provides_capability ") + provides + "\n";
        if (requires_capability)
            text += AnsiString("// @ulre requires_capability ") + requires_capability + "\n";
        text += "// @ulre end\nvoid ModuleBody() {}\n";
        return text;
    }

    bool WriteFixtureSet(FixtureDirectory &fixtures,
                         const char *directory,
                         const FixtureVariant variant)
    {
        for (int index = 0; index < root_spec_count; ++index)
        {
            const RootModuleSpec &spec = root_specs[index];
            if (!fixtures.Write(directory, spec.relative_path,
                                ModuleText(spec, variant)))
                return false;
        }
        return true;
    }

    bool LoadFixtureRegistry(FixtureDirectory &fixtures,
                             const char *directory,
                             ShaderCodeModuleRegistry &out_registry)
    {
        int file_count = 0;
        int error_count = 0;
        if (!out_registry.LoadDirectory(
                fixtures.Resolve(directory, nullptr), &file_count, &error_count))
            return false;
        return file_count == root_spec_count
            && error_count == 0
            && out_registry.GetCount() == root_spec_count;
    }
}

int main()
{
    ShaderDocument document;
    document.Add(ShaderDocumentBlockKind::Version, "#version 460");
    document.Add(ShaderDocumentBlockKind::Define, "#define TEST_DEFINE 1");
    document.Add(ShaderDocumentBlockKind::Raw, "void main() {}");

    AnsiString serialized;
    ShaderDocumentDiagnostics diagnostics;
    if (!document.Serialize(serialized, diagnostics))
        return 1;
    if (serialized.IsEmpty() || diagnostics.GetCount() != 0)
        return 6;
    ShaderDocumentDiagnostics fragment_diagnostics;
    AnsiString fragment;
    if (!document.SerializeFragment(fragment, fragment_diagnostics)
     || fragment != serialized)
        return 9;
    const hgl::uint64 first_hash = document.GetSerializedHash(diagnostics);
    const hgl::uint64 second_hash = document.GetSerializedHash(diagnostics);
    if (first_hash != second_hash)
        return 5;

    ShaderDocument invalid;
    ShaderDocumentSource invalid_source;
    invalid_source.stage = "fragment";
    invalid.Add(ShaderDocumentBlockKind::Raw, "raw");
    invalid.Add(ShaderDocumentBlockKind::Version, "#version 460", invalid_source);
    invalid.Add(ShaderDocumentBlockKind::Version, "#version 460", invalid_source);
    if (invalid.Serialize(serialized, diagnostics)
     || diagnostics.GetCount() < 2)
        return 2;
    if (diagnostics[0]->source.stage.IsEmpty())
        return 10;

    ShaderDocument invalid_order;
    invalid_order.Add(ShaderDocumentBlockKind::Version, "#version 460\n");
    invalid_order.Add(ShaderDocumentBlockKind::MainBody, "void main() {}\n");
    invalid_order.Add(ShaderDocumentBlockKind::Define, "#define TEST 1\n");
    if (invalid_order.Serialize(serialized, diagnostics)
     || diagnostics.GetCount() != 1
     || diagnostics[0]->code != "block-order")
        return 24;

    const AnsiString version = "#version 460\n";
    ShaderDocument version_document;
    ShaderDocumentSource raw_source;
    raw_source.stage = "fragment";
    version_document.Add(ShaderDocumentBlockKind::Version, version, raw_source);
    AnsiString raw_serialized;
    ShaderDocumentDiagnostics raw_diagnostics;
    if (!version_document.Serialize(raw_serialized, raw_diagnostics)
     || raw_serialized != version
     || version_document.GetBlockCount() != 1
     || version_document.GetBlock(0).kind != ShaderDocumentBlockKind::Version
     || version_document.GetBlock(0).source.stage != "fragment")
        return 3;

    ShaderDocument injected;
    ShaderDocumentSource injected_source;
    injected_source.stage = "mesh";
    injected.Add(
        ShaderDocumentBlockKind::Version,
        "#version 460\n",
        injected_source);
    injected.Add(
        ShaderDocumentBlockKind::Define,
        "#define INJECTED 1\n",
        injected_source);
    injected.Add(
        ShaderDocumentBlockKind::Raw,
        "void main() {}\n",
        injected_source);
    AnsiString injected_serialized;
    ShaderDocumentDiagnostics injected_diagnostics;
    if (!injected.Serialize(injected_serialized, injected_diagnostics)
     || injected_serialized !=
            "#version 460\n#define INJECTED 1\nvoid main() {}\n")
        return 7;


    RenderTemplateRequest template_request{};
    template_request.template_id = RenderTemplateID::ForwardLitShadowedAO;
    template_request.template_version = 1;
    for (int index = 0; index < root_spec_count; ++index)
    {
        if (!template_request.AddModuleRoot(
                root_specs[index].role, root_specs[index].name))
            return 14;
    }
    for (hgl::uint32 index = 0;
         index < template_request.module_root_count; ++index)
        template_request.module_roots[index].include_path =
            root_specs[index].relative_path;

    RenderTemplateValidationDiagnostic template_diagnostic{};
    if (!ValidateRenderTemplateRequest(template_request, template_diagnostic)
     || template_request.GetHash() == 0)
        return 15;

    RenderTemplateRequest incomplete_request{};
    incomplete_request.template_id = RenderTemplateID::ForwardLitShadowedAO;
    incomplete_request.template_version = 1;
    if (ValidateRenderTemplateRequest(incomplete_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::MissingRequiredSlot)
        return 16;

    RenderTemplateRequest unknown_template_request{};
    unknown_template_request.template_version = 1;
    if (ValidateRenderTemplateRequest(
            unknown_template_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::UnknownTemplate)
        return 17;

    RenderTemplateRequest wrong_stage_request = template_request;
    wrong_stage_request.stage = hgl::graph::ShaderStage::Mesh;
    if (ValidateRenderTemplateRequest(wrong_stage_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::StageMismatch)
        return 18;

    RenderTemplateRequest wrong_version_request = template_request;
    ++wrong_version_request.template_version;
    if (ValidateRenderTemplateRequest(wrong_version_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::VersionMismatch)
        return 19;

    RenderTemplateRequest unexpected_slot_request{};
    unexpected_slot_request.template_id = RenderTemplateID::ForwardUnlit;
    unexpected_slot_request.template_version = 1;
    if (!unexpected_slot_request.AddModuleRoot(
            ShaderModuleSlotRole::SurfaceProvider, "surface/pbr_texture")
     || !unexpected_slot_request.AddModuleRoot(
            ShaderModuleSlotRole::ShadowProvider, "shadow/pcf")
     || ValidateRenderTemplateRequest(
            unexpected_slot_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::UnexpectedSlotRole)
        return 20;

    RenderTemplateRequest duplicate_slot_request = template_request;
    duplicate_slot_request.module_roots[duplicate_slot_request.module_root_count] =
        duplicate_slot_request.module_roots[0];
    ++duplicate_slot_request.module_root_count;
    if (ValidateRenderTemplateRequest(duplicate_slot_request, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::DuplicateSlotRole)
        return 21;

    const RenderTemplateDefinition *forward_lit_template =
        FindRenderTemplate(RenderTemplateID::ForwardLitShadowedAO);
    if (!forward_lit_template
     || forward_lit_template->version != 1
     || forward_lit_template->stage != hgl::graph::ShaderStage::Fragment)
        return 22;

    const char slot_module[] =
        "// @ulre begin\n"
        "// @ulre name test_ambient\n"
        "// @ulre kind Utility\n"
        "// @ulre slot ambient_light_provider\n"
        "// @ulre provides_capability ambient_diffuse|ambient_specular\n"
        "// @ulre requires_capability surface_normal\n"
        "// @ulre end\n"
        "void TestAmbient() {}\n";
    ShaderCodeModuleFileData module_file_data{};
    if (ParseShaderCodeModuleFile(
            slot_module,
            int(sizeof(slot_module) - 1),
            module_file_data) != ShaderCodeModuleParseResult::OK
     || module_file_data.slot_role
            != ShaderModuleSlotRole::AmbientLightProvider
     || module_file_data.provided_capabilities
            != (hgl::uint32(ShaderModuleCapability::AmbientDiffuse)
              | hgl::uint32(ShaderModuleCapability::AmbientSpecular))
     || module_file_data.required_capabilities
            != hgl::uint32(ShaderModuleCapability::SurfaceNormal))
        return 23;

    FixtureDirectory fixtures;
    if (!fixtures.Init())
        return 30;
    for (int index = 0; index < fixture_variant_count; ++index)
    {
        if (!WriteFixtureSet(fixtures,
                             fixture_variants[index].directory,
                             fixture_variants[index].variant))
            return 30;
    }

    ShaderCodeModuleRegistry root_registry;
    if (!LoadFixtureRegistry(fixtures, fixture_variants[0].directory, root_registry))
        return 24;
    if (!ValidateRenderTemplateRequest(
            template_request, root_registry, template_diagnostic))
        return 25;
    ResolvedRenderTemplate resolved_template{};
    if (!ResolveRenderTemplate(
            template_request, root_registry, resolved_template,
            template_diagnostic)
     || !resolved_template.IsValid()
     || resolved_template.definition->id != template_request.template_id
     || resolved_template.module_root_count != template_request.module_root_count
     || resolved_template.manifest.code_module_count != 11
     || resolved_template.stable_hash == 0)
        return 29;

    ShaderCodeModuleRegistry missing_capability_registry;
    if (!LoadFixtureRegistry(
            fixtures, fixture_variants[1].directory, missing_capability_registry))
        return 31;
    if (ValidateRenderTemplateRequest(
            template_request, missing_capability_registry, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::MissingModuleCapability)
        return 26;

    ShaderCodeModuleRegistry slot_mismatch_registry;
    if (!LoadFixtureRegistry(
            fixtures, fixture_variants[2].directory, slot_mismatch_registry))
        return 32;
    if (ValidateRenderTemplateRequest(
            template_request, slot_mismatch_registry, template_diagnostic)
     || template_diagnostic.error
            != RenderTemplateValidationError::ModuleSlotMismatch)
        return 27;

    return 0;
}
