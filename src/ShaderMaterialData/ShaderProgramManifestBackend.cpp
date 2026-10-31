#include <hgl/mtl/ShaderProgramManifestBackend.h>

#include <hgl/common/ShaderStageDef.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include <hgl/mtl/ShaderProgramManifestKey.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/utf.h>

namespace hgl::graph::mtl
{
    namespace
    {
        struct ManifestState
        {
            ShaderProgramManifest manifest;
            ShaderArtifactStore *store = nullptr;
            OSString path;
            bool load_attempted = false;
            bool stale_warned = false;
            bool profile_warned = false;
        };

        ManifestState &State()
        {
            static ManifestState state;
            return state;
        }

        /// 绑定到某个 store 的清单。首次调用读盘；读不到只代表「还没有条目」，
        /// 记录路径仍然可用（第一次烘焙会建出文件）。
        bool LoadManifest(ShaderArtifactStore *store)
        {
            ManifestState &state = State();

            if (state.store != store)
            {
                state.store = store;
                state.manifest.Clear();
                state.path = store
                    ? MakeShaderProgramManifestPath(store->GetRootPath())
                    : OSString();
                state.load_attempted = false;
                state.stale_warned = false;
                state.profile_warned = false;
            }

            if (state.load_attempted)
                return !state.manifest.IsEmpty();

            state.load_attempted = true;

            if (state.path.IsEmpty())
                return false;

            if (!state.manifest.Load(state.path))
                return false;

            if (state.manifest.GetCodegenGeneration()
                != contract::MATERIAL_CODEGEN_GENERATION)
            {
                if (!state.stale_warned)
                {
                    state.stale_warned = true;
                    GLogWarning("[ShaderProgramManifest] codegen generation mismatch: "
                                "manifest=%u runtime=%u; manifest ignored",
                                state.manifest.GetCodegenGeneration(),
                                contract::MATERIAL_CODEGEN_GENERATION);
                }
                state.manifest.Clear();
                return false;
            }

            GLogInfo("[ShaderProgramManifest] loaded %u entries from %s",
                     state.manifest.GetEntryCount(),
                     ToU8String(state.path).c_str());
            return !state.manifest.IsEmpty();
        }

        void WarnIfProfileMismatch(
            ManifestState &state,
            const contract::PhysicalDeviceProfileLite *profile)
        {
            if (state.manifest.IsEmpty())
                return;

            const uint64 runtime_profile_hash =
                contract::GetShaderCompilerProfileHash(profile);
            if (state.manifest.GetCompilerProfileHash() == runtime_profile_hash)
                return;

            if (state.profile_warned)
                return;
            state.profile_warned = true;
            GLogError("[ShaderProgramManifest] compiler profile mismatch: "
                      "manifest=0x%llx runtime=0x%llx; lookup will miss",
                      static_cast<unsigned long long>(state.manifest.GetCompilerProfileHash()),
                      static_cast<unsigned long long>(runtime_profile_hash));
        }

        class ManifestShaderProgramBuildBackend final : public ShaderProgramBuildBackend
        {
            ShaderBuildContext *CreateBuildContext(
                const contract::PhysicalDeviceProfileLite *profile,
                const MaterialDefinition &definition,
                const MaterialDefinitionBuildRequest &request) override
            {
                ShaderArtifactStore *store = State().store;
                if (!store || !LoadManifest(store))
                {
                    GLogError("[ShaderProgramManifest] no manifest available for id=%s "
                              "(release form requires a baked shader-cache)",
                              definition.definition_id.c_str());
                    return nullptr;
                }

                ManifestState &state = State();
                WarnIfProfileMismatch(state, profile);

                const uint64 lookup_key =
                    ComputeMaterialProgramManifestKey(definition, request, profile);
                const ShaderProgramManifestEntry *entry = state.manifest.Find(lookup_key);
                if (!entry)
                {
                    GLogError("[ShaderProgramManifest] missing program entry: id=%s key=0x%llx "
                              "purpose=%u ambient=%s entries=%u",
                              definition.definition_id.c_str(),
                              static_cast<unsigned long long>(lookup_key),
                              static_cast<uint32>(request.shader_program_purpose),
                              request.ambient_module_name ? request.ambient_module_name : "<default>",
                              state.manifest.GetEntryCount());
                    return nullptr;
                }

                const ShaderLinkSpec &link = entry->program_link;

                ShaderProgramArtifactMetadata metadata{};
                if (!store->LoadProgramMetadata(link, metadata))
                {
                    GLogError("[ShaderProgramManifest] program metadata missing: id=%s program=%s",
                              definition.definition_id.c_str(),
                              link.BuildKey().ToString().c_str());
                    return nullptr;
                }

                ShaderBuildContext *ctx = new ShaderBuildContext(
                    request.primitive_type,
                    static_cast<uint32_t>(ShaderStage::Mesh)
                        | static_cast<uint32_t>(ShaderStage::Fragment));

                const ShaderStage stages[] = { ShaderStage::Mesh, ShaderStage::Fragment };
                const ShaderStageKey *stage_keys[] =
                {
                    &link.mesh_stage,
                    &link.fragment_stage
                };

                for (uint32 i = 0; i < 2; ++i)
                {
                    ShaderCreateInfo *stage_info = ctx->GetStageShader(stages[i]);
                    if (!stage_info)
                    {
                        GLogError("[ShaderProgramManifest] build context missing stage %u",
                                  static_cast<uint32>(stages[i]));
                        delete ctx;
                        return nullptr;
                    }

                    ValueArray<uint8> spv;
                    if (!store->LoadStageSPV(*stage_keys[i], spv)
                     || !stage_info->SetCachedSPVData(spv.GetData(), spv.GetCount()))
                    {
                        GLogError("[ShaderProgramManifest] stage SPV missing: id=%s stage=%s",
                                  definition.definition_id.c_str(),
                                  stage_keys[i]->ToString().c_str());
                        delete ctx;
                        return nullptr;
                    }
                }

                ctx->SetProgramLink(link);
                ctx->SetProgramArtifactMetadata(metadata);
                ctx->SetArtifactStore(store);
                ctx->SetUsedCachedStages(true, true);
                ctx->SetRequiresRecipeRuntimeRows(entry->requires_recipe_runtime_rows);
                ctx->SetRequiresRecipePayloadRow(entry->requires_recipe_payload_row);
                return ctx;
            }

            /// Manifest 路径下 SPV 在 `CreateBuildContext` 里已经装好，收尾只做完整性校验。
            bool FinalizeBuildContext(ShaderBuildContext *ctx) override
            {
                if (!ctx || !ctx->HasProgramLink() || !ctx->HasProgramArtifactMetadata())
                    return false;

                const ShaderStage stages[] = { ShaderStage::Mesh, ShaderStage::Fragment };
                for (const ShaderStage stage : stages)
                {
                    const ShaderCreateInfo *info = ctx->GetStageShader(stage);
                    if (!info || !info->HasSPVData() || info->GetSPVSize() == 0)
                        return false;
                }

                return true;
            }
        };

        ManifestShaderProgramBuildBackend *GetBackendInstance()
        {
            static ManifestShaderProgramBuildBackend backend;
            return &backend;
        }
    }//namespace

    bool EnsureShaderProgramManifestBackend(ShaderArtifactStore *store)
    {
        // 生成器后端先到先得：开发形态里 InitShaderCompiler() 早已装好，
        // 这里绝不能顶替它。
        if (GetShaderProgramBuildBackend() != nullptr)
            return true;

        if (!store)
            return false;

        LoadManifest(store);
        SetShaderProgramBuildBackend(GetBackendInstance());
        return true;
    }

    void UninstallShaderProgramManifestBackend()
    {
        if (GetShaderProgramBuildBackend() == GetBackendInstance())
            SetShaderProgramBuildBackend(nullptr);

        ManifestState &state = State();
        state.manifest.Clear();
        state.store = nullptr;
        state.path.Clear();
        state.load_attempted = false;
    }

    bool RecordShaderProgramManifestEntry(
        ShaderArtifactStore *store,
        const contract::PhysicalDeviceProfileLite *profile,
        const ShaderProgramManifestEntry &entry)
    {
        if (!store || store->GetCacheMode() == ShaderCacheMode::ReadOnly)
            return false;

        ManifestState &state = State();
        LoadManifest(store);

        if (state.path.IsEmpty())
            return false;

        const uint64 profile_hash = contract::GetShaderCompilerProfileHash(profile);
        if (state.manifest.GetCodegenGeneration() != contract::MATERIAL_CODEGEN_GENERATION
         || state.manifest.GetCompilerProfileHash() != profile_hash)
        {
            // 章不同说明这份清单来自另一代生成器/另一个编译目标 —— 它的键与当前
            // 运行时算出的键不在同一空间，整体作废重来，不做局部合并。
            state.manifest.Clear();
            state.manifest.SetStamps(profile_hash, contract::MATERIAL_CODEGEN_GENERATION);
        }

        if (!state.manifest.Upsert(entry))
            return true;

        if (!state.manifest.Save(state.path))
        {
            GLogWarning("[ShaderProgramManifest] failed to save manifest: %s",
                        ToU8String(state.path).c_str());
            return false;
        }

        return true;
    }

    uint32 GetShaderProgramManifestEntryCount()
    {
        return State().manifest.GetEntryCount();
    }
}//namespace hgl::graph::mtl
