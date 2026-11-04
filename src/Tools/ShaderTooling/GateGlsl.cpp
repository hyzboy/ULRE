// 阶段 5 拆分自 ShaderResourceSchemaRegressionGate.cpp：组 descriptor,glsl,module_invariants,reflection 的 case 实现。
#include "GateTestSupport.h"

namespace gate
{
    GateResult RunUnifiedMaterialContractCase()
    {
        GateResult result;
        result.name = "U.unified-material-contract";

        // 领域身份必须由契约给出：UnlitSolid 的 id 就是领域名，不再是手写的
        // `builtin/pure_color`（那个字符串随 definition 文件一起消失）。
        if (std::strcmp(GetMaterialDomainName(MaterialDomain::UnlitSolid),
                        "UnlitSolid") != 0)
            result.diagnostics.emplace_back("canonical UnlitSolid domain name changed unexpectedly");

        ResolvedMaterial pure_color{};
        if (!BuildResolvedMaterialForDomain(MaterialDomain::UnlitSolid, pure_color))
        {
            result.diagnostics.emplace_back("canonical UnlitSolid definition missing during contract phase");
        }
        else
        {
            if (pure_color.material_private_data != GlobalSSBOType::EmissiveSurface
             || pure_color.vertex_semantic_requirements.GetCount() != 1)
                result.diagnostics.emplace_back("PureColor contract is not canonical");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunGlobalSSBOBindingKeyCase()
    {
        GateResult result;
        result.name = "Y1a.material-ssbo-binding-key";

        MaterialRecipe recipe{};
        recipe.material_ssbo_binding = {
            GlobalSSBOType::EmissiveSurface,
            11,
            7};
        if (!recipe.material_ssbo_binding.IsValid())
        {
            result.diagnostics.emplace_back(
                "material SSBO binding rejected a valid binding");
        }

        recipe.material_ssbo_binding = {
            GlobalSSBOType::EmissiveSurface,
            44,
            9};
        if (!recipe.material_ssbo_binding.IsValid())
        {
            result.diagnostics.emplace_back(
                "material SSBO binding replacement is invalid");
        }

        MaterialRecipe missing_row_id_recipe{};
        missing_row_id_recipe.material_ssbo_binding = {
            GlobalSSBOType::EmissiveSurface,
            55,
            uint32_t(-1)};
        if (missing_row_id_recipe.material_ssbo_binding.IsValid())
        {
            result.diagnostics.emplace_back(
                "material SSBO binding accepted a missing active row ID");
        }

        const auto *material_data =
            recipe.material_ssbo_binding.IsValid()
                ? &recipe.material_ssbo_binding : nullptr;
        if (!material_data || material_data->ssbo_id != 44
         || material_data->data_index != 9
         || material_data->ssbo_type != GlobalSSBOType::EmissiveSurface)
        {
            result.diagnostics.emplace_back(
                "material SSBO binding replacement did not preserve its identity");
        }

        if (!material_data
         || material_data->ssbo_type != GlobalSSBOType::EmissiveSurface)
        {
            result.diagnostics.emplace_back(
                "material SSBO binding type did not remain explicit");
        }

        MaterialRecipe no_binding_recipe{};
        if (ResolveRecipeSSBOType(
                recipe,
                GlobalSSBOType::PBRSurface)
                != GlobalSSBOType::EmissiveSurface
         || ResolveRecipeSSBOType(
                no_binding_recipe,
                GlobalSSBOType::PBRSurface)
                != GlobalSSBOType::PBRSurface
         || ResolveRecipeSSBOType(
                recipe,
                GlobalSSBOType::EmissiveSurface)
                != GlobalSSBOType::EmissiveSurface)
        {
            result.diagnostics.emplace_back(
                "material authoring must inherit recipe SSBO type unless an explicit material override is supplied");
        }

        if (recipe.material_ssbo_binding.data_index == uint32_t(-1))
        {
            result.diagnostics.emplace_back(
                "valid material SSBO binding lost its active row ID");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunProviderResourceManifestCase()
    {
        GateResult result;
        result.name = "H1.provider-resource-manifest";

        GlslModuleRegistry registry;

        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(
                hgl::ToOSString(GetShaderLibraryPath()), &file_count, &error_count))
        {
            result.diagnostics.emplace_back("provider registry directory scan failed");
        }
        else if (error_count != 0)
        {
            result.diagnostics.emplace_back(
                "provider registry directory scan reported parse errors");
        }

        const auto *pbr_2d = registry.FindByName("material_source_pbr_surface");
        const auto *ntb_2d = registry.FindByName("ntb_tangent_vbo_normalmap");
        const auto *unlit = registry.FindByName("material_source_texture");
        if (!pbr_2d || !ntb_2d || !unlit)
        {
            result.diagnostics.emplace_back("provider metadata modules are missing");
        }
        else
        {
            const auto has_texture_reference =
                [](const ShaderCodeResourceManifest &manifest,
                   const char *name)
                {
                    for (uint32_t i = 0;
                         i < manifest.texture_reference_count;
                         ++i)
                    {
                        if (std::strcmp(
                                manifest.texture_references[i].texture_name,
                                name) == 0)
                            return true;
                    }
                    return false;
                };
            ResolvedMaterial lit_definition{};
            ResolvedMaterial unlit_definition{};
            if (!BuildResolvedMaterialForDomain(MaterialDomain::StandardOpaque, lit_definition)
             || !BuildResolvedMaterialForDomain(
                    MaterialDomain::UnlitTexture,
                    unlit_definition))
            {
                result.diagnostics.emplace_back(
                    "provider texture-reference material definitions are missing");
            }

            const char *roots_2d[] = {pbr_2d->name, ntb_2d->name};
            ShaderCodeResourceManifest manifest_2d{};
            if (!BuildShaderCodeResourceManifest(
                    roots_2d, uint32_t(std::size(roots_2d)), manifest_2d, &registry))
            {
                result.diagnostics.emplace_back(
                    std::string("Texture2D provider manifest failed: ")
                    + GetShaderCodeResourceManifestErrorName(manifest_2d.error));
            }
            else
            {
                if (manifest_2d.texture_reference_count != 8
                 || !has_texture_reference(manifest_2d, "base_color")
                 || !has_texture_reference(manifest_2d, "roughness")
                 || !has_texture_reference(manifest_2d, "metallic")
                 || !has_texture_reference(manifest_2d, "occlusion")
                 || !has_texture_reference(manifest_2d, "opacity_mask")
                 || !has_texture_reference(manifest_2d, "normal")
                 || !has_texture_reference(manifest_2d, "emissive")
                 || !has_texture_reference(manifest_2d, "orm")
                 || !ValidateShaderCodeResourceManifestTextureReferences(
                        manifest_2d,
                        lit_definition))
                    result.diagnostics.emplace_back(
                        "Texture2D providers must declare TOML-aligned texture references");
            }

            const char *ntb_root = ntb_2d->name;
            ShaderCodeResourceManifest ntb_manifest{};
            if (!BuildShaderCodeResourceManifest(
                    &ntb_root, 1, ntb_manifest, &registry)
             || ntb_manifest.texture_reference_count != 1
             || !has_texture_reference(ntb_manifest, "normal"))
               result.diagnostics.emplace_back(
                   "Normal-map provider must declare its named texture reference");

            const char *unlit_root = unlit->name;
            ShaderCodeResourceManifest unlit_manifest{};
            if (!BuildShaderCodeResourceManifest(
                    &unlit_root,
                    1,
                    unlit_manifest,
                    &registry)
             || unlit_manifest.texture_reference_count != 1
             || !has_texture_reference(unlit_manifest, "base_color")
             || !ValidateShaderCodeResourceManifestTextureReferences(
                    unlit_manifest,
                    unlit_definition))
            {
                result.diagnostics.emplace_back(
                    "generic texture material must use one TOML texture reference");
            }

            ShaderCodeResourceManifest invalid_reference_manifest{};
            if (invalid_reference_manifest.texture_references.Add(
                    {"missing_texture",
                     uint32_t(hgl::graph::ShaderStage::Fragment),
                     false,
                     false}) < 0)
            {
                result.diagnostics.emplace_back(
                    "invalid texture-reference manifest setup failed");
            }
            else
                invalid_reference_manifest.texture_reference_count = 1;
            const char *invalid_texture_name = nullptr;
            if (ValidateShaderCodeResourceManifestTextureReferences(
                    invalid_reference_manifest,
                    unlit_definition,
                    &invalid_texture_name)
             || !invalid_texture_name
             || std::strcmp(invalid_texture_name, "missing_texture") != 0)
            {
                result.diagnostics.emplace_back(
                    "TOML-external provider texture reference must be rejected");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunGlslModuleFileCase()
    {
        GateResult result;
        result.name = "I.glsl-code-module-file-parse";

        auto expect_parse = [&result](const char *content,
                                      const GlslModuleParseResult expected,
                                      const char *label)
        {
            GlslModuleFileData data;
            const GlslModuleParseResult actual =
                ParseGlslModuleFile(content, int(std::strlen(content)), data);
            if (actual != expected)
            {
                result.diagnostics.emplace_back(std::string(label) + ": got "
                    + GetGlslModuleParseResultName(actual)
                    + " expected " + GetGlslModuleParseResultName(expected));
                return false;
            }
            return true;
        };

        // Valid full metadata block.
        const char full_meta[] =
            "// @ulre begin\n"
            "// @ulre name sample_ntb\n"
            "// @ulre kind Utility\n"
            "// @ulre priority 10\n"
            "// @ulre require GeometryAttribute Normal Float 3 3\n"
            "// @ulre require GeometryAttribute Tangent Any\n"
            "// @ulre provide Normal\n"
            "// @ulre provide Tangent\n"
            "// @ulre texture_reference normal Fragment optional fallback\n"
            "// @ulre uses position_lift_xy0\n"
            "// @ulre end\n";
        {
            GlslModuleFileData data;
            const GlslModuleParseResult parse =
                ParseGlslModuleFile(full_meta, int(std::strlen(full_meta)), data);
            if (parse != GlslModuleParseResult::OK)
                result.diagnostics.emplace_back("full-metadata parse failed");
            else
            {
                if (std::strcmp(data.name.c_str(), "sample_ntb") != 0)
                    result.diagnostics.emplace_back("full-metadata name mismatch");
                if (data.kind != GlslModuleKind::Utility)
                    result.diagnostics.emplace_back("full-metadata kind mismatch");
                if (data.priority != 10)
                    result.diagnostics.emplace_back("full-metadata priority mismatch");
                if (data.semantic_requirements.GetCount() != 2)
                    result.diagnostics.emplace_back("full-metadata require count mismatch");
                else
                {
                    const auto &normal_req = data.semantic_requirements[0];
                    if (normal_req.source != GlslModuleCapabilitySource::GeometryAttribute
                     || normal_req.semantic != GlslModuleSemantic::Normal
                     || normal_req.numeric_class_mask != uint32_t(GlslModuleNumericClass::Float)
                     || normal_req.min_component_count != 3
                     || normal_req.max_component_count != 3)
                        result.diagnostics.emplace_back("full-metadata require[0] mismatch");

                    const auto &tangent_req = data.semantic_requirements[1];
                    if (tangent_req.source != GlslModuleCapabilitySource::GeometryAttribute
                     || tangent_req.semantic != GlslModuleSemantic::Tangent
                     || tangent_req.numeric_class_mask != uint32_t(GlslModuleNumericClass::Any)
                     || tangent_req.min_component_count != 0
                     || tangent_req.max_component_count != 0)
                        result.diagnostics.emplace_back("full-metadata require[1] mismatch");
                }
                if (data.semantic_provides.GetCount() != 2
                 || data.semantic_provides[0] != GlslModuleSemantic::Normal
                 || data.semantic_provides[1] != GlslModuleSemantic::Tangent)
                    result.diagnostics.emplace_back("full-metadata provide mismatch");
                if (data.texture_reference_requirements.GetCount() != 1
                 || !data.texture_reference_requirements[0].texture_name
                 || std::strcmp(
                        data.texture_reference_requirements[0].texture_name,
                        "normal") != 0
                 || data.texture_reference_requirements[0].stage_flags
                    != uint32_t(hgl::graph::ShaderStage::Fragment)
                 || data.texture_reference_requirements[0].required
                 || !data.texture_reference_requirements[0].allow_fallback)
                {
                    result.diagnostics.emplace_back(
                        "full-metadata texture-reference mismatch");
                }
                if (data.pending_module_requirements.GetCount() != 1
                 || std::strcmp(data.pending_module_requirements[0].c_str(), "position_lift_xy0") != 0)
                    result.diagnostics.emplace_back("full-metadata uses mismatch");
            }
        }

        expect_parse("// @ulre begin\n// @ulre end\n", GlslModuleParseResult::OK, "minimal");
        expect_parse("void main() {}\n", GlslModuleParseResult::Skipped, "no-metadata");
        expect_parse("// @ulre name x\n// @ulre begin\n// @ulre end\n", GlslModuleParseResult::MissingBegin, "missing-begin");
        expect_parse("// @ulre begin\n// @ulre begin\n// @ulre end\n", GlslModuleParseResult::DuplicateBegin, "duplicate-begin");
        expect_parse("// @ulre begin\n// @ulre name x\n", GlslModuleParseResult::MissingEnd, "missing-end");
        expect_parse("// @ulre begin\n// @ulre nope 1\n// @ulre end\n", GlslModuleParseResult::UnknownDirective, "unknown-directive");
        expect_parse("// @ulre begin\n// @ulre name a\n// @ulre name b\n// @ulre end\n", GlslModuleParseResult::DuplicateDirective, "duplicate-directive");
        expect_parse("// @ulre begin\n// @ulre kind\n// @ulre end\n", GlslModuleParseResult::MissingDirectiveArgument, "missing-argument");
        expect_parse("// @ulre begin\n// @ulre kind NoSuchKind\n// @ulre end\n", GlslModuleParseResult::InvalidKind, "invalid-kind");
        expect_parse("// @ulre begin\n// @ulre provide NoSuchSemantic\n// @ulre end\n", GlslModuleParseResult::InvalidSemantic, "invalid-semantic");
        expect_parse("// @ulre begin\n// @ulre require BadSource Normal\n// @ulre end\n", GlslModuleParseResult::InvalidSource, "invalid-source");
        expect_parse("// @ulre begin\n// @ulre require GeometryAttribute Normal NotAClass\n// @ulre end\n", GlslModuleParseResult::InvalidNumericClass, "invalid-numclass");
        expect_parse("// @ulre begin\n// @ulre priority notanumber\n// @ulre end\n", GlslModuleParseResult::InvalidNumber, "invalid-number");
        expect_parse("// @ulre begin\n// @ulre texture_reference invalid-name Fragment\n// @ulre end\n", GlslModuleParseResult::InvalidResource, "invalid-texture-reference");
        expect_parse("// @ulre begin\n// @ulre conflicts\n// @ulre end\n", GlslModuleParseResult::UnknownDirective, "conflicts-directive-removed");

        // Registry scan of the real ShaderLibrary directory.
        GlslModuleRegistry registry;

        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(hgl::ToOSString(GetShaderLibraryPath()), &file_count, &error_count))
            result.diagnostics.emplace_back("LoadDirectory failed to scan directory");
        else
        {
            // Consolidated scene Set 0 UBOs into single scene_ubo.glsl; added RenderItemResolve.glsl, shadow_receiver_source.glsl, pcf_shadow.glsl.
            // Stage 5 removed the duplicate ntb_derivative_normalmap provider (71 → 70).
            // 回退标记色：新增 error_checkerboard_3d_source（70 → 71）。
            // 阶段一：新增 pass_environment_interface（71 → 72）。
            // 超薄架构阶段一：直接光拆成 diffuse/specular 两槽，删除
            // direct_cook_torrance_pbr，新增 diffuse_{lambert,burley,toon,none}
            // + specular_{cook_torrance,toon_step,none} + indirect_none +
            // forward_toon + shadow/full（72 → 81）。
            // 超薄架构阶段二之二：新增 ntb_none 空对象（81 → 82）。
            // 顶点色统一阶段：新增 s1_color_default 空对象（82 → 83）。
            // 网格阶段收口 Layer 2a：新增 s1_stream_defaults 空对象（83 → 84）。
            // 材质源投影阶段：新增 texture_projection_{2d,cube,octahedral,equirect}
            // （84 → 88）；同名同签名的 SampleMaterialProjected 由
            // frag_options 选其一，主着色器不出现投影分支。
            // 计数常量已收敛到 `kGlslModuleCount`（文件顶部），增删模块时
            // 只改那一处并在此追加一行理由。
            // 高级材质阶段一（Filament 移植）：新增 clearcoat_light_{none,ggx}、
            // sheen_light_{none,charlie}、subsurface_light_{none,wrap}、
            // specular_light_anisotropic（90 → 97）。
            if (file_count != kGlslModuleCount)
                result.diagnostics.emplace_back("LoadDirectory expected "
                    + std::to_string(kGlslModuleCount)
                    + " file modules, got "
                    + std::to_string(file_count));
            if (error_count != 0)
                result.diagnostics.emplace_back("LoadDirectory reported "
                    + std::to_string(error_count) + " errors");

            if (registry.GetCount() != kGlslModuleCount)
                result.diagnostics.emplace_back("registry count after LoadDirectory mismatch: got "
                    + std::to_string(registry.GetCount()));

            GlslModuleMetadataValidationDiagnostic metadata_diagnostic{};
            if (!ValidateGlslModuleRegistryMetadata(
                    registry, metadata_diagnostic))
            {
                result.diagnostics.emplace_back(
                    "formal metadata validation failed: error="
                    + std::string(
                        GetGlslModuleMetadataValidationErrorName(
                            metadata_diagnostic.error))
                    + " module="
                    + std::string(metadata_diagnostic.module_name.c_str())
                    + " related="
                    + std::string(
                        metadata_diagnostic.related_module_name.c_str()));
            }
        }

        const auto *lift = registry.FindByName("position_lift_xy0");
        if (!lift)
            result.diagnostics.emplace_back("s2_lift_xy0 not found by name");
        else if (lift->kind != GlslModuleKind::Position
              || lift->semantic_requirement_count != 1
              || lift->semantic_requirements[0].semantic != GlslModuleSemantic::Position
              || lift->semantic_requirements[0].min_component_count != 2
              || lift->semantic_requirements[0].max_component_count != 2)
            result.diagnostics.emplace_back("s2_lift_xy0 capability metadata mismatch");

        const auto *surface_interface = registry.FindByName("surface_interface");
        if (!surface_interface)
            result.diagnostics.emplace_back("surface_interface not found by name");
        else if (surface_interface->kind != GlslModuleKind::Shared)
            result.diagnostics.emplace_back("surface_interface kind mismatch");

        // 单趟发射改造（2026-09）：compositor 骨架降级为发射器常量，
        // skeleton key 为 "forward_surface" / "depth_only" / "forward_sky"。

        const auto *forward_input = registry.FindByName("output_policy_forward");
        if (!forward_input || forward_input->kind != GlslModuleKind::Utility)
            result.diagnostics.emplace_back("forward_lighting input assembly module is missing or has wrong kind");

        const auto *forward_algorithm = registry.FindByName("lighting_model_pbr");
        if (!forward_algorithm || forward_algorithm->kind != GlslModuleKind::Utility)
            result.diagnostics.emplace_back("forward_pbr lighting algorithm module is missing or has wrong kind");

        const auto *alternate_algorithm = registry.FindByName("lighting_model_flat");
        if (!alternate_algorithm || alternate_algorithm->kind != GlslModuleKind::Utility)
            result.diagnostics.emplace_back("forward_flat alternate lighting algorithm is missing or has wrong kind");

        // 超薄架构阶段一：直接光拆成两条正交轴，每条轴都必须有“真实实现 + 空对象”。
        // 缺任何一个都会让“换模型 = 换一行 include”的承诺失效，因此在这里钉住。
        const auto require_slot_module = [&](const char *name,
                                             const char *slot_name)
        {
            const auto *module = registry.FindByName(name);
            if (!module)
            {
                result.diagnostics.emplace_back(
                    std::string(name) + " slot module is missing");
                return;
            }
            if (module->kind != GlslModuleKind::Utility)
                result.diagnostics.emplace_back(
                    std::string(name) + " kind mismatch (expected Utility)");
            if (std::strcmp(GetShaderModuleSlotRoleName(module->slot_role),
                            slot_name) != 0)
                result.diagnostics.emplace_back(
                    std::string(name) + " declares wrong slot (expected "
                    + slot_name + ")");
        };

        for (const char *name : { "diffuse_light_lambert", "diffuse_light_burley",
                                  "diffuse_light_toon", "diffuse_light_none" })
            require_slot_module(name, "diffuse_light_provider");
        for (const char *name : { "specular_light_cook_torrance", "specular_light_toon_step",
                                  "specular_light_anisotropic", "specular_light_none" })
            require_slot_module(name, "specular_light_provider");
        // 高级直接光三槽（清漆/绒面/次表面）：每条轴同样必须「真实实现 + 空对象」，
        // 基线 ForwardLit 绑定 none，Filament 实现按需换绑，无 #ifdef。
        for (const char *name : { "clearcoat_light_ggx", "clearcoat_light_none" })
            require_slot_module(name, "clearcoat_light_provider");
        for (const char *name : { "sheen_light_charlie", "sheen_light_none" })
            require_slot_module(name, "sheen_light_provider");
        for (const char *name : { "subsurface_light_wrap", "subsurface_light_none" })
            require_slot_module(name, "subsurface_light_provider");
        require_slot_module("ambient_light_none", "ambient_light_provider");
        require_slot_module("lighting_model_toon", "lighting_model");
        require_slot_module("shadow_full", "shadow_provider");
        // 超薄架构阶段二之二：NTB / AO 变成「必需槽 + 空对象补位」，
        // 用空对象替掉编译期开关（HGL_USE_NTB_PROVIDER / HGL_USE_AMBIENT_OCCLUSION_PROVIDER）。
        require_slot_module("ntb_none", "ntb_provider");
        require_slot_module("ambient_occlusion_identity", "ambient_occlusion_provider");


        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunGlslModuleMetadataValidationCase()
    {
        GateResult result;
        result.name = "I1.glsl-code-module-metadata-validation";

        const auto make_definition = [](
            const char *name) -> GlslModuleDefinition
        {
            GlslModuleDefinition definition{};
            definition.name = name;
            definition.glsl_code = "// metadata validation";
            definition.kind = GlslModuleKind::Utility;
            return definition;
        };

        {
            const GlslModuleSemantic duplicate_provides[] =
            {
                GlslModuleSemantic::Position,
                GlslModuleSemantic::Position
            };
            GlslModuleDefinition definition =
                make_definition("duplicate_provide");
            definition.semantic_provides = duplicate_provides;
            definition.semantic_provide_count = 2;

            GlslModuleMetadataValidationDiagnostic diagnostic{};
            if (ValidateGlslModuleMetadata(definition, diagnostic)
             || diagnostic.error
                    != GlslModuleMetadataValidationError::DuplicateProvide)
            {
                result.diagnostics.emplace_back(
                    "duplicate provide metadata must be rejected");
            }
        }

        {
            // 依赖环的权威判据是装配期 DFS（`BuildShaderCodeResourceManifest`），
            // 不是元数据静态校验 —— 元数据层不再重复检测。
            GlslModuleDefinition first =
                make_definition("cycle_first");
            GlslModuleDefinition second =
                make_definition("cycle_second");
            const GlslModuleDependency first_dependencies[] =
            {
                {second.name}
            };
            const GlslModuleDependency second_dependencies[] =
            {
                {first.name}
            };
            first.dependencies = first_dependencies;
            first.dependency_count = 1;
            second.dependencies = second_dependencies;
            second.dependency_count = 1;

            GlslModuleRegistry registry;
            ShaderCodeResourceManifest manifest{};
            const char *roots[] = {first.name};
            if (!registry.Register(first)
             || !registry.Register(second)
             || BuildShaderCodeResourceManifest(roots, 1, manifest, &registry)
             || manifest.error
                    != ShaderCodeResourceManifestError::CodeModuleCycle)
            {
                result.diagnostics.emplace_back(
                    "explicit dependency cycle must be rejected by the assembly DFS");
            }

            const char *missing_roots[] = {"cycle_first_missing_probe"};
            GlslModuleDefinition orphan =
                make_definition(missing_roots[0]);
            const GlslModuleDependency orphan_dependencies[] =
            {
                {"no_such_module"}
            };
            orphan.dependencies = orphan_dependencies;
            orphan.dependency_count = 1;

            GlslModuleRegistry orphan_registry;
            ShaderCodeResourceManifest orphan_manifest{};
            if (!orphan_registry.Register(orphan)
             || BuildShaderCodeResourceManifest(
                    missing_roots, 1, orphan_manifest, &orphan_registry)
             || orphan_manifest.error
                    != ShaderCodeResourceManifestError::UnknownCodeModule)
            {
                result.diagnostics.emplace_back(
                    "missing dependency must be rejected by the assembly DFS");
            }
        }

        {
            const GlslModuleSemantic position[] =
            {
                GlslModuleSemantic::Position
            };
            GlslModuleDefinition first =
                make_definition("ambiguous_first");
            GlslModuleDefinition second =
                make_definition("ambiguous_second");
            first.semantic_provides = position;
            first.semantic_provide_count = 1;
            second.semantic_provides = position;
            second.semantic_provide_count = 1;

            GlslModuleRegistry registry;
            GlslModuleMetadataValidationDiagnostic diagnostic{};
            if (!registry.Register(first)
             || !registry.Register(second)
             || ValidateGlslModuleRegistryMetadata(registry, diagnostic)
             || diagnostic.error
                    != GlslModuleMetadataValidationError::
                        AmbiguousProviderPriority)
            {
                result.diagnostics.emplace_back(
                    "equal-priority providers must be rejected as ambiguous");
            }
        }

        {
            GlslModuleDefinition base =
                make_definition("metadata_hash");
            GlslModuleDefinition changed = base;
            changed.priority = 10;
            if (GetGlslModuleDefinitionHash(base)
                == GetGlslModuleDefinitionHash(changed))
            {
                result.diagnostics.emplace_back(
                    "module metadata changes must affect definition hash");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    // S3a（2026-10-11）：Vk 枚举 name↔value 单一真源（inc/hgl/mtl/VkEnumText.h）。
    // 判据用 for 遍历全表 —— 表里新增一项自动纳入，不需要手列断言。
    GateResult RunVkEnumTextCase()
    {
        GateResult result;
        result.name = "C.vk-enum-text";

        // ① 每个条目：正查能命中且值一致，反查名字一致。
        for (const auto &e : hgl::graph::mtl::kCullModeText)
        {
            VkCullModeFlags parsed = VK_CULL_MODE_NONE;
            if (!hgl::graph::mtl::ParseVkCullMode(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("CullMode 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkCullModeName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("CullMode 条目不可反查: ") + e.text);
        }
        for (const auto &e : hgl::graph::mtl::kCompareOpText)
        {
            VkCompareOp parsed = VK_COMPARE_OP_NEVER;
            if (!hgl::graph::mtl::ParseVkCompareOp(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("CompareOp 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkCompareOpName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("CompareOp 条目不可反查: ") + e.text);
        }
        for (const auto &e : hgl::graph::mtl::kBlendFactorText)
        {
            VkBlendFactor parsed = VK_BLEND_FACTOR_ZERO;
            if (!hgl::graph::mtl::ParseVkBlendFactor(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("BlendFactor 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkBlendFactorName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("BlendFactor 条目不可反查: ") + e.text);
        }
        for (const auto &e : hgl::graph::mtl::kFilterText)
        {
            VkFilter parsed = VK_FILTER_LINEAR;
            if (!hgl::graph::mtl::ParseVkFilter(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("Filter 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkFilterName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("Filter 条目不可反查: ") + e.text);
        }
        for (const auto &e : hgl::graph::mtl::kSamplerMipmapModeText)
        {
            VkSamplerMipmapMode parsed = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            if (!hgl::graph::mtl::ParseVkSamplerMipmapMode(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("MipmapMode 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkSamplerMipmapModeName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("MipmapMode 条目不可反查: ") + e.text);
        }
        for (const auto &e : hgl::graph::mtl::kSamplerAddressModeText)
        {
            VkSamplerAddressMode parsed = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            if (!hgl::graph::mtl::ParseVkSamplerAddressMode(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("AddressMode 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkSamplerAddressModeName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("AddressMode 条目不可反查: ") + e.text);
        }

        // ② 未知名 / 空名必须被拒（不静默落默认值）。
        {
            VkCompareOp op = VK_COMPARE_OP_NEVER;
            if (hgl::graph::mtl::ParseVkCompareOp("Bogus", op)
             || hgl::graph::mtl::ParseVkCompareOp("", op)
             || hgl::graph::mtl::ParseVkCompareOp(nullptr, op))
                result.diagnostics.emplace_back("CompareOp 未知名/空名必须被拒");

            VkFilter f = VK_FILTER_LINEAR;
            if (hgl::graph::mtl::ParseVkFilter("Trilinear", f))
                result.diagnostics.emplace_back("Filter 未知名必须被拒");

            VkSamplerAddressMode am = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            if (hgl::graph::mtl::ParseVkSamplerAddressMode("Clamp", am))
                result.diagnostics.emplace_back("AddressMode 未知名必须被拒");

            VkBlendFactor bf = VK_BLEND_FACTOR_ZERO;
            if (hgl::graph::mtl::ParseVkBlendFactor("Src", bf))
                result.diagnostics.emplace_back("BlendFactor 未知名必须被拒");

            VkCullModeFlags cm = VK_CULL_MODE_NONE;
            if (hgl::graph::mtl::ParseVkCullMode("FrontAndBackBit", cm))
                result.diagnostics.emplace_back("CullMode 未知名必须被拒");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunSPVReflectionCase()
    {
        GateResult result;
        result.name = "SPV.reflection-api";

        static constexpr char shader_source[] = R"GLSL(#version 460
layout(location = 0) in vec3 in_position;
layout(location = 1) out vec3 out_position;
layout(set = 0, binding = 7, std140) uniform ReflectionProbe
{
    vec4 tint;
} probe;
layout(push_constant) uniform ReflectionPush
{
    vec4 offset;
} push_data;

void main()
{
    vec3 position = in_position + probe.tint.xyz + push_data.offset.xyz;
    out_position = position;
    gl_Position = vec4(position, 1.0);
}
)GLSL";

        SPVData *spv = CompileShader(
            static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT), shader_source);
        if (!spv)
        {
            result.diagnostics.emplace_back("failed to compile reflection probe GLSL");
            return result;
        }

        hgl::ValueArray<uint8_t> flat_storage;
        ShaderFlatView view{};
        const bool reflected = ReflectSPVData(
            spv,
            static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT),
            "main",
            flat_storage,
            view);
        FreeSPVData(spv);

        if (!reflected)
        {
            result.diagnostics.emplace_back("ReflectSPVData failed");
            return result;
        }

        hgl::graph::mtl::ShaderReflectionData extracted;
        if (!extracted.Extract(view))
        {
            result.diagnostics.emplace_back("ShaderReflectionData extraction failed");
            return result;
        }
        flat_storage.Clear();

        if (extracted.shader_stage != static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT)
         || std::strcmp(extracted.entrypoint.c_str(), "main") != 0)
            result.diagnostics.emplace_back("reflection stage or entrypoint mismatch");

        if (extracted.stage_inputs.GetCount() != 1
         || extracted.stage_inputs[0].location != 0
         || extracted.stage_inputs[0].basetype != SF_BASETYPE_FLOAT
         || extracted.stage_inputs[0].vec_size != 3)
            result.diagnostics.emplace_back("vertex input reflection mismatch");

        if (!extracted.FindDescriptor(
                SF_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 7))
            result.diagnostics.emplace_back("uniform-buffer set/binding reflection mismatch");

        bool found_output = false;
        for (int i = 0; i < extracted.stage_outputs.GetCount(); ++i)
        {
            if (extracted.stage_outputs[i].location == 1
             && extracted.stage_outputs[i].basetype == SF_BASETYPE_FLOAT
             && extracted.stage_outputs[i].vec_size == 3)
            {
                found_output = true;
                break;
            }
        }
        if (!found_output)
            result.diagnostics.emplace_back("vertex output reflection mismatch");

        uint32_t push_constant_end = 0;
        if (extracted.push_constants.GetCount() != 1
         || extracted.push_constants[0].offset != 0
         || !extracted.GetPushConstantEnd(push_constant_end)
         || push_constant_end < sizeof(float) * 4)
            result.diagnostics.emplace_back("push-constant reflection mismatch");

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunSPVBufferReferenceReflectionCase()
    {
        GateResult result;
        result.name = "SPV.buffer-reference-reflection";

        static constexpr char shader_source[] = R"GLSL(#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require

layout(buffer_reference, scalar, buffer_reference_align = 16) buffer PayloadRow
{
    vec4 color;
};
layout(push_constant) uniform RootAddresses
{
    uint64_t addr_payload;
} pc_root;
layout(location = 0) out vec4 out_color;

void main()
{
    PayloadRow row = PayloadRow(pc_root.addr_payload);
    out_color = row.color;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
)GLSL";

        SPVData *spv = CompileShader(
            static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT), shader_source);
        if (!spv)
        {
            result.diagnostics.emplace_back(
                "failed to compile buffer-reference reflection probe");
            return result;
        }

        hgl::ValueArray<uint8_t> flat_storage;
        ShaderFlatView view{};
        const bool reflected = ReflectSPVData(
            spv,
            static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT),
            "main",
            flat_storage,
            view);
        FreeSPVData(spv);
        if (!reflected)
        {
            result.diagnostics.emplace_back(
                "buffer-reference SPIR-V reflection failed");
            return result;
        }

        hgl::graph::mtl::ShaderReflectionData extracted;
        if (!extracted.Extract(view))
        {
            result.diagnostics.emplace_back(
                "buffer-reference reflection extraction failed");
            return result;
        }

        for (uint32_t type = 0; type < SF_DESCRIPTOR_TYPE_COUNT; ++type)
        {
            if (!extracted.resources[type].IsEmpty())
            {
                result.diagnostics.emplace_back(
                    "buffer_reference was unexpectedly reported as a descriptor resource");
                break;
            }
        }

        uint32_t push_constant_end = 0;
        if (extracted.push_constants.GetCount() != 1
         || !extracted.GetPushConstantEnd(push_constant_end)
         || push_constant_end != sizeof(uint64_t))
            result.diagnostics.emplace_back(
                "buffer-reference address push-constant reflection mismatch");

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunModuleInvariantsCase()
    {
        GateResult result;
        result.name = "MI.module-invariants";

        // 1. Real ShaderLibrary scan: names unique, stable IDs unique.
        GlslModuleRegistry registry;
        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(
                hgl::ToOSString(GetShaderLibraryPath()),
                &file_count, &error_count))
        {
            result.diagnostics.emplace_back(
                "module registry directory scan failed");
        }
        if (error_count != 0)
        {
            result.diagnostics.emplace_back(
                "module registry scan reported "
                + std::to_string(error_count) + " errors");
        }

        const int count = registry.GetCount();
        for (int i = 0; i < count; ++i)
        {
            const auto *left = registry.GetModuleByIndex(i);
            if (!left || !left->name || !left->name[0])
            {
                result.diagnostics.emplace_back(
                    "module at index " + std::to_string(i)
                    + " has no name");
                continue;
            }

            for (int j = i + 1; j < count; ++j)
            {
                const auto *right = registry.GetModuleByIndex(j);
                if (!right || !right->name)
                    continue;

                if (std::strcmp(left->name, right->name) == 0)
                {
                    result.diagnostics.emplace_back(
                        "duplicate module name: "
                        + std::string(left->name));
                }
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    // ══════════════════════════════════════════════════════════════════════
    // MI.sampler-reference-contract
    //
    // 模块正文里的 <Name>Sampler 宏只由材质 TOML 的 [resources].samplers 生成
    // （BuildSamplerMacros）。模块必须用
    //   // @ulre sampler_reference <PresetName> [required|optional]
    // 自证依赖，否则"换了 provider / 材质漏配 sampler"只会在 GLSL 编译期以
    // 未定义宏的形式炸出来，且索引还可能被硬编码兜底掩盖成静默错值。
    // 本用例锁定两侧：正文出现的 *Sampler 必须被声明；声明必须是真实预设。
    // ══════════════════════════════════════════════════════════════════════
    GateResult RunSamplerReferenceContractCase()
    {
        GateResult result;
        result.name = "MI.sampler-reference-contract";

        GlslModuleRegistry registry;
        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(
                hgl::ToOSString(GetShaderLibraryPath()),
                &file_count, &error_count))
        {
            result.diagnostics.emplace_back(
                "module registry directory scan failed");
            result.passed = false;
            return result;
        }
        if (error_count != 0)
        {
            result.diagnostics.emplace_back(
                "module registry scan reported "
                + std::to_string(error_count) + " errors");
        }

        auto &lib = SamplerPresetLibrary::Instance();
        if (lib.GetCount() == 0
         && !lib.Load(hgl::ToOSString(GetShaderLibraryPath() + "/contract/sampler.toml")))
        {
            result.diagnostics.emplace_back("contract/sampler.toml load failed");
            result.passed = false;
            return result;
        }

        const auto declares = [](const GlslModuleDefinition &definition,
                                 const char *preset)
        {
            for (uint32_t i = 0;
                 i < definition.sampler_reference_requirement_count;
                 ++i)
            {
                const auto &requirement =
                    definition.sampler_reference_requirements[i];
                if (requirement.sampler_name
                 && std::strcmp(requirement.sampler_name, preset) == 0)
                    return true;
            }
            return false;
        };

        const auto is_identifier_char = [](const char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '_';
        };

        for (int i = 0; i < registry.GetCount(); ++i)
        {
            const auto *definition = registry.GetModuleByIndex(i);
            if (!definition || !definition->name || !definition->glsl_code)
                continue;

            // 1. 声明的预设必须真实存在于 contract/sampler.toml。
            for (uint32_t j = 0;
                 j < definition->sampler_reference_requirement_count;
                 ++j)
            {
                const char *name =
                    definition->sampler_reference_requirements[j].sampler_name;
                if (!name || lib.GetIndex(name) == ~0u)
                {
                    result.diagnostics.emplace_back(
                        std::string("module ") + definition->name
                        + " declares unknown sampler preset: "
                        + (name ? name : "<null>"));
                }
            }

            // 2. 正文中出现的每个 <Name>Sampler 都必须自我声明（跳过注释行）。
            const std::string text(definition->glsl_code);
            size_t pos = 0;
            while (pos < text.size())
            {
                const size_t eol = text.find('\n', pos);
                const size_t line_end =
                    eol == std::string::npos ? text.size() : eol;

                size_t first = pos;
                while (first < line_end
                    && (text[first] == ' ' || text[first] == '\t'))
                    ++first;

                const bool is_comment =
                    first + 1 < line_end
                 && text[first] == '/' && text[first + 1] == '/';

                size_t cursor = pos;
                while (cursor < line_end)
                {
                    if (!is_identifier_char(text[cursor]))
                    {
                        ++cursor;
                        continue;
                    }

                    size_t ident_end = cursor;
                    while (ident_end < line_end
                        && is_identifier_char(text[ident_end]))
                        ++ident_end;

                    constexpr size_t kSamplerSuffixLength = 7; // "Sampler"
                    const size_t length = ident_end - cursor;
                    if (!is_comment
                     && length > kSamplerSuffixLength
                     && text.compare(ident_end - kSamplerSuffixLength,
                                     kSamplerSuffixLength, "Sampler") == 0)
                    {
                        const std::string preset =
                            text.substr(cursor, length - kSamplerSuffixLength);
                        if (!declares(*definition, preset.c_str()))
                        {
                            result.diagnostics.emplace_back(
                                "module " + std::string(definition->name)
                                + " uses " + preset + "Sampler without "
                                  "@ulre sampler_reference " + preset);
                        }
                    }
                    cursor = ident_end;
                }

                pos = line_end + 1;
            }
        }

        // 3. 声明→材质校验器：required 缺失必须失败并报出名字，optional 放行。
        {
            ShaderCodeResourceManifest manifest;
            GlslModuleSamplerReferenceRequirement required_sampler;
            required_sampler.sampler_name = "Trilinear";
            required_sampler.required = true;
            if (manifest.sampler_references.Add(required_sampler) < 0)
                result.diagnostics.emplace_back(
                    "manifest sampler reference add failed");
            manifest.sampler_reference_count =
                static_cast<uint32_t>(manifest.sampler_references.GetCount());

            ResolvedMaterial material;
            const char *missing = nullptr;
            if (ValidateShaderCodeResourceManifestSamplerReferences(
                    manifest, material, &missing))
            {
                result.diagnostics.emplace_back(
                    "undeclared required sampler must fail validation");
            }
            else if (!missing || std::strcmp(missing, "Trilinear") != 0)
            {
                result.diagnostics.emplace_back(
                    "undeclared required sampler diagnostic name mismatch");
            }

            material.sampler_names.push_back("Trilinear");
            if (!ValidateShaderCodeResourceManifestSamplerReferences(
                    manifest, material))
            {
                result.diagnostics.emplace_back(
                    "declared required sampler must pass validation");
            }

            manifest.sampler_references[0].required = false;
            ResolvedMaterial empty_material;
            if (!ValidateShaderCodeResourceManifestSamplerReferences(
                    manifest, empty_material))
            {
                result.diagnostics.emplace_back(
                    "optional sampler must not require a material declaration");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    // ══════════════════════════════════════════════════════════════════════
    // MI.module-include-declaration-contract
    //
    // `@ulre uses` 是模块依赖的唯一真源：manifest 模块图、能力闭包、
    // 资源/采样器契约与模块冲突检查只沿 uses 边展开。而真正被编译进
    // 程序的是正文手写的 `#include`。两侧一旦不一致，include 进来的
    // 模块就整条从图里消失——它的 provides/require 不计入能力闭合、
    // 它声明的 texture/sampler 引用不被校验、它与其他模块的冲突不被
    // 检测（P4：provider 靠 include 隐式拉入依赖模块，正是这种隐形边）。
    //
    // 本用例锁定三件事：
    //   1. 扫描器只认行首 `#include`（注释掉的 include 不算依赖）；
    //   2. 全库加载零错误（Registry 已把「include 未声明」判为硬错误）；
    //   3. 关键隐式边必须在 dependencies 里可见，且真的进入 manifest 图。
    // ══════════════════════════════════════════════════════════════════════
    GateResult RunModuleIncludeDeclarationContractCase()
    {
        GateResult result;
        result.name = "MI.module-include-declaration-contract";

        // 1. 扫描器：行首 include 才计入，注释行/行尾不算。
        {
            static constexpr char kProbe[] =
                "// #include \"common/commented_out.glsl\"\n"
                "/* #include \"common/block_comment.glsl\" */\n"
                "#include \"module/descriptor/ubo/scene_ubo.glsl\"\n"
                "  #include <contract/descriptor_macros.glsl>\n"
                "float x; // #include \"common/trailing.glsl\"\n";

            hgl::AnsiStringList paths;
            const int collected = CollectGlslModuleIncludePaths(
                kProbe, static_cast<int>(sizeof(kProbe) - 1), paths);

            if (collected != 2 || paths.GetCount() != 2)
                result.diagnostics.emplace_back(
                    "include scanner must ignore commented includes (expected 2, got "
                    + std::to_string(collected) + ")");
            else if (!(paths[0] == "module/descriptor/ubo/scene_ubo.glsl")
                  || !(paths[1] == "contract/descriptor_macros.glsl"))
                result.diagnostics.emplace_back(
                    "include scanner collected unexpected paths");
        }

        GlslModuleRegistry registry;
        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(
                hgl::ToOSString(GetShaderLibraryPath()),
                &file_count, &error_count))
        {
            result.diagnostics.emplace_back(
                "module registry directory scan failed");
            result.passed = false;
            return result;
        }

        // 2. Registry 加载期已强制 include ⊆ uses，任何违例都是硬错误。
        if (error_count != 0)
            result.diagnostics.emplace_back(
                "module registry scan reported "
                + std::to_string(error_count)
                + " errors (undeclared #include dependency?)");

        const auto depends_on = [&registry](const char *module_name,
                                            const char *dependency_name)
        {
            const auto *definition = registry.FindByName(module_name);
            if (!definition)
                return false;

            for (uint32_t i = 0; i < definition->dependency_count; ++i)
            {
                const char *name = definition->dependencies[i].module_name;
                if (name && std::strcmp(name, dependency_name) == 0)
                    return true;
            }
            return false;
        };

        // 3. P4 的核心隐式边：provider 正文 include 拉入的模块必须显式声明。
        struct ExpectedEdge
        {
            const char *module_name;
            const char *dependency_name;
            const char *why;
        };

        static const ExpectedEdge kExpectedEdges[] =
        {
            { "ambient_light_sky", "sky_atmosphere",
              "sky ambient provider pulls the atmosphere model in by #include" },
            { "surface_sky_minimal", "sky_atmosphere",
              "sky surface provider pulls the atmosphere model in by #include" },
            { "sky_atmosphere", "scene_ubo",
              "atmosphere reads main light / sky rows from the global tables" },
            { "ambient_light_ibl", "bindless_textures",
              "IBL provider samples env maps through bindless textures" },
            { "surface_material", "ntb_interface",
              "surface provider branches on the NTB provider contract" },
            { "ntb_tangent_vbo_normalmap", "ntb_orthonormal",
              "NTB provider falls back to ntb_orthonormal on UV-degenerate frames" },
            { "surface_interface", "descriptor_macros",
              "surface interface depends on the standard descriptor macros" },
            { "scene_ubo", "descriptor_macros",
              "scene data declarations depend on the standard descriptor macros" },
            { "entity_gpu_cull", "entity_gpu",
              "GPU cull reads the entity row mirror" },
            { "entity_gpu_cull", "render_item_resolve",
              "GPU cull resolves render items through RenderItemResolve" },
        };

        for (const ExpectedEdge &edge : kExpectedEdges)
        {
            if (registry.FindByName(edge.module_name) == nullptr)
            {
                result.diagnostics.emplace_back(
                    std::string("module missing from registry: ") + edge.module_name);
                continue;
            }

            if (!depends_on(edge.module_name, edge.dependency_name))
                result.diagnostics.emplace_back(
                    std::string("module ") + edge.module_name
                    + " must declare @ulre uses " + edge.dependency_name
                    + " (" + edge.why + ")");
        }

        // 4. 声明必须真的进图：以 sky_minimal_surface 为根构建 manifest，
        //    sky_atmosphere 必须出现在 code_module_names 里（P4 之前它是
        //    被 include 编进去、却在图里完全不可见的模块）。
        {
            const char *roots[] = { "surface_sky_minimal" };
            ShaderCodeResourceManifest manifest{};
            if (!BuildShaderCodeResourceManifest(
                    roots, 1, manifest, &registry))
            {
                result.diagnostics.emplace_back(
                    std::string("sky_minimal_surface manifest failed: ")
                    + GetShaderCodeResourceManifestErrorName(manifest.error));
            }
            else
            {
                bool found_atmosphere = false;
                bool found_scene_ubo = false;
                for (uint32_t i = 0; i < manifest.code_module_count; ++i)
                {
                    const char *name = manifest.code_module_names[i];
                    if (!name)
                        continue;
                    if (std::strcmp(name, "sky_atmosphere") == 0)
                        found_atmosphere = true;
                    else if (std::strcmp(name, "scene_ubo") == 0)
                        found_scene_ubo = true;
                }

                if (!found_atmosphere)
                    result.diagnostics.emplace_back(
                        "manifest closure of sky_minimal_surface must contain "
                        "sky_atmosphere (forced #include must be a declared edge)");
                if (!found_scene_ubo)
                    result.diagnostics.emplace_back(
                        "manifest closure of sky_minimal_surface must contain scene_ubo");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }


    // ══════════════════════════════════════════════════════════════════════
    // SPV.generated-interface
    //
    // 生成接口的**反射门**：SPIR-V 是接口的唯一权威。
    //
    // 生成器在编译前会自述「我生成了什么」，而自述可以自洽却与真正编出的二进
    // 制不一致——唯一能证伪它的是编译器。本用例把整张覆盖矩阵真的编译成
    // SPIR-V 并反射，按 Vulkan 的链接规则校验：
    //   ① 两 stage 都必须编译成功。此前这 16 个变体的 GLSL 在门里从未被编译过，
    //      生成器发错一行就能让整套 shader 只在跑 demo 时才炸；
    //   ② 片元每个 stage input 必须在 mesh 的 stage output 里找到同 location 同
    //      类型的对应项 —— 这是管线创建时驱动会硬拒的条件；
    //   ③ 前向着色必须写至少一个颜色附件；纯深度/阴影用途必须一个都不写；
    //   ④ 两 stage 的 push constant 末端必须等于 RootAddresses 的 CPU 侧大小。
    // ①②③④ 全部来自二进制，完全不看生成器怎么描述自己。
    GateResult RunGeneratedInterfaceReflectionCase()
    {
        GateResult result;
        result.name = "SPV.generated-interface";

        std::string preset_diagnostic;
        if (!EnsureSamplerPresetsLoaded(preset_diagnostic))
        {
            result.diagnostics.emplace_back(preset_diagnostic);
            result.passed = false;
            return result;
        }

        // RootAddresses = 10 个 uint64 地址 + 2 个 uint 标量（camera_row + 显式
        // padding）。这里刻意写死数值（与 `PM.compute-release-cache` 的 112 同理）：
        // 若两侧一起漂，本门才会失败。
        const uint32_t kRootAddressesPCBytes = 88;

        // 诊断通道自检：故意编一段非法 GLSL，必须同时得到 nullptr 与非空报错。
        // 这道断言保护的是「本用例下方那些编译失败诊断有内容」这件事本身 ——
        // 没有它，编译器一旦退回「静默 false」，本门就只会输出「没编过」。
        {
            hgl::AnsiString broken_log;
            if (hgl::graph::CompileShader(
                    uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                    "layout(location=0) out vec4 c;\nvoid main() { c = not_declared; }\n",
                    &broken_log) != nullptr)
            {
                result.diagnostics.emplace_back(
                    "invalid GLSL compiled successfully (diagnostic channel broken)");
            }
            else if (broken_log.IsEmpty())
            {
                result.diagnostics.emplace_back(
                    "compile failure carried no compiler log (diagnostic channel broken)");
            }
        }

        // 完整性：每个变体都必须走到「两 stage 都反射成功」这一步。
        // 任何 `continue` 提前退出都会留下诊断，这里再加一道计数，
        // 防止将来新增的跳过分支把变体静默漏掉。
        uint32_t reflected_variant_count = 0;

        std::vector<std::string> fixed_mesh_catalog;
        {
            std::string catalog_diagnostic;
            if (!LoadFixedMeshCatalog(fixed_mesh_catalog, catalog_diagnostic))
                result.diagnostics.emplace_back(catalog_diagnostic);
        }

        for (const ReflectionCoverageVariant &variant : kReflectionCoverageVariants)
        {
            std::string build_diagnostic;
            FixedFragmentSource fixed_source;
            hgl::AnsiString mesh_options_text;
            hgl::AnsiString common_options_text;
            std::unique_ptr<ShaderBuildContext> ctx =
                BuildReflectionCoverageVariant(
                    variant, build_diagnostic, &fixed_source,
                    &mesh_options_text, &common_options_text);
            if (!ctx)
            {
                result.diagnostics.emplace_back(build_diagnostic);
                continue;
            }

            // 网格主着色器里有 `#include "generated/mesh_options.glsl"`：先在
            // 片段 options 已落盘的那个根下把它写出来，下面手写编译才有第二根可读。
            if (!mesh_options_text.IsEmpty()
             && !hgl::graph::mtl::WriteMeshOptionsSource(
                    fixed_source.options_root, mesh_options_text))
            {
                result.diagnostics.emplace_back(
                    std::string(variant.variant_slug)
                    + ": cannot write generated/mesh_options.glsl | "
                    + hgl::graph::mtl::GetLastDomainShaderBuildBackendError());
                continue;
            }

            // 网格主着色器同时也 include `generated/common_options.glsl`
            //（阶段间 varying 开关）：它与片段共用一份，必须落在同一个根。
            if (!common_options_text.IsEmpty()
             && !hgl::graph::mtl::WriteInterStageVaryingOptionsSource(
                    fixed_source.options_root, common_options_text))
            {
                result.diagnostics.emplace_back(
                    std::string(variant.variant_slug)
                    + ": cannot write generated/common_options.glsl | "
                    + hgl::graph::mtl::GetLastDomainShaderBuildBackendError());
                continue;
            }

            const std::string label = variant.variant_slug;

            hgl::graph::mtl::ShaderReflectionData reflection[2];
            const uint32_t stage_bits[2] =
            {
                VK_SHADER_STAGE_MESH_BIT_EXT,
                VK_SHADER_STAGE_FRAGMENT_BIT
            };
            const hgl::graph::ShaderStage stage_enums[2] =
            {
                hgl::graph::ShaderStage::Mesh,
                hgl::graph::ShaderStage::Fragment
            };
            const char *const stage_names[2] = { "mesh", "fragment" };

            bool reflected[2] = { false, false };

            for (int i = 0; i < 2; ++i)
            {
                // 编译由本门自己做（而不是让 BuildReflectionCoverageVariant 顺手 finalize），
                // 这样「哪个 stage 的哪一步失败」能落到诊断里：
                // `FinalizeShaderBuildContext` 对空源码是静默 false，看不出根因。
                ShaderCreateInfo *si = ctx->GetStageShader(stage_enums[i]);
                if (!si)
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i] + ": stage missing");
                    continue;
                }

                if (si->GetFinalGLSL().empty())
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i]
                        + ": generated GLSL is empty");
                    continue;
                }

                // 每个变体的网格阶段都必须能在固定源目录里找到同一份文本。
                // 6.5 删掉 meshgen 之后没有生成器兜底：目录里缺这一份，该变体
                // 就没有 mesh 阶段可编译。这条断言把「矩阵新增了一个形状但没人
                // 捕获固定源」变成红灯，而不是等到运行时才发现某个域画不出来。
                if (i == 0 && !FixedMeshCatalogHas(fixed_mesh_catalog, si->GetFinalGLSL()))
                {
                    result.diagnostics.emplace_back(
                        label + ": mesh stage has no source in "
                        "ShaderLibrary/topology 下没有对应形状 master"
                        "（形状 master 需人工按形状新增，见 plan.md 阶段七-2）");
                }

                if (!si->HasSPVData())
                {
                    // 带诊断的编译入口：glslang 的原始报错（含行号与生成的 GLSL 片段）
                    // 直接进诊断。没有它，失败信息只剩「没编过」—— SPVData 一被释
                    // 放日志就取不回来了。
                    //
                    // 片段走生产那条编译路径（第二 include 根由后端管）：固定源里含
                    // `#include "generated/frag_options.glsl"`，手写编译没有那个根就必然
                    // 编不过，而复制一份根管理只会等它慢慢漂移。
                    hgl::AnsiString compile_diagnostic;
                    const bool compiled =
                        i == 1
                            ? CompileFixedFragmentSource(*ctx, fixed_source, compile_diagnostic)
                            : CompileFixedMeshSource(*ctx, fixed_source, compile_diagnostic);
                    if (!compiled)
                    {
                        result.diagnostics.emplace_back(
                            label + " " + stage_names[i]
                            + ": generated GLSL failed to compile: "
                            + (compile_diagnostic.c_str() ? compile_diagnostic.c_str() : ""));
                        continue;
                    }
                }

                if (!si->HasSPVData() || si->GetSPVSize() == 0)
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i]
                        + ": no SPIR-V after compile");
                    continue;
                }

                hgl::ValueArray<uint8_t> flat_storage;
                ShaderFlatView view{};
                if (!ReflectSPVData(
                        si->GetSPVData(), si->GetSPVSize(), stage_bits[i],
                        "main", flat_storage, view)
                 || !reflection[i].Extract(view))
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i]
                        + ": SPIR-V reflection failed");
                    continue;
                }

                reflected[i] = true;

                if (reflection[i].push_constants.IsEmpty())
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i]
                        + ": push constant block missing");
                    continue;
                }

                uint32_t pc_end = 0;
                if (!reflection[i].GetPushConstantEnd(pc_end)
                 || pc_end != kRootAddressesPCBytes)
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i]
                        + ": push constant end " + std::to_string(pc_end)
                        + " != " + std::to_string(kRootAddressesPCBytes));
                }
            }

            if (!reflected[0] || !reflected[1])
                continue;

            ++reflected_variant_count;

            // ② 片元 input ⊆ mesh output（同 location、同 basetype、同 vec_size）
            for (int i = 0; i < reflection[1].stage_inputs.GetCount(); ++i)
            {
                const hgl::graph::mtl::ShaderReflectionAttribute &fin =
                    reflection[1].stage_inputs[i];

                const hgl::graph::mtl::ShaderReflectionAttribute *match = nullptr;
                for (int j = 0; j < reflection[0].stage_outputs.GetCount(); ++j)
                {
                    if (reflection[0].stage_outputs[j].location == fin.location)
                    {
                        match = &reflection[0].stage_outputs[j];
                        break;
                    }
                }

                if (!match)
                {
                    result.diagnostics.emplace_back(
                        label + ": fragment input #"
                        + std::to_string(int(fin.location)) + " ("
                        + fin.name + ") has no matching mesh output");
                    continue;
                }

                if (match->basetype != fin.basetype
                 || match->vec_size != fin.vec_size
                 || match->col_count != fin.col_count)
                {
                    result.diagnostics.emplace_back(
                        label + ": interface type mismatch at #"
                        + std::to_string(int(fin.location))
                        + " mesh(" + match->name + ",base="
                        + std::to_string(int(match->basetype)) + ",vec="
                        + std::to_string(int(match->vec_size)) + ",col="
                        + std::to_string(int(match->col_count)) + ") fragment("
                        + fin.name + ",base=" + std::to_string(int(fin.basetype))
                        + ",vec=" + std::to_string(int(fin.vec_size)) + ",col="
                        + std::to_string(int(fin.col_count)) + ")");
                }
            }

            // ③ 输出附件：前向着色写颜色，纯深度/阴影一个都不写
            const bool expects_color =
                variant.purpose == ShaderProgramPurpose::ForwardColor;
            const int color_outputs = reflection[1].stage_outputs.GetCount();

            if (expects_color && color_outputs <= 0)
                result.diagnostics.emplace_back(
                    label + ": forward purpose wrote no color attachment");
            else if (!expects_color && color_outputs != 0)
                result.diagnostics.emplace_back(
                    label + ": depth purpose wrote "
                    + std::to_string(color_outputs) + " color attachment(s)");
        }

        const uint32_t expected_variant_count =
            static_cast<uint32_t>(sizeof(kReflectionCoverageVariants)
                                  / sizeof(kReflectionCoverageVariants[0]));
        if (reflected_variant_count != expected_variant_count)
            result.diagnostics.emplace_back(
                "only " + std::to_string(reflected_variant_count) + " of "
                + std::to_string(expected_variant_count)
                + " structure-dump variants reached SPIR-V reflection");

        result.passed = result.diagnostics.empty();
        return result;
    }

} // namespace gate
