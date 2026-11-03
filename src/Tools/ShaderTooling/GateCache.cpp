// 阶段 5 拆分自 ShaderResourceSchemaRegressionGate.cpp：组 cache 的 case 实现。
#include "GateTestSupport.h"

namespace gate
{

    GateResult RunComposedStageInterfaceCase()
    {
        GateResult result;
        result.name = "P.composed-stage-interface";

        ShaderStageBuildContext vertex{};
        vertex.stage = ShaderStage::Mesh;
        vertex.outputs.Add({0, ShaderStageValueType::Vec3, 0, 0});
        vertex.outputs.Add({0, ShaderStageValueType::Vec2, 1, 0});
        ShaderStageBuildContext fragment{};
        fragment.stage = ShaderStage::Fragment;
        fragment.inputs.Add({0, ShaderStageValueType::Vec3, 0, 0});
        fragment.inputs.Add({0, ShaderStageValueType::Vec2, 1, 0});
        if (!HasCompatibleStageInterface(vertex, fragment))
            result.diagnostics.emplace_back("matching composed stage interfaces rejected");

        fragment.inputs[1].value_type = ShaderStageValueType::Vec4;
        if (HasCompatibleStageInterface(vertex, fragment))
            result.diagnostics.emplace_back("incompatible composed stage interfaces accepted");

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunResolvedStageCacheIdentityCase()
    {
        GateResult result;
        result.name = "Q.resolved-stage-cache-identity";

        const GeometryVertexFormat rg16_geometry{{
            VertexSemantic::Position, VF_V2HF
        }};
        const GeometryVertexFormat rg32_geometry{{
            VertexSemantic::Position, VF_V2F
        }};

        ShaderStageBuildContext stage{};
        stage.stage = ShaderStage::Mesh;
        stage.compile_inputs_hash = 0x13579bdf2468ace0ull;
        const ShaderStageKey stage_key = stage.BuildKey();
        const ShaderStageKey equivalent_stage_key = stage.BuildKey();
        if (!(stage_key == equivalent_stage_key))
            result.diagnostics.emplace_back("equivalent provider stages must share cache identity");

        if (rg16_geometry.GetVertexInputHash() == rg32_geometry.GetVertexInputHash())
            result.diagnostics.emplace_back("raw Geometry formats must remain distinct");

        ShaderLinkSpec rg16_link{};
        rg16_link.mesh_stage = stage_key;
        rg16_link.fragment_stage.stage = ShaderStage::Fragment;
        // 顶点输入格式差异只能通过 mesh stage 的 interface_hash 进键（VS 已废弃，
        // link 层不再单独存一份 vertex_input_hash）。
        rg16_link.mesh_stage.interface_hash ^=
            rg16_geometry.GetVertexInputHash();
        ShaderLinkSpec rg32_link = rg16_link;
        rg32_link.mesh_stage.interface_hash ^=
            rg32_geometry.GetVertexInputHash();
        if (rg16_link.BuildKey() == rg32_link.BuildKey())
            result.diagnostics.emplace_back(
                "equivalent shader stages must retain distinct program input identity");

        ShaderArtifactStore store(RepoRootOSPath("build"), ShaderCacheMode::BuildIfMissing);
        const uint32_t payload[] = {0x07230203u, 1u, 2u, 3u};
        constexpr hgl::uint64 stage_source_digest = 0x534F55524345ull;
        if (!store.SaveStageSPV(
                stage_key, payload, sizeof(payload), stage_source_digest))
        {
            result.diagnostics.emplace_back("stage SPV cache save failed");
        }
        else
        {
            hgl::ValueArray<hgl::uint8> loaded;
            hgl::uint64 loaded_source_digest = 0;
            if (!store.LoadStageSPV(
                    stage_key, loaded, &loaded_source_digest)
             || loaded.GetCount() != static_cast<int>(sizeof(payload))
             || std::memcmp(loaded.GetData(), payload, sizeof(payload)) != 0
             || loaded_source_digest != stage_source_digest)
            {
                result.diagnostics.emplace_back(
                    "stage SPV/source digest cache round-trip failed");
            }

            ShaderStageKey wrong_key = stage_key;
            wrong_key.compile_inputs_hash ^= 1ull;
            if (store.LoadStageSPV(wrong_key, loaded))
                result.diagnostics.emplace_back("cache must reject a different stage key");
        }

        ShaderArtifactStore read_only_store(
            RepoRootOSPath("build"), ShaderCacheMode::ReadOnly);
        if (read_only_store.SaveStageSPV(stage_key, payload, sizeof(payload)))
            result.diagnostics.emplace_back("read-only cache must reject writes");
        const uint32_t invalid_spv[] = {0u, 1u, 2u, 3u};
        if (store.SaveStageSPV(
                stage_key, invalid_spv, sizeof(invalid_spv)))
            result.diagnostics.emplace_back(
                "invalid SPV payload must be rejected");

        ShaderLinkSpec program_link{};
        program_link.mesh_stage = stage_key;
        program_link.fragment_stage.stage = ShaderStage::Fragment;
        program_link.fragment_stage.definition_hash = 0x2201u;
        program_link.fragment_stage.compile_inputs_hash = 0x2202u;
        program_link.fragment_stage.interface_hash = 0x2203u;
        program_link.fragment_stage.compiler_hash =
            stage_key.compiler_hash != 0
                ? stage_key.compiler_hash : 0x2304u;
        program_link.mesh_stage.compiler_hash =
            program_link.fragment_stage.compiler_hash;
        program_link.link_contract_hash = 0x2303u;

        ShaderProgramArtifactMetadata metadata{};
        metadata.program_key_digest =
            program_link.BuildKey().GetDigest();
        metadata.generated_source_digest = 0x2406u;

        const uint32_t fragment_payload[] =
            {0x07230203u, 7u, 8u, 9u};
        if (!store.SaveStageSPV(
                program_link.mesh_stage,
                payload,
                sizeof(payload))
         || !store.SaveStageSPV(
                program_link.fragment_stage,
                fragment_payload,
                sizeof(fragment_payload))
         || !store.SaveProgramMetadata(program_link, metadata))
        {
            result.diagnostics.emplace_back(
                "complete program artifact save failed");
        }
        else
        {
            hgl::ValueArray<hgl::uint8> cached_vertex;
            hgl::ValueArray<hgl::uint8> cached_fragment;
            if (!store.HasProgramMetadata(program_link)
             || !store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    cached_vertex,
                    cached_fragment)
             || cached_vertex.GetCount()
                    != static_cast<int>(sizeof(payload))
             || cached_fragment.GetCount()
                    != static_cast<int>(sizeof(fragment_payload))
             || std::memcmp(
                    cached_vertex.GetData(),
                    payload,
                    sizeof(payload)) != 0
             || std::memcmp(
                    cached_fragment.GetData(),
                    fragment_payload,
                    sizeof(fragment_payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "complete program artifact load failed");
            }

            // 命中判定的唯一维度是 program 键摘要：把它改掉必须拒。
            ShaderProgramArtifactMetadata changed_metadata = metadata;
            changed_metadata.program_key_digest ^= 1u;
            if (store.LoadProgramArtifacts(
                    program_link,
                    changed_metadata,
                    cached_vertex,
                    cached_fragment))
            {
                result.diagnostics.emplace_back(
                    "metadata mismatch must reject stage artifacts");
            }

            // 第 3 步：generated_source_digest 是 emit 之后的文本产物，
            // 不参与命中判定——否则命中之前就必须先跑完 GLSL 生成，正是
            // 解耦要消除的耦合。它单独变化仍应命中，由命中后校验
            // （VerifyGeneratedSourceDigest）负责发现"声明键欠近似"。
            ShaderProgramArtifactMetadata text_only_changed = metadata;
            text_only_changed.generated_source_digest ^= 1u;
            if (!store.LoadProgramArtifacts(
                    program_link,
                    text_only_changed,
                    cached_vertex,
                    cached_fragment))
            {
                result.diagnostics.emplace_back(
                    "generated_source_digest must not gate the cache hit");
            }

            // 命中后校验的语义：同一份文本 + 同一摘要必须通过；
            // 摘要被改动必须被拒（这正是"声明键欠近似"的兜底）。
            {
                const char mesh_glsl[] = "void main(){}";
                const char fragment_glsl[] = "void main(){}";
                const size_t mesh_len = sizeof(mesh_glsl) - 1;
                const size_t fragment_len = sizeof(fragment_glsl) - 1;
                const hgl::uint64 digest = ComputeGeneratedSourceDigest(
                    mesh_glsl, mesh_len, fragment_glsl, fragment_len);

                if (digest == 0
                 || !MatchesGeneratedSourceDigest(
                        mesh_glsl, mesh_len, fragment_glsl, fragment_len, digest)
                 || MatchesGeneratedSourceDigest(
                        mesh_glsl, mesh_len, fragment_glsl, fragment_len,
                        digest ^ 1u)
                 || MatchesGeneratedSourceDigest(
                        mesh_glsl, mesh_len, fragment_glsl, fragment_len, 0))
                {
                    result.diagnostics.emplace_back(
                        "post-hit generated_source_digest verification is broken");
                }
            }

            if (!read_only_store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    cached_vertex,
                    cached_fragment)
             || read_only_store.SaveProgramMetadata(
                    program_link, metadata))
            {
                result.diagnostics.emplace_back(
                    "read-only complete program artifact behavior failed");
            }

            hgl::filesystem::Path metadata_path(
                RepoRootOSPath("build"));
            metadata_path /=
                hgl::ToOSString(hgl::graph::mtl::ShaderArtifactCacheDirectory);
            metadata_path /=
                hgl::ToOSString(hgl::graph::mtl::ShaderArtifactProgramDirectory);
            metadata_path /= hgl::ToOSString(
                (program_link.BuildKey().ToString()
                    + hgl::AnsiString(hgl::graph::mtl::ShaderArtifactMetadataExtension)).c_str());
            hgl::int64 metadata_file_size = 0;
            void *metadata_file =
                hgl::filesystem::LoadFileToMemory(
                    metadata_path.ToOSString(),
                    metadata_file_size);
            if (!metadata_file || metadata_file_size <= 0)
            {
                result.diagnostics.emplace_back(
                    "failed to load metadata corruption fixture");
            }
            else
            {
                auto *metadata_bytes =
                    static_cast<hgl::uint8 *>(metadata_file);
                metadata_bytes[metadata_file_size - 1] ^= 0xffu;
                if (hgl::filesystem::SaveMemoryToFile(
                        metadata_path.ToOSString(),
                        metadata_bytes,
                        metadata_file_size)
                        != metadata_file_size)
                {
                    result.diagnostics.emplace_back(
                        "failed to corrupt program metadata");
                }
                delete[] metadata_bytes;

                ShaderProgramArtifactMetadata corrupted{};
                if (store.LoadProgramMetadata(
                        program_link, corrupted))
                {
                    result.diagnostics.emplace_back(
                        "corrupt program metadata must be rejected");
                }
                if (!store.SaveProgramMetadata(
                        program_link, metadata))
                {
                    result.diagnostics.emplace_back(
                        "failed to restore program metadata fixture");
                }
            }
        }

        ShaderProgramArtifactMetadata invalid_metadata = metadata;
        invalid_metadata.program_key_digest = 0;
        if (store.SaveProgramMetadata(
                program_link, invalid_metadata))
            result.diagnostics.emplace_back(
                "invalid program metadata must reject writes");

        ShaderLinkSpec stage_only_link = program_link;
        stage_only_link.link_contract_hash ^= 0x1000u;
        ShaderProgramArtifactMetadata stage_only_metadata = metadata;
        stage_only_metadata.program_key_digest =
            stage_only_link.BuildKey().GetDigest();
        hgl::ValueArray<hgl::uint8> stage_only_vertex;
        hgl::ValueArray<hgl::uint8> stage_only_fragment;
        if (store.LoadProgramArtifacts(
                stage_only_link,
                stage_only_metadata,
                stage_only_vertex,
                stage_only_fragment))
        {
            result.diagnostics.emplace_back(
                "stage-only artifacts must not count as a program hit");
        }

        ShaderLinkSpec metadata_only_link = program_link;
        metadata_only_link.fragment_stage.definition_hash ^= 0x2000u;
        ShaderProgramArtifactMetadata metadata_only = metadata;
        metadata_only.program_key_digest =
            metadata_only_link.BuildKey().GetDigest();
        if (!store.SaveProgramMetadata(
                metadata_only_link, metadata_only))
        {
            result.diagnostics.emplace_back(
                "metadata-only fixture save failed");
        }
        else if (store.LoadProgramArtifacts(
                    metadata_only_link,
                    metadata_only,
                    stage_only_vertex,
                    stage_only_fragment))
        {
            result.diagnostics.emplace_back(
                "metadata without all stages must not count as a program hit");
        }

        ShaderArtifactStore shadow_store(
            RepoRootOSPath("build/cache-isolation"),
            ShaderCacheMode::BuildIfMissing);
        const uint32_t shadow_payload[] =
            {0x07230203u, 4u, 5u, 6u};
        const uint32_t shadow_fragment_payload[] =
            {0x07230203u, 10u, 11u, 12u};
        if (!shadow_store.SaveStageSPV(
                program_link.mesh_stage,
                shadow_payload,
                sizeof(shadow_payload))
         || !shadow_store.SaveStageSPV(
                program_link.fragment_stage,
                shadow_fragment_payload,
                sizeof(shadow_fragment_payload))
         || !shadow_store.SaveProgramMetadata(
                program_link, metadata))
        {
            result.diagnostics.emplace_back(
                "isolated cache save failed");
        }
        else
        {
            hgl::ValueArray<hgl::uint8> legacy_loaded;
            hgl::ValueArray<hgl::uint8> shadow_loaded;
            if (!store.LoadStageSPV(stage_key, legacy_loaded)
             || legacy_loaded.GetCount() != static_cast<int>(sizeof(payload))
             || std::memcmp(legacy_loaded.GetData(), payload, sizeof(payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "isolated cache must not replace the primary artifact");
            }

            if (!shadow_store.LoadStageSPV(
                    program_link.mesh_stage, shadow_loaded)
             || shadow_loaded.GetCount() != static_cast<int>(sizeof(shadow_payload))
             || std::memcmp(
                    shadow_loaded.GetData(), shadow_payload, sizeof(shadow_payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "isolated cache must load its own artifact");
            }

            hgl::ValueArray<hgl::uint8> shadow_fragment_loaded;
            if (!shadow_store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    shadow_loaded,
                    shadow_fragment_loaded)
             || shadow_fragment_loaded.GetCount()
                    != static_cast<int>(
                        sizeof(shadow_fragment_payload))
             || std::memcmp(
                    shadow_fragment_loaded.GetData(),
                    shadow_fragment_payload,
                    sizeof(shadow_fragment_payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "program artifacts must remain root-isolated");
            }

            ShaderArtifactStore contract_store(
                RepoRootOSPath("build/cache-miss"),
                ShaderCacheMode::ReadOnly);
            if (contract_store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    shadow_loaded,
                    shadow_fragment_loaded))
            {
                result.diagnostics.emplace_back(
                    "independent cache root must not hit other programs");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunAuthoritativeMaterialCacheIdentityCase()
    {
        GateResult result;
        result.name = "Q0.authoritative-material-cache-identity";

        ResolvedMaterial lit{};
        ResolvedMaterial vertex_color{};
        if (!BuildResolvedMaterialForDomain(MaterialDomain::StandardOpaque, lit)
         || !BuildResolvedMaterialForDomain(MaterialDomain::VertexColor, vertex_color))
        {
            result.diagnostics.emplace_back(
                "cache identity material definitions unavailable");
            result.passed = false;
            return result;
        }

        const GeometryVertexFormat &lit_geometry_a = kCacheIdentityLitGeometryA;
        const GeometryVertexFormat &lit_geometry_b = kCacheIdentityLitGeometryB;
        const GeometryVertexFormat &color_geometry = kCacheIdentityColorGeometry;

        // 失败原因必须逐次记录：`GetLastMaterialProgramAssemblyError()` 只反映
        // **最后一次**构建，后面的构建一旦成功就会把它清空，而空串的 `c_str()`
        // 返回 nullptr——诊断里直接取它会把错误路径变成崩溃。
        std::string build_error;
        const auto build = [&build_error](
            const contract::PhysicalDeviceProfileLite *profile,
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
            {
                build_error = "render template request resolution failed";
                return std::unique_ptr<ShaderBuildContext>();
            }
            // 片段源走领域固定源：本用例算的是缓存身份（链接规格 + 各种摘要），
            // 与片段怎么写无关。
            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(definition, request, fixed))
            {
                build_error = "fixed fragment source resolution failed";
                return std::unique_ptr<ShaderBuildContext>();
            }
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;
            std::unique_ptr<ShaderBuildContext> context(
                AssembleMaterialProgram(
                    profile, request, definition, nullptr, fixed_stage));
            if (!context || !context->HasProgramLink())
            {
                const hgl::AnsiString &err = GetLastMaterialProgramAssemblyError();
                build_error = err.IsEmpty()
                    ? "(no diagnostic)" : err.c_str();
            }
            return context;
        };
        const auto lit_a = build(
            nullptr, lit, lit_geometry_a,
            PassProfileID::ForwardLitShadowedAO,
            MakeIdentityForwardLitProfile());
        const auto lit_b = build(
            nullptr, lit, lit_geometry_b,
            PassProfileID::ForwardLitShadowedAO,
            MakeIdentityForwardLitProfile());
        const auto color = build(
            nullptr, vertex_color, color_geometry,
            PassProfileID::ForwardUnlit,
            MakeForwardUnlitProfile());
        contract::PhysicalDeviceProfileLite profile{};
        profile.api_version = contract::MakeVkVersion(1, 4);
        profile.limits.max_uniform_buffer_range = 65536;
        profile.limits.max_storage_buffer_range = 1ull << 30;
        const auto lit_targeted = build(
            &profile, lit, lit_geometry_a,
            PassProfileID::ForwardLitShadowedAO,
            MakeIdentityForwardLitProfile());

        if (!lit_a || !lit_b || !color || !lit_targeted
         || !lit_a->HasProgramLink()
         || !lit_b->HasProgramLink()
         || !color->HasProgramLink()
         || !lit_targeted->HasProgramLink()
         || !lit_a->HasProgramArtifactMetadata()
         || !lit_b->HasProgramArtifactMetadata()
         || !color->HasProgramArtifactMetadata()
         || !lit_targeted->HasProgramArtifactMetadata())
        {
            result.diagnostics.emplace_back(
                std::string("material builds must produce ProgramLink (last error: ")
                + (build_error.empty() ? "(no diagnostic)" : build_error)
                + ")");
        }
        else
        {
            const ShaderLinkSpec &a = lit_a->GetProgramLink();
            const ShaderLinkSpec &b = lit_b->GetProgramLink();
            const ShaderLinkSpec &c = color->GetProgramLink();
            const ShaderLinkSpec &targeted =
                lit_targeted->GetProgramLink();
            if (lit_a->GetProgramArtifactMetadata().
                    program_key_digest
                    != a.BuildKey().GetDigest())
            {
                result.diagnostics.emplace_back(
                    "BuildContext program metadata does not match ProgramLink");
            }

            ShaderArtifactStore hot_store(
                RepoRootOSPath("build/cache-hot"),
                ShaderCacheMode::BuildIfMissing);
            const uint32_t cached_vertex_spv[] =
                {ShaderArtifactSPVMagic, 0x3101u, 0x3102u, 0x3103u};
            const uint32_t cached_fragment_spv[] =
                {ShaderArtifactSPVMagic, 0x3201u, 0x3202u, 0x3203u};
            const ShaderCreateInfo *lit_mesh =
                lit_a->GetStageShader(ShaderStage::Mesh);
            const ShaderCreateInfo *lit_fragment =
                lit_a->GetStageShader(ShaderStage::Fragment);
            if (!hot_store.SaveStageSPV(
                    a.mesh_stage,
                    cached_vertex_spv,
                    sizeof(cached_vertex_spv),
                    ComputeGeneratedStageSourceDigest(
                        lit_mesh->GetFinalGLSL().data(),
                        lit_mesh->GetFinalGLSL().size()))
             || !hot_store.SaveStageSPV(
                    a.fragment_stage,
                    cached_fragment_spv,
                    sizeof(cached_fragment_spv),
                    ComputeGeneratedStageSourceDigest(
                        lit_fragment->GetFinalGLSL().data(),
                        lit_fragment->GetFinalGLSL().size()))
             || !hot_store.SaveProgramMetadata(
                    a,
                    lit_a->GetProgramArtifactMetadata()))
            {
                result.diagnostics.emplace_back(
                    "hot program artifact fixture save failed");
            }
            else
            {
                lit_a->SetArtifactStore(&hot_store);
                if (!FinalizeShaderBuildContext(lit_a.get())
                 || !lit_a->UsedCachedMeshStage()
                 || !lit_a->UsedCachedFragmentStage()
                 || !lit_a->GetStageShader(ShaderStage::Mesh)
                 || !lit_a->GetStageShader(ShaderStage::Fragment)
                 || lit_a->GetStageShader(
                        ShaderStage::Mesh)->GetSPVSize()
                        != sizeof(cached_vertex_spv)
                 || lit_a->GetStageShader(
                        ShaderStage::Fragment)->GetSPVSize()
                        != sizeof(cached_fragment_spv)
                 || std::memcmp(
                        lit_a->GetStageShader(
                            ShaderStage::Mesh)->GetSPVData(),
                        cached_vertex_spv,
                        sizeof(cached_vertex_spv)) != 0
                 || std::memcmp(
                        lit_a->GetStageShader(
                            ShaderStage::Fragment)->GetSPVData(),
                        cached_fragment_spv,
                        sizeof(cached_fragment_spv)) != 0)
                {
                    result.diagnostics.emplace_back(
                        "hot program hit must populate BuildContext SPV");
                }
            }

            ShaderArtifactStore read_only_miss_store(
                RepoRootOSPath("build/cache-read-only-miss"),
                ShaderCacheMode::ReadOnly);
            lit_b->SetArtifactStore(&read_only_miss_store);
            // read-only miss：任一 stage artifact 缺失都应失败、
            // 不应产生 SPV 数据（stage 对象存在但 SPV 为空）
            if (read_only_miss_store.HasProgramMetadata(
                    lit_b->GetProgramLink())
             || FinalizeShaderBuildContext(lit_b.get())
             || (lit_b->GetStageShader(ShaderStage::Mesh)
                 && lit_b->GetStageShader(
                        ShaderStage::Mesh)->GetSPVSize() != 0)
             || (lit_b->GetStageShader(ShaderStage::Fragment)
                 && lit_b->GetStageShader(
                        ShaderStage::Fragment)->GetSPVSize() != 0))
            {
                result.diagnostics.emplace_back(
                    "read-only program miss must not compile or write");
            }

            if (a.mesh_stage.definition_hash == 0
             || a.mesh_stage.compile_inputs_hash == 0
             || a.mesh_stage.interface_hash == 0
             || a.mesh_stage.compiler_hash == 0
             || a.fragment_stage.definition_hash == 0
             || a.fragment_stage.compile_inputs_hash == 0
             || a.fragment_stage.interface_hash == 0
             || a.fragment_stage.compiler_hash == 0
             || a.link_contract_hash == 0)
            {
                result.diagnostics.emplace_back(
                    "authoritative stage/program keys contain zero identity");
            }

            if (a.fragment_stage.GetDigest()
                    == c.fragment_stage.GetDigest()
             || a.BuildKey() == c.BuildKey())
            {
                result.diagnostics.emplace_back(
                    "different materials must not share authoritative keys");
            }

            if (a.mesh_stage.GetDigest()
                    == b.mesh_stage.GetDigest()
             || a.BuildKey() == b.BuildKey())
            {
                result.diagnostics.emplace_back(
                    "different geometry formats must not share authoritative keys");
            }

            // 编译目标哈希只由「目标 Vulkan 版本 + 目标 SPIR-V 版本 + 代码生成代数」
            // 决定。物理设备量（vendor / device / api_version / limits / features）
            // **不得**再进任何 SPV 缓存键 —— SPIR-V 是平台中立字节码，设备量只影响
            // glslang 的校验门槛，不改变发射出的指令；一旦进键，发行版就得按显卡型号
            // 各烧一份产物，"只打包一套 SPV"这条产品要求直接不成立。
            //
            // 唯一真正进 GLSL 文本的设备量是 mesh invocation 上限，它走的是
            // ClampMeshInvocationsByDevice → definition_hash / interface_hash，
            // 与本断言关心的一般性设备身份无关。
            if (a.mesh_stage.compiler_hash
                    != targeted.mesh_stage.compiler_hash
             || a.fragment_stage.compiler_hash
                    != targeted.fragment_stage.compiler_hash
             || !(a.BuildKey() == targeted.BuildKey()))
            {
                result.diagnostics.emplace_back(
                    "physical device profile must NOT change authoritative keys");
            }

            const char *const shared_domain_names[] =
            {
                "HumanSkinDefinition",
                "WoodDefinition",
                "StoneDefinition",
                "MetalDefinition"
            };
            std::vector<std::unique_ptr<ShaderBuildContext>>
                shared_builds;
            for (int i = 0; i < 4; ++i)
            {
                ResolvedMaterial definition = lit;
                definition.domain_name = shared_domain_names[i];
                shared_builds.push_back(
                    build(
                        nullptr, definition, lit_geometry_a,
                        PassProfileID::ForwardLitShadowedAO,
                        MakeIdentityForwardLitProfile()));
            }

            if (shared_builds.size() != 4
             || !shared_builds[0]
             || !shared_builds[1]
             || !shared_builds[2]
             || !shared_builds[3])
            {
                result.diagnostics.emplace_back(
                    "cross-Definition ProgramKey fixtures failed");
            }
            else
            {
                const ShaderProgramKey shared_key =
                    shared_builds[0]->GetProgramLink().BuildKey();
                for (int i = 1; i < 4; ++i)
                {
                    if (!(shared_builds[i]->
                            GetProgramLink().BuildKey() == shared_key))
                    {
                        result.diagnostics.emplace_back(
                            "equivalent Definitions must share ProgramKey identity");
                        break;
                    }
                }
            }

            const uint64_t context_a =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr);
            const uint64_t context_b =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_b,
                    nullptr);
            const uint64_t context_targeted =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    &profile);
            if (context_a == 0
             || context_a == context_b
             || context_a != context_targeted)
            {
                result.diagnostics.emplace_back(
                    "program build context must include geometry but not physical device");
            }

            // ambient provider 是**前向着色**的 program 身份维度（环境层选模块，
            // 不同模块 ⇒ 不同 fragment 源码）；depth/shadow 不消费该槽，必须不受影响。
            const uint64_t context_ambient =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr,
                    ShaderProgramPurpose::ForwardColor,
                    "ambient_light_ibl");
            const uint64_t context_ambient_empty =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr,
                    ShaderProgramPurpose::ForwardColor,
                    "");
            const uint64_t context_ambient_depth =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr,
                    ShaderProgramPurpose::DepthOnly,
                    "ambient_light_ibl");
            const uint64_t context_depth =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr,
                    ShaderProgramPurpose::DepthOnly,
                    nullptr);
            if (context_ambient == context_a
             || context_ambient_empty != context_a
             || context_ambient_depth != context_depth)
            {
                result.diagnostics.emplace_back(
                    "program build context must include forward ambient provider only");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunCanonicalShaderContractCase()
    {
        GateResult result;
        result.name = "Q1.canonical-shader-contract";

        // Q1 只保留 OutputContract 序列化恒等/校验段——ShaderInterfaceContract
        // 体系（interface 序列化恒等/location 冲突段）已随生产删除一并移除。

        OutputContract output_a{};
        output_a.purpose = ShaderProgramPurpose::ForwardColor;
        output_a.attachments.Add(
            {
                StableID("output.color"),
                ShaderStageValueType::Vec4,
                0,
                1,
                0
            });
        output_a.attachments.Add(
            {
                StableID("output.material_id"),
                ShaderStageValueType::UInt,
                1,
                1,
                0
            });
        OutputContract output_b = output_a;
        output_b.attachments.Clear();
        output_b.attachments.Add(output_a.attachments[1]);
        output_b.attachments.Add(output_a.attachments[0]);
        if (GetOutputContractHash(output_a) == 0
         || GetOutputContractHash(output_a)
                != GetOutputContractHash(output_b))
        {
            result.diagnostics.emplace_back(
                "output serialization must ignore attachment order");
        }

        OutputContract depth_output{};
        depth_output.purpose = ShaderProgramPurpose::DepthOnly;
        depth_output.depth_only = true;
        if (!ValidateOutputContract(depth_output))
            result.diagnostics.emplace_back(
                "depth-only output contract must allow no attachments");

        OutputContract unsupported_output = depth_output;
        unsupported_output.purpose =
            static_cast<ShaderProgramPurpose>(3);
        if (ValidateOutputContract(unsupported_output))
            result.diagnostics.emplace_back(
                "purpose validation must accept exactly three values");

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 发行形态清单门
    ///
    /// 发行版没有生成器、没有 GLSL 源码，只能靠 `program_manifest.pmf` 把「运行时
    /// 自算的语义键」映射到 program link。本门守三件事：
    ///   1) 磁盘往返逐字段等值（序列化/反序列化不许丢字段、不许张冠李戴）；
    ///   2) `ComputeMaterialProgramManifestKey` 对每个决定 SPV 内容的维度都敏感
    ///      —— 任一维度漏折，发行形态就会取到别人的 SPV，而且是**静默**的；
    ///   3) 损坏的清单必须整体拒绝，绝不"尽力装载半份"。
    GateResult RunProgramManifestCase()
    {
        GateResult result;
        result.name = "PM.program-manifest-roundtrip";

        // ── 1. 磁盘往返 ──
        ShaderProgramManifest source;
        constexpr hgl::uint64 kProfileHash = 0x1234'5678'9ABC'DEF0ull;
        source.SetStamps(kProfileHash, contract::MATERIAL_CODEGEN_GENERATION);

        for (hgl::uint32 i = 1; i <= 4; ++i)
        {
            ShaderProgramManifestEntry entry{};
            entry.lookup_key = 0x1000'0000ull + i;
            entry.program_link.mesh_stage.stage = ShaderStage::Mesh;
            entry.program_link.mesh_stage.definition_hash = 0x11ull * i;
            entry.program_link.mesh_stage.compile_inputs_hash = 0x22ull * i;
            entry.program_link.mesh_stage.interface_hash = 0x33ull * i;
            entry.program_link.mesh_stage.compiler_hash = 0x55ull * i;
            entry.program_link.fragment_stage.stage = ShaderStage::Fragment;
            entry.program_link.fragment_stage.definition_hash = 0x66ull * i;
            entry.program_link.fragment_stage.compile_inputs_hash = 0x77ull * i;
            entry.program_link.fragment_stage.interface_hash = 0x88ull * i;
            entry.program_link.fragment_stage.compiler_hash = 0xAAull * i;
            entry.program_link.link_contract_hash = 0xDDull * i;
            // 行需求的两个信号必须能各自独立往返：三个合法组合来自
            // 「payload ⊂ rows」这条不变量 ——
            //   (rows=F,payload=F) 无行无 payload（VertexColor 一类）
            //   (rows=T,payload=F) 只要纹理引用行表（UnlitTexture 一类）
            //   (rows=T,payload=T) 读 payload 行（Lit 一类）
            // (rows=F,payload=T) 非法，由下面的拒绝断言单独覆盖，不放进往返样本。
            // 下标 0 是占位（下面按 1..4 取样本），数组长度必须是 5 —— 长度 4 时
            // `[4]` 会读到数组外，静态存储的越界读会随链接布局改变，表现为
            // 「这条断言时好时坏」而不是编译错误。
            static const bool kRuntimeRows[5] = { false, false, true, true, true };
            static const bool kPayloadRows[5] = { false, false, false, true, false };
            entry.requires_recipe_runtime_rows = kRuntimeRows[i];
            entry.requires_recipe_payload_row = kPayloadRows[i];

            if (!source.Upsert(entry))
            {
                result.diagnostics.emplace_back(
                    "Upsert rejected a valid manifest entry");
                break;
            }

            // 幂等：同一份内容重复写入不得报"有变化"。
            if (source.Upsert(entry))
                result.diagnostics.emplace_back(
                    "Upsert reported a change for identical content");
        }

        if (source.GetEntryCount() != 4)
            result.diagnostics.emplace_back(
                "manifest entry count mismatch after upsert");

        // ── 行需求不变量：payload ⊂ rows ──
        //
        // 「要 payload 行但不要行表」不可能成立（payload_index 来自行表）。放行会
        // 变成「程序读 payload，运行期从不物化行」——运行期读到越界/陈旧 SSBO，
        // 而不是一条明确失败。三个合法组合已在上面往返过，这里守非法的那一个。
        {
            ShaderProgramManifestEntry illegal{};
            illegal.lookup_key = 0x2000'0000ull;
            illegal.program_link.mesh_stage.stage = ShaderStage::Mesh;
            illegal.program_link.mesh_stage.compile_inputs_hash = 0x11ull;
            illegal.program_link.mesh_stage.interface_hash = 0x22ull;
            illegal.program_link.mesh_stage.compiler_hash = 0x44ull;
            illegal.program_link.fragment_stage.stage = ShaderStage::Fragment;
            illegal.program_link.fragment_stage.compile_inputs_hash = 0x55ull;
            illegal.program_link.fragment_stage.interface_hash = 0x66ull;
            illegal.program_link.fragment_stage.compiler_hash = 0x88ull;
            illegal.program_link.link_contract_hash = 0xBBull;
            illegal.requires_recipe_runtime_rows = false;
            illegal.requires_recipe_payload_row = true;

            if (illegal.IsConsistent())
                result.diagnostics.emplace_back(
                    "manifest entry invariant: payload row without runtime rows "
                    "must be reported inconsistent");

            ShaderProgramManifest reject_source;
            reject_source.SetStamps(kProfileHash,
                                    contract::MATERIAL_CODEGEN_GENERATION);
            if (reject_source.Upsert(illegal))
                result.diagnostics.emplace_back(
                    "Upsert accepted a payload-row-without-runtime-rows entry");
        }

        const std::filesystem::path temp_root =
            std::filesystem::temp_directory_path() / "ulre-program-manifest-gate";
        std::error_code ec;
        std::filesystem::remove_all(temp_root, ec);

        const hgl::OSString manifest_path =
            MakeShaderProgramManifestPath(hgl::OSString(temp_root.wstring().c_str()));

        if (!source.Save(manifest_path))
        {
            result.diagnostics.emplace_back("manifest Save failed");
            result.passed = false;
            return result;
        }

        ShaderProgramManifest loaded;
        if (!loaded.Load(manifest_path))
        {
            result.diagnostics.emplace_back("manifest Load failed on freshly saved file");
            result.passed = false;
            return result;
        }

        // ── Load 侧的行需求不变量守卫 ──
        //
        // 合法清单落盘后把第 0 条（rows=F,payload=F）改成 (rows=F,payload=T)：
        // 这是「要 payload 行却不要行表」的非法组合，必须整份拒绝。v3 载荷是
        // AssetGen 生成的 FlatBuffers，按固定字节偏移盲改不再成立——这里解出
        // 载荷、直接改解码后的条目、重编码并同步头部 payload_size/payload_hash，
        // 确保测到的是不变量守卫而不是校验和守卫。
        {
            hgl::uint8 *raw = nullptr;
            const hgl::int64 raw_size = hgl::filesystem::LoadFileToMemory(
                manifest_path, reinterpret_cast<void **>(&raw));

            ShaderArtifactFileHeader header{};
            ShaderProgramManifestPayload payload_struct;
            bool fixture_ok = false;

            if (raw && static_cast<hgl::uint64>(raw_size) >= sizeof(header))
            {
                std::memcpy(&header, raw, sizeof(header));
                if (static_cast<hgl::uint64>(raw_size)
                        == static_cast<hgl::uint64>(header.header_size)
                          + header.payload_size)
                {
                    const hgl::uint8 *payload = raw + header.header_size;
                    fixture_ok =
                        hgl::serialize::fb::VerifyAndDecodeFlatBuffers(
                            payload, static_cast<size_t>(header.payload_size),
                            payload_struct)
                     && !payload_struct.entries.IsEmpty();
                }
            }

            if (raw)
                delete[] raw;

            if (!fixture_ok)
            {
                result.diagnostics.emplace_back(
                    "manifest fixture unreadable for load-time invariant test");
            }
            else
            {
                ShaderProgramManifestEntry *first = payload_struct.entries.GetData();
                first->requires_recipe_runtime_rows = false;
                first->requires_recipe_payload_row = true;

                hgl::ValueArray<hgl::uint8> bytes;
                if (!hgl::serialize::fb::EncodeToFlatBuffers(bytes, payload_struct))
                    result.diagnostics.emplace_back(
                        "failed to re-encode tampered manifest fixture");
                else
                {
                    header.payload_size = static_cast<hgl::uint64>(bytes.GetCount());
                    hgl::hash::FNV1aHasher64 hasher;
                    hasher.AppendBytes(bytes.GetData(),
                                       static_cast<size_t>(bytes.GetCount()));
                    header.payload_hash = hasher;

                    hgl::ValueArray<hgl::uint8> file_bytes;
                    file_bytes.Resize(static_cast<int>(
                        sizeof(header) + static_cast<hgl::uint64>(bytes.GetCount())));
                    std::memcpy(file_bytes.GetData(), &header, sizeof(header));
                    std::memcpy(file_bytes.GetData() + sizeof(header),
                                bytes.GetData(),
                                static_cast<size_t>(bytes.GetCount()));

                    const hgl::int64 written = hgl::filesystem::SaveMemoryToFile(
                        manifest_path, file_bytes.GetData(),
                        static_cast<hgl::int64>(file_bytes.GetCount()));

                    if (written != static_cast<hgl::int64>(file_bytes.GetCount()))
                        result.diagnostics.emplace_back(
                            "failed to write tampered manifest fixture");
                    else
                    {
                        ShaderProgramManifest tampered;
                        if (tampered.Load(manifest_path))
                            result.diagnostics.emplace_back(
                                "Load accepted a manifest carrying "
                                "payload-row-without-runtime-rows");
                    }
                }
            }
        }

        if (loaded.GetCompilerProfileHash() != kProfileHash
         || loaded.GetCodegenGeneration() != contract::MATERIAL_CODEGEN_GENERATION
         || loaded.GetEntryCount() != source.GetEntryCount())
            result.diagnostics.emplace_back("manifest stamps/entry count lost in roundtrip");

        for (hgl::uint32 i = 1; i <= 4; ++i)
        {
            const ShaderProgramManifestEntry *original =
                source.Find(0x1000'0000ull + i);
            const ShaderProgramManifestEntry *restored =
                loaded.Find(0x1000'0000ull + i);

            if (!original || !restored)
            {
                result.diagnostics.emplace_back(
                    "manifest entry vanished in roundtrip");
                continue;
            }

            if (*original != *restored)
                result.diagnostics.emplace_back(
                    "manifest entry differs after roundtrip");
        }

        // ── 2. 语义键敏感性 ──
        //
        // 每个维度都必须独立地把键推开；反之（两个不同变体算出同一个键）就会让
        // 发行形态取到不匹配的 SPV —— 这种错误在运行时完全不报错。
        {
            ResolvedMaterial definition{};
            if (!BuildResolvedMaterialForDomain(MaterialDomain::StandardOpaque, definition))
                result.diagnostics.emplace_back(
                    "domain definition unavailable: StandardOpaque");
            else
            {
                const GeometryVertexFormat geometry{
                    {VertexSemantic::Position, VF_V3F},
                    {VertexSemantic::TexCoord, VF_V2F},
                    {VertexSemantic::Normal,   VF_V3F}
                };
                const GeometryVertexFormat other_geometry{
                    {VertexSemantic::Position, VF_V3F},
                    {VertexSemantic::TexCoord, VF_V2F}
                };

                MaterialBuildRequest base{};
                base.recipe.domain = definition.domain;
                NormalizeRecipe(base.recipe);
                base.geometry_vertex_format = &geometry;
                base.defer_finalize = true;
                base.shader_program_purpose = ShaderProgramPurpose::ForwardColor;

                const hgl::uint64 base_key =
                    ComputeMaterialProgramManifestKey(definition, base, nullptr);

                if (base_key == 0)
                    result.diagnostics.emplace_back(
                        "manifest key is zero for a valid request");

                if (ComputeMaterialProgramManifestKey(definition, base, nullptr)
                    != base_key)
                    result.diagnostics.emplace_back(
                        "manifest key is not deterministic across calls");

                MaterialBuildRequest purpose = base;
                purpose.shader_program_purpose = ShaderProgramPurpose::DepthOnly;
                if (ComputeMaterialProgramManifestKey(definition, purpose, nullptr)
                    == base_key)
                    result.diagnostics.emplace_back(
                        "manifest key ignores shader_program_purpose");

                MaterialBuildRequest ambient = base;
                ambient.ambient_module_name = "AmbientVertexGlobalColor";
                if (ComputeMaterialProgramManifestKey(definition, ambient, nullptr)
                    == base_key)
                    result.diagnostics.emplace_back(
                        "manifest key ignores ambient_module_name");

                MaterialBuildRequest vertex_format = base;
                vertex_format.geometry_vertex_format = &other_geometry;
                if (ComputeMaterialProgramManifestKey(definition, vertex_format, nullptr)
                    == base_key)
                    result.diagnostics.emplace_back(
                        "manifest key ignores geometry_vertex_format");

                MaterialBuildRequest recipe = base;
                recipe.recipe.recipe_name = "gate-variant";

                // ── 变体维度：必须把键推开 ──
                //
                // 只保留**真正改变 GLSL** 的字段。反过来（两个不同变体算出同一个
                // 键）会让发行形态取到不匹配的 SPV，且运行时完全不报错。
                //
                // 旧的断言用 `recipe_name` 当「配方」的代表 —— 那是把**实例**
                // 身份当变体身份，正是缺陷 #1/#3 本身：recipe_name 变了不会重新
                // 发射 GLSL，键却变了 ⇒ 烘焙期只填一条、发行期查另一条即 miss。
                // 现在拆成下面两组正/负断言。
                {
                    MaterialBuildRequest alpha_test = base;
                    alpha_test.recipe.render_state_overrides.has_alpha_test = true;
                    alpha_test.recipe.render_state_overrides.alpha_test = true;
                    if (ComputeMaterialProgramManifestKey(definition, alpha_test, nullptr)
                        == base_key)
                        result.diagnostics.emplace_back(
                            "manifest key ignores alpha_test coverage");

                    MaterialBuildRequest dither = base;
                    dither.recipe.render_state_overrides.has_dither = true;
                    dither.recipe.render_state_overrides.dither = true;
                    if (ComputeMaterialProgramManifestKey(definition, dither, nullptr)
                        == base_key)
                        result.diagnostics.emplace_back(
                            "manifest key ignores dither coverage");

                    MaterialBuildRequest alpha_cutoff = base;
                    alpha_cutoff.recipe.render_state_overrides.has_alpha_cutoff = true;
                    alpha_cutoff.recipe.render_state_overrides.alpha_cutoff =
                        base.recipe.render_state_overrides.alpha_cutoff + 0.125f;
                    if (ComputeMaterialProgramManifestKey(
                            definition, alpha_cutoff, nullptr) == base_key)
                        result.diagnostics.emplace_back(
                            "manifest key ignores alpha_cutoff (baked as a GLSL literal)");

                    MaterialBuildRequest alpha_blend = base;
                    alpha_blend.recipe.render_state_overrides.has_pipeline_config = true;
                    alpha_blend.recipe.render_state_overrides.pipeline_config.alpha_blend = true;
                    if (ComputeMaterialProgramManifestKey(
                            definition, alpha_blend, nullptr) == base_key)
                        result.diagnostics.emplace_back(
                            "manifest key ignores alpha_blend (changes fragment output)");

                    MaterialBuildRequest vertex_nodes = base;
                    vertex_nodes.recipe.vertex_node_config =
                        Make2DNodeConfigNDC(true);
                    if (ComputeMaterialProgramManifestKey(
                            definition, vertex_nodes, nullptr) == base_key)
                        result.diagnostics.emplace_back(
                            "manifest key ignores vertex_node_config");
                }

                // ── 实例维度：必须**不**改变键 ──
                //
                // 这些字段改的是描述符绑定、运行时材质行或 VkPipeline，不是 SPV。
                // 若被折进变体键，同一条 program link 会在烘焙清单里分裂成多条
                // lookup_key，烘焙期触碰到哪条由遍历顺序决定 ⇒ 发行期查另一条
                // 必然 miss（缺陷 #1/#3 的根因）。
                {
                    if (ComputeMaterialProgramManifestKey(definition, recipe, nullptr)
                        != base_key)
                        result.diagnostics.emplace_back(
                            "manifest key must not depend on recipe_name (instance identity)");

                    MaterialBuildRequest texture = base;
                    UpsertRecipeTextureBinding(
                        texture.recipe, "base_color", "gate/texture-a", true);
                    if (ComputeMaterialProgramManifestKey(definition, texture, nullptr)
                        != base_key)
                        result.diagnostics.emplace_back(
                            "manifest key must not depend on texture bindings (instance identity)");

                    MaterialBuildRequest ssbo = base;
                    ssbo.recipe.material_ssbo_binding.ssbo_type = GlobalSSBOType::PBRSurface;
                    ssbo.recipe.material_ssbo_binding.ssbo_id = 7;
                    ssbo.recipe.material_ssbo_binding.data_index = 3;
                    if (ComputeMaterialProgramManifestKey(definition, ssbo, nullptr)
                        != base_key)
                        result.diagnostics.emplace_back(
                            "manifest key must not depend on material_ssbo_binding (instance identity)");

                    MaterialBuildRequest cull = base;
                    cull.recipe.render_state_overrides.has_pipeline_config = true;
                    cull.recipe.render_state_overrides.pipeline_config.cull_mode =
                        VK_CULL_MODE_NONE;
                    if (ComputeMaterialProgramManifestKey(definition, cull, nullptr)
                        != base_key)
                        result.diagnostics.emplace_back(
                            "manifest key must not depend on pipeline-only state (cull_mode)");

                    MaterialBuildRequest fallback_color = base;
                    fallback_color.recipe.fallback_marker_color[0] = 0.25f;
                    if (ComputeMaterialProgramManifestKey(
                            definition, fallback_color, nullptr) != base_key)
                        result.diagnostics.emplace_back(
                            "manifest key must not depend on fallback_marker_color "
                            "(delivered through the material data row)");
                }

                MaterialBuildRequest primitive = base;
                primitive.primitive_type =
                    primitive.primitive_type == PrimitiveType::Triangles
                        ? PrimitiveType::Lines
                        : PrimitiveType::Triangles;
                if (ComputeMaterialProgramManifestKey(definition, primitive, nullptr)
                    == base_key)
                    result.diagnostics.emplace_back(
                        "manifest key ignores primitive_type");
            }
        }

        // ── 3. 损坏清单必须整体拒绝 ──
        {
            const std::wstring manifest_file = manifest_path.c_str();

            std::string bytes;
            if (!ReadGoldenFile(
                    std::filesystem::path(manifest_file).string(), bytes))
                result.diagnostics.emplace_back(
                    "cannot re-read saved manifest for corruption test");
            else
            {
                // 改最后一个字节：落在最后一条 payload 里，应被校验和发现。
                bytes.back() = static_cast<char>(bytes.back() ^ 0x5A);

                const std::wstring corrupt_root =
                    (temp_root / "corrupt").wstring();
                const hgl::OSString corrupt_path =
                    MakeShaderProgramManifestPath(
                        hgl::OSString(corrupt_root.c_str()));

                if (!WriteTextFile(
                        std::filesystem::path(corrupt_root).string(), bytes))
                    result.diagnostics.emplace_back(
                        "cannot write corrupted manifest");
                else
                {
                    ShaderProgramManifest corrupt;
                    if (corrupt.Load(corrupt_path) || !corrupt.IsEmpty())
                        result.diagnostics.emplace_back(
                            "corrupted manifest was accepted");
                }
            }
        }

        std::filesystem::remove_all(temp_root, ec);

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// keep_artifacts=true 时不清空 store_root，并把本次烘焙的期望值（含清单与两
    /// stage SPV 的完整校验值）留在磁盘上 —— 供发行形态探针（RuntimeFormProbe，
    /// ULRE_ENABLE_SHADER_PROGRAM_BUILD=0 构建）跨进程消费，证明「生成器烘焙 → 无生成器运行时
    /// 查表」这条链路成立。
    GateResult RunProgramManifestLookupCase(
        const std::filesystem::path &store_root,
        const bool keep_artifacts)
    {
        GateResult result;
        result.name = keep_artifacts
            ? "PM.emit-release-cache"
            : "PM.release-manifest-lookup";

        ResolvedMaterial definition{};
        if (!BuildResolvedMaterialForDomain(MaterialDomain::StandardOpaque, definition))
        {
            result.diagnostics.emplace_back(
                "domain definition unavailable: StandardOpaque");
            result.passed = false;
            return result;
        }

        const GeometryVertexFormat geometry{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F}
        };

        MaterialBuildRequest request{};
        request.recipe.domain = definition.domain;
        NormalizeRecipe(request.recipe);
        request.geometry_vertex_format = &geometry;
        request.defer_finalize = true;
        request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;
        PassProfileValidationDiagnostic template_diagnostic{};
        if (!ResolveScenePassProfileRequest(
                PassProfileID::ForwardLitShadowedAO,
                hgl::graph::ShaderStage::Fragment,
                MakeIdentityForwardLitProfile(),
                request.render_template_request, template_diagnostic)
         || !AppendMaterialPassProfileRoots(
                definition, request.render_template_request))
        {
            result.diagnostics.emplace_back(
                "render template resolution failed for Lit");
            result.passed = false;
            return result;
        }

        std::unique_ptr<ShaderBuildContext> baked;
        {
            // 片段源走领域固定源：本用例算的是清单/缓存身份，与片段怎么写无关。
            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(definition, request, fixed))
            {
                result.diagnostics.emplace_back(
                    std::string("fixed fragment source unavailable: ")
                    + GetLastDomainShaderBuildBackendError());
                result.passed = false;
                return result;
            }
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;
            baked.reset(
                AssembleMaterialProgram(nullptr, request, definition, nullptr, fixed_stage));
        }
        if (!baked || !baked->HasProgramLink() || !baked->HasProgramArtifactMetadata())
        {
            result.diagnostics.emplace_back(
                "generator-side build failed to produce a program identity");
            result.passed = false;
            return result;
        }

        std::error_code ec;
        if (!keep_artifacts)
            std::filesystem::remove_all(store_root, ec);

        ShaderArtifactStore store(
            hgl::OSString(store_root.wstring().c_str()),
            ShaderCacheMode::BuildIfMissing);

        // 参数化成对的假 SPIR-V：只要魔数正确、长度 4 字节对齐，装载侧就应当收下。
        const uint32_t fake_mesh_spv[] =
            {ShaderArtifactSPVMagic, 0x4101u, 0x4102u, 0x4103u};
        const uint32_t fake_fragment_spv[] =
            {ShaderArtifactSPVMagic, 0x4201u, 0x4202u, 0x4203u};

        const ShaderLinkSpec &link = baked->GetProgramLink();
        const ShaderProgramArtifactMetadata metadata =
            baked->GetProgramArtifactMetadata();

        if (!store.SaveStageSPV(
                link.mesh_stage, fake_mesh_spv, sizeof(fake_mesh_spv), 0)
         || !store.SaveStageSPV(
                link.fragment_stage, fake_fragment_spv,
                sizeof(fake_fragment_spv), 0)
         || !store.SaveProgramMetadata(link, metadata))
        {
            result.diagnostics.emplace_back(
                "failed to write program artifacts into the baking store");
            result.passed = false;
            return result;
        }

        ShaderProgramManifestEntry entry{};
        entry.lookup_key =
            ComputeMaterialProgramManifestKey(definition, request, nullptr);
        entry.program_link = link;
        entry.requires_recipe_runtime_rows = baked->RequiresRecipeRuntimeRows();

        ShaderProgramBuildBackend *previous_backend =
            GetShaderProgramBuildBackend();
        SetShaderProgramBuildBackend(nullptr);

        if (!EnsureShaderProgramManifestBackend(&store))
        {
            result.diagnostics.emplace_back(
                "manifest backend installation failed");
        }
        else if (!RecordShaderProgramManifestEntry(&store, nullptr, entry))
        {
            result.diagnostics.emplace_back(
                "manifest entry recording failed");
        }
        else
        {
            // 关键：把内存态丢掉，逼后端从磁盘重新装载，证明查表不依赖任何
            // 同进程残留状态（发行形态开机就是这样冷的）。
            UninstallShaderProgramManifestBackend();
            if (!EnsureShaderProgramManifestBackend(&store))
            {
                result.diagnostics.emplace_back(
                    "manifest backend reinstallation failed");
            }
            else if (GetShaderProgramManifestEntryCount() != 1)
            {
                result.diagnostics.emplace_back(
                    "manifest reloaded from disk with unexpected entry count");
            }
            else
            {
                ShaderProgramBuildBackend *backend =
                    GetShaderProgramBuildBackend();

                std::unique_ptr<ShaderBuildContext> loaded(
                    backend->CreateBuildContext(nullptr, definition, request));
                if (!loaded)
                {
                    result.diagnostics.emplace_back(
                        "manifest backend failed to assemble a build context");
                }
                else if (!loaded->HasProgramLink()
                      || loaded->GetProgramLink().BuildKey().GetDigest()
                            != link.BuildKey().GetDigest()
                      || !loaded->UsedCachedMeshStage()
                      || !loaded->UsedCachedFragmentStage()
                      || loaded->RequiresRecipeRuntimeRows()
                            != entry.requires_recipe_runtime_rows
                      || !backend->FinalizeBuildContext(loaded.get()))
                {
                    result.diagnostics.emplace_back(
                        "manifest-assembled build context is inconsistent");
                }
                else
                {
                    const ShaderCreateInfo *mesh =
                        loaded->GetStageShader(ShaderStage::Mesh);
                    const ShaderCreateInfo *fragment =
                        loaded->GetStageShader(ShaderStage::Fragment);
                    if (!mesh || !fragment
                     || mesh->GetSPVSize() != sizeof(fake_mesh_spv)
                     || fragment->GetSPVSize() != sizeof(fake_fragment_spv)
                     || std::memcmp(
                            mesh->GetSPVData(), fake_mesh_spv,
                            sizeof(fake_mesh_spv)) != 0
                     || std::memcmp(
                            fragment->GetSPVData(), fake_fragment_spv,
                            sizeof(fake_fragment_spv)) != 0)
                    {
                        result.diagnostics.emplace_back(
                            "manifest-lookup SPV bytes do not match the baked artifacts");
                    }
                }

                // 查不到必须硬失败：发行形态没有编译器可回落。
                MaterialBuildRequest ambient_miss = request;
                ambient_miss.ambient_module_name = "AmbientVertexGlobalColor";
                std::unique_ptr<ShaderBuildContext> miss(
                    backend->CreateBuildContext(nullptr, definition, ambient_miss));
                if (miss)
                    result.diagnostics.emplace_back(
                        "manifest backend must hard-fail on an unknown key");

                // ── 多实例共享同一变体（缺陷 #1/#3 的回归）──
                //
                // 同一变体的多个实例只在**实例维度**上不同（配方名、贴图绑定、
                // SSBO 行）。它们发射出的 GLSL 逐字节相同 ⇒ 同一条 program link。
                // 若清单键折进实例维度，烘焙期只填了第一个实例的那条键，发行期
                // 用第二个实例查表就必然 miss —— 现象是「换张贴图材质就没了」。
                MaterialBuildRequest second_instance = request;
                second_instance.recipe.recipe_name = "gate-secondary-instance";
                UpsertRecipeTextureBinding(
                    second_instance.recipe, "base_color",
                    "gate/secondary-texture", true);
                second_instance.recipe.material_ssbo_binding.ssbo_type =
                    GlobalSSBOType::PBRSurface;
                second_instance.recipe.material_ssbo_binding.ssbo_id = 11;
                second_instance.recipe.material_ssbo_binding.data_index = 5;

                if (ComputeMaterialProgramManifestKey(
                        definition, second_instance, nullptr)
                    != entry.lookup_key)
                    result.diagnostics.emplace_back(
                        "manifest key splits on instance-only recipe data");

                std::unique_ptr<ShaderBuildContext> second_instance_ctx(
                    backend->CreateBuildContext(
                        nullptr, definition, second_instance));
                if (!second_instance_ctx
                 || !second_instance_ctx->HasProgramLink()
                 || second_instance_ctx->GetProgramLink().BuildKey().GetDigest()
                        != link.BuildKey().GetDigest())
                    result.diagnostics.emplace_back(
                        "second instance of the same variant misses the baked manifest");
            }
        }

        // 无论成败都要把后端槽位还原，避免污染同进程里的其它门。
        UninstallShaderProgramManifestBackend();
        SetShaderProgramBuildBackend(previous_backend);

        result.passed = result.diagnostics.empty();

        if (!keep_artifacts)
        {
            std::filesystem::remove_all(store_root, ec);
            return result;
        }

        // 发行形态探针要对着「已知正确值」比对，故把本次烘焙的结果固化成文本。
        if (result.passed && !WriteManifestExpectation(
                store, definition, entry))
            result.diagnostics.emplace_back(
                "failed to write manifest expectation file");

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// compute 程序的「烘焙 → 只读查表」端到端门。
    ///
    /// 与上面两条门互补：那两条走材质语义键（program_manifest.pmf），这条走
    /// compute 程序名（compute_manifest.pmf）。发行形态里 GPU cull 这一类程序
    /// 既没有材质定义也没有 GLSL 源，唯一的来源就是这里烘焙出的 SPV ——
    /// 门挂掉就意味着发行包起不来。
    ///
    /// 编译用的是**真实** `entity_gpu_cull.glsl`（跑通反而能提前发现源被改坏），
    /// 校验的却只有「发行形态真正需要的那几块数据」：清单可装载、SPV 字节与
    /// digest 对得上、push constant 契约带得回来。
    GateResult RunComputeProgramCacheCase(
        const std::filesystem::path &store_root,
        const bool keep_artifacts)
    {
        GateResult result;
        result.name = keep_artifacts
            ? "PM.emit-compute-release-cache"
            : "PM.compute-release-cache";

        const char *kProgramName = "GPUCullSystem.Pipeline";
        // 与 GPUCullSystem.cpp 的 CullPushConstants 同值：那边 static_assert 锁死 112B，
        // 这里用字面量是刻意的——门要独立断言这个数，而不是把被测代码的常量抄一遍。
        const uint32_t kPushConstantSize = 112;

        std::string glsl_source;
        const std::filesystem::path glsl_path =
            std::filesystem::path(GetShaderLibraryPath().c_str())
            / "module" / "descriptor" / "ecs" / "entity_gpu_cull.glsl";
        if (!ReadGoldenFile(glsl_path.string(), glsl_source)
         || glsl_source.empty())
        {
            result.diagnostics.emplace_back(
                "cannot read compute GLSL source: " + glsl_path.string());
            result.passed = false;
            return result;
        }

        ShaderCreateInfo sci(ShaderStage::Compute);
        sci.SetFinalGLSL(glsl_source.c_str());
        if (!sci.CompileFinalGLSLToSPV()
         || !sci.HasSPVData() || sci.GetSPVSize() == 0)
        {
            result.diagnostics.emplace_back(
                "compute GLSL failed to compile (source or include graph broken)");
            result.passed = false;
            return result;
        }

        const uint64_t spv_size = static_cast<uint64_t>(sci.GetSPVSize());
        uint32_t reflected_push_constant_size = 0;
        if (!GetSPVPushConstantEnd(
                sci.GetSPVData(),
                sci.GetSPVSize(),
                VK_SHADER_STAGE_COMPUTE_BIT,
                reflected_push_constant_size))
        {
            result.diagnostics.emplace_back(
                "compute SPIR-V push-constant reflection failed");
            result.passed = false;
            return result;
        }
        if (reflected_push_constant_size != kPushConstantSize)
        {
            result.diagnostics.emplace_back(
                "reflected compute push-constant extent differs from CullPushConstants");
            result.passed = false;
            return result;
        }

        std::error_code ec;
        if (!keep_artifacts)
            std::filesystem::remove_all(store_root, ec);

        ShaderArtifactStore store(
            hgl::OSString(store_root.wstring().c_str()),
            ShaderCacheMode::BuildIfMissing);

        const hgl::OSString manifest_path =
            MakeShaderComputeProgramManifestPath(store.GetRootPath());

        if (!RecordShaderComputeProgramArtifact(
                store, kProgramName, sci.GetSPVData(), spv_size,
                reflected_push_constant_size))
        {
            result.diagnostics.emplace_back(
                "failed to record the compute artifact");
            result.passed = false;
            return result;
        }

        // 重复烘焙同一份产物必须幂等：清单不该长出第二条，文件也不该被改坏。
        if (!RecordShaderComputeProgramArtifact(
                store, kProgramName, sci.GetSPVData(), spv_size,
                reflected_push_constant_size))
        {
            result.diagnostics.emplace_back(
                "re-recording the same compute artifact failed");
        }

        {
            ShaderComputeProgramManifest manifest;
            if (!manifest.Load(manifest_path))
                result.diagnostics.emplace_back(
                    "compute manifest unreadable after baking");
            else if (manifest.GetEntryCount() != 1)
                result.diagnostics.emplace_back(
                    "compute manifest must hold exactly one entry after re-baking");
            else if (manifest.GetCodegenGeneration()
                        != contract::MATERIAL_CODEGEN_GENERATION)
                result.diagnostics.emplace_back(
                    "compute manifest carried a stale codegen generation");
        }

        // 关键：换一个**只读** store 冷装，证明查表不依赖烘焙时留在内存里的任何东西。
        ShaderArtifactStore readonly_store(
            hgl::OSString(store_root.wstring().c_str()),
            ShaderCacheMode::ReadOnly);

        if (readonly_store.GetCacheMode() != ShaderCacheMode::ReadOnly)
            result.diagnostics.emplace_back(
                "read-only store did not honour its cache mode");

        // 只读 store 绝不能回写：哪怕烘焙入口被误调，也必须被拒。
        if (RecordShaderComputeProgramArtifact(
                readonly_store, kProgramName, sci.GetSPVData(),
                spv_size, reflected_push_constant_size))
            result.diagnostics.emplace_back(
                "read-only store accepted a compute artifact write");

        hgl::ValueArray<uint8_t> loaded_spv{};
        uint32_t loaded_push_constant_size = 0;
        if (!LoadShaderComputeProgramArtifact(
                readonly_store, kProgramName, loaded_spv,
                &loaded_push_constant_size))
        {
            result.diagnostics.emplace_back(
                "read-only lookup of the baked compute artifact failed");
        }
        else
        {
            if (loaded_spv.GetCount() != static_cast<int>(spv_size))
                result.diagnostics.emplace_back(
                    "loaded compute SPV size differs from the baked size");
            else if (std::memcmp(loaded_spv.GetData(), sci.GetSPVData(),
                                 static_cast<size_t>(spv_size)) != 0)
                result.diagnostics.emplace_back(
                    "loaded compute SPV bytes differ from the baked bytes");

            if (loaded_push_constant_size != reflected_push_constant_size)
                result.diagnostics.emplace_back(
                    "compute push constant contract did not survive the cache");
        }

        // 查不到必须硬失败：发行形态没有编译器可回落。
        {
            hgl::ValueArray<uint8_t> missing{};
            if (LoadShaderComputeProgramArtifact(
                    readonly_store, "NoSuch.Compute.Program", missing))
                result.diagnostics.emplace_back(
                    "unknown compute program must not resolve from the cache");
        }

        // 清单被改一个字节就必须整份作废（宁可缺 artifact，也不要用坏数据建管线）。
        if (result.diagnostics.empty())
        {
            std::string bytes;
            if (!ReadGoldenFile(std::filesystem::path(manifest_path.c_str()).string(),
                                bytes))
                result.diagnostics.emplace_back(
                    "cannot re-read the compute manifest for corruption test");
            else if (bytes.empty())
                result.diagnostics.emplace_back(
                    "compute manifest is empty");
            else
            {
                bytes.back() = static_cast<char>(bytes.back() ^ 0x5A);
                const std::filesystem::path corrupt_root =
                    std::filesystem::path(store_root) / "compute-corrupt";

                const hgl::OSString corrupt_path =
                    MakeShaderComputeProgramManifestPath(
                        hgl::OSString(corrupt_root.wstring().c_str()));

                if (!WriteTextFile(
                        std::filesystem::path(corrupt_path.c_str()).string(),
                        bytes))
                    result.diagnostics.emplace_back(
                        "cannot write the corrupted compute manifest");
                else
                {
                    ShaderComputeProgramManifest corrupt;
                    if (corrupt.Load(corrupt_path) || !corrupt.IsEmpty())
                        result.diagnostics.emplace_back(
                            "corrupted compute manifest was accepted");
                }

                std::filesystem::remove_all(corrupt_root, ec);
            }
        }

        result.passed = result.diagnostics.empty();

        if (!keep_artifacts)
        {
            std::filesystem::remove_all(store_root, ec);
            return result;
        }

        if (result.passed)
        {
            // 发行形态探针要对着「已知正确值」比对；字段名与 RuntimeFormProbe.cpp 的
            // LoadComputeExpectation() 严格对应，改一处必须同步另一处。
            const std::filesystem::path expect_path =
                std::filesystem::path(manifest_path.c_str()).parent_path()
                / "compute_expectation.txt";

            hgl::hash::FNV1aHasher64 hasher;
            hasher.AppendBytes(sci.GetSPVData(), static_cast<size_t>(spv_size));

            std::ofstream out(expect_path, std::ios::binary);
            if (!out)
                result.diagnostics.emplace_back(
                    "cannot write the compute expectation file");
            else
            {
                out << "program_name=" << kProgramName << "\n"
                    << "spv_bytes=" << spv_size << "\n"
                    << "spv_digest=" << static_cast<uint64_t>(hasher) << "\n"
                    << "push_constant_size=" << kPushConstantSize << "\n";

                if (!out.good())
                    result.diagnostics.emplace_back(
                        "failed writing the compute expectation file");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

} // namespace gate
