// 阶段 5 拆分自 ShaderResourceSchemaRegressionGate.cpp：组 pipeline 的 case 实现。
#include "GateTestSupport.h"

namespace gate
{
    GateResult RunUnifiedMaterialBaselineCase()
    {
        GateResult result;
        result.name = "T.unified-material-baseline";

        ResolvedMaterial pure_color{};
        if (!BuildResolvedMaterialForDomain(MaterialDomain::UnlitSolid, pure_color))
        {
            result.diagnostics.emplace_back("canonical UnlitSolid must exist");
        }
        else if (pure_color.material_private_data != GlobalSSBOType::EmissiveSurface
               || pure_color.vertex_semantic_requirements.GetCount() != 1)
            result.diagnostics.emplace_back("canonical PureColor contract is not semantic-only");

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunMaterialOutputContractCase()
    {
        GateResult result;
        result.name = "V1.material-output-contract";

        ResolvedMaterial lit{};
        if (!BuildResolvedMaterialForDomain(MaterialDomain::StandardOpaque, lit))
            {
                result.diagnostics.emplace_back(
                    "domain definition unavailable for output contract");
            }
            else
            {
                const GeometryVertexFormat geometry{
                    {VertexSemantic::Position, VF_V3F},
                    {VertexSemantic::TexCoord, VF_V2F},
                    {VertexSemantic::Normal, VF_V3F}
                };
                const auto build = [&](
                    const ShaderProgramPurpose purpose,
                    const bool alpha_test,
                    const bool dither,
                    std::string *out_options = nullptr)
                {
                    ResolvedMaterial selected = lit;
                    MaterialBuildRequest request{};
                    request.recipe.domain = selected.domain;
                    request.recipe.render_state_overrides.has_alpha_test = true;
                    request.recipe.render_state_overrides.alpha_test = alpha_test;
                    request.recipe.render_state_overrides.has_dither = true;
                    request.recipe.render_state_overrides.dither = dither;
                    request.recipe.render_state_overrides.has_pipeline_config = true;
                    request.geometry_vertex_format = &geometry;
                    request.defer_finalize = true;
                    request.shader_program_purpose = purpose;
                    const bool depth_purpose =
                        purpose == ShaderProgramPurpose::DepthOnly
                     || purpose == ShaderProgramPurpose::ShadowDepth;
                    const bool masked =
                        alpha_test || dither;
                    const PassProfileID profile_id = depth_purpose
                        ? (masked ? PassProfileID::ShadowCasterMasked
                                  : PassProfileID::ShadowCasterOpaque)
                        : PassProfileID::ForwardLitShadowedAO;
                    const ScenePassProfile profile = depth_purpose
                        ? MakeShadowCasterProfile()
                        : MakeIdentityForwardLitProfile();
                    PassProfileValidationDiagnostic template_diagnostic{};
                    if (!ResolveScenePassProfileRequest(
                            profile_id, hgl::graph::ShaderStage::Fragment,
                            profile, request.render_template_request,
                            template_diagnostic))
                    {
                        result.diagnostics.emplace_back(std::string("ResolveScenePassProfileRequest failed for ") + GetPassProfileName(profile_id) + " err=" + std::to_string(int(template_diagnostic.error)));
                        return std::unique_ptr<ShaderBuildContext>();
                    }
                    if (!AppendMaterialPassProfileRoots(
                            selected, request.render_template_request))
                    {
                        result.diagnostics.emplace_back("AppendMaterialPassProfileRoots failed");
                        return std::unique_ptr<ShaderBuildContext>();
                    }
                    // 本用例断言的是输出契约 / program link 形状，片段文本不参与。
                    // 片段源照样按领域固定源填进请求——不填会被
                    // MaterialProgramAssembler 直接拒收（片段拼装链已删除）。
                    FixedFragmentSource fixed;
                    if (!ResolveFixedFragmentSource(selected, request, fixed))
                    {
                        result.diagnostics.emplace_back(
                            std::string("ResolveFixedFragmentSource failed for ")
                            + GetPassProfileName(profile_id));
                        return std::unique_ptr<ShaderBuildContext>();
                    }
                    FixedStageInputs fixed_stage;
                    fixed_stage.fragment_source = fixed.source.c_str();
                    fixed_stage.fragment_interface_hash = fixed.interface_hash;
                    fixed_stage.fragment_options_digest = fixed.options_digest;
                    if (out_options)
                    {
                        // 覆盖语义（alpha test / cutoff / dither）**不在**固定主
                        // 着色器文本里，而全部落在 generated/frag_options.glsl 的
                        // #define 上。要让本用例仍能断言它，就必须走生产同一条
                        // 发射路径取值（手抄宏表会随发射器漂移）。
                        hgl::AnsiString options_text;
                        if (!EmitDomainOptionsForDefinition(
                                selected, request, fixed.domain, purpose,
                                options_text))
                        {
                            result.diagnostics.emplace_back(
                                std::string("options emission failed for ")
                                + GetPassProfileName(profile_id));
                        }
                        else if (options_text.Length() > 0)
                        {
                            out_options->assign(
                                options_text.c_str(),
                                static_cast<size_t>(options_text.Length()));
                        }
                    }
                    auto ctx = std::unique_ptr<ShaderBuildContext>(
                        AssembleMaterialProgram(
                            nullptr, request, selected, nullptr, fixed_stage));
                    if (!ctx)
                    {
                        const hgl::AnsiString &err = GetLastMaterialProgramAssemblyError();
                        result.diagnostics.emplace_back(std::string("AssembleMaterialProgram returned null: ") + (err.IsEmpty() ? "unknown error" : err.c_str()));
                    }
                    else if (!ctx->HasProgramLink())
                    {
                        result.diagnostics.emplace_back(std::string("HasProgramLink false for ") + GetPassProfileName(profile_id));
                    }
                    return ctx;
                };

                std::string masked_depth_options;
                std::string dither_shadow_options;
                const auto opaque = build(
                    ShaderProgramPurpose::ForwardColor,
                    false,
                    false);
                const auto transparent = build(
                    ShaderProgramPurpose::ForwardColor,
                    false,
                    false);
                const auto depth = build(
                    ShaderProgramPurpose::DepthOnly,
                    false,
                    false);
                const auto shadow = build(
                    ShaderProgramPurpose::ShadowDepth,
                    false,
                    false);
                const auto masked_depth = build(
                    ShaderProgramPurpose::DepthOnly,
                    true,
                    false,
                    &masked_depth_options);
                const auto dither_shadow = build(
                    ShaderProgramPurpose::ShadowDepth,
                    false,
                    true,
                    &dither_shadow_options);

                if (!opaque || !opaque->HasProgramLink()) result.diagnostics.emplace_back("opaque failed");
                if (!transparent || !transparent->HasProgramLink()) result.diagnostics.emplace_back("transparent failed");
                if (!depth || !depth->HasProgramLink()) result.diagnostics.emplace_back("depth failed");
                if (!shadow || !shadow->HasProgramLink()) result.diagnostics.emplace_back("shadow failed");
                if (!masked_depth || !masked_depth->HasProgramLink()) result.diagnostics.emplace_back("masked_depth failed");
                if (!dither_shadow || !dither_shadow->HasProgramLink()) result.diagnostics.emplace_back("dither_shadow failed");

                if (!opaque || !transparent || !depth || !shadow
                 || !masked_depth || !dither_shadow
                 || !opaque->HasProgramLink()
                 || !transparent->HasProgramLink()
                 || !depth->HasProgramLink()
                 || !shadow->HasProgramLink()
                 || !masked_depth->HasProgramLink()
                 || !dither_shadow->HasProgramLink())
                {
                    result.diagnostics.emplace_back(
                        "production output contract builds failed");
                }
                else
                {
                    // 安全取 stage GLSL：depth/shadow 变体本无 Fragment stage，
                    // GetStageShader 可能返回悬挂指针——先 has_fragment() 守卫 + 空指针检查，
                    // 并用值拷贝避免悬挂引用。
                    const auto safe_fs = [](const ShaderBuildContext *ctx) -> std::string
                    {
                        if (!ctx || !ctx->has_fragment())
                            return {};
                        const auto *stage = ctx->GetStageShader(ShaderStage::Fragment);
                        return stage ? stage->GetFinalGLSL() : std::string{};
                    };
                    const auto safe_ms = [](const ShaderBuildContext *ctx) -> std::string
                    {
                        if (!ctx || !ctx->has_mesh())
                            return {};
                        const auto *stage = ctx->GetStageShader(ShaderStage::Mesh);
                        return stage ? stage->GetFinalGLSL() : std::string{};
                    };

                    const std::string opaque_fs = safe_fs(opaque.get());
                    const std::string depth_fs = safe_fs(depth.get());
                    const std::string depth_ms = safe_ms(depth.get());
                    const std::string masked_depth_fs = safe_fs(masked_depth.get());
                    const std::string masked_depth_ms = safe_ms(masked_depth.get());
                    const std::string dither_shadow_fs = safe_fs(dither_shadow.get());
                    // 结构化断言：opaque 必有 Fragment（强检查）；
                    // depth/masked/dither 变体仅当 has_fragment() 时检查内容，
                    // 否则跳过对应子项（避免解引用悬挂指针 + 假阳性）。
                    const auto has = [](const ShaderBuildContext *ctx, ShaderStage s)
                    { return ctx && (ctx->GetShaderStage() & uint32_t(s)); };
                    const auto lacks = [](std::string_view src, const char *needle)
                    { return src.find(needle) == std::string::npos; };
                    const auto contains = [](std::string_view src, const char *needle)
                    { return src.find(needle) != std::string::npos; };

                    bool mismatch = false;
                    // opaque：ForwardColor，必有 Fragment
                    if (lacks(opaque_fs, "layout(location=0) out vec4 outColor;")
                     || lacks(opaque_fs, "WriteMaterialOutput(")
                     || contains(opaque_fs, "ULRE_OUTPUT_CONTRACT"))
                        mismatch = true;
                    // depth：DepthOnly，期望无 Fragment 输出相关符号（若有 Fragment 才查）
                    if (has(depth.get(), ShaderStage::Fragment))
                    {
                        if (contains(depth_fs, "outColor")
                         || contains(depth_fs, "WriteMaterialOutput(")
                         || contains(depth_fs, "ULRE_OUTPUT_CONTRACT")
                         || contains(depth_fs, "EvalAlpha("))
                            mismatch = true;
                    }
                    // depth：DepthOnly 的 mesh 源不应含 fragDataIndexID（depth 无需 data index）
                    if (has(depth.get(), ShaderStage::Mesh)
                     && contains(depth_ms, "fragDataIndexID"))
                        mismatch = true;
                    // masked_depth：alpha test，固定主着色器必须真的走覆盖判定
                    // 通路（EvalAlpha + HGLApplyAlpha），且不得引入光照合成。
                    if (has(masked_depth.get(), ShaderStage::Fragment)
                     && (lacks(masked_depth_fs, "EvalAlpha(")
                      || lacks(masked_depth_fs, "HGLApplyAlpha(")
                      || contains(masked_depth_fs, "EvalLighting(")))
                        mismatch = true;
                    // 覆盖宏不在片段文本里（固定源无条件 include
                    // generated/frag_options.glsl），因此改断 options 文本：
                    // alpha test 必须真的发射 HGL_ALPHA_TEST + cutoff。
                    if (masked_depth_options.find("#define HGL_ALPHA_TEST 1")
                            == std::string::npos
                     || masked_depth_options.find("#define HGL_ALPHA_CUTOFF ")
                            == std::string::npos)
                        mismatch = true;
                    // masked_depth：mesh 源不应含 fragWorldPos（depth 无需世界坐标输出）
                    if (has(masked_depth.get(), ShaderStage::Mesh)
                     && contains(masked_depth_ms, "fragWorldPos"))
                        mismatch = true;
                    // dither_shadow：dither 与 alpha test 共用同一份 masked 主着色器，
                    // 区别只在 options —— 必须是 HGL_ALPHA_DITHER（而非 ALPHA_TEST）。
                    if (has(dither_shadow.get(), ShaderStage::Fragment)
                     && (lacks(dither_shadow_fs, "HGLApplyAlpha(")
                      || contains(dither_shadow_fs, "EvalLighting(")))
                        mismatch = true;
                    if (dither_shadow_options.find("#define HGL_ALPHA_DITHER 1")
                            == std::string::npos
                     || dither_shadow_options.find("#define HGL_ALPHA_TEST ")
                            != std::string::npos)
                        mismatch = true;

                    if (mismatch)
                    {
                        result.diagnostics.emplace_back(
                            "production fragment output generation mismatch");
                    }

                    const auto has_material_resource =
                        [](const ShaderBuildContext &spec)
                    {
                        // 运行时数据行需求由编译期直判信号承载（2026-10-11 S1）。
                        return spec.RequiresRecipeRuntimeRows();
                    };
                    // sky 已 BDA 化（不在任何 shader 的资源声明里）⇒ 原「depth 不得携带
                    // sky 资源」的检查已结构性成立，随 SkyInfo 语义退场一并删除。
                    if (has_material_resource(*depth)
                     || !has_material_resource(*masked_depth))
                    {
                        result.diagnostics.emplace_back(
                            "depth coverage resource pruning mismatch");
                    }

                    if (!(opaque->GetProgramLink().BuildKey()
                            == transparent->GetProgramLink().BuildKey())
                     || depth->GetProgramLink().BuildKey()
                            == shadow->GetProgramLink().BuildKey()
                     || opaque->GetProgramLink().BuildKey()
                            == depth->GetProgramLink().BuildKey()
                     || depth->GetProgramLink().BuildKey()
                            == masked_depth->GetProgramLink().BuildKey())
                    {
                        result.diagnostics.emplace_back(
                            "output purpose ProgramKey identity mismatch");
                    }
                }
            }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunPrimitiveVariantPurposeCase()
    {
        GateResult result;
        result.name = "V2.primitive-variant-purpose";

        MaterialRecipe surface_recipe{};
        surface_recipe.recipe_name = "Surface";
        MaterialRecipe depth_recipe{};
        depth_recipe.recipe_name = "Depth";
        MaterialRecipe shadow_recipe{};
        shadow_recipe.recipe_name = "Shadow";
        const PrimitiveVariant variants[] =
        {
            {
                &surface_recipe,
                PrimitiveVariantPurpose::Surface,
                0
            },
            {
                &depth_recipe,
                PrimitiveVariantPurpose::DepthOnly,
                0
            },
            {
                &shadow_recipe,
                PrimitiveVariantPurpose::ShadowCaster,
                0
            }
        };
        const PrimitiveAsset asset(
            nullptr,
            variants,
            static_cast<uint32_t>(std::size(variants)));

        const PrimitiveVariant *surface =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::Surface, 2);
        const PrimitiveVariant *depth =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::DepthOnly, 0);
        const PrimitiveVariant *shadow =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::ShadowCaster, 0);
        const PrimitiveVariant *fallback =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::Picking, 2);
        if (!surface
         || surface->material_recipe != &surface_recipe
         || !depth
         || depth->material_recipe != &depth_recipe
         || !shadow
         || shadow->material_recipe != &shadow_recipe
         || !fallback
         || fallback->material_recipe != &surface_recipe)
        {
            result.diagnostics.emplace_back(
                "primitive variants were not resolved by purpose");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunUnifiedForwardSkeletonCase()
    {
        GateResult result;
        result.name = "V3.unified-forward-skeleton";

        struct ExpectedSource
        {
            MaterialDomain domain;
            const char *source_module;
        };
        static const ExpectedSource expected_sources[] =
        {
            {MaterialDomain::VertexPalette, "material_source_vertex_color"},
            {MaterialDomain::VertexColor, "material_source_vertex_color"},
            {MaterialDomain::VertexLuminance, "material_source_luminance"},
            {MaterialDomain::UnlitTexture, "material_source_texture"},
            {MaterialDomain::DebugNormal, "material_source_debug_normal"},
            {MaterialDomain::StandardOpaque, "material_source_pbr_surface"},
            {MaterialDomain::UnlitSolid, "material_source_unlit"},
            {MaterialDomain::Text2D, "material_source_text_gpu"}
        };

        for (const ExpectedSource &expected : expected_sources)
        {
            ResolvedMaterial definition{};
            if (!BuildResolvedMaterialForDomain(expected.domain, definition)
             || !definition.material_source_module_name
             || std::strcmp(
                    definition.material_source_module_name,
                    expected.source_module) != 0)
            {
                result.diagnostics.emplace_back(
                    std::string("domain did not use unified skeleton: ")
                    + GetMaterialDomainName(expected.domain));
            }
        }

        ResolvedMaterial lit{};
        ResolvedMaterial unlit{};
        if (!BuildResolvedMaterialForDomain(MaterialDomain::StandardOpaque, lit)
         || !BuildResolvedMaterialForDomain(MaterialDomain::UnlitTexture, unlit))
        {
            result.diagnostics.emplace_back(
                "unified skeleton build definitions unavailable");
        }
        else
        {
            const GeometryVertexFormat lit_geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2F},
                {VertexSemantic::Normal, VF_V3F}
            };
            const GeometryVertexFormat unlit_geometry{
                {VertexSemantic::Position, VF_V2F},
                {VertexSemantic::TexCoord, VF_V2F}
            };
            const auto build = [](
                const ResolvedMaterial &definition,
                const GeometryVertexFormat &geometry,
                const PassProfileID profile_id,
                const ScenePassProfile &scene_profile)
            {
                MaterialBuildRequest request{};
                request.recipe.domain = definition.domain;
                request.geometry_vertex_format = &geometry;
                request.defer_finalize = true;
                PassProfileValidationDiagnostic template_diagnostic{};
                if (!ResolveScenePassProfileRequest(
                        profile_id, hgl::graph::ShaderStage::Fragment,
                        scene_profile, request.render_template_request,
                        template_diagnostic)
                 || !AppendMaterialPassProfileRoots(
                        definition, request.render_template_request))
                    return std::unique_ptr<ShaderBuildContext>();
                // 本用例断言的是统一骨架的 program link 形状，片段文本不参与。
                // 片段源照样按领域固定源填进请求——不填会被
                // MaterialProgramAssembler 直接拒收（片段拼装链已删除）。
                FixedFragmentSource fixed;
                if (!ResolveFixedFragmentSource(definition, request, fixed))
                    return std::unique_ptr<ShaderBuildContext>();
                FixedStageInputs fixed_stage;
                fixed_stage.fragment_source = fixed.source.c_str();
                fixed_stage.fragment_interface_hash = fixed.interface_hash;
                fixed_stage.fragment_options_digest = fixed.options_digest;
                return std::unique_ptr<ShaderBuildContext>(
                    AssembleMaterialProgram(
                        nullptr, request, definition, nullptr, fixed_stage));
            };
            const auto lit_build = build(
                lit, lit_geometry, PassProfileID::ForwardLitShadowedAO,
                MakeIdentityForwardLitProfile());
            const auto unlit_build = build(
                unlit, unlit_geometry, PassProfileID::ForwardUnlit,
                MakeForwardUnlitProfile());
            if (!lit_build || !unlit_build)
            {
                result.diagnostics.emplace_back(
                    "unified forward skeleton build failed");
            }
            else
            {
                const std::string &lit_fs =
                    lit_build->GetStageShader(
                        ShaderStage::Fragment)->GetFinalGLSL();
                const std::string &unlit_fs =
                    unlit_build->GetStageShader(
                        ShaderStage::Fragment)->GetFinalGLSL();
                if (lit_fs.find(
                        "#include \"module/provider/lighting_model/lighting_model_pbr.glsl\"")
                        == std::string::npos
                 || lit_fs.find(
                        "#include \"module/provider/output_policy/output_policy_forward.glsl\"")
                        == std::string::npos
                 || lit_fs.find(
                        "EvalSurface(si, materialDataIndex)")
                        == std::string::npos
                 || lit_fs.find("EvalPassEnvironment(")
                        == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "unified Lit module schedule mismatch");
                }
                // 「有无场景光照」在新架构里不再是宏（旧的
                // `#define HGL_USE_SCENE_LIGHTING 0/1` 属于已删除的生成器 define
                // 文档），而是**结构性的**：Lit 走 forward_pbr + forward_lighting
                // 合成器，无光照领域走 flat_lighting 且完全不含光照模块。
                if (unlit_fs.find(
                        "#include \"module/provider/output_policy/output_policy_flat.glsl\"")
                        == std::string::npos
                 || unlit_fs.find(
                        "#include \"module/interface/surface_interface.glsl\"")
                        == std::string::npos
                 || unlit_fs.find(
                        "EvalSurface(si, materialDataIndex)")
                        == std::string::npos
                 || unlit_fs.find("EvalPassEnvironment(")
                        == std::string::npos
                 || unlit_fs.find(
                        "#include \"module/provider/output_policy/output_policy_forward.glsl\"")
                        != std::string::npos
                 || unlit_fs.find(
                        "#include \"module/provider/lighting_model/lighting_model_pbr.glsl\"")
                        != std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "unified Unlit module schedule mismatch");
                }
                // AO 槽必须真的被消费：provider 的 #include 排在 compositor 之前
                // （否则 GetAmbientOcclusion 未声明即调用）。此前该槽只 include
                // 不调用，属死契约。
                // 注：调用点在 module/provider/output_policy/output_policy_forward.glsl 文件内部，
                // 不会出现在组装后的模板正文里，故此处不查调用点文本。
                // 「是否调用」已不再由宏表达（HGL_USE_AMBIENT_OCCLUSION_PROVIDER
                // 已删除，forward_lighting 无条件接线），故这里只钉 include 顺序。
                const size_t ao_include =
                    lit_fs.find("#include \"module/provider/ambient_occlusion/ambient_occlusion_identity.glsl\"");
                const size_t output_include =
                    lit_fs.find("#include \"module/provider/output_policy/output_policy_forward.glsl\"");
                if (ao_include == std::string::npos
                 || output_include == std::string::npos
                 || ao_include > output_include)
                {
                    result.diagnostics.emplace_back(
                        "ambient occlusion provider slot is not wired");
                }
                // LightingInput.ambientColor 已删：全树零读取点的死字段，
                // 残留会让"天空色算两遍"复活。
                if (lit_fs.find("ambientColor") != std::string::npos
                 || unlit_fs.find("ambientColor") != std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "dead LightingInput.ambientColor reappeared");
                }
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunUnifiedPureColorFragmentCase()
    {
        GateResult result;
        result.name = "V.unified-purecolor-fragment";

        ResolvedMaterial pure_color{};
        if (!BuildResolvedMaterialForDomain(MaterialDomain::UnlitSolid, pure_color))
        {
            result.diagnostics.emplace_back("unified UnlitSolid definition is unavailable");
            result.passed = false;
            return result;
        }

        if (!pure_color.material_source_module_name
         || std::strcmp(
                pure_color.material_source_module_name,
                "material_source_unlit") != 0)
            result.diagnostics.emplace_back("PureColor must use one FS module");

        // 片段阶段由领域主着色器物理固定：PureColor 走 UnlitSolid，那份
        // `.frag` 必须真的 include unlit 的 surface provider 与 flat 光照
        // compositor。原先这里组装一遍模板再查文本，组装器已删除；直接查真源
        // 更硬——它查的就是运行时会被编译的那份字节。
        const std::string unlit_fs = ReadFileText(
            RepoRootPath("ShaderLibrary/domain/unlit_solid/unlit_solid.frag"));
        if (unlit_fs.empty())
        {
            result.diagnostics.emplace_back(
                "unlit_solid.frag is unavailable");
        }
        else
        {
            if (unlit_fs.find("#include \"module/provider/material_source/material_source_unlit.glsl\"")
                == std::string::npos)
                result.diagnostics.emplace_back(
                    "unlit_solid.frag does not use module/provider/material_source/material_source_unlit.glsl");
            if (unlit_fs.find("#include \"module/provider/output_policy/output_policy_flat.glsl\"")
                == std::string::npos)
                result.diagnostics.emplace_back(
                    "unlit_solid.frag does not use module/provider/output_policy/output_policy_flat.glsl");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunMaterialBDAFixedBindingsCase()
    {
        GateResult result;
        result.name = "Z.material-bda-fixed-bindings";
        const StageSPVCompilerInput compiler_input{
            "MaterialPrivateDataMaterial",
            PrimitiveType::Triangles
        };

        const std::string root_bindings =
            ReadFileText(RepoRootPath("ShaderLibrary/domain/root_bindings.glsl"));
        const std::string emissive_bindings =
            ReadFileText(RepoRootPath("ShaderLibrary/domain/emissive_row_bindings.glsl"));
        // 结构本体已拆到独立文件（由 C++ 结构 + 注解生成）；bindings 只留寻址宏。
        const std::string emissive_row_glsl =
            ReadFileText(RepoRootPath("ShaderLibrary/module/descriptor/ssbo/emissive_surface_row.glsl"));

        // 本门用**合成源**（root_bindings + emissive bindings 文本直拼），不经过编译器
        // 的正文拼接 ⇒ 独立结构文件的 `#include` 要在这里就地展开，否则
        // `GetFinalGLSL()` 里只有一行 #include、看不到结构声明。
        std::string emissive_bindings_expanded = emissive_bindings;
        static constexpr char kRowInclude[] = "#include \"module/descriptor/ssbo/emissive_surface_row.glsl\"";
        {
            const size_t include_pos = emissive_bindings_expanded.find(kRowInclude);
            if (include_pos != std::string::npos)
                emissive_bindings_expanded.replace(include_pos,
                                                   sizeof(kRowInclude) - 1,
                                                   emissive_row_glsl);
        }

        const std::string emissive_frag_source =
            root_bindings + "\n" + emissive_bindings_expanded;

        StageSPVCompileConfig config{};
        config.material_private_data = GlobalSSBOType::EmissiveSurface;
        config.defer_finalize = true;
        config.fixed_mesh_source = "#version 460\nvoid main(){}\n";
        config.fixed_fragment_source = emissive_frag_source.c_str();
        // mesh 化后顶点路径统一走 Mesh stage（VS 已彻底废弃）
        config.shader_stage_flag_bits = uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *build_spec = CompileMaterialStages(
            nullptr,
            compiler_input,
            config);
        if (!build_spec)
        {
            result.diagnostics.emplace_back("material compiler did not produce a build spec");
            result.passed = false;
            return result;
        }

        const ShaderCreateInfo *fragment =
            build_spec->GetStageShader(ShaderStage::Fragment);
        if (!fragment)
        {
            result.diagnostics.emplace_back("material compiler did not produce a fragment stage");
        }
        else
        {
            const std::string &source = fragment->GetFinalGLSL();
            const auto count_occurrences = [&source](const char *token)
            {
                size_t count = 0;
                size_t offset = 0;
                while (true)
                {
                    const size_t found = source.find(token, offset);
                    if (found == std::string::npos)
                        break;
                    ++count;
                    offset = found + 1;
                }
                return count;
            };

            // Arena+BDA：材质数据经设备地址行表寻址。
            // 域固定架构后这些文本**不再被发射**：它们物理固定在
            // ShaderLibrary/domain/emissive_row_bindings.glsl 里。所以判据改成
            // 「库文件自己声明的行别名，是否原样、只出现一次地进了最终 stage 文档」——
            // 测试不再自己拼一份期望字符串去回声（那样连发射器丢了整段内容都能过）。
            const std::string expected_alias = ExtractDefineLine(
                emissive_bindings, "#define MTL_ROW(i)");
            if (expected_alias.empty())
            {
                result.diagnostics.emplace_back(
                    "emissive_row_bindings.glsl 不再定义 MTL_ROW 行别名"
                    "（材质私有数据行无法按 payload_index 解引用）");
            }
            else
            {
                const size_t alias_hits = count_occurrences(expected_alias.c_str());
                if (alias_hits != 1)
                {
                    result.diagnostics.emplace_back(
                        "库文件的 MTL_ROW 行别名没有恰好一次地进入 stage 文档（hits="
                        + std::to_string(alias_hits) + "）");
                }
            }

            // 三要素（值结构 / 行引用 / 行别名）的声明点是库文件本身：
            // 少一个、改名、或多个文件重复声明，都在这里硬失败。
            for (const char *required : {"struct EmissiveSurfaceData",
                                         "buffer EmissiveSurfaceRow",
                                         "MaterialInstanceAddressesRef"})
            {
                if (emissive_bindings.find(required) == std::string::npos
                 && root_bindings.find(required) == std::string::npos
                 && emissive_row_glsl.find(required) == std::string::npos)
                    result.diagnostics.emplace_back(
                        std::string("库文件缺少材质私有数据行声明：") + required);
            }

            // payload 行需求必须与 MTL_ROW 一起出现：运行期的材质私有数据 SSBO
            // 绑定校验只认这一位。
            if (!build_spec->RequiresRecipePayloadRow())
                result.diagnostics.emplace_back(
                    "payload material did not report a payload row requirement");

            if (source.find("struct MaterialInstanceAddresses") == std::string::npos
             || source.find("MaterialInstanceAddresses values[]") == std::string::npos)
                result.diagnostics.emplace_back(
                    "dual-address material instance table was not emitted");

            if (count_occurrences("struct EmissiveSurfaceData") != 1)
                result.diagnostics.emplace_back("repeated SSBO type emitted duplicate GLSL struct");

            if (source.find("buffer EmissiveSurfaceRow") == std::string::npos)
                result.diagnostics.emplace_back("arena row reference declaration is incomplete");

            const size_t extension = source.find(
                "#extension GL_EXT_mesh_shader : require");
            const size_t declaration = source.find(
                "struct EmissiveSurfaceData");
            const size_t alias = source.find(expected_alias);
            if (extension == std::string::npos)
                result.diagnostics.emplace_back("extension missing");
            if (declaration == std::string::npos)
                result.diagnostics.emplace_back("declaration missing");
            if (alias == std::string::npos)
                result.diagnostics.emplace_back("alias missing");
            if (extension == std::string::npos
             || declaration == std::string::npos
             || alias == std::string::npos
             || !(extension < declaration && declaration < alias))
            {
                result.diagnostics.emplace_back(
                    "material stage document injection order changed: ext=" + std::to_string(extension) + " decl=" + std::to_string(declaration) + " alias=" + std::to_string(alias));
            }
        }

        delete build_spec;

        // 独立纯纹理材质：没有 payload 时仍必须发射双地址行表和 MTL_TEX。
        // 说明：这里的片元源是**合成**的（真实库里 `standard_opaque_bindings.glsl`
        // 同时声明 MTL_ROW/MTL_TEX，无法用来证明「只有贴图行时不会多发 payload 行」）。
        // 本段要证明的是编译器的**判定语义**，不是库文件内容，故刻意自建最小源。
        ResolvedMaterial texture_definition{};
        texture_definition.domain_name = "MaterialTextureReferenceOnly";
        texture_definition.texture_declarations.push_back(
            {"base_color", SamplerType::Sampler2D, true});
        texture_definition.texture_declarations.push_back(
            {"normal", SamplerType::Sampler2DArray, false});
        texture_definition.vertex_varying.emit_data_index_id = false;

        const std::string texture_frag_source =
            root_bindings + "\n"
            "struct MaterialTextureReferences { uvec2 tex_base_color; uvec2 tex_normal; };\n"
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef { uvec2 tex_base_color; uvec2 tex_normal; };\n"
            "#define MTL_TEX(i) MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(16))\n";

        StageSPVCompilerInput texture_compiler_input{
            "MaterialTextureReferenceOnly",
            PrimitiveType::Triangles};
        StageSPVCompileConfig texture_config{};
        texture_config.material_private_data = GlobalSSBOType::PBRSurface;
        texture_config.material_definition = &texture_definition;
        texture_config.defer_finalize = true;
        texture_config.fixed_mesh_source = "#version 460\nvoid main(){}\n";
        texture_config.fixed_fragment_source = texture_frag_source.c_str();
        texture_config.shader_stage_flag_bits =
            uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *texture_build_spec = CompileMaterialStages(
            nullptr,
            texture_compiler_input,
            texture_config);
        if (!texture_build_spec)
        {
            result.diagnostics.emplace_back(
                "texture-reference-only compiler did not produce a build spec");
        }
        else
        {
            if (!texture_build_spec->RequiresRecipeRuntimeRows())
            {
                result.diagnostics.emplace_back(
                    "pure texture material did not require runtime address rows");
            }

            // 合并信号为真，但 payload 行需求必须是假 —— 运行期靠这一位决定
            // 「recipe 是否必须给 material_ssbo_binding」，两者混淆会误杀纯纹理材质。
            if (texture_build_spec->RequiresRecipePayloadRow())
            {
                result.diagnostics.emplace_back(
                    "pure texture material reported a payload row requirement");
            }

            const ShaderCreateInfo *texture_fragment =
                texture_build_spec->GetStageShader(ShaderStage::Fragment);
            if (!texture_fragment)
            {
                result.diagnostics.emplace_back(
                    "texture-reference-only compiler did not produce a fragment stage");
            }
            else
            {
                const std::string &source =
                    texture_fragment->GetFinalGLSL();
                if (source.find("struct MaterialInstanceAddresses")
                        == std::string::npos
                 || source.find("MaterialInstanceAddresses values[]")
                        == std::string::npos
                 || source.find("buffer MaterialTextureReferencesRef")
                        == std::string::npos
                 || source.find("uvec2 tex_base_color;")
                        == std::string::npos
                 || source.find("uvec2 tex_normal;")
                        == std::string::npos
                 || source.find(
                        "#define MTL_TEX(i) MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(16))")
                        == std::string::npos)
                    result.diagnostics.emplace_back(
                        "named material texture reference row was not emitted");

                if (source.find("#define MTL_ROW(i)") != std::string::npos)
                    result.diagnostics.emplace_back(
                        "payload row alias emitted for a pure texture material");
            }

            delete texture_build_spec;
        }

        // 无纹理材质（VertexColor / Checkerboard 类）不一定需要 payload 行：
        // 判据是材质提供的 GLSL 模块有没有 `require Resource MaterialData`。
        // 这类材质既不读 MTL_ROW 也不该向 recipe 索要 material_ssbo_binding，
        // 按「有没有纹理声明」反推会把它们全部误杀。
        ResolvedMaterial vertex_color_definition{};
        vertex_color_definition.domain_name = "MaterialPayloadFreeVertexColor";
        vertex_color_definition.material_source_module_name = "material_source_vertex_color";
        vertex_color_definition.vertex_varying.emit_data_index_id = false;

        StageSPVCompilerInput vertex_color_compiler_input{
            "MaterialPayloadFreeVertexColor",
            PrimitiveType::Triangles};
        StageSPVCompileConfig vertex_color_config{};
        vertex_color_config.material_private_data = GlobalSSBOType::PBRSurface;
        vertex_color_config.material_definition = &vertex_color_definition;
        vertex_color_config.defer_finalize = true;
        vertex_color_config.fixed_mesh_source = "#version 460\nvoid main(){}\n";
        vertex_color_config.fixed_fragment_source = root_bindings.c_str();
        vertex_color_config.shader_stage_flag_bits =
            uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *vertex_color_build_spec = CompileMaterialStages(
            nullptr,
            vertex_color_compiler_input,
            vertex_color_config);
        if (!vertex_color_build_spec)
        {
            result.diagnostics.emplace_back(
                "no-texture vertex-color compiler did not produce a build spec");
        }
        else
        {
            if (vertex_color_build_spec->RequiresRecipePayloadRow())
                result.diagnostics.emplace_back(
                    "no-texture vertex-color material reported a payload row requirement");

            const ShaderCreateInfo *vertex_color_fragment =
                vertex_color_build_spec->GetStageShader(ShaderStage::Fragment);
            if (!vertex_color_fragment)
            {
                result.diagnostics.emplace_back(
                    "no-texture vertex-color compiler did not produce a fragment stage");
            }
            else
            {
                const std::string &source =
                    vertex_color_fragment->GetFinalGLSL();
                if (source.find("#define MTL_ROW(i)") != std::string::npos)
                    result.diagnostics.emplace_back(
                        "payload row alias emitted for a payload-free material");
                if (source.find("#define MTL_TEX(i)") != std::string::npos)
                    result.diagnostics.emplace_back(
                        "texture reference alias emitted for a texture-free material");
                if (source.find("struct MaterialInstanceAddresses")
                        == std::string::npos)
                    result.diagnostics.emplace_back(
                        "material instance address rows missing for a payload-free material");
            }

            delete vertex_color_build_spec;
        }

        // 反向守卫：同样没有纹理声明，但材质源显式声明了 MaterialData
        // （material_source_pbr_surface）⇒ payload 行需求必须为真，别把判据放宽成恒假。
        ResolvedMaterial pbr_definition{};
        pbr_definition.domain_name = "MaterialPayloadDeclaredNoTexture";
        pbr_definition.material_source_module_name = "material_source_pbr_surface";
        pbr_definition.vertex_varying.emit_data_index_id = true;

        StageSPVCompilerInput pbr_compiler_input{
            "MaterialPayloadDeclaredNoTexture",
            PrimitiveType::Triangles};
        StageSPVCompileConfig pbr_config{};
        pbr_config.material_private_data = GlobalSSBOType::PBRSurface;
        pbr_config.material_definition = &pbr_definition;
        pbr_config.defer_finalize = true;
        pbr_config.fixed_mesh_source = "#version 460\nvoid main(){}\n";
        pbr_config.fixed_fragment_source = root_bindings.c_str();
        pbr_config.shader_stage_flag_bits =
            uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *pbr_build_spec = CompileMaterialStages(
            nullptr,
            pbr_compiler_input,
            pbr_config);
        if (!pbr_build_spec)
        {
            result.diagnostics.emplace_back(
                "payload-declared compiler did not produce a build spec");
        }
        else
        {
            if (!pbr_build_spec->RequiresRecipePayloadRow())
                result.diagnostics.emplace_back(
                    "texture-free material with a MaterialData source lost its payload row requirement");

            delete pbr_build_spec;
        }

        // 6 槽 StandardOpaque MTL_TEX 行跨度守卫：`standard_opaque_bindings.glsl`
        // 把行跨度硬编码为 uint64_t(48)。它必须与其 struct 槽数自洽（8B/槽向上
        // 取整 16B），并与 C++ `BuildMaterialTextureReferenceLayout` 按规范 6 槽
        // 声明集算出的 row_stride 交叉一致 —— 任何一侧增减槽位而另一侧漏改，
        // 运行期都会静默跨材质错位采样。
        {
            const std::string standard_bindings = ReadFileText(
                RepoRootPath("ShaderLibrary/domain/standard_opaque_bindings.glsl"));

            const size_t ref_pos = standard_bindings.find(
                "buffer MaterialTextureReferencesRef");
            const size_t ref_end = ref_pos == std::string::npos
                ? std::string::npos
                : standard_bindings.find("};", ref_pos);
            if (ref_end == std::string::npos)
            {
                result.diagnostics.emplace_back(
                    "standard opaque MaterialTextureReferencesRef struct not found");
            }

            size_t slot_count = 0;
            if (ref_end != std::string::npos)
            {
                for (size_t p = standard_bindings.find("uvec2", ref_pos);
                     p != std::string::npos && p < ref_end;
                     p = standard_bindings.find("uvec2", p + 5))
                    ++slot_count;
            }

            uint64_t glsl_stride = 0;
            const size_t mtl_tex_pos =
                standard_bindings.find("#define MTL_TEX(i)");
            if (mtl_tex_pos == std::string::npos)
            {
                result.diagnostics.emplace_back(
                    "standard opaque MTL_TEX alias not found");
            }
            else
            {
                const size_t tex_idx_pos = standard_bindings.find(
                    "texture_reference_index)", mtl_tex_pos);
                const size_t num_pos = tex_idx_pos == std::string::npos
                    ? std::string::npos
                    : standard_bindings.find("uint64_t(", tex_idx_pos);
                if (num_pos == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "MTL_TEX row stride literal not found");
                }
                else
                {
                    glsl_stride = std::strtoull(
                        standard_bindings.c_str() + num_pos + 9, nullptr, 10);
                }
            }

            if (slot_count > 0
             && glsl_stride > 0
             && ((slot_count * 8u + 15u) & ~static_cast<size_t>(15u))
                != static_cast<size_t>(glsl_stride))
            {
                result.diagnostics.emplace_back(
                    "standard opaque MTL_TEX stride disagrees with its struct layout");
            }

            ResolvedMaterial standard_definition{};
            standard_definition.domain_name = "StandardOpaqueGate";
            for (const char *slot : {"base_color", "roughness", "metallic",
                                     "occlusion", "opacity_mask", "normal"})
            {
                MaterialTextureDeclaration decl{};
                decl.name = slot;
                standard_definition.texture_declarations.push_back(decl);
            }
            MaterialTextureReferenceLayout layout{};
            if (!BuildMaterialTextureReferenceLayout(standard_definition, layout))
            {
                result.diagnostics.emplace_back(
                    "canonical 6-slot texture layout failed to build");
            }
            else if (layout.reference_count != 6u
                  || layout.row_stride != static_cast<uint32_t>(glsl_stride))
            {
                result.diagnostics.emplace_back(
                    "C++ texture layout row_stride no longer matches standard opaque GLSL hard-coded stride");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunBindingMacroSingleSourceCase()
    {
        GateResult result;
        result.name = "AE.binding-macro-single-source";

        // 固定 ABI 的 set/binding 宏唯一真源是 descriptor_macros.glsl（生成物）。
        // 编译器不得再注入 #define（原 kBindingDefineTable 注入路径已删除，Phase 3）。
        // 输入 GLSL 特意 include 并使用宏（与真实模板一致）——旧注入路径会因此向
        // mesh 阶段注入 L2W_SET/L2W_BINDING（该注入路径已删除，本用例钉住不许复活）。
        const StageSPVCompilerInput compiler_input{
            "BindingMacroSingleSourceMaterial",
            PrimitiveType::Triangles
        };

        static const char kMeshSource[] =
            "#version 460\n"
            "#include \"contract/descriptor_macros.glsl\"\n"
            "layout(location=0) in vec2 Position;\n"
            "void main(){gl_Position=vec4(Position,L2W_SET,L2W_BINDING);}\n";
        static const char kFragSource[] =
            "#version 460\n"
            "layout(location=0) out vec4 outColor;\n"
            "void main(){outColor=vec4(1.0);}\n";

        StageSPVCompileConfig config{};
        config.defer_finalize = true;
        config.fixed_mesh_source = kMeshSource;
        config.fixed_fragment_source = kFragSource;
        config.shader_stage_flag_bits = uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *build_spec = CompileMaterialStages(
            nullptr,
            compiler_input,
            config);
        if (!build_spec)
        {
            result.diagnostics.emplace_back("compiler did not produce a build spec");
            result.passed = false;
            return result;
        }

        static const char *injected_defines[] = {
            "#define L2W_SET",
            "#define L2W_BINDING",
            "#define MESH_DRAW_PARAMS_SET",
            "#define MESH_DRAW_PARAMS_BINDING",
            "#define VIEWPORT_SET",
            "#define CAMERA_SET",
            "#define SKY_SET",
            "#define COLOR_PALETTE_SET",
        };

        const ShaderCreateInfo *mesh = build_spec->GetStageShader(ShaderStage::Mesh);
        if (!mesh)
        {
            result.diagnostics.emplace_back("did not produce a mesh stage");
        }
        else
        {
            const std::string &source = mesh->GetFinalGLSL();
            if (source.find("#include \"contract/descriptor_macros.glsl\"")
                == std::string::npos)
                result.diagnostics.emplace_back("descriptor_macros include is missing");

            for (const char *macro : injected_defines)
                if (source.find(macro) != std::string::npos)
                    result.diagnostics.emplace_back(
                        std::string("compiler injected fixed-ABI define: ") + macro);
        }

        delete build_spec;
        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunShaderLibraryPathCase()
    {
        GateResult result;
        result.name = "AB.shader-library-path";

        // Baseline: default resolution must find the repository ShaderLibrary.
        const std::string root = GetShaderLibraryPath("ShaderLibrary");
        const hgl::filesystem::Path root_path(hgl::ToOSString(root));
        if (!IsShaderLibraryRoot(root_path))
        {
            result.diagnostics.emplace_back(
                "ShaderLibrary path resolver did not find a valid material root");
        }

        const hgl::OSString env_name(kShaderLibraryPathEnvironmentVariable);
        const hgl::OSString env_root_os =
            hgl::ToOSString(RepoRootPath("ShaderLibrary"));

        // Capture the inherited value BEFORE any in-process mutation, so the
        // final restore puts the environment back exactly as the gate found it.
        const wchar_t *saved_env =
            _wgetenv(kShaderLibraryPathEnvironmentVariable);
        const hgl::OSString saved_env_os =
            saved_env && saved_env[0] ? hgl::OSString(saved_env) : hgl::OSString();

        // 1. Env-var override wins over an explicit (invalid) request.
        _wputenv((env_name + OS_TEXT("=") + env_root_os).c_str());
        const std::string env_override =
            GetShaderLibraryPath("C:/definitely/not/a/library");
        const hgl::filesystem::Path env_override_path(
            hgl::ToOSString(env_override));
        if (!IsShaderLibraryRoot(env_override_path))
        {
            result.diagnostics.emplace_back(
                "ULRE_SHADERLIBRARY_PATH override must beat the requested path");
        }

        // 2. Invalid env-var value must be ignored, not fatal.
        _wputenv((env_name + OS_TEXT("=C:/definitely/not/a/library")).c_str());
        const std::string invalid_env = GetShaderLibraryPath("ShaderLibrary");
        if (invalid_env.empty())
        {
            result.diagnostics.emplace_back(
                "invalid ULRE_SHADERLIBRARY_PATH must fall back, not fail");
        }

        // 3. Restoring the original value (or clearing, when unset) restores
        //    default resolution and must not poison later gate cases.
        if (!saved_env_os.IsEmpty())
            _wputenv((env_name + OS_TEXT("=") + saved_env_os).c_str());
        else
            _wputenv(env_name.c_str());
        const std::string cleared = GetShaderLibraryPath("ShaderLibrary");
        if (cleared.empty())
        {
            result.diagnostics.emplace_back(
                "restoring ULRE_SHADERLIBRARY_PATH must not break resolution");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunDomainBackendEquivalenceCase()
    {
        GateResult result;
        result.name = "DH.domain-backend-equivalence";

        std::string preset_diagnostic;
        if (!EnsureSamplerPresetsLoaded(preset_diagnostic))
        {
            result.diagnostics.emplace_back(preset_diagnostic);
            result.passed = false;
            return result;
        }

        for (const DomainBackendEquivShape &shape : kDomainBackendEquivShapes)
        {
            const std::string label = std::string(shape.label)
                                    + " (" + GetMaterialDomainName(shape.domain) + ")";

            ResolvedMaterial definition{};
            if (!BuildResolvedMaterialForDomain(shape.domain, definition))
            {
                result.diagnostics.emplace_back(label + ": domain definition unavailable");
                continue;
            }

            GeometryVertexFormat geometry;
            MaterialBuildRequest request{};
            std::string build_diagnostic;
            if (!BuildDomainEquivRequest(
                    shape, definition, geometry, request, build_diagnostic))
            {
                result.diagnostics.emplace_back(label + ": " + build_diagnostic);
                continue;
            }

            // ── ① 领域后端路径（临时接管槽位，跑完交还）────────────────────
            request.defer_finalize = false;
            std::string build_error;
            std::unique_ptr<ShaderBuildContext> domain_built =
                BuildViaDomainBackend(definition, request, build_error);
            if (!domain_built)
            {
                result.diagnostics.emplace_back(
                    label + ": domain backend build failed: " + build_error);
                continue;
            }

            // ── ② 片段阶段必须有 SPV，且必须真的吃 generated/frag_options.glsl ──
            const ShaderCreateInfo *dom_fs = domain_built->has_fragment()
                ? domain_built->GetStageShader(ShaderStage::Fragment) : nullptr;
            if (!dom_fs)
            {
                result.diagnostics.emplace_back(label + ": fragment stage missing");
                continue;
            }
            if (!dom_fs->HasSPVData())
            {
                result.diagnostics.emplace_back(label + ": fragment SPIR-V missing");
                continue;
            }

            const std::string &dom_glsl = dom_fs->GetFinalGLSL();
            if (dom_glsl.find("generated/frag_options.glsl") == std::string::npos)
            {
                result.diagnostics.emplace_back(
                    label + ": domain GLSL does not include generated/frag_options.glsl"
                    " (include root wiring broken)");
                continue;
            }

            hgl::ValueArray<uint8_t> dom_storage;
            ShaderFlatView dom_view{};
            if (!ReflectSPVData(
                    dom_fs->GetSPVData(), dom_fs->GetSPVSize(),
                    uint32_t(ShaderStage::Fragment), "main", dom_storage, dom_view))
            {
                result.diagnostics.emplace_back(label + ": domain reflection failed");
                continue;
            }

            const std::string dom_text = FormatDomainEquivReflection(dom_view);

            // ── ③ 冻结基线 ─────────────────────────────────────────────────
            //
            // 这里曾经有第二个"生成器"侧构建，用两条独立拼装路径产出同一形状
            // 再逐字节比对——那才是"等价"的证据。片段阶段被领域主着色器物理
            // 固定、且 MaterialProgramAssembler 已拒收没有固定片段源的构建之后，
            // 第二条路径不存在了，继续留着只会是同一份输出的自证。
            //
            // 基线本身仍然有价值：它是这 15 个形状的反射文本 + SPV 体积的
            // 变更探测器，任何领域主着色器 / options 发射的漂移都会在这里
            // 现形。逐字节 SPV 比对已无对象，故不再参与。
            //
            // 注意：本用例**不做** mesh ↔ fragment varying 链接校验，
            // `fragDataIndexID` 那类"片段声明了、网格没发射"的缺陷它看不见。
            // 那类问题由 SPV.generated-interface 用真实 Vulkan 规则覆盖。
            const std::string &golden_text = dom_text;
            const size_t golden_size = dom_fs->GetSPVSize();

            // ── ④ 与冻结基线比对 ─────────────────────────────────────────
            {
                char header[256];
                std::snprintf(header, sizeof(header),
                              "# DH.domain-backend-equivalence shape=%s"
                              " domain=%s purpose=%d alpha_test=%d\n",
                              shape.label, GetMaterialDomainName(shape.domain),
                              int(shape.purpose), shape.alpha_test ? 1 : 0);
                char size_line[64];
                std::snprintf(size_line, sizeof(size_line),
                              "spv_bytes=%zu\n", golden_size);

                const std::string dump =
                    std::string(header) + NormalizeDumpEOL(golden_text) + size_line;

                const std::string golden_path = RepoRootPath(
                    (std::string("src/Tools/ShaderTooling/golden/domain-backend/")
                     + shape.label + ".equiv.txt").c_str());

                CompareAgainstGolden(
                    golden_path, dump, "DH.domain-equiv", result.diagnostics);

                char ok[160];
                std::snprintf(ok, sizeof(ok), "%s: baseline ok (spv=%zu bytes)",
                              label.c_str(), golden_size);
                std::fprintf(stdout, "[INFO] DH.domain-backend %s\n", ok);
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // DH.domain-backend-cache — 领域后端产物真的能烘焙进 SPV Cache
    // ═══════════════════════════════════════════════════════════════════════
    //
    // 等价性用例比的是「两条路径编出来的东西一样」，但它**故意不带**
    // artifact store（带了就可能读到彼此的缓存，测到的就不再是编译）。这条
    // 用例反过来只证明落盘/复用这条链，以及换源后的元数据自洽：
    //
    //   ① 冷构建：空目录 ⇒ 两阶段都 miss，编译并写盘；
    //   ② 重开一个新 store 对象（同一目录）⇒ 两阶段都 hit，且 SPV 逐字节
    //      等于冷构建那次；
    //   ③ `generated_source_digest` 必须等于**当前**两阶段最终 GLSL 的摘要。
    //      `AssembleMaterialProgram` 是在换源前建元数据的，它算的是那份根本没被
    //      编译的生成物——不在换源后重算，这个字段就永远描述另一份文本；
    //   ④ 落盘的 `.frag` 源必须含 `generated/frag_options.glsl`，即缓存里存的是
    //      固定主着色器，不是旧的生成物。
    //
    // 只跑 `StandardOpaque` 的 4 个 pass 形状，外加无光族的两个前向形状
    // （`UnlitTexture` 与 `VertexColor`）：前者覆盖「同 domain 多 pass」的
    // 缓存分歧，后者覆盖「多 domain 共享同一份缓存机制」且两个形状的主着色器
    // 文本与 options 都不同 ⇒ 摘要必然不同，能继续守住「摘要必须覆盖 options」。
    //
    // 再加两个必须单独覆盖的形态：
    //   · `DebugNormal` —— 材质私有行是 `EmissiveSurface`（跨度 16），与前面
    //     全部 `PBRSurface`（跨度 32）不同，缓存键里若掺了行布局差异会在这里炸；
    //   · `builtin/text_gpu` —— 走 unlit 骨架 + 贴图表 + `styleID` 通道，
    //     是唯一 `HGL_ALPHA_OUTPUT` 真的生效的形状。
    //
    // 不加 `VertexPaletteColor`：它与 `VertexColor` 的固定主着色器**逐字节相同**、
    // options 也相同，摘要天然相等——放进「两两互不相同」的断言里会假失败
    //（它们本来就该共享同一份产物，只是落在不同定义各自的缓存目录里）。

    GateResult RunDomainBackendCacheCase()
    {
        GateResult result;
        result.name = "DH.domain-backend-cache";

        std::string preset_diagnostic;
        if (!EnsureSamplerPresetsLoaded(preset_diagnostic))
        {
            result.diagnostics.emplace_back(preset_diagnostic);
            result.passed = false;
            return result;
        }

        const DomainBackendEquivShape *shapes[] =
        {
            &kDomainBackendEquivShapes[0],  // fwd
            &kDomainBackendEquivShapes[1],  // fwd-alpha
            &kDomainBackendEquivShapes[2],  // shadow
            &kDomainBackendEquivShapes[3],  // shadow-alpha
            &kDomainBackendEquivShapes[5],  // unlit-tex
            &kDomainBackendEquivShapes[6],  // vert-color
            &kDomainBackendEquivShapes[13], // text-gpu
            &kDomainBackendEquivShapes[14], // debug-normal
        };
        constexpr size_t kCacheShapeCount = sizeof(shapes) / sizeof(shapes[0]);

        // 每个形状的片段阶段 compile_inputs_hash。最后要求两两互不相同：
        // domain options 的差异只体现在 `generated/frag_options.glsl`，主着色器文本
        // 逐字节相同，所以选项必须进 compile_inputs_hash —— 否则两个不同 pass 变体
        // 会共享同一个 stage 键，撞上缓存文件名就是静默复用错误的 SPV。
        std::string digest_labels[kCacheShapeCount];
        hgl::uint64 digest_values[kCacheShapeCount] = {};
        size_t digest_count = 0;

        for (const DomainBackendEquivShape *shape : shapes)
        {
            const std::string label = std::string(shape->label)
                                    + " (" + GetMaterialDomainName(shape->domain) + ")";

            ResolvedMaterial definition{};
            if (!BuildResolvedMaterialForDomain(shape->domain, definition))
            {
                result.diagnostics.emplace_back(label + ": domain definition unavailable");
                continue;
            }

            GeometryVertexFormat geometry;
            MaterialBuildRequest request{};
            std::string build_diagnostic;
            if (!BuildDomainEquivRequest(
                    *shape, definition, geometry, request, build_diagnostic))
            {
                result.diagnostics.emplace_back(label + ": " + build_diagnostic);
                continue;
            }

            // 每个形状独占一个目录：共用会让别条用例的产物冒充命中。
            const std::filesystem::path cache_root =
                std::filesystem::path(RepoRootPath("build"))
                / "domain-backend-cache" / shape->label;
            std::error_code remove_ec;
            std::filesystem::remove_all(cache_root, remove_ec);
            // store 自己在写盘时才建 `<root>/shader-cache/stage`，而它的 `MakePath`
            // 依赖父目录已存在——先把根建出来，否则失败看起来像「元数据不合法」。
            std::filesystem::create_directories(cache_root, remove_ec);

            const hgl::OSString cache_root_os =
                hgl::ToOSString(cache_root.generic_string());
            request.defer_finalize = true;

            // ── ① 冷构建 ─────────────────────────────────────────────────
            std::unique_ptr<ShaderBuildContext> cold_ctx;
            {
                ShaderArtifactStore store(cache_root_os, ShaderCacheMode::BuildIfMissing);
                request.shader_artifact_store = &store;

                InstallDomainShaderBuildBackend();
                ShaderProgramBuildBackend *backend = GetShaderProgramBuildBackend();
                if (!backend)
                {
                    InstallDomainShaderBuildBackend();
                    result.diagnostics.emplace_back(label + ": domain backend slot empty");
                    continue;
                }

                cold_ctx.reset(backend->CreateBuildContext(nullptr, definition, request));
                if (!cold_ctx)
                {
                    const char *err = GetLastDomainShaderBuildBackendError();
                    InstallDomainShaderBuildBackend();
                    result.diagnostics.emplace_back(
                        label + ": cold build failed: "
                        + (err && err[0] ? err : "unknown error"));
                    continue;
                }
                if (!backend->FinalizeBuildContext(cold_ctx.get()))
                {
                    InstallDomainShaderBuildBackend();
                    char detail[256];
                    std::snprintf(detail, sizeof(detail),
                                  ": cold finalize failed (link=%d metadata=%d"
                                  " mesh_spv=%d frag_spv=%d)",
                                  cold_ctx->HasProgramLink() ? 1 : 0,
                                  cold_ctx->HasProgramArtifactMetadata() ? 1 : 0,
                                  cold_ctx->GetStageShader(ShaderStage::Mesh)
                                      && cold_ctx->GetStageShader(ShaderStage::Mesh)->HasSPVData() ? 1 : 0,
                                  cold_ctx->GetStageShader(ShaderStage::Fragment)
                                      && cold_ctx->GetStageShader(ShaderStage::Fragment)->HasSPVData() ? 1 : 0);
                    result.diagnostics.emplace_back(label + detail);
                    continue;
                }
                InstallDomainShaderBuildBackend();
            }

            const ShaderCreateInfo *cold_fs =
                cold_ctx->GetStageShader(ShaderStage::Fragment);
            const ShaderCreateInfo *cold_ms =
                cold_ctx->GetStageShader(ShaderStage::Mesh);
            if (!cold_fs || !cold_fs->HasSPVData() || !cold_ms || !cold_ms->HasSPVData())
            {
                result.diagnostics.emplace_back(label + ": cold build produced no SPIR-V");
                continue;
            }

            // 冷构建必须真的记账为 miss——不然「命中」这条断言恒真，什么也没测到。
            if (cold_ctx->UsedCachedFragmentStage() || cold_ctx->UsedCachedMeshStage())
            {
                result.diagnostics.emplace_back(
                    label + ": cold build reported a cache hit on an empty store");
                continue;
            }

            const std::string cold_frag_glsl = cold_fs->GetFinalGLSL();
            const std::string cold_mesh_glsl = cold_ms->GetFinalGLSL();
            const hgl::uint64 cold_frag_digest =
                ComputeGeneratedStageSourceDigest(cold_frag_glsl.data(), cold_frag_glsl.size());

            // ── ② 重开 store，同目录 ⇒ 必须命中且 SPV 一致 ────────────────
            std::unique_ptr<ShaderBuildContext> warm_ctx;
            {
                ShaderArtifactStore store(cache_root_os, ShaderCacheMode::BuildIfMissing);
                request.shader_artifact_store = &store;

                InstallDomainShaderBuildBackend();
                ShaderProgramBuildBackend *backend = GetShaderProgramBuildBackend();
                if (!backend)
                {
                    InstallDomainShaderBuildBackend();
                    result.diagnostics.emplace_back(label + ": domain backend slot empty (warm)");
                    continue;
                }

                warm_ctx.reset(backend->CreateBuildContext(nullptr, definition, request));
                if (!warm_ctx)
                {
                    const char *err = GetLastDomainShaderBuildBackendError();
                    InstallDomainShaderBuildBackend();
                    result.diagnostics.emplace_back(
                        label + ": warm build failed: "
                        + (err && err[0] ? err : "unknown error"));
                    continue;
                }
                if (!backend->FinalizeBuildContext(warm_ctx.get()))
                {
                    InstallDomainShaderBuildBackend();
                    result.diagnostics.emplace_back(label + ": warm finalize failed");
                    continue;
                }
                InstallDomainShaderBuildBackend();
            }

            const ShaderCreateInfo *warm_fs =
                warm_ctx->GetStageShader(ShaderStage::Fragment);
            const ShaderCreateInfo *warm_ms =
                warm_ctx->GetStageShader(ShaderStage::Mesh);
            if (!warm_fs || !warm_fs->HasSPVData() || !warm_ms || !warm_ms->HasSPVData())
            {
                result.diagnostics.emplace_back(label + ": warm build produced no SPIR-V");
                continue;
            }
            if (!warm_ctx->UsedCachedFragmentStage())
            {
                result.diagnostics.emplace_back(
                    label + ": fragment stage missed on a warm store"
                    " (domain master shader is not being baked)");
                continue;
            }
            if (!warm_ctx->UsedCachedMeshStage())
            {
                result.diagnostics.emplace_back(label + ": mesh stage missed on a warm store");
                continue;
            }
            if (warm_fs->GetSPVSize() != cold_fs->GetSPVSize()
             || std::memcmp(warm_fs->GetSPVData(), cold_fs->GetSPVData(),
                            cold_fs->GetSPVSize()) != 0)
            {
                char detail[192];
                std::snprintf(detail, sizeof(detail),
                              ": warm fragment SPIR-V differs from cold (cold=%zu warm=%zu)",
                              cold_fs->GetSPVSize(), warm_fs->GetSPVSize());
                result.diagnostics.emplace_back(label + detail);
                continue;
            }

            // ── ③ 磁盘上的 stage digest / 元数据摘要必须自洽 ──────────────
            {
                ShaderArtifactStore verify_store(cache_root_os, ShaderCacheMode::ReadOnly);

                hgl::ValueArray<uint8_t> stored_spv;
                hgl::uint64 stored_digest = 0;
                if (!verify_store.LoadStageSPV(
                        cold_ctx->GetProgramLink().fragment_stage,
                        stored_spv, &stored_digest))
                {
                    result.diagnostics.emplace_back(
                        label + ": fragment stage SPV not present on disk");
                    continue;
                }
                if (stored_digest != cold_frag_digest)
                {
                    result.diagnostics.emplace_back(
                        label + ": on-disk fragment source digest does not describe"
                        " the domain master shader that was compiled");
                    continue;
                }

                ShaderProgramArtifactMetadata metadata{};
                if (!verify_store.LoadProgramMetadata(
                        cold_ctx->GetProgramLink(), metadata))
                {
                    result.diagnostics.emplace_back(
                        label + ": program metadata not present on disk");
                    continue;
                }

                // 换源后不重算元数据 ⇒ 这里比的就是生成器那份没参与编译的文本。
                const hgl::uint64 expected_source_digest = ComputeGeneratedSourceDigest(
                    cold_mesh_glsl.data(), cold_mesh_glsl.size(),
                    cold_frag_glsl.data(), cold_frag_glsl.size());
                if (metadata.generated_source_digest != expected_source_digest)
                {
                    char detail[224];
                    std::snprintf(detail, sizeof(detail),
                                  ": program metadata generated_source_digest=%llu"
                                  " does not match the compiled sources (%llu)",
                                  static_cast<unsigned long long>(
                                      metadata.generated_source_digest),
                                  static_cast<unsigned long long>(
                                      expected_source_digest));
                    result.diagnostics.emplace_back(label + detail);
                    continue;
                }

                // ── ④ 落盘的 .frag 就是固定主着色器 ──────────────────────
                // `GetStagePath()` 是私有的，所以按契约（stage 与 .frag 同目录、
                // 同主文件名、只换扩展名）去 `shader-cache/stage/` 里找。每个
                // 形状独占一个目录，故这里恰好只有一个 `.frag`。
                bool found_frag_source = false;
                bool has_options_include = false;
                {
                    std::filesystem::path stage_dir(cache_root);
                    stage_dir /= ShaderArtifactCacheDirectory;
                    stage_dir /= ShaderArtifactStageDirectory;

                    std::error_code iter_ec;
                    for (std::filesystem::directory_iterator it(stage_dir, iter_ec), end;
                         !iter_ec && it != end; it.increment(iter_ec))
                    {
                        if (it->path().extension()
                            != GetShaderArtifactStageSourceExtension(ShaderStage::Fragment))
                            continue;

                        found_frag_source = true;

                        std::ifstream source_stream(it->path(), std::ios::binary);
                        if (!source_stream)
                            continue;
                        const std::string text(
                            (std::istreambuf_iterator<char>(source_stream)),
                            std::istreambuf_iterator<char>());
                        has_options_include =
                            text.find("generated/frag_options.glsl") != std::string::npos;
                    }
                }

                if (!found_frag_source)
                {
                    result.diagnostics.emplace_back(
                        label + ": no cached .frag source next to the stage SPV");
                    continue;
                }
                if (!has_options_include)
                {
                    result.diagnostics.emplace_back(
                        label + ": cached .frag source does not include"
                        " generated/frag_options.glsl (a generated shader was baked"
                        " instead of the fixed domain master)");
                    continue;
                }
            }

            char ok[160];
            std::snprintf(ok, sizeof(ok),
                          "%s: cold miss -> warm hit, spv=%zu bytes",
                          label.c_str(), warm_fs->GetSPVSize());
            std::fprintf(stdout, "[INFO] DH.domain-backend-cache %s\n", ok);

            if (digest_count < kCacheShapeCount)
            {
                digest_labels[digest_count] = label;
                // 片段阶段主着色器文本对同形状是逐字节相同的（选项住在
                // generated/frag_options.glsl），所以「选项有没有进身份」只能看
                // 文本之外的 compile_inputs_hash。这也是阶段 4 之后唯一的口径：
                // 文本摘要 = definition_hash，选项/依赖 = compile_inputs_hash。
                digest_values[digest_count] =
                    cold_ctx->GetProgramLink().fragment_stage.compile_inputs_hash;
                ++digest_count;
            }
        }

        for (size_t i = 0; i < digest_count; ++i)
            for (size_t j = i + 1; j < digest_count; ++j)
                if (digest_values[i] == digest_values[j])
                    result.diagnostics.emplace_back(
                        digest_labels[i] + " and " + digest_labels[j]
                        + ": identical fragment stage compile-inputs digest — the"
                          " domain options are not covered by the key, so two"
                          " different option sets can silently reuse one SPV");

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// `emit-fixed-mesh` 模式：一次跑完三张矩阵，逐形状点名网格阶段是否已有
    /// 内容一致的形状 master。
    ///
    /// 为什么这个探针放在门禁里而不是生产回归工具里：形状由**请求**决定
    /// （几何语义集合、位置格式、节点配置、图元类型），而矩阵只在这里维护一份。
    /// 在别处复现同一张矩阵，等于把「哪个形状需要覆盖」抄两份，抄歪了不会报错，
    /// 只会静默少覆盖一个形状。
    ///
    /// 三张矩阵都要走：
    ///   · `kReflectionCoverageVariants`——反射门逐 stage 编译的变体；
    ///   · `kDomainBackendEquivShapes`——领域后端等价门逐域的形状。
    ///     后者的几何是**由领域契约推导**的，与前者手写的几何不是同一份请求，
    ///     形状自然也不同（`builtin/error_checkerboard_3d` 就是这么漏掉的：
    ///     它只出现在领域后端矩阵里，第一次跑「缺源即失败」才现形）；
    ///   · `kCacheIdentityEmitShapes`——缓存身份 ABI 与 LoadGeometry 线框形状。
    ///
    /// 本探针**只读**。七-2 之后形状 master 是按形状命名、人工维护的物理源，
    /// 不再有「按哈希键把生成结果落盘」这件事；留着写盘分支只会把
    /// `mesh/fixed/` 那套哈希命名重新养出来。
    int EmitFixedMeshCatalog()
    {
        std::string preset_diagnostic;
        if (!EnsureSamplerPresetsLoaded(preset_diagnostic))
        {
            std::fprintf(stdout, "[emit-fixed-mesh] %s\n", preset_diagnostic.c_str());
            return 1;
        }

        std::vector<std::string> catalog;
        std::string catalog_diagnostic;
        if (!LoadFixedMeshCatalog(catalog, catalog_diagnostic))
        {
            std::fprintf(stdout, "[emit-fixed-mesh] %s\n", catalog_diagnostic.c_str());
            return 1;
        }

        int covered = 0;
        int missing = 0;
        int failed = 0;

        for (const ReflectionCoverageVariant &variant : kReflectionCoverageVariants)
        {
            std::string build_diagnostic;
            std::unique_ptr<ShaderBuildContext> ctx =
                BuildReflectionCoverageVariant(
                    variant, build_diagnostic, nullptr, nullptr);
            if (!ctx)
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n",
                          variant.variant_slug, build_diagnostic.c_str());
                ++failed;
                continue;
            }

            if (!CheckOneFixedMeshSource(variant.variant_slug, *ctx, catalog,
                                        covered, missing))
                ++failed;
        }

        // ── 领域后端矩阵 ────────────────────────────────────────────────
        //
        // 这条路径不吃 `fixed_fragment_source`（片段由领域后端自己解析主着色器），
        // 所以这里只取网格阶段。`AssembleMaterialProgram` 在这里的作用等价于
        // 「按请求生成网格阶段」，片段侧产出什么无关紧要。
        for (const DomainBackendEquivShape &shape : kDomainBackendEquivShapes)
        {
            ResolvedMaterial definition{};
            if (!BuildResolvedMaterialForDomain(shape.domain, definition))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: domain definition unavailable\n",
                             shape.label);
                ++failed;
                continue;
            }

            GeometryVertexFormat geometry;
            MaterialBuildRequest request{};
            std::string build_diagnostic;
            if (!BuildDomainEquivRequest(
                    shape, definition, geometry, request, build_diagnostic))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n", shape.label,
                          build_diagnostic.c_str());
                ++failed;
                continue;
            }

            // 只需要网格阶段文本，所以**不** finalize：领域后端在 finalize 阶段
            // 才编译固定片段（include 根要按 options 域就位），捕获用不到。
            request.defer_finalize = true;

            // 片段侧同样是物理固定的：`AssembleMaterialProgram` 在没有
            // `fixed_fragment_source` 时会直接拒绝。这里照抄领域后端的做法先解析
            // 固定片段源——否则捕获路径根本走不到网格阶段。
            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(definition, request, fixed))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n", shape.label,
                             GetLastDomainShaderBuildBackendError());
                ++failed;
                continue;
            }
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;

            MaterialProgramBuildCapture capture{};
            std::unique_ptr<ShaderBuildContext> ctx(
                AssembleMaterialProgram(nullptr, request, definition, &capture, fixed_stage));
            if (!ctx)
            {
                const hgl::AnsiString &build_error =
                    GetLastMaterialProgramAssemblyError();
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n", shape.label,
                          build_error.IsEmpty()
                              ? "(no diagnostic)" : build_error.c_str());
                ++failed;
                continue;
            }

            if (!CheckOneFixedMeshSource(shape.label, *ctx, catalog,
                                        covered, missing))
                ++failed;
        }

        // ── Q0 缓存身份矩阵 ────────────────────────────────────────────
        //
        // 形状键与模板/profile 无关（只吃顶点节点配置、varying、位置格式、
        // mesh 模式、invocation 上限），所以这里让路由器选模板即可，不必复现
        // 那条用例手选的模板 ID；几何则**必须**复用同一批常量。
        for (const CacheIdentityEmitShape &shape : kCacheIdentityEmitShapes)
        {
            ResolvedMaterial definition{};
            if (!BuildResolvedMaterialForDomain(shape.domain, definition))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: domain definition unavailable\n",
                             shape.label);
                ++failed;
                continue;
            }

            MaterialBuildRequest request{};
            request.recipe.domain = definition.domain;
            request.geometry_vertex_format = shape.geometry;
            request.primitive_type = shape.primitive_type;
            request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;
            request.defer_finalize = true;

            if (!SelectScenePassProfileRequest(
                    definition, ShaderProgramPurpose::ForwardColor, request.recipe,
                    request.render_template_request, nullptr)
             || !AppendMaterialPassProfileRoots(
                    definition, request.render_template_request))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: template request failed\n",
                             shape.label);
                ++failed;
                continue;
            }

            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(definition, request, fixed))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n", shape.label,
                             GetLastDomainShaderBuildBackendError());
                ++failed;
                continue;
            }
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;

            MaterialProgramBuildCapture capture{};
            std::unique_ptr<ShaderBuildContext> ctx(
                AssembleMaterialProgram(nullptr, request, definition, &capture, fixed_stage));
            if (!ctx)
            {
                const hgl::AnsiString &build_error =
                    GetLastMaterialProgramAssemblyError();
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n", shape.label,
                             build_error.IsEmpty()
                                 ? "(no diagnostic)" : build_error.c_str());
                ++failed;
                continue;
            }

            if (!CheckOneFixedMeshSource(shape.label, *ctx, catalog,
                                        covered, missing))
                ++failed;
        }

        std::fprintf(stdout,
                     "[emit-fixed-mesh] covered=%d missing=%d failed=%d\n",
                     covered, missing, failed);
        return (failed == 0 && missing == 0) ? 0 : 1;
    }

} // namespace gate
