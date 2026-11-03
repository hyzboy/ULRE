// 阶段 5 拆分自 ShaderResourceSchemaRegressionGate.cpp：组 interface 的 case 实现。
#include "GateTestSupport.h"

namespace gate
{
    GateResult RunMaterialVertexABICharacterizationCase()
    {
        GateResult result;
        result.name = "K.material-vertex-abi-characterization";

        const auto check_entries = [&](const char *name,
                                       const GeometryVertexFormat &geometry,
                                       const vertex_builder_common::VertexSemanticDecl *decls,
                                       const uint32_t decl_count,
                                       const std::vector<SerializedVertexEntry> &expected)
        {
            const vertex_builder_common::VertexBuildInput input{
                PrimitiveType::Triangles, &geometry, decls, decl_count
            };
            const auto actual = vertex_builder_common::BuildVertexEntries(input);
            std::string diagnostic;
            if (!CheckVertexEntries(actual, expected, diagnostic))
                result.diagnostics.emplace_back(std::string(name) + ": " + diagnostic);
        };

        // Position-only geometry is the baseline for both 2D and 3D paths.
        {
            const GeometryVertexFormat geometry{{VertexSemantic::Position, VF_V3F}};
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F}
            };
            check_entries("position-only", geometry, decls, 1,
                {{VF_V3F, VertexSemantic::Position}});
        }

        // UV and color are independently resolved from Geometry, not by location.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2HF},
                {VertexSemantic::Color, VF_V4UN8}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2F},
                {VertexSemantic::Color, VF_V4F}
            };
            check_entries("uv-and-color", geometry, decls, 3,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V2HF, VertexSemantic::TexCoord},
                 {VF_V4UN8, VertexSemantic::Color}});
        }

        // Normal providers may expose only Normal, Normal+Tangent, or full NTB.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F}
            };
            check_entries("normal-only", geometry, decls, 2,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V3F, VertexSemantic::Normal}});
        }
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3HF}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3F}
            };
            check_entries("normal-and-tangent", geometry, decls, 3,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V3F, VertexSemantic::Normal},
                 {VF_V3HF, VertexSemantic::Tangent}});
        }
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3F},
                {VertexSemantic::Bitangent, VF_V3F}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3F},
                {VertexSemantic::Bitangent, VF_V3F}
            };
            check_entries("full-ntb", geometry, decls, 4,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V3F, VertexSemantic::Normal},
                 {VF_V3F, VertexSemantic::Tangent},
                 {VF_V3F, VertexSemantic::Bitangent}});
        }

        // Packed normal formats remain a Geometry-owned format and preserve
        // their ABI identity for the legacy path.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, PF_A2BGR10UN, 4}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F}
            };
            check_entries("packed-normal", geometry, decls, 2,
                {{VF_V3F, VertexSemantic::Position},
                 {PF_A2BGR10UN, VertexSemantic::Normal}});
        }

        // Multiple color-related attributes must remain independently
        // addressable until the provider graph replaces this legacy ABI.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Color, VF_V4UN8},
                {VertexSemantic::Luminance, VF_V1UN8}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Color, VF_V4F},
                {VertexSemantic::Luminance, VF_V1F}
            };
            check_entries("multi-color-attributes", geometry, decls, 3,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V4UN8, VertexSemantic::Color},
                 {VF_V1UN8, VertexSemantic::Luminance}});
        }

        // Equivalent floating-point formats resolve to their own Geometry
        // formats while keeping the same semantic declaration shape.
        {
            const GeometryVertexFormat rg16_geometry{{VertexSemantic::Position, VF_V2HF}};
            const GeometryVertexFormat rg32_geometry{{VertexSemantic::Position, VF_V2F}};
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F}
            };
            const vertex_builder_common::VertexBuildInput rg16_input{
                PrimitiveType::Triangles, &rg16_geometry, decls, 1
            };
            const vertex_builder_common::VertexBuildInput rg32_input{
                PrimitiveType::Triangles, &rg32_geometry, decls, 1
            };
            const auto rg16_entries = vertex_builder_common::BuildVertexEntries(rg16_input);
            const auto rg32_entries = vertex_builder_common::BuildVertexEntries(rg32_input);
            if (rg16_entries.size() != rg32_entries.size()
             || rg16_entries.size() != 1
             || rg16_entries[0].semantic != rg32_entries[0].semantic
             || rg16_entries[0].format != VF_V2HF
             || rg32_entries[0].format != VF_V2F)
            {
                result.diagnostics.emplace_back("equivalent-format ABI shape mismatch");
            }
        }

        // Every special-purpose domain must expose the legacy
        // position-first contract while Phase 4 is introduced incrementally.
        static const MaterialDomain special_domains[] = {
            MaterialDomain::UnlitSolid,
            MaterialDomain::Checker2D,
            MaterialDomain::Checker3D,
            MaterialDomain::ErrorChecker3D,
            MaterialDomain::Text2D
        };
        for (const MaterialDomain domain : special_domains)
        {
            const char * domain_name = GetMaterialDomainName(domain);
            ResolvedMaterial definition{};
            if (!BuildResolvedMaterialForDomain(domain, definition))
            {
                result.diagnostics.emplace_back(
                    std::string("missing domain definition: ") + domain_name);
                continue;
            }
            if (definition.vertex_semantic_requirements.IsEmpty())
            {
                result.diagnostics.emplace_back(
                    std::string("invalid domain vertex ABI: ") + domain_name);
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunMaterialSemanticABIParityCase()
    {
        GateResult result;
        result.name = "L.material-semantic-abi-parity";

        static const MaterialDomain abi_domains[] = {
            MaterialDomain::UnlitSolid,
            MaterialDomain::Text2D,
            MaterialDomain::Checker2D,
            MaterialDomain::Checker3D,
            MaterialDomain::ErrorChecker3D
        };

        for (const MaterialDomain domain : abi_domains)
        {
            const char * domain_name = GetMaterialDomainName(domain);
            ResolvedMaterial definition{};
            if (!BuildResolvedMaterialForDomain(domain, definition))
            {
                result.diagnostics.emplace_back(
                    std::string("missing migrated domain: ") + domain_name);
                continue;
            }

            if (definition.vertex_provider_policy != MaterialVertexProviderPolicy::GeometryOnly)
            {
                result.diagnostics.emplace_back(
                    std::string("unexpected provider policy: ") + domain_name);
                continue;
            }

            if (definition.vertex_semantic_requirements.IsEmpty())
                result.diagnostics.emplace_back(
                    std::string("empty semantic-only ABI: ") + domain_name);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 深度/阴影 purpose 的 varying 裁剪必须完全由 coverage.required_semantics 决定。
    ///
    /// 反证（历史缺陷）：裁剪按语义逐项手写白名单时，StyleID / WorldPosition /
    /// WorldNormal / FragDirection 永远被清零 —— 即使 coverage 在要求它们。
    /// 生成的 shadow program 于是满足不了自己的 coverage 契约且不报错：
    /// FS 侧 si 字段恒为零值，alpha 求值静默走错分支。
    GateResult RunDepthVaryingMaskParityCase()
    {
        GateResult result;
        result.name = "L0.depth-varying-mask-parity";

        // ── ① 定义表对语义枚举的覆盖（新增语义未登记 = 正反两向同时漏）──
        // WorldTangent / WorldBinormal 是材质表面内部量，不存在跨阶段字段。
        for (hgl::uint32 value = 1;
             value < static_cast<hgl::uint32>(InterStageSemantic::RANGE_SIZE);
             ++value)
        {
            const InterStageSemantic semantic =
                static_cast<InterStageSemantic>(value);
            if (semantic == InterStageSemantic::WorldTangent
             || semantic == InterStageSemantic::WorldBinormal)
                continue;

            bool covered = false;
            for (const MaterialVaryingSemanticBinding &binding :
                    kMaterialVaryingSemanticBindings)
            {
                if (binding.semantic == semantic)
                {
                    covered = true;
                    break;
                }
            }
            if (!covered)
                result.diagnostics.emplace_back(
                    "varying/semantic 定义表漏登记语义 value="
                    + std::to_string(value));
        }

        // ── ② 声明 → 掩码（正向）：两条入口必须同源 ──
        MaterialVertexVaryingConfig declared{};
        declared.emit_data_index_id = true;
        declared.emit_world_pos = true;
        declared.emit_world_normal = true;
        declared.emit_uv0 = true;
        declared.emit_vertex_color = true;
        declared.emit_vertex_color_from_palette = true;
        declared.emit_frag_direction = true;
        declared.emit_luminance = true;
        declared.emit_style_id = true;
        declared.use_transform_id_attr = true;

        const InterStageSemanticMask declared_mask =
            GetInterStageSemanticMask(InterStageSemantic::DataIndexID)
          | GetInterStageSemanticMask(InterStageSemantic::WorldPosition)
          | GetInterStageSemanticMask(InterStageSemantic::WorldNormal)
          | GetInterStageSemanticMask(InterStageSemantic::UV0)
          | GetInterStageSemanticMask(InterStageSemantic::Color)
          | GetInterStageSemanticMask(InterStageSemantic::FragDirection)
          | GetInterStageSemanticMask(InterStageSemantic::Luminance)
          | GetInterStageSemanticMask(InterStageSemantic::StyleID);
        if (BuildMaterialInterStageSemanticMaskFromVarying(declared)
                != declared_mask
         || GetMaterialInterStageSemanticMask(declared) != declared_mask)
        {
            result.diagnostics.emplace_back(
                "varying → 掩码 与登记表不一致（正反两向不同源）");
        }

        ResolvedMaterial definition{};
        definition.vertex_varying = declared;

        // ── ③ 掩码裁剪：要什么留什么，多字段语义由定义自己挑字段 ──
        MaterialCoverageContract masked_coverage{};
        masked_coverage.mode = MaterialCoverageMode::AlphaTest;
        masked_coverage.requires_alpha_evaluation = true;
        masked_coverage.required_semantics =
            GetInterStageSemanticMask(InterStageSemantic::WorldNormal)
          | GetInterStageSemanticMask(InterStageSemantic::StyleID)
          | GetInterStageSemanticMask(InterStageSemantic::DataIndexID)
          | GetInterStageSemanticMask(InterStageSemantic::Color);

        const MaterialVertexVaryingConfig shadow =
            ResolveMaterialVertexVaryingConfig(
                definition, ShaderProgramPurpose::ShadowDepth,
                masked_coverage);
        if (!shadow.emit_world_normal)
            result.diagnostics.emplace_back(
                "shadow varying 未恢复 WorldNormal（coverage 要求但取不到）");
        if (!shadow.emit_style_id)
            result.diagnostics.emplace_back(
                "shadow varying 未恢复 StyleID（coverage 要求但取不到）");
        if (!shadow.emit_data_index_id)
            result.diagnostics.emplace_back(
                "shadow varying 未恢复 DataIndexID（coverage 要求但取不到）");
        if (!shadow.emit_vertex_color
         || !shadow.emit_vertex_color_from_palette)
            result.diagnostics.emplace_back(
                "Color 语义的多个输出字段被替定义挑选（定义声明了两个，结果缺一个）");
        if (shadow.emit_world_pos || shadow.emit_uv0
         || shadow.emit_frag_direction || shadow.emit_luminance)
            result.diagnostics.emplace_back(
                "shadow varying 保留了 coverage 未要求的语义");
        if (!shadow.use_transform_id_attr)
            result.diagnostics.emplace_back(
                "顶点属性选择 use_transform_id_attr 被跨阶段裁剪误伤");
        if (BuildMaterialInterStageSemanticMaskFromVarying(shadow)
                != masked_coverage.required_semantics)
        {
            result.diagnostics.emplace_back(
                "裁剪结果反推掩码 ≠ coverage.required_semantics");
        }

        // ── ④ 不评估 alpha ⇒ 一条跨阶段语义都不留（顶点属性选择仍保留）──
        MaterialCoverageContract opaque_coverage{};
        const MaterialVertexVaryingConfig depth_only =
            ResolveMaterialVertexVaryingConfig(
                definition, ShaderProgramPurpose::DepthOnly,
                opaque_coverage);
        if (BuildMaterialInterStageSemanticMaskFromVarying(depth_only) != 0)
            result.diagnostics.emplace_back(
                "不评估 alpha 的 depth program 仍带着跨阶段语义");
        if (!depth_only.use_transform_id_attr)
            result.diagnostics.emplace_back(
                "不评估 alpha 的 depth program 丢了顶点属性选择");

        // ── ⑤ 前向 purpose 不受裁剪影响（原样带出定义声明）──
        const MaterialVertexVaryingConfig forward =
            ResolveMaterialVertexVaryingConfig(
                definition, ShaderProgramPurpose::ForwardColor,
                opaque_coverage);
        if (BuildMaterialInterStageSemanticMaskFromVarying(forward)
                != declared_mask)
        {
            result.diagnostics.emplace_back(
                "前向 purpose 的 varying 被裁剪（只应作用于 depth/shadow）");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// CharQuad 的 mesh 工作组尺寸必须与 CPU dispatch 常量**逐位一致**。
    ///
    /// 反证（历史缺陷）：生成侧用 min(TOML 声明, 64) 再经设备能力二次钳制，
    /// 而 TextRenderPipeline 固定按 TEXT_CHARQUAD_MAX_INVOCATIONS 整除算
    /// DispatchMesh 组数、拿不到 shader 的 local_size_x。两者一旦分叉，
    /// 每个工作组只处理不足 64 个字符，尾部字符整段漏画且无任何报错。
    GateResult RunCharQuadWorkgroupContractCase()
    {
        GateResult result;
        result.name = "L0.charquad-workgroup-contract";

        MaterialRecipe recipe{};
        recipe.domain = MaterialDomain::Text2D;

        ResolvedMaterial definition{};
        if (!ResolveMaterialFromRecipe(recipe, definition))
        {
            result.diagnostics.emplace_back("missing domain definition: Text2D");
            result.passed = false;
            return result;
        }

        const auto build = [&recipe](const contract::PhysicalDeviceProfileLite *profile,
                              const ResolvedMaterial &def,
                              const char *mesh_source_override,
                              FixedFragmentSource *out_fixed = nullptr,
                              MaterialProgramBuildCapture *out_capture = nullptr)
            -> std::unique_ptr<ShaderBuildContext>
        {
            MaterialBuildRequest request{};
            request.recipe = recipe;
            request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;
            // 只到 GLSL 文本为止：本用例断言的是发射文本里的 local_size_x，
            // 不需要（也不该依赖）SPV 编译。
            request.defer_finalize = true;

            PassProfileValidationDiagnostic template_diagnostic{};
            const ScenePassProfile scene_profile =
                MakeForwardUnlitProfile();
            if (!ResolveScenePassProfileRequest(
                    PassProfileID::ForwardUnlit,
                    hgl::graph::ShaderStage::Fragment,
                    scene_profile, request.render_template_request,
                    template_diagnostic)
             || !AppendMaterialPassProfileRoots(
                    def, request.render_template_request))
            {
                return std::unique_ptr<ShaderBuildContext>();
            }

            // 本用例断言的是**网格阶段**发射文本里的 local_size_x 与设备上限
            // 守卫，片段根本不参与。片段源照样按领域固定源填进请求——不填就
            // 会退回生成器的片段拼装链（阶段 5.2 后那条链不复存在）。
            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(def, request, fixed))
            {
                return std::unique_ptr<ShaderBuildContext>();
            }

            // 非空即绕过固定源目录，直接把给定文本当网格阶段源——⑤⑥⑦ 用它注入
            // 被篡改的拓扑行，验证装配期校验真的会拒绝。
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;
            fixed_stage.mesh_source = mesh_source_override;

            if (out_fixed)
                *out_fixed = fixed;

            // 不编译 SPV：本用例只读网格阶段 GLSL 文本。
            return std::unique_ptr<ShaderBuildContext>(
                AssembleMaterialProgram(profile, request, def, out_capture,
                                        fixed_stage));
        };

        // ── ① 无 profile（无约束）：local_size_x 必须等于 CPU dispatch 常量 ──
        const auto plain = build(nullptr, definition, nullptr);
        if (!plain)
        {
            result.diagnostics.emplace_back(
                "CharQuad material build failed without device profile");
        }
        else
        {
            const ShaderCreateInfo *mesh =
                plain->GetStageShader(ShaderStage::Mesh);
            if (!mesh)
            {
                result.diagnostics.emplace_back(
                    "CharQuad material emitted no mesh stage");
            }
            else
            {
                const std::string &glsl = mesh->GetFinalGLSL();
                if (glsl.find("local_size_x = ")
                        == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "CharQuad mesh stage emits no local_size_x");
                }
                else if (glsl.find(
                        "local_size_x = "
                        + std::to_string(
                            static_cast<unsigned>(
                                TEXT_CHARQUAD_MAX_INVOCATIONS))
                        + ")") == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "CharQuad mesh stage local_size_x != "
                        "TEXT_CHARQUAD_MAX_INVOCATIONS（CPU dispatch 组数会算错）");
                }
            }
        }

        // ── ② 材质声明更小的线程数 ⇒ 必须构建失败，不得静默钳制 ──
        ResolvedMaterial lowered = definition;
        lowered.mesh_shader_max_invocations =
            TEXT_CHARQUAD_MAX_INVOCATIONS / 2u;
        if (build(nullptr, lowered, nullptr))
        {
            result.diagnostics.emplace_back(
                "CharQuad 接受 mesh_shader_max_invocations < "
                "TEXT_CHARQUAD_MAX_INVOCATIONS（CPU 按常量派发会漏字）");
        }

        // ── ③ 设备能力上限低于契约 ⇒ 必须构建失败 ──
        contract::PhysicalDeviceProfileLite narrow{};
        narrow.api_version = contract::MakeVkVersion(1, 4);
        narrow.limits.max_mesh_work_group_size_x =
            TEXT_CHARQUAD_MAX_INVOCATIONS / 2u;
        if (build(&narrow, definition, nullptr))
        {
            result.diagnostics.emplace_back(
                "CharQuad 在设备上限 < TEXT_CHARQUAD_MAX_INVOCATIONS "
                "时仍生成了更小的 group size");
        }

        // ── ④ 设备上限高于契约 ⇒ 仍锁在契约值（不退让也不放大）──
        contract::PhysicalDeviceProfileLite wide{};
        wide.api_version = contract::MakeVkVersion(1, 4);
        wide.limits.max_mesh_work_group_size_x =
            TEXT_CHARQUAD_MAX_INVOCATIONS * 2u;
        wide.limits.max_mesh_output_vertices = 1024u;
        wide.limits.max_mesh_output_primitives = 512u;
        const auto wide_ctx = build(&wide, definition, nullptr);
        if (!wide_ctx)
        {
            result.diagnostics.emplace_back(
                "CharQuad 在宽裕设备能力下构建失败");
        }
        else
        {
            const ShaderCreateInfo *mesh =
                wide_ctx->GetStageShader(ShaderStage::Mesh);
            if (!mesh
             || mesh->GetFinalGLSL().find(
                    "local_size_x = "
                    + std::to_string(static_cast<unsigned>(
                        TEXT_CHARQUAD_MAX_INVOCATIONS))
                    + ")") == std::string::npos)
            {
                result.diagnostics.emplace_back(
                    "CharQuad 的 group size 随设备能力放大（契约必须锁死）");
            }
        }

        // ── ⑤⑥⑦ 固定源被改写 ⇒ 必须构建失败，不得照着文本编译 ──
        //
        // 6.2 之后 topology 三行是**外部数据**：形状键只哈希「模式 + invocation 数」，
        // 不哈希这三行，所以手改文件里的数字不会让键失配——文件照样被找到、照样编译。
        // 以下三条把「数据被改动」变成硬失败；否则缺陷会以「每组处理不足 / 输出容量
        // 不够」的形式静默漏画，正是本用例登记的那类历史问题，只是从代码搬到了数据。
        const ShaderCreateInfo *plain_mesh =
            plain ? plain->GetStageShader(ShaderStage::Mesh) : nullptr;
        const MeshModeCapacity charquad_capacity = ResolveMeshModeTopology(
            MeshShaderMode::CharQuad, TEXT_CHARQUAD_MAX_INVOCATIONS);

        if (!plain_mesh)
        {
            result.diagnostics.emplace_back(
                "CharQuad mesh stage unavailable: topology tampering not exercised");
        }
        else
        {
            const std::string baseline = plain_mesh->GetFinalGLSL();

            struct TopologyTamper
            {
                const char *label;
                std::string from;
                std::string to;
            };

            const TopologyTamper tampers[3] =
            {
                {
                    "local_size_x",
                    "local_size_x = "
                        + std::to_string(TEXT_CHARQUAD_MAX_INVOCATIONS),
                    "local_size_x = "
                        + std::to_string(TEXT_CHARQUAD_MAX_INVOCATIONS / 2u)
                },
                {
                    "max_vertices",
                    "max_vertices = "
                        + std::to_string(charquad_capacity.max_vertices),
                    "max_vertices = "
                        + std::to_string(charquad_capacity.max_vertices / 2u)
                },
                {
                    "max_primitives",
                    "max_primitives = "
                        + std::to_string(charquad_capacity.max_primitives),
                    "max_primitives = "
                        + std::to_string(charquad_capacity.max_primitives / 2u)
                },
            };

            for (const TopologyTamper &tamper : tampers)
            {
                std::string tampered = baseline;
                const size_t at = tampered.find(tamper.from);
                if (at == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        std::string("CharQuad 固定源里找不到可篡改的 ")
                        + tamper.label + " 声明");
                    continue;
                }
                tampered.replace(at, tamper.from.size(), tamper.to);

                if (build(nullptr, definition, tampered.c_str()))
                {
                    result.diagnostics.emplace_back(
                        std::string("CharQuad 接受了被改写的 ")
                        + tamper.label
                        + "（固定源拓扑与 CPU 契约分叉却不报错）");
                }
            }
        }

        // ── ⑧ 反射取证：驱动真正读到的那份 SPIR-V 里的执行模式 ──
        //
        // ①④ 只证明文本里有那串数字，⑤⑥⑦ 只证明装配期把文本解析回了数值并比对。
        // 这一条钉住第三段：编译产物里的 LocalSize / OutputVertices /
        // OutputPrimitivesEXT。三者任一分叉，CPU 按常量派发就与 GPU 实际执行规模不符。
        // 执行模式枚举按 SPIR-V 规范写死（LocalSize=17 / OutputVertices=26 /
        // OutputPrimitivesEXT=5270），与 `PM.compute-release-cache` 写死 112 同理：
        // 若两侧一起漂，本门才会失败。
        if (plain_mesh && !plain_mesh->GetFinalGLSL().empty())
        {
            // 网格主着色器 include `generated/common_options.glsl`（阶段间 varying
            // 开关，两 stage 共用一份）。裸 `CompileShader` 只有库根，解析不到这份
            // 生成文件；必须先把 options 补进片段的 options 根，再走生产的双根编译。
            FixedFragmentSource toggle_fixed;
            MaterialProgramBuildCapture toggle_capture{};
            std::unique_ptr<ShaderBuildContext> toggle_ctx =
                build(nullptr, definition, nullptr, &toggle_fixed, &toggle_capture);

            hgl::AnsiString compile_log;
            const uint32_t *spv_words = nullptr;
            uint32_t spv_size = 0u;
            if (!toggle_ctx)
            {
                compile_log = "material build failed (no options root)";
            }
            else if (!hgl::graph::mtl::WriteInterStageVaryingOptionsSource(
                         toggle_fixed.options_root,
                         toggle_capture.interstage_varying_options))
            {
                compile_log =
                    hgl::graph::mtl::GetLastDomainShaderBuildBackendError();
            }
            else if (!CompileFixedMeshSource(*toggle_ctx, toggle_fixed,
                                             compile_log))
            {
                // compile_log 已由后端写入
            }
            else
            {
                const ShaderCreateInfo *compiled =
                    toggle_ctx->GetStageShader(ShaderStage::Mesh);
                if (compiled && compiled->HasSPVData())
                {
                    spv_words = compiled->GetSPVData();
                    spv_size = compiled->GetSPVSize();
                }
            }

            if (!spv_words || spv_size == 0u)
            {
                result.diagnostics.emplace_back(
                    "CharQuad mesh stage failed to compile: "
                    + std::string(compile_log.c_str() ? compile_log.c_str() : ""));
            }
            else
            {
                hgl::ValueArray<uint8_t> storage;
                ShaderFlatView view{};
                if (!ReflectSPVData(
                        spv_words, spv_size,
                        static_cast<uint32_t>(VK_SHADER_STAGE_MESH_BIT_EXT),
                        "main", storage, view))
                {
                    result.diagnostics.emplace_back(
                        "CharQuad mesh stage SPIR-V reflection failed");
                }
                else
                {
                    struct ReflectedMode
                    {
                        const char *label;
                        uint32_t    expected;
                    };

                    // 17 = LocalSize / 26 = OutputVertices / 5270 = OutputPrimitivesEXT。
                    // 写死枚举与 `PM.compute-release-cache` 写死 112 同理：两侧一起漂
                    // 才会漏过本门。
                    const ReflectedMode modes[3] =
                    {
                        { "LocalSize", TEXT_CHARQUAD_MAX_INVOCATIONS },
                        { "OutputVertices", charquad_capacity.max_vertices },
                        { "OutputPrimitivesEXT", charquad_capacity.max_primitives },
                    };

                    for (uint32_t m = 0; m < 3u; ++m)
                    {
                        uint32_t actual = 0;
                        // 数组顺序与 modes[] 一致：0=LocalSize 走 331/LocalSizeId
                        // 回退，1/2 是字面量 `OpExecutionMode`。
                        const bool found = (m == 0u)
                            ? FindSPVLocalSizeX(view, actual)
                            : FindSPVExecutionModeLiteral(
                                  view, (m == 1u) ? 26u : 5270u, 0, actual);

                        if (!found)
                            result.diagnostics.emplace_back(
                                std::string("CharQuad mesh SPIR-V has no ")
                                + modes[m].label + " execution mode");
                        else if (actual != modes[m].expected)
                            result.diagnostics.emplace_back(
                                std::string("CharQuad mesh SPIR-V ")
                                + modes[m].label + " = " + std::to_string(actual)
                                + " != " + std::to_string(modes[m].expected)
                                + "（CPU 派发契约与实际执行规模分叉）");
                    }
                }
                // SPV 归 toggle_ctx 所有，此处不得释放。
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// `DrawTriangle` 的真实形状：`VertexColor` 域 + 2 分量整数位置。
    ///
    /// 失败现场（用户运行 DrawTriangle 示例时）：
    /// ```
    /// [VertexNodeConfigResolver.h:30] 位置映射与顶点位置输入不匹配：
    ///   position_mapping=Passthrough3D 需要 Position 分量数 3，
    ///   而 Stage 1 输入模块提供 2 个分量（VertexColor）
    /// ```
    /// 根因是 `vertex_node_config` 有四层来源（request 覆盖 > recipe >
    /// definition > 默认），而 ABI 对账读的是 `definition` 里最弱的那一层。
    /// 阶段 4.4 用一行回写修掉了它（`plan.vertex_definition.vertex_node_config
    /// = plan.vertex_node_config;`）——本用例就是那行的看守：`VertexColor`
    /// 的 definition 是 `Passthrough3D`（3 分量），而 DrawTriangle 的 recipe
    /// 是 `Make2DNodeConfigOrtho`（2 分量），只有回写生效时两者才一致。
    ///
    /// 同时守第二件事：`VK_FORMAT_R16G16_SINT` 必须选到 4B packed int16x2 的
    /// `s1_position_vec2i.glsl`，不是 8B 浮点 `s1_position_vec2.glsl`——两者
    /// 都能编译通过，选错了整批顶点静默错位。
    GateResult RunDrawTriangleVertexColor2DCase()
    {
        GateResult result;
        result.name = "L0.draw-triangle-vertex-color-2d";

        MaterialRecipe recipe{};
        recipe.domain = MaterialDomain::VertexColor;

        ResolvedMaterial definition{};
        if (!ResolveMaterialFromRecipe(recipe, definition))
        {
            result.diagnostics.emplace_back("missing definition: VertexColor");
            result.passed = false;
            return result;
        }

        // DrawTriangle 的几何：像素坐标是 int16x2（`VF_V2I16`），不是 float。
        GeometryVertexFormat geometry;
        if (!geometry.Add(VertexSemantic::Position, VF_V2I16, 0, 0)
         || !geometry.Add(VertexSemantic::Color, VF_V4F, 0, 0))
        {
            result.diagnostics.emplace_back("cannot build DrawTriangle geometry");
            result.passed = false;
            return result;
        }

        // 顶点解码 / 位置映射 / 投影全部落在 `generated/mesh_options.glsl` 里，
        // 网格主着色器文本对同形状是逐字节相同的。断言必须看 options 文本，
        // 否则「形状对了但顶点格式选错」会静默通过。
        hgl::AnsiString mesh_options_text;

        const auto build = [&geometry, &mesh_options_text, &recipe](
                               const ResolvedMaterial &def,
                               const VertexShaderNodeConfig &node_cfg,
                               const GeometryVertexFormat *gvf)
            -> std::unique_ptr<ShaderBuildContext>
        {
            mesh_options_text.Clear();

            MaterialBuildRequest request{};
            request.recipe = recipe;
            request.recipe.vertex_node_config = node_cfg;
            request.recipe.render_state_overrides.has_alpha_test = false;
            request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;
            request.geometry_vertex_format = gvf;
            // 只到 GLSL 文本为止：本用例断言的是发射文本，不需要 SPV。
            request.defer_finalize = true;

            PassProfileValidationDiagnostic template_diagnostic{};
            const ScenePassProfile scene_profile =
                MakeForwardUnlitProfile();
            if (!ResolveScenePassProfileRequest(
                    PassProfileID::ForwardUnlit,
                    hgl::graph::ShaderStage::Fragment,
                    scene_profile, request.render_template_request,
                    template_diagnostic)
             || !AppendMaterialPassProfileRoots(
                    def, request.render_template_request))
                return std::unique_ptr<ShaderBuildContext>();

            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(def, request, fixed))
                return std::unique_ptr<ShaderBuildContext>();
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;

            MaterialProgramBuildCapture capture{};
            std::unique_ptr<ShaderBuildContext> ctx(
                AssembleMaterialProgram(nullptr, request, def, &capture, fixed_stage));
            mesh_options_text = capture.mesh_options;
            return ctx;
        };

        const VertexShaderNodeConfig node_2d = Make2DNodeConfigOrtho(false);

        // ── ① recipe 的 2D 节点配置 + 2 分量位置 ⇒ 必须构建成功 ──
        // 回写若失效，ABI 会读 definition 的 Passthrough3D（3 分量）与 s1 的
        // 2 分量对不上，这里直接返回 null。
        const auto ctx = build(definition, node_2d, &geometry);
        if (!ctx)
        {
            result.diagnostics.emplace_back(
                "DrawTriangle 形状构建失败（recipe 的 2D 节点配置未回写到 ABI 求解用的定义）");
        }
        else
        {
            const ShaderCreateInfo *mesh = ctx->GetStageShader(ShaderStage::Mesh);
            if (!mesh)
            {
                result.diagnostics.emplace_back(
                    "DrawTriangle 形状没有网格阶段");
            }
            else
            {
                // 断言看 options 文本：顶点解码与映射都在那里，主着色器已不含这些。
                const std::string glsl = mesh_options_text.c_str()
                    ? std::string(mesh_options_text.c_str())
                    : std::string();

                // ── ② 位置解码模块必须是 packed int16x2 那一份 ──
                if (glsl.find("#include \"module/vertex/stream_position_vec2i.glsl\"")
                        == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "R16G16_SINT 没有选到 module/vertex/stream_position_vec2i.glsl"
                        "（4B packed int16x2 与 8B float vec2 是两个 ABI）");
                }
                if (glsl.find("#include \"module/vertex/stream_position_vec2.glsl\"")
                        != std::string::npos
                 || glsl.find("#include \"module/vertex/stream_position_vec3.glsl\"")
                        != std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "位置解码模块指向浮点 vec2/vec3（与 R16G16_SINT 不匹配）");
                }

                // ── ③ 映射必须取 recipe 的 PixelToLocal，而不是 definition 的 Passthrough3D ──
                if (glsl.find("#include \"module/vertex/position_pixel_to_local.glsl\"")
                        == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "网格阶段没有使用 recipe 的 s2_pixel_to_local 映射");
                }
                if (glsl.find("#include \"module/vertex/position_passthrough3d.glsl\"")
                        != std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "网格阶段用了 definition 的 Passthrough3D 映射（recipe 覆盖丢失）");
                }

                // ── ④ 该形状必须在形状 master 目录里有一份，且内容就是这份 GLSL ──
                // ShaderGen 删除后没有生成器兜底：目录里缺这一份，该域就没有
                // mesh 阶段可编译。按内容比对（而不是按形状键硬编码文件名），
                // 重新捕获时只需替换文件，门禁仍然有效。
                //
                // 形状主着色器只在 `topology/` 下（`mesh/fixed/` 已删除）。
                bool matched = false;
                {
                    // 生产文本就是形状 master 本身（options 摘要已迁出文本），
                    // 逐字节比对，不做任何剥离。
                    const std::string body = mesh->GetFinalGLSL();

                    const std::filesystem::path dirs[1] =
                    {
                        RepoRootPath("ShaderLibrary/topology")
                    };
                    try
                    {
                        for (const std::filesystem::path &fixed_dir : dirs)
                        {
                            if (matched
                             || !std::filesystem::exists(fixed_dir))
                                continue;
                            for (const auto &entry :
                                 std::filesystem::directory_iterator(fixed_dir))
                            {
                                if (!entry.is_regular_file()
                                 || entry.path().extension() != ".mesh")
                                    continue;
                                if (ReadFileText(entry.path().string()) == body)
                                {
                                    matched = true;
                                    break;
                                }
                            }
                        }
                    }
                    catch (const std::filesystem::filesystem_error &)
                    {
                        result.diagnostics.emplace_back(
                            "cannot enumerate ShaderLibrary/topology");
                    }
                }
                if (!matched)
                {
                    result.diagnostics.emplace_back(
                        "ShaderLibrary/topology 没有与该形状一致的 .mesh"
                        "（删除 ShaderGen 后该域将无 mesh 阶段可用）");
                }
            }
        }

        // ── ⑤ AutoInstance 形状：VF_V2F + Make2DNodeConfigNDC(true) ──
        GeometryVertexFormat autoinstance_geometry;
        if (!autoinstance_geometry.Add(VertexSemantic::Position, VF_V2F, 0, 0)
         || !autoinstance_geometry.Add(VertexSemantic::Color, VF_V4F, 0, 0))
        {
            result.diagnostics.emplace_back("cannot build AutoInstance geometry");
        }
        else
        {
            const VertexShaderNodeConfig autoinstance_node = Make2DNodeConfigNDC(true);
            const auto autoinstance_ctx = build(definition, autoinstance_node, &autoinstance_geometry);
            if (!autoinstance_ctx)
            {
                result.diagnostics.emplace_back("AutoInstance 形状构建失败");
            }
            else
            {
                const ShaderCreateInfo *mesh = autoinstance_ctx->GetStageShader(ShaderStage::Mesh);
                if (!mesh)
                    result.diagnostics.emplace_back("AutoInstance 没有网格阶段");
                else
                {
                    const std::string glsl = mesh_options_text.c_str()
                        ? std::string(mesh_options_text.c_str())
                        : std::string();
                    if (glsl.find("#include \"module/vertex/stream_position_vec2.glsl\"") == std::string::npos)
                        result.diagnostics.emplace_back("AutoInstance 没有选到 s1_position_vec2.glsl");
                    if (glsl.find("#include \"module/vertex/position_ndc_lift.glsl\"") == std::string::npos)
                        result.diagnostics.emplace_back("AutoInstance 没有选到 s2_ndc_lift.glsl");
                    if (glsl.find("#include \"module/vertex/transform_l2w_only.glsl\"") == std::string::npos)
                        result.diagnostics.emplace_back("AutoInstance 没有选到 s3_l2w_only.glsl");
                }
            }
        }

        // ── ⑥ 2 分量几何 + definition 默认的 3D 节点配置 ⇒ 必须构建失败 ──
        // 这是「没有回写时会发生什么」的等价现场：两个所有者不匹配必须硬失败，
        // 不得产出一份编不过（或能编过但顶点错位）的 GLSL。
        if (build(definition, definition.vertex_node_config, &geometry))
        {
            result.diagnostics.emplace_back(
                "Passthrough3D + 2 分量位置几何被接受（分量数对账失效）");
        }

        // ── ⑦ 3 分量几何 + 2D 节点配置 ⇒ 同样必须失败 ──
        // 反方向：证明对账不是「只认 recipe」的单向放行。
        GeometryVertexFormat geometry_3d;
        if (!geometry_3d.Add(VertexSemantic::Position, VF_V3F, 0, 0)
         || !geometry_3d.Add(VertexSemantic::Color, VF_V4F, 0, 0))
        {
            result.diagnostics.emplace_back("cannot build 3D control geometry");
        }
        else if (build(definition, node_2d, &geometry_3d))
        {
            result.diagnostics.emplace_back(
                "PixelToLocal（2 分量）+ 3 分量位置几何被接受（分量数对账失效）");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunShaderSemanticRegistryCase()
    {
        GateResult result;
        result.name = "L0.shader-semantic-registry";


        ShaderSemanticRegistryValidationResult validation{};
        if (!ValidateInterStageSemanticRegistry(validation))
        {
            result.diagnostics.emplace_back(
                "semantic registry validation failed: error="
                + std::to_string(static_cast<hgl::uint32>(validation.error))
                + " first=" + std::to_string(validation.first_index)
                + " second=" + std::to_string(validation.second_index));
        }

        if (GetInterStageSemanticInfoCount()
                != static_cast<hgl::uint32>(InterStageSemantic::RANGE_SIZE))
        {
            result.diagnostics.emplace_back("semantic registry coverage mismatch");
        }

        const InterStageSemanticInfo *data_index =
            GetInterStageSemanticInfo(InterStageSemantic::DataIndexID);
        const InterStageSemanticInfo *color =
            GetInterStageSemanticInfo(InterStageSemantic::Color);
        if (!data_index
         || data_index->stable_location != 0
         || data_index->interpolation != InterStageInterpolation::Flat
         || !color
         || color->stable_location != 5)
        {
            result.diagnostics.emplace_back(
                "inter-stage stable ABI metadata mismatch");
        }

        // 生成器已物理删除：原来这里拿「生成器产出的 lit/color 顶点文本」当
        // varying ABI 的判据。现在稳定 ABI 的唯一真源就是上面这张注册表，
        // 而「库里的主着色器是否照着它发射」由 MV.fixed-mesh-varying-invariants
        // 检查三份拓扑 master —— 两边分工，避免一处失败两处报。

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// C++ 表结构（GlobalAddresses.h）与 GLSL 侧 GlobalAddressesRef 必须逐字段同序同型。
    /// 为什么必须门禁：C++ 加了字段而 GLSL 漏加时，材质 shader 编译失败会**静默回退默认材质**
    /// ——画面照常有内容、其余门全绿、0 校验层消息，只有渲染契约门（ATS D3）才抓得到。
    GateResult RunGlobalAddressesStructParityCase()
    {
        GateResult result;
        result.name = "S.global-addresses-struct-parity";

        const std::string cpp_text =
            ReadFileText(RepoRootPath("inc/hgl/graph/ubo/GlobalAddresses.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/module/descriptor/ssbo/global_addresses.glsl"));

        if (cpp_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 GlobalAddresses.h / module/descriptor/ssbo/global_addresses.glsl（结构对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        const std::vector<std::string> cpp_fields =
            ExtractU64Fields(cpp_text, "GlobalAddresses");
        const std::vector<std::string> glsl_fields =
            ExtractU64Fields(glsl_text, "GlobalAddressesRef");

        if (cpp_fields.empty() || glsl_fields.empty())
        {
            result.diagnostics.emplace_back(
                "GlobalAddresses 字段提取为空（结构标记被改写？）C++="
                + std::to_string(cpp_fields.size())
                + " GLSL=" + std::to_string(glsl_fields.size()));
            result.passed = result.diagnostics.empty();
            return result;
        }

        if (cpp_fields != glsl_fields)
        {
            std::string detail = "GlobalAddresses 字段漂移（C++ "
                + std::to_string(cpp_fields.size()) + " 项 / GLSL "
                + std::to_string(glsl_fields.size()) + " 项）：";
            const size_t count = cpp_fields.size() > glsl_fields.size()
                               ? cpp_fields.size() : glsl_fields.size();
            for (size_t i = 0; i < count; ++i)
            {
                const std::string lhs =
                    i < cpp_fields.size() ? cpp_fields[i] : std::string("<缺>");
                const std::string rhs =
                    i < glsl_fields.size() ? glsl_fields[i] : std::string("<缺>");
                if (lhs != rhs)
                {
                    detail += "第 " + std::to_string(i) + " 项 C++=" + lhs
                            + " GLSL=" + rhs;
                    break;
                }
            }
            detail += "；漏加字段会让材质 shader 静默回退默认材质";
            result.diagnostics.emplace_back(detail);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// C++ 世界地址表（WorldAddresses.h）与 GLSL 侧 WorldAddressesRef 必须逐字段同序同型。
    /// 与 S.global-addresses-struct-parity 同理：漏加字段 = shader 静默回退默认材质 / 解引用错地址。
    GateResult RunWorldAddressesStructParityCase()
    {
        GateResult result;
        result.name = "W.world-addresses-struct-parity";

        const std::string cpp_text =
            ReadFileText(RepoRootPath("inc/hgl/graph/ubo/WorldAddresses.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/module/descriptor/ssbo/world_addresses.glsl"));

        if (cpp_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 WorldAddresses.h / module/descriptor/ssbo/world_addresses.glsl（结构对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        const std::vector<std::string> cpp_fields =
            ExtractU64Fields(cpp_text, "WorldAddresses");
        const std::vector<std::string> glsl_fields =
            ExtractU64Fields(glsl_text, "WorldAddressesRef");

        if (cpp_fields.empty() || glsl_fields.empty())
        {
            result.diagnostics.emplace_back(
                "WorldAddresses 字段提取为空（结构标记被改写？）C++="
                + std::to_string(cpp_fields.size())
                + " GLSL=" + std::to_string(glsl_fields.size()));
            result.passed = result.diagnostics.empty();
            return result;
        }

        if (cpp_fields != glsl_fields)
        {
            std::string detail = "WorldAddresses 字段漂移（C++ "
                + std::to_string(cpp_fields.size()) + " 项 / GLSL "
                + std::to_string(glsl_fields.size()) + " 项）：";
            const size_t count = cpp_fields.size() > glsl_fields.size()
                               ? cpp_fields.size() : glsl_fields.size();
            for (size_t i = 0; i < count; ++i)
            {
                const std::string lhs =
                    i < cpp_fields.size() ? cpp_fields[i] : std::string("<缺>");
                const std::string rhs =
                    i < glsl_fields.size() ? glsl_fields[i] : std::string("<缺>");
                if (lhs != rhs)
                {
                    detail += "第 " + std::to_string(i) + " 项 C++=" + lhs
                            + " GLSL=" + rhs;
                    break;
                }
            }
            detail += "；漏加字段会让世界私有地址（相机行/渲染项）解引用错地址";
            result.diagnostics.emplace_back(detail);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 机制层契约（A6-b）：**批信息行** + **批的 dispatch 档**两端同序同型同值。
    /// · `BatchInfoRow`（16 B：row_base / capacity / topology / group_size）是 gather 逐批读的
    ///   切片描述 —— 漂移 = 按错偏移读切片（静默错位/错画）；
    /// · 档号（`kBatchTopologyTriangles/Lines` ↔ GLSL `#define TOPOLOGY_*`）**两处各写一遍**
    ///   ⇒ 漂移 = 线的 X 公式选错档（按顶点数而非段数）⇒ 整批线段少画/错画。
    /// 两条都是 S./E. 同族的**静默**错位坑（像素判据只在特定角度才看得见），必须门禁。
    GateResult RunBatchInfoStructParityCase()
    {
        GateResult result;
        result.name = "B.batch-info-struct-parity";

        const std::string cpp_text =
            ReadFileText(RepoRootPath("inc/hgl/ecs/support/cull/GPUCullSystem.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl"));
        // 派发档 / 遍数的 GLSL 侧数值已收敛为 ShaderConstGen 生成物
        // （真源 inc/hgl/shader/contract/GpuCullContract.h）；entity_gpu_cull.glsl 只 #include。
        const std::string glsl_const_text =
            ReadFileText(RepoRootPath("ShaderLibrary/contract/engine_constants.glsl"));

        if (cpp_text.empty() || glsl_text.empty() || glsl_const_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 GPUCullSystem.h / entity_gpu_cull.glsl / engine_constants.glsl"
                "（结构对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        // ── ① 行形状：逐字段同序同型（类型/长度已归一） ──
        const std::vector<StructFieldEntry> cpp_fields =
            ExtractStructFieldsLineWise(cpp_text, "BatchInfoRow");
        const std::vector<StructFieldEntry> glsl_fields =
            ExtractStructFieldsLineWise(glsl_text, "BatchInfoRow");

        if (cpp_fields.empty() || glsl_fields.empty())
        {
            result.diagnostics.emplace_back(
                "BatchInfoRow 字段提取为空（块头被改写？）C++="
                + std::to_string(cpp_fields.size())
                + " GLSL=" + std::to_string(glsl_fields.size()));
            result.passed = result.diagnostics.empty();
            return result;
        }

        if (cpp_fields.size() != glsl_fields.size())
        {
            result.diagnostics.emplace_back(
                "BatchInfoRow 字段数漂移：C++ " + std::to_string(cpp_fields.size())
                + " 项 / GLSL " + std::to_string(glsl_fields.size())
                + " 项；漂移 = gather 按错偏移读切片（静默错位/错画）");
        }
        else
        {
            for (size_t i = 0; i < cpp_fields.size(); ++i)
            {
                if (cpp_fields[i].type == glsl_fields[i].type
                 && cpp_fields[i].name == glsl_fields[i].name
                 && cpp_fields[i].dim  == glsl_fields[i].dim)
                    continue;

                result.diagnostics.emplace_back(
                    "BatchInfoRow 第 " + std::to_string(i) + " 字段漂移：C++="
                    + cpp_fields[i].type + " " + cpp_fields[i].name
                    + " / GLSL=" + glsl_fields[i].type + " " + glsl_fields[i].name
                    + "；漂移 = gather 按错偏移读切片（静默错位/错画）");
                break;
            }
        }

        // ── ② 派发档 / 每 pass 块常量：C++ 常量 ↔ GLSL #define 数值一致（两处各写
        //    一遍的漂移面；格 = 整数常量，故与档号同表） ──
        struct TierPair
        {
            const char *cpp_token;
            const char *glsl_token;
        };

        const TierPair tiers[] =
        {
            { "kBatchTopologyTriangles", "#define TOPOLOGY_TRIANGLES" },
            { "kBatchTopologyLines",     "#define TOPOLOGY_LINES"     },
            // T4c2：批行/计数按 pass 分块 ⇒ 遍数两端各写一遍（漂移 = 阴影块永不被写或被越界写）
            // ⚠ C++ 端令牌必须带类型+声明符：`kArenaPasses` 的**首次出现**是
            //   GetBatchLayout 的越界判断（同一行无数字）⇒ 只用标识符会提取失败误报。
            { "static constexpr uint32_t kArenaPasses", "#define ULRE_ARENA_PASSES" },
        };

        for (const TierPair &tier : tiers)
        {
            const int cpp_value  = ExtractFirstIntAfter(cpp_text,  tier.cpp_token);
            const int glsl_value = ExtractFirstIntAfter(glsl_const_text, tier.glsl_token);

            if (cpp_value < 0 || glsl_value < 0)
            {
                result.diagnostics.emplace_back(
                    std::string("派发档找不到数值：C++ ") + tier.cpp_token
                    + " / GLSL " + tier.glsl_token
                    + "（常量改名？两端各写一遍，改名必须同步）");
                continue;
            }

            if (cpp_value != glsl_value)
            {
                result.diagnostics.emplace_back(
                    std::string("派发档数值漂移：") + tier.cpp_token + "="
                    + std::to_string(cpp_value) + " vs " + tier.glsl_token + "="
                    + std::to_string(glsl_value)
                    + "；档号错 = 线/网格的 X 公式选错（整批少画/错画）");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// C++ EntityGPU（inc/hgl/ecs/core/entity/EntityGPU.h）与 GLSL 侧 EntityGPUData
    /// （ShaderLibrary/module/descriptor/ecs/entity_gpu.glsl）必须逐字段同序同型同长度（§7.4 / §8.3-③）。
    /// 为什么必须门禁：这是 128 B 行的 GPU 镜像 —— 漂移 = 剔除着色器按错步长解引用
    /// 实体行（W.world-addresses-struct-parity 同族的静默错位坑）。
    /// 两端唯一容许的形状差 = GLSL 行尾**显式**填充 `_pad`（scalar layout 下 GLSL
    /// 不会隐式补 alignas 的 40 B，不写死则数组步长 = 88 而非 128）：比对时要求
    /// C++ 字段列表是 GLSL 字段列表的前缀，余量全部由 `_pad*` 承担，且 GLSL 侧
    /// 总存储尺寸必须) 的头文件断言同值，此处对拍）。
    GateResult RunEntityGPUStructParityCase()
    {
        GateResult result;
        result.name = "E.entity-gpu-struct-parity";

        const std::string cpp_text =
            ReadFileText(RepoRootPath("inc/hgl/ecs/core/entity/EntityGPU.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/module/descriptor/ecs/entity_gpu.glsl"));

        if (cpp_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 EntityGPU.h / entity_gpu.glsl（结构对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        const std::vector<StructFieldEntry> cpp_fields =
            ExtractStructFieldsLineWise(cpp_text, "EntityGPU");
        const std::vector<StructFieldEntry> glsl_fields =
            ExtractStructFieldsLineWise(glsl_text, "EntityGPUData");

        const auto fmt = [](const std::vector<StructFieldEntry> &fields) -> std::string
        {
            std::string out;
            for (const StructFieldEntry &f : fields)
            {
                if (!out.empty()) out += ", ";
                out += f.type + " " + f.name;
                if (!f.dim.empty()) out += "[" + f.dim + "]";
            }
            return out;
        };

        if (cpp_fields.empty() || glsl_fields.empty())
        {
            result.diagnostics.emplace_back(
                std::string("EntityGPU 字段提取为空（结构标记被改写？）取块器 = 逐行式，")
                + "C++= " + fmt(cpp_fields) + " | GLSL= " + fmt(glsl_fields));
            result.passed = result.diagnostics.empty();
            return result;
        }

        // C++ 字段必须是 GLSL 字段的前缀（同序同型同长度逐字段比对）
        const size_t common = cpp_fields.size() < glsl_fields.size()
                            ? cpp_fields.size() : glsl_fields.size();
        for (size_t i = 0; i < common; ++i)
        {
            if (cpp_fields[i].type != glsl_fields[i].type
             || cpp_fields[i].name != glsl_fields[i].name
             || cpp_fields[i].dim  != glsl_fields[i].dim)
            {
                result.diagnostics.emplace_back(
                    "EntityGPU 字段漂移：第 " + std::to_string(i) + " 项 C++="
                    + cpp_fields[i].type + " " + cpp_fields[i].name
                    + (cpp_fields[i].dim.empty() ? "" : "[" + cpp_fields[i].dim + "]")
                    + " GLSL=" + glsl_fields[i].type + " " + glsl_fields[i].name
                    + (glsl_fields[i].dim.empty() ? "" : "[" + glsl_fields[i].dim + "]")
                    + "；两端必须同序同型同长度");
                result.passed = false;
                return result;
            }
        }
        if (cpp_fields.size() != glsl_fields.size())
        {
            // 余量必须全是显式填充 `_pad*`，否则 = 一端多出真实字段
            const std::vector<StructFieldEntry> &extra
                = cpp_fields.size() > glsl_fields.size() ? cpp_fields : glsl_fields;
            for (size_t i = common; i < extra.size(); ++i)
            {
                if (extra[i].name.rfind("_pad", 0) != 0)
                {
                    result.diagnostics.emplace_back(
                        "EntityGPU 字段数不一致且余量不是显式填充：多余项 "
                        + extra[i].type + " " + extra[i].name
                        + "（C++= " + fmt(cpp_fields) + " | GLSL= " + fmt(glsl_fields)
                        + "）；GLSL 只容许行尾 _pad* 承担 alignas(64) 的 40 B 填充");
                    result.passed = false;
                    return result;
                }
            }
        }

        // GLSL 总存储尺寸必须恰为 128 B（数组步长契约：88 载荷 + 显式 _pad）
        std::string glsl_stripped = glsl_text;
        StripCommentTokens(glsl_stripped);
        const auto glsl_defines = CollectDefineConstantsWithContract(glsl_stripped);
        size_t glsl_bytes = 0;
        for (const StructFieldEntry &f : glsl_fields)
        {
            size_t bytes = 0;
            if (!StructFieldStorageSize(f, glsl_defines, bytes))
            {
                result.diagnostics.emplace_back(
                    "EntityGPU GLSL 字段 " + f.name + " 的数组长度不是可求值常数："
                    + f.dim);
                result.passed = false;
                return result;
            }
            glsl_bytes += bytes;
        }
        if (glsl_bytes != 128)
        {
            result.diagnostics.emplace_back(
                "EntityGPUData 的 GLSL 总存储 = " + std::to_string(glsl_bytes)
                + " B，必须 == 128 B（sizeof(EntityGPU)；scalar layout 无隐式填充，"
                "差值必须由行尾 _pad 显式写死）");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// C++ `LightRow`（inc/hgl/graph/ubo/LightRow.h）与 GLSL 侧 `LightRowData`
    /// （ShaderLibrary/module/descriptor/ubo/scene_ubo.glsl）必须逐字段同序同型同长度（T6b）。
    /// 为什么必须门禁：这是光源的 64 B GPU 行 —— 漂移 = 光照按错偏移取值（方向/颜色/强度
    /// 静默变垃圾，画面偏暗或光从侧面来，而没有任何报错）。
    /// 手法与 E.entity-gpu-struct-parity 同（逐行取块器），差别两条：
    ///   ① 字段数必须**相等**（LightRow 的填充两端都显式写 `_pad*`，没有 alignas 余量）；
    ///   ② GLSL 总存储必须 == 64 B（数组步长契约；C++ 侧另有 sizeof 静态断言把守）。
    GateResult RunLightRowStructParityCase()
    {
        GateResult result;
        result.name = "W.light-row-struct-parity";

        const std::string cpp_text =
            ReadFileText(RepoRootPath("inc/hgl/graph/ubo/LightRow.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/module/descriptor/ssbo/light_row.glsl"));

        if (cpp_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 LightRow.h / module/descriptor/ssbo/light_row.glsl（结构对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        const std::vector<StructFieldEntry> cpp_fields =
            ExtractStructFieldsLineWise(cpp_text, "LightRow");
        const std::vector<StructFieldEntry> glsl_fields =
            ExtractStructFieldsLineWise(glsl_text, "LightRowData");

        const auto fmt = [](const std::vector<StructFieldEntry> &fields) -> std::string
        {
            std::string out;
            for (const StructFieldEntry &f : fields)
            {
                if (!out.empty()) out += ", ";
                out += f.type + " " + f.name;
                if (!f.dim.empty()) out += "[" + f.dim + "]";
            }
            return out;
        };

        if (cpp_fields.empty() || glsl_fields.empty())
        {
            result.diagnostics.emplace_back(
                std::string("LightRow 字段提取为空（结构标记被改写？）取块器 = 逐行式，")
                + "C++= " + fmt(cpp_fields) + " | GLSL= " + fmt(glsl_fields));
            result.passed = result.diagnostics.empty();
            return result;
        }

        if (cpp_fields.size() != glsl_fields.size())
        {
            result.diagnostics.emplace_back(
                "LightRow 字段数不一致（C++ " + std::to_string(cpp_fields.size())
                + " 项 / GLSL " + std::to_string(glsl_fields.size())
                + " 项）：两端逐字段同序同型，填充也必须两端显式写 `_pad*`"
                "（C++= " + fmt(cpp_fields) + " | GLSL= " + fmt(glsl_fields) + "）");
            result.passed = false;
            return result;
        }

        for (size_t i = 0; i < cpp_fields.size(); ++i)
        {
            if (cpp_fields[i].type != glsl_fields[i].type
             || cpp_fields[i].name != glsl_fields[i].name
             || cpp_fields[i].dim  != glsl_fields[i].dim)
            {
                result.diagnostics.emplace_back(
                    "LightRow 字段漂移：第 " + std::to_string(i) + " 项 C++="
                    + cpp_fields[i].type + " " + cpp_fields[i].name
                    + (cpp_fields[i].dim.empty() ? "" : "[" + cpp_fields[i].dim + "]")
                    + " GLSL=" + glsl_fields[i].type + " " + glsl_fields[i].name
                    + (glsl_fields[i].dim.empty() ? "" : "[" + glsl_fields[i].dim + "]")
                    + "；两端必须同序同型同长度（改一侧必改另一侧）");
                result.passed = false;
                return result;
            }
        }

        // GLSL 总存储必须恰为 64 B（行步长契约；不写行尾 `_pad1[2]` 会得 48 B）
        std::string glsl_stripped = glsl_text;
        StripCommentTokens(glsl_stripped);
        const auto glsl_defines = CollectDefineConstantsWithContract(glsl_stripped);
        size_t glsl_bytes = 0;
        for (const StructFieldEntry &f : glsl_fields)
        {
            size_t bytes = 0;
            if (!StructFieldStorageSize(f, glsl_defines, bytes))
            {
                result.diagnostics.emplace_back(
                    "LightRow GLSL 字段 " + f.name + " 的数组长度不是可求值常数：" + f.dim);
                result.passed = false;
                return result;
            }
            glsl_bytes += bytes;
        }
        if (glsl_bytes != 64)
        {
            result.diagnostics.emplace_back(
                "LightRowData 的 GLSL 总存储 = " + std::to_string(glsl_bytes)
                + " B，必须 == 64 B（sizeof(LightRow)；scalar layout 无隐式填充，"
                  "差值必须由行尾 `_pad` 显式写死）");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// X 列表（`HGL_MESH_DRAW_PARAMS_FIELD_LIST`，ShaderBufferSources.h）前 9 个头部
    /// 字段与剔除着色器里的**头部镜像**（`MeshDrawParamsHeadData`，entity_gpu_cull.glsl）
    /// 必须逐字段同序同型（E.entity-gpu-struct-parity 同族手法；§7.6 取块器复用）。
    /// 为什么必须门禁（T3f-2 形态 B）：镜像是 gather 里 `groupCountX` 的**唯一读源**
    /// ——X 列表头部漂移（改名/调序/插字段）而镜像没跟 ⇒ CS 按错偏移读 is_indexed/
    /// index_count/vertex_count ⇒ 组数算错 = 静默画错/画漏，实机与现有门都抓不到。
    /// 镜像只承诺头部前 9 项（尾部 11×uint64 地址不进镜像 ⇒ 不做全表前缀断言），
    /// 但**步长必须 ==104**（池行 sizeof 契约；`_pad` 承担余量，与 entity_gpu 同纪律）。
    GateResult RunMeshDrawParamsHeadParityCase()
    {
        GateResult result;
        result.name = "E.mesh-draw-params-head-parity";

        const std::string cpu_text =
            ReadFileText(RepoRootPath("inc/hgl/graph/ShaderBufferSources.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl"));

        if (cpu_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 ShaderBufferSources.h / entity_gpu_cull.glsl（头部对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        // 端 1：X 列表前 9 项 `M(name, "glsl_type", cpu_type)`（宏定义体按行取）。
        const size_t xl = cpu_text.find("#define HGL_MESH_DRAW_PARAMS_FIELD_LIST(M)");
        if (xl == std::string::npos)
        {
            result.diagnostics.emplace_back(
                "X 列表宏 `HGL_MESH_DRAW_PARAMS_FIELD_LIST` 未找到（改名?）");
            result.passed = false;
            return result;
        }

        std::vector<StructFieldEntry> x_fields;
        {
            std::istringstream stream(cpu_text.substr(xl));
            std::string line;
            while (std::getline(stream, line))
            {
                // X 列表体每行 = `M(name, "type", cpu_type) \`；首个不以 M( 开头的
                // 非续行即列表结束（`#undef` 前）
                const size_t m = line.find("M(");
                if (m == std::string::npos)
                {
                    if (!x_fields.empty())
                        break;
                    continue;
                }

                // name
                const size_t n0 = m + 2;
                const size_t n1 = line.find(',', n0);
                // "glsl_type"
                const size_t q0 = line.find('"', n1);
                const size_t q1 = line.find('"', q0 + 1);
                if (n1 == std::string::npos || q0 == std::string::npos
                 || q1 == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "X 列表行解析失败: " + line);
                    break;
                }

                StructFieldEntry f;
                f.name = line.substr(n0, n1 - n0);
                // 去名字前后空白
                const size_t ns = f.name.find_first_not_of(" \t\r");
                const size_t ne = f.name.find_last_not_of(" \t\r");
                f.name = (ns == std::string::npos) ? "" : f.name.substr(ns, ne - ns + 1);
                f.type = line.substr(q0 + 1, q1 - q0 - 1);   // GLSL 类型原文
                f.dim.clear();
                NormalizeStructField(f);
                x_fields.push_back(f);
            }
        }

        if (x_fields.size() < 9)
        {
            result.diagnostics.emplace_back(
                "X 列表头部取项异常（前 9 项要求，实取 " + std::to_string(x_fields.size()) + "）");
            result.passed = false;
            return result;
        }
        x_fields.resize(9);   // 镜像只承诺头部 9 项

        // 端 2：GLSL `MeshDrawParamsHeadData`（逐行取块器；_pad 是余量承担者）。
        const std::vector<StructFieldEntry> glsl_fields_all =
            ExtractStructFieldsLineWise(glsl_text, "MeshDrawParamsHeadData");

        std::vector<StructFieldEntry> glsl_head;
        for (const StructFieldEntry &f : glsl_fields_all)
        {
            if (f.name.rfind("_pad", 0) == 0)
                continue;
            glsl_head.push_back(f);
        }

        if (glsl_head.empty())
        {
            result.diagnostics.emplace_back(
                "MeshDrawParamsHeadData 字段提取为空（结构标记被改写？取块器 = 逐行式）");
            result.passed = false;
            return result;
        }

        // 逐字段同序同型（X 列表的 dim 恒空：头部 9 项全是标量）
        const size_t common = glsl_head.size() < x_fields.size()
                            ? glsl_head.size() : x_fields.size();
        for (size_t i = 0; i < common; ++i)
        {
            if (x_fields[i].type != glsl_head[i].type
             || x_fields[i].name != glsl_head[i].name)
            {
                result.diagnostics.emplace_back(
                    "MeshDrawParams 头部漂移：第 " + std::to_string(i) + " 项 X列表="
                    + x_fields[i].type + " " + x_fields[i].name + " / GLSL 镜像="
                    + glsl_head[i].type + " " + glsl_head[i].name
                    + "；gather 的 groupCountX 读源失效（画错组数=静默回归）");
                result.passed = false;
                return result;
            }
        }
        if (glsl_head.size() != x_fields.size())
        {
            result.diagnostics.emplace_back(
                "头部项数不一致（余量必须全由 _pad 承担）：X 前 9 / 镜像非 pad 项 "
                + std::to_string(glsl_head.size()));
            result.passed = false;
            return result;
        }

        // 步长契约：载荷 + _pad == 128（池行 sizeof；scalar layout 无隐式填充）
        std::string glsl_stripped = glsl_text;
        StripCommentTokens(glsl_stripped);
        const auto glsl_defines = CollectDefineConstantsWithContract(glsl_stripped);

        size_t glsl_bytes = 0;
        for (const StructFieldEntry &f : glsl_fields_all)
        {
            size_t bytes = 0;
            if (!StructFieldStorageSize(f, glsl_defines, bytes))
            {
                result.diagnostics.emplace_back(
                    "MeshDrawParamsHeadData 字段 " + f.name + " 的长度不可求值：" + f.dim);
                result.passed = false;
                return result;
            }
            glsl_bytes += bytes;
        }

        // CPU 端对照：X 列表头部 9 项 = 4B×8 + float×1 = 36 B（sizeof 断言在头文件里，
        // 门里核对镜像总尺寸 == 池行 104 即可；36+68=104 是本用例的算术前提）
        if (glsl_bytes != 104)
        {
            result.diagnostics.emplace_back(
                "MeshDrawParamsHeadData 的 GLSL 总存储 = " + std::to_string(glsl_bytes)
                + " B，必须 == 104 B（池行 sizeof；差值必须由行尾 _pad 显式写死）");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 「表归属」契约（§6 契约 ④）：`world_side=true` 检查世界表，`false` 检查全局表。
    ///
    /// 两条判据（都按**字段集合**比对，与顺序无关；顺序由 parity 用例各管）：
    /// ① 该侧（C++ 与 GLSL 两端）提取出的字段集合 == 分类表里登记的该侧集合
    ///    —— 多出来的是「未分类字段」（新字段忘了登记 / 放错表），少了的是「漏字段」；
    /// ② 该侧字段**不得**出现在另一侧 —— 放错表就是跨世界串数据：世界私有字段（相机行 /
    ///    实体行表 / 实体 4-ID 表 / L2W / 光源行 / 影子数据）一旦落进设备级表，就退化成
    ///    "所有世界共用一份"，多世界同帧互相覆写。
    GateResult RunAddressOwnershipCase(const bool world_side)
    {
        GateResult result;
        result.name = world_side ? "W.world-addresses-field-ownership"
                                 : "S.global-addresses-field-ownership";

        const std::string world_cpp =
            ReadFileText(RepoRootPath("inc/hgl/graph/ubo/WorldAddresses.h"));
        const std::string global_cpp =
            ReadFileText(RepoRootPath("inc/hgl/graph/ubo/GlobalAddresses.h"));
        const std::string world_glsl =
            ReadFileText(RepoRootPath("ShaderLibrary/module/descriptor/ssbo/world_addresses.glsl"));
        const std::string global_glsl =
            ReadFileText(RepoRootPath("ShaderLibrary/module/descriptor/ssbo/global_addresses.glsl"));

        if (world_cpp.empty() || global_cpp.empty()
         || world_glsl.empty() || global_glsl.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 WorldAddresses.h / GlobalAddresses.h / "
                "module/descriptor/ssbo/world_addresses.glsl / module/descriptor/ssbo/global_addresses.glsl");
            result.passed = result.diagnostics.empty();
            return result;
        }

        const std::vector<std::string> world_fields_cpp =
            ExtractU64Fields(world_cpp, "WorldAddresses");
        const std::vector<std::string> world_fields_glsl =
            ExtractU64Fields(world_glsl, "WorldAddressesRef");
        const std::vector<std::string> global_fields_cpp =
            ExtractU64Fields(global_cpp, "GlobalAddresses");
        const std::vector<std::string> global_fields_glsl =
            ExtractU64Fields(global_glsl, "GlobalAddressesRef");

        const std::vector<std::string> &this_cpp  = world_side ? world_fields_cpp  : global_fields_cpp;
        const std::vector<std::string> &this_glsl = world_side ? world_fields_glsl : global_fields_glsl;
        const std::vector<std::string> &other_cpp = world_side ? global_fields_cpp : world_fields_cpp;
        const std::vector<std::string> &other_glsl= world_side ? global_fields_glsl: world_fields_glsl;
        const char *this_name  = world_side ? "WorldAddresses" : "GlobalAddresses";
        const char *other_name = world_side ? "GlobalAddresses" : "WorldAddresses";

        if (this_cpp.empty() || this_glsl.empty())
        {
            result.diagnostics.emplace_back(
                std::string(this_name) + " 字段提取为空（结构标记被改写？）C++="
                + std::to_string(this_cpp.size()) + " GLSL=" + std::to_string(this_glsl.size()));
            result.passed = result.diagnostics.empty();
            return result;
        }

        // 分类表里登记的本侧字段集合
        std::vector<std::string> expected;
        for (const AddressOwnershipEntry &entry : kAddressOwnershipTable)
        {
            if (entry.world_owned == world_side)
                expected.emplace_back(entry.field);
        }

        const auto contains = [](const std::vector<std::string> &list, const std::string &name)
        {
            for (const std::string &item : list)
                if (item == name)
                    return true;
            return false;
        };

        // ① 该侧字段集合必须与登记集合一致
        for (const std::string &field : this_cpp)
        {
            if (!contains(expected, field))
            {
                result.diagnostics.emplace_back(
                    std::string(this_name) + " 出现未分类字段 `" + field
                    + "`（C++ 侧）：请在 ShaderResourceSchemaRegressionGate 的 kAddressOwnershipTable"
                      " 里登记它属于世界私有还是设备级全局");
            }
        }
        for (const std::string &field : this_glsl)
        {
            if (!contains(expected, field))
            {
                result.diagnostics.emplace_back(
                    std::string(this_name) + " 出现未分类字段 `" + field
                    + "`（GLSL 侧）：同上，先在 kAddressOwnershipTable 登记");
            }
        }
        for (const std::string &field : expected)
        {
            if (!contains(this_cpp, field))
            {
                result.diagnostics.emplace_back(
                    std::string(this_name) + " 缺少登记字段 `" + field
                    + "`（C++ 侧）：字段被删/改名后请同步 kAddressOwnershipTable");
            }
            if (!contains(this_glsl, field))
            {
                result.diagnostics.emplace_back(
                    std::string(this_name) + " 缺少登记字段 `" + field + "`（GLSL 侧）");
            }
        }

        // ② 该侧字段不得出现在另一侧
        for (const std::string &field : other_cpp)
        {
            if (contains(expected, field))
            {
                result.diagnostics.emplace_back(
                    std::string("字段 `") + field + "` 出现在 " + other_name
                    + "（C++ 侧）却在分类表里登记为 " + this_name
                    + " —— 放错表：世界私有字段进设备级表会让多世界共用一份、同帧互相覆写");
            }
        }
        for (const std::string &field : other_glsl)
        {
            if (contains(expected, field))
            {
                result.diagnostics.emplace_back(
                    std::string("字段 `") + field + "` 出现在 " + other_name + "（GLSL 侧）却在分类表里登记为 "
                    + this_name + " —— 放错表");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

        // ══════════════════════════════════════════════════════════════════════
        // MV.fixed-mesh-varying-invariants
        //
        // 原 MV.mesh-varying-writability（生成器可写性表测试）的终态替代：
        // 在完全物理固定的网格主着色器库（ShaderLibrary/topology/*.mesh）上，
        // 直接断言所有 varying 声明、调度契约、真实写入及 Stage2/3 组合不变量。
        // ══════════════════════════════════════════════════════════════════════
        GateResult RunFixedMeshVaryingInvariantsCase()
        {
            GateResult result;
            result.name = "MV.fixed-mesh-varying-invariants";
            AuditFixedMeshMasters(result);
            result.passed = result.diagnostics.empty();
            return result;
        }

} // namespace gate
