#include<hgl/graph/module/ShaderMaterialProgramManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/module/MaterialAssetLoader.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLayoutData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKResourceDomain.h>
#include<hgl/vk/VKDomainResourceBinding.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialCreatePrecheckAdapter.h>
#include<hgl/graph/module/MaterialFinalizeFlowAdapter.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderStageIO.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/RecipeToKey.h>
#include<hgl/object/ObjectTracker.h>
#include<cstdio>
#include<cstdint>
#include<vector>
#include<algorithm>

namespace hgl::graph{


namespace
{
    void CreateShaderStageList(ShaderStageCreateInfoList &shader_stage_list,ShaderModuleMap *shader_maps)
    {
        const ShaderModule *sm;

        const int shader_count=shader_maps->GetCount();
        shader_stage_list.resize(shader_count);

        VkPipelineShaderStageCreateInfo *p=shader_stage_list.data();

        for(auto [stage, module] : *shader_maps)
        {
            sm = module;
            mem_copy(p,sm->GetCreateInfo(),1);

            ++p;
        }
    }

    bool BuildLegacyShaderModules(ShaderMaterialProgramManager *manager,
                                  const AnsiString &mtl_name,
                                  const ShaderStageMap &sci_map,
                                  ShaderModuleMap *shader_maps)
    {
        if (!manager || !shader_maps)
        {
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] BuildLegacyShaderModules failed for '%s': manager=%p shader_maps=%p\n",
                mtl_name.c_str(),
                manager,
                shader_maps);
            return false;
        }

        if (sci_map.GetCount() < 2)
        {
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] BuildLegacyShaderModules failed for '%s': shader count=%d (expected >= 2)\n",
                mtl_name.c_str(),
                sci_map.GetCount());
            return false;
        }

        for (auto [stage, sci_ptr] : sci_map)
        {
            (void)stage;

            if (!sci_ptr)
            {
                std::fprintf(stderr,
                    "[ShaderMaterialProgramManager] BuildLegacyShaderModules failed for '%s': shader create info is null\n",
                    mtl_name.c_str());
                return false;
            }

            const ShaderModule *module = manager->CreateShaderModule(mtl_name, sci_ptr);
            if (!module)
            {
                std::fprintf(stderr,
                    "[ShaderMaterialProgramManager] BuildLegacyShaderModules failed for '%s': CreateShaderModule returned null for stage=%u\n",
                    mtl_name.c_str(),
                    static_cast<unsigned>(sci_ptr->GetShaderStage()));
                return false;
            }

            shader_maps->Add(module);
        }

        return true;
    }

    static mtl::MaterialKey MakeMaterialKeyFromVariantKey(const mtl::MaterialVariantKey &vk,
                                                          const mtl::MaterialCreateConfig *cfg = nullptr)
    {
        mtl::MaterialKey k{};
        k.variant = vk;
        k.pass    = vk.pass_hint;

        // Distinguish cache entries by configured primitive type so one variant
        // can safely compile both line and triangle materials in the same run.
        if (cfg)
        {
            k.def_id = static_cast<mtl::StaticMaterialDefId>(static_cast<uint32_t>(cfg->prim));
        }

        // schema / version fields: placeholder zeros (Step 6 will fill)
        return k;
    }

    template<typename CreateConfigT>
    static void ForceVertexFragmentStages(CreateConfigT &cfg)
    {
        cfg.shader_stage_flag_bit &= (uint32_t(ShaderStage::Vertex) | uint32_t(ShaderStage::Fragment));
    }

    static const VIAArray *TryGetVertexInputArray(const mtl::MaterialCreateInfo *mci,
                                                  const AnsiString &mtl_name)
    {
        if (!mci)
            return nullptr;

        const ShaderCreateInfo *vertex_shader = mci->GetStageShader(ShaderStage::Vertex);
        if (!vertex_shader)
            return nullptr;

        const auto *vertex_io = dynamic_cast<const VertexShaderStageIO *>(vertex_shader->GetShaderStageIO());
        if (!vertex_io)
        {
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] WARN material='%s' vertex stage exists but StageIO is not VertexShaderStageIO; vertex_input fallback to null\n",
                mtl_name.c_str());
            return nullptr;
        }

        const VIAArray &via = vertex_io->GetInput();
        if (via.count == 0)
            return nullptr;

        return &via;
    }

}//namespace

GRAPH_MODULE_CONSTRUCT(ShaderMaterialProgramManager)
{
}

const ShaderModule *ShaderMaterialProgramManager::CreateShaderModule(const AnsiString &sm_name,const ShaderCreateInfo *sci)
{
    VulkanDevice *device = GetDevice();
    if(!device)
    {
        std::fprintf(stderr, "[ShaderMaterialProgramManager] CreateShaderModule failed: device is null for '%s'\n", sm_name.c_str());
        return(nullptr);
    }
    if(sm_name.IsEmpty())
    {
        std::fprintf(stderr, "[ShaderMaterialProgramManager] CreateShaderModule failed: shader module name is empty\n");
        return(nullptr);
    }
    if(!sci)
    {
        std::fprintf(stderr, "[ShaderMaterialProgramManager] CreateShaderModule failed for '%s': ShaderCreateInfo is null\n", sm_name.c_str());
        return(nullptr);
    }

    const int bit_offset=GetBitOffset((uint32_t)sci->GetShaderStage());

    if(bit_offset<0||bit_offset>VK_SHADER_STAGE_TYPE_COUNT)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateShaderModule failed for '%s': invalid stage bit offset=%d stage=%u\n",
            sm_name.c_str(),
            bit_offset,
            static_cast<unsigned>(sci->GetShaderStage()));
        return(nullptr);
    }

    ShaderModule *sm;

    ShaderModuleMapByName &sm_map=shader_module_by_name[bit_offset];

    if(sm_map.Get(sm_name,sm))
        return sm;

    sm=device->CreateShaderModule((VkShaderStageFlagBits)sci->GetShaderStage(),sci->GetSPVData(),sci->GetSPVSize());

    if(!sm)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateShaderModule failed for '%s': VulkanDevice::CreateShaderModule returned null (stage=%u spv_size=%zu)\n",
            sm_name.c_str(),
            static_cast<unsigned>(sci->GetShaderStage()),
            sci->GetSPVSize());
        return(nullptr);
    }

    sm_map.Add(sm_name,sm);

    #ifdef _DEBUG
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
            {
                AnsiString shader_name = "Shader:" + sm_name + AnsiString(":") + GetShaderStageName((VkShaderStageFlagBits)sci->GetShaderStage());
                du->SetShaderModule(*sm, shader_name);
            }
        }
    #endif//_DEBUG

    return sm;
}

const ShaderModule *ShaderMaterialProgramManager::CreateShaderModuleFromSPV(const AnsiString &sm_name,
                                                                const VkShaderStageFlagBits stage,
                                                                const uint32_t *spv_data,
                                                                const size_t spv_size)
{
    VulkanDevice *device = GetDevice();
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);
    if(!spv_data||spv_size==0)return(nullptr);

    const int bit_offset=GetBitOffset((uint32_t)stage);

    if(bit_offset<0||bit_offset>VK_SHADER_STAGE_TYPE_COUNT)return(nullptr);

    ShaderModule *sm;

    ShaderModuleMapByName &sm_map=shader_module_by_name[bit_offset];

    if(sm_map.Get(sm_name,sm))
        return sm;

    sm=device->CreateShaderModule(stage,spv_data,spv_size);

    if(!sm)
        return(nullptr);

    sm_map.Add(sm_name,sm);

    #ifdef _DEBUG
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
            {
                AnsiString shader_name = "Shader:" + sm_name + AnsiString(":") + GetShaderStageName(stage);
                du->SetShaderModule(*sm, shader_name);
            }
        }
    #endif//_DEBUG

    return sm;
}

GraphicsPipelineLayoutData *ShaderMaterialProgramManager::CreateMaterialGraphicsPipelineLayoutData(const AnsiString &mtl_name, const MaterialDescriptorManager *desc_manager)
{
    VulkanDevice *device = GetDevice();
    if(!device)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterialGraphicsPipelineLayoutData failed for '%s': device is null\n",
            mtl_name.c_str());
        return nullptr;
    }

    GraphicsPipelineLayoutData *pld = device->CreateGraphicsPipelineLayoutData(desc_manager);

    if(pld)
    {
        #ifdef _DEBUG
            DebugUtils *du = device->GetDebugUtils();
            if(du)
                du->SetPipelineLayout(pld->pipeline_layout, "PipelineLayout:" + mtl_name);
        #endif//_DEBUG
    }

    return pld;
}

MaterialParameters *ShaderMaterialProgramManager::CreateMaterialMP(const AnsiString &mtl_name, const MaterialDescriptorManager *desc_manager, const GraphicsPipelineLayoutData *pld, const DescriptorSetType &desc_set_type)
{
    VulkanDevice *device = GetDevice();
    if(!device)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterialMP failed for '%s': device is null (set=%d)\n",
            mtl_name.c_str(),
            static_cast<int>(desc_set_type));
        return nullptr;
    }

    MaterialParameters *mp = device->CreateMP(desc_manager, pld, desc_set_type);

    if(mp)
    {
        #ifdef _DEBUG
            DebugUtils *du = device->GetDebugUtils();
            if(du)
            {
                AnsiString debug_name = mtl_name + AnsiString(":") + GetDescriptorSetTypeName(desc_set_type);
                du->SetDescriptorSet(mp->GetVkDescriptorSet(), "DescSet:" + debug_name);
                du->SetDescriptorSetLayout(pld->layouts[static_cast<int>(desc_set_type)], "DescSetLayout:" + debug_name);
            }
        #endif//_DEBUG
    }

    return mp;
}

void ShaderMaterialProgramManager::ApplyMaterialFinalizePlan(ShaderMaterialProgram *mtl, const AnsiString &mtl_name, const mtl::MaterialCreateInfo &mci)
{
    if(!mtl)
        return;

    MaterialFinalizePlan finalize_plan;
    BuildMaterialFinalizePlan(mtl->desc_manager, mci, finalize_plan);

    mtl->pipeline_layout_data = CreateMaterialGraphicsPipelineLayoutData(mtl_name, mtl->desc_manager);

    for(const auto set_type : finalize_plan.mp_set_types)
    {
        mtl->mp_array[(int)set_type] = CreateMaterialMP(mtl_name, mtl->desc_manager, mtl->pipeline_layout_data, set_type);
    }

    mtl->mi_schema     = finalize_plan.mi_schema;

    std::fprintf(stderr,
        "[ShaderMaterialProgramManager] Finalize material='%s' mi_bytes=%u mi_max=%u schema=%u schema_file=%s descriptor_sets=%zu\n",
        mtl_name.c_str(),
        finalize_plan.mi_data_bytes,
        finalize_plan.mi_max_count,
        static_cast<unsigned>(finalize_plan.mi_schema),
        finalize_plan.mi_schema_file.empty() ? "<none>" : finalize_plan.mi_schema_file.c_str(),
        finalize_plan.mp_set_types.size());
}

ShaderMaterialProgram *ShaderMaterialProgramManager::TryInitializeFallbackMaterial()
{
    if(fallback_material)
        return fallback_material;

    mtl::Material3DCreateConfig cfg;
    fallback_material = CreateMaterial(mtl::MaterialPreset::Checkerboard3D, &cfg);

    if(!fallback_material)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] TryInitializeFallbackMaterial failed: Checkerboard3D creation failed\n");
        return nullptr;
    }

    std::fprintf(stdout,
        "[ShaderMaterialProgramManager] Fallback material initialized: default checkerboard name=%s\n",
        fallback_material->GetName().c_str());

    return fallback_material;
}

ShaderMaterialProgram *ShaderMaterialProgramManager::GetFallbackMaterial()
{
    if(!fallback_material)
        TryInitializeFallbackMaterial();

    return fallback_material;
}

ShaderMaterialProgram *ShaderMaterialProgramManager::ResolveCreateFailureWithFallback(ShaderMaterialProgram *created)
{
    if(created)
        return created;

    ShaderMaterialProgram *fallback = GetFallbackMaterial();
    if(fallback)
        acquire_fallback_used.fetch_add(1);

    return fallback;
}

bool ShaderMaterialProgramManager::ExecuteMaterialBuildPipeline(ShaderMaterialProgram *mtl,
                                                   const AnsiString &mtl_name,
                                                   const mtl::MaterialCreateInfo *mci,
                                                   const ShaderStageMap &sci_map)
{
    if(!mtl || !mci)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] ExecuteMaterialBuildPipeline failed for '%s': mtl=%p mci=%p\n",
            mtl_name.c_str(),
            mtl,
            mci);
        return false;
    }

    if(!BuildLegacyShaderModules(this,
                                 mtl_name,
                                 sci_map,
                                 mtl->shader_maps))
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] ExecuteMaterialBuildPipeline failed for '%s': BuildLegacyShaderModules returned false\n",
            mtl_name.c_str());
        return false;
    }

    CreateShaderStageList(mtl->shader_stage_list,mtl->shader_maps);

    const VIAArray *vertex_input_array = TryGetVertexInputArray(mci, mtl_name);
    mtl->vertex_input = vertex_input_array ? GetVertexInput(*vertex_input_array) : nullptr;

    const auto &mdi = mci->GetDescriptorInfo();
    if(mdi.GetCount() > 0)
        mtl->desc_manager = new MaterialDescriptorManager(mtl_name, mdi.Get());
    else
        mtl->desc_manager = nullptr;

    ApplyMaterialFinalizePlan(mtl, mtl_name, *mci);

    return true;
}

ShaderMaterialProgram *ShaderMaterialProgramManager::CreateMaterial(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci)
{
    HGL_CAPTURE_SCOPE();

    if(!mci)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterial(name,mci) failed for '%s': mci is null\n",
            mtl_name.c_str());
        return(nullptr);
    }

    MaterialCreatePrecheckResult precheck_result;
    const MaterialCreatePrecheckDecision precheck_decision = RunMaterialCreatePrecheck(
        mci,
        mtl_name,
        [](const AnsiString &) -> ShaderMaterialProgram * { return nullptr; },
        precheck_result);

    if(precheck_decision == MaterialCreatePrecheckDecision::UseCached)
        return precheck_result.cached_material;

    if(precheck_decision != MaterialCreatePrecheckDecision::Proceed)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterial(name,mci) failed for '%s': precheck decision=%d\n",
            mtl_name.c_str(),
            static_cast<int>(precheck_decision));
        return nullptr;
    }

    const ShaderStageMap &sci_map = *precheck_result.shader_map;

    AutoDelete<ShaderMaterialProgram> mtl=new ShaderMaterialProgram(mtl_name,mci);
    if(!ExecuteMaterialBuildPipeline(mtl,
                                     mtl_name,
                                     mci,
                                     sci_map))
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterial(name,mci) failed for '%s': ExecuteMaterialBuildPipeline returned false\n",
            mtl_name.c_str());
        return nullptr;
    }

    Add(mtl);
    acquire_material_created.fetch_add(1);

    // ShaderMaterialProgram is a C++ object managed by ShaderMaterialProgramManager, not a Vulkan object
    // No need to track with ObjectTracker
    return mtl.Finish();
}

ShaderMaterialProgram *ShaderMaterialProgramManager::CreateMaterial(const mtl::MaterialPreset mtl_id,mtl::Material2DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
        return(nullptr);

    cfg->preset_name=mtl::GetMaterialPresetName(mtl_id);

    // [Step 3.5 T1] RouteKey is the single entry; ApplyCreateConfigToVariantKey
    // remains the cfg-derived overlay until T3 unifies them.
    mtl::MaterialVariantKey key = mtl::RouteKey(mtl_id);
    mtl::ApplyCreateConfigToVariantKey(key, cfg);
    return CreateMaterial(key, cfg);
}

// file-static: moved from MaterialAssetLoader.h (was inline)
static ShaderMaterialProgram *CreateMaterialFromRecord(
    ShaderMaterialProgramManager *mm,
    const mtl::MaterialRecipe &rec)
{
    if (!mm) return nullptr;

    using namespace mtl;

    // ── Billboard2DFixed / Billboard2DDynamic ────────────────────────────────
    if (rec.preset == MaterialPreset::Billboard2DFixed ||
        rec.preset == MaterialPreset::Billboard2DDynamic)
    {
        BillboardMaterialCreateConfig cfg(rec.prim);
        cfg.local_to_world  = rec.l2w;
        cfg.fixed_size      = rec.billboard.fixed_size;
        cfg.pixel_size      = { rec.billboard.pixel_w, rec.billboard.pixel_h };
        cfg.blend_mode      = rec.billboard.blend_mode;
        cfg.base_color_channel = rec.billboard.base_color_channel;
        cfg.front_face      = rec.billboard.front_face_ccw
                              ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                              : VK_FRONT_FACE_CLOCKWISE;
        if (!rec.billboard.texture_id.empty())
            cfg.texture_id = rec.billboard.texture_id;
        if (rec.pos_format.Check())
            cfg.position_format = rec.pos_format;
        for (const auto &tc : rec.textures)
            if (tc.source_mode == TextureSourceMode::Array)
            { cfg.use_texture_array = true; break; }

        ForceVertexFragmentStages(cfg);

        std::fprintf(stderr, "[CreateMaterialFromRecord] Billboard preset=%d  use_texture_array=%d  blend=%d\n",
            (int)rec.preset, (int)cfg.use_texture_array, (int)cfg.blend_mode);

        return mm->ResolveOrCreateProgram(rec.preset, &cfg);
    }
    // ── 2D ──────────────────────────────────────────────────────────────────
    else if (rec.dim == MaterialRecipe::Dim::D2)
    {
        Material2DCreateConfig cfg(
            rec.prim,
            rec.coord_2d,
            rec.l2w ? IncludeL2W::With : IncludeL2W::Without);
        if (rec.pos_format.Check())
            cfg.position_format = rec.pos_format;
        for (const auto &tc : rec.textures)
            if (tc.source_mode != TextureSourceMode::None)
                cfg.SetTextureSourceModeOverride(tc.slot, tc.source_mode);

        ForceVertexFragmentStages(cfg);

        // Keep 2D recipe and key paths consistent with 3D: route through
        // ResolveRecipePrimaryKey so attribute/position provider overrides are
        // preserved for vertex pulling paths.
        cfg.preset_name = mtl::GetMaterialPresetName(rec.preset);
        mtl::MaterialVariantKey vk = mtl::ResolveRecipePrimaryKey(rec).variant;
        return mm->ResolveOrCreateProgram(vk, &cfg);
    }
    // ── 3D ─────────────────────────────────────────────────────────────────
    else
    {
        const mtl::MaterialFeatureMask feature_mask = hgl::graph::ResolveRecipeIntentFeatureMask(rec);
        const auto feature_validation = mtl::ValidateFeatureMask(feature_mask);

        if (rec.intent_features != 0 && !feature_validation.well_formed)
        {
            const std::string warning = mtl::BuildMalformedIntentFeatureWarningMessage(feature_mask,
                                                                                       rec.preset,
                                                                                       feature_validation);
            std::fprintf(stderr, "%s\n", warning.c_str());
        }

        const bool include_camera = mtl::HasFeature(feature_mask, mtl::MaterialFeature::NeedsCamera);
        const bool include_sky = mtl::HasFeature(feature_mask, mtl::MaterialFeature::NeedsSky);

        Material3DCreateConfig cfg(
            rec.prim,
            include_camera ? IncludeCamera::With : IncludeCamera::Without,
            rec.l2w    ? IncludeL2W::With    : IncludeL2W::Without,
            include_sky ? IncludeSky::With : IncludeSky::Without);
        cfg.sky_ambient_model = rec.sky_ambient;

        cfg.lighting_model = mtl::ResolveLightingModelFromFeatures(feature_mask, mtl::LightingModel::Lambert);

        if (!mtl::HasFeature(feature_mask, mtl::MaterialFeature::EnableLighting))
            cfg.lighting_model = mtl::LightingModel::Lambert;

        if (rec.pos_format.Check())
            cfg.position_format = rec.pos_format;
        for (const auto &tc : rec.textures)
            if (tc.source_mode != TextureSourceMode::None)
                cfg.SetTextureSourceModeOverride(tc.slot, tc.source_mode);

        ForceVertexFragmentStages(cfg);

        // Use the recipe-derived variant key so that attribute_providers and
        // position_provider (vertex pulling) are propagated through to
        // Standard_Adapter → AddVertexStreamSSBOs.  RouteKey(preset) drops them.
        cfg.preset_name = mtl::GetMaterialPresetName(rec.preset);
        mtl::MaterialVariantKey vk = mtl::ResolveRecipePrimaryKey(rec).variant;
        return mm->ResolveOrCreateProgram(vk, &cfg);
    }
}

ShaderMaterialProgram *ShaderMaterialProgramManager::GetOrCreateProgramByKey(
    const mtl::MaterialKey &key,
    const mtl::MaterialRecipe &recipe)
{
    // Fast path: key already in cache (populated by ResolveOrCreateProgram on first call)
    auto it = material_by_key.find(key);
    if (it != material_by_key.end())
    {
        by_key_hits.fetch_add(1, std::memory_order_relaxed);
        return it->second;
    }

    // Miss — fall back to recipe-based creation.  CreateMaterialFromRecord calls
    // ResolveOrCreateProgram which populates material_by_key as a side-effect (Step 3).
    ShaderMaterialProgram *prog = CreateMaterialFromRecord(this, recipe);

    if (prog)
    {
        // Step 5 key-path coherence: recipe callers use a full MaterialKey
        // (schema/def/version included), while ResolveOrCreateProgram currently
        // caches by a variant-derived key.  Alias the request key to the same
        // program so subsequent GetOrCreateProgramByKey calls hit fast-path.
        auto it2 = material_by_key.find(key);
        if (it2 == material_by_key.end())
        {
            material_by_key[key] = prog;
        }
        else if (it2->second != prog)
        {
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] WARN GetOrCreateProgramByKey: "
                "key.Hash=0x%llx maps to another program (existing=%p new=%p).\n",
                static_cast<unsigned long long>(key.Hash()),
                static_cast<void *>(it2->second),
                static_cast<void *>(prog));
        }
    }

    return prog;
}


ShaderMaterialProgram *ShaderMaterialProgramManager::ResolveOrCreateProgram(const mtl::MaterialPreset mtl_id, mtl::Material2DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderMaterialProgram *mtl = ResolveCreateFailureWithFallback(CreateMaterial(mtl_id, cfg));

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

ShaderMaterialProgram *ShaderMaterialProgramManager::ResolveOrCreateProgram(const mtl::MaterialPreset mtl_id, mtl::Material3DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderMaterialProgram *mtl = ResolveCreateFailureWithFallback(CreateMaterial(mtl_id, cfg));

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

ShaderMaterialProgram *ShaderMaterialProgramManager::ResolveOrCreateProgram(const mtl::MaterialVariantKey &key, mtl::Material2DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderMaterialProgram *mtl = ResolveCreateFailureWithFallback(CreateMaterial(key, cfg));

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

ShaderMaterialProgram *ShaderMaterialProgramManager::ResolveOrCreateProgram(const mtl::MaterialVariantKey &key, mtl::Material3DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderMaterialProgram *mtl = ResolveCreateFailureWithFallback(CreateMaterial(key, cfg));

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

void ShaderMaterialProgramManager::ResetShaderGenProfiler()
{
    // Legacy-only mode keeps ShaderGen debug APIs as no-op for compatibility.
}

ShaderGenProfilerSnapshot ShaderMaterialProgramManager::GetShaderGenProfilerSnapshot() const
{
    return {};
}

bool ShaderMaterialProgramManager::GetShaderGenLastValidationReport(ShaderGenValidationReport &out_report, std::string *out_material_name) const
{
    out_report = {};
    if (out_material_name)
        out_material_name->clear();
    return false;
}

std::vector<ShaderGenValidationReportRecord> ShaderMaterialProgramManager::GetShaderGenRecentValidationReports(const uint32_t max_count) const
{
    (void)max_count;
    return {};
}

std::map<std::string, uint32_t> ShaderMaterialProgramManager::GetShaderGenRecentValidationCategoryHistogram(const uint32_t max_count) const
{
    (void)max_count;
    return {};
}

ShaderMaterialProgram *ShaderMaterialProgramManager::CreateMaterial(const mtl::MaterialPreset mtl_id,mtl::Material3DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterial(preset=Standard/3D) failed: cfg is null (preset=%s)\n",
            mtl::GetMaterialPresetName(mtl_id));
        return(nullptr);
    }

    cfg->preset_name=mtl::GetMaterialPresetName(mtl_id);

    // [Step 3.5 T1] RouteKey is the single entry (see VertexInputFormat_plan.md).
    mtl::MaterialVariantKey key = mtl::RouteKey(mtl_id);
    mtl::ApplyCreateConfigToVariantKey(key, cfg);
    return CreateMaterial(key, cfg);
}

ShaderMaterialProgram *ShaderMaterialProgramManager::CreateMaterial(const mtl::MaterialVariantKey &key,mtl::Material2DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
        return(nullptr);

    char key_hash[32] = {};
    std::snprintf(key_hash, sizeof(key_hash), "%llu", static_cast<unsigned long long>(key.Hash()));
    AnsiString mtl_debug_name = cfg->preset_name
        ? AnsiString(cfg->preset_name) + "#" + key_hash
        : AnsiString("variant#") + key_hash;

    const mtl::MaterialKey mkey = MakeMaterialKeyFromVariantKey(key, cfg);

    // Fast path: material_by_key
    {
        auto it = material_by_key.find(mkey);
        if (it != material_by_key.end())
        {
            by_key_hits.fetch_add(1);

            if (it->second && it->second->GetPrimitiveType() != cfg->prim)
            {
                std::fprintf(stderr,
                    "[ShaderMaterialProgramManager] Material cache primitive mismatch on hit: key_hash=%llu requested_prim=%u cached_prim=%u material=%s\n",
                    static_cast<unsigned long long>(mkey.Hash()),
                    static_cast<unsigned>(cfg->prim),
                    static_cast<unsigned>(it->second->GetPrimitiveType()),
                    it->second->GetName().c_str());
            }

            return it->second;
        }
    }

    const auto *profile=GetPhysicalDeviceProfile();

    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo(profile,key,cfg);

    if(!mci)
        return(nullptr);

    ShaderMaterialProgram *mat = this->CreateMaterial(mtl_debug_name,mci);
    if (mat)
    {
        uint8_t flags = 0;
        for (uint8_t s = 0; s < uint8_t(mtl::SamplerSlot::RANGE_SIZE); ++s)
            if (key.GetTextureSourceMode(mtl::SamplerSlot(s)) == mtl::TextureSourceMode::Array)
                flags |= (1u << s);
        mat->SetTextureArraySlotFlags(flags);
        // Dual-write to key map
        mat->SetMaterialKey(mkey);
        material_by_key[mkey] = mat;
    }
    return mat;
}

ShaderMaterialProgram *ShaderMaterialProgramManager::CreateMaterial(const mtl::MaterialVariantKey &key,mtl::Material3DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
    {
        std::fprintf(stderr, "[ShaderMaterialProgramManager] CreateMaterial(key/3D) failed: cfg is null\n");
        return(nullptr);
    }

    // For Billboard materials, the variant registry key is shared between sampler2D and
    // sampler2DArray variants (differentiated inside the factory via cfg->use_texture_array).
    // Use a separate cache_key that encodes the array flag to avoid collisions in material_by_key.
    mtl::MaterialVariantKey cache_key = key;
    if (const auto *billboard_cfg = AsBillboard(cfg))
    {
        if (billboard_cfg->use_texture_array)
        {
            cache_key.SetTextureSourceMode(mtl::SamplerSlot::BaseColor, mtl::TextureSourceMode::Array);
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] Billboard domain: use_texture_array=true cache_key_hash=%llu\n",
                static_cast<unsigned long long>(cache_key.Hash()));
        }
    }

    // Compute debug name using cache_key (not key) so that billboard array/non-array variants
    // get distinct names — prevents VirtualLibraryCache from returning the wrong shader module
    // when both variants share the same preset key.
    char key_hash[32] = {};
    std::snprintf(key_hash, sizeof(key_hash), "%llu", static_cast<unsigned long long>(cache_key.Hash()));
    AnsiString mtl_debug_name = cfg->preset_name
        ? AnsiString(cfg->preset_name) + "#" + key_hash
        : AnsiString("variant#") + key_hash;

    const mtl::MaterialKey mkey = MakeMaterialKeyFromVariantKey(cache_key, cfg);

    // Fast path: material_by_key
    {
        auto it = material_by_key.find(mkey);
        if (it != material_by_key.end())
        {
            by_key_hits.fetch_add(1);

            if (it->second && it->second->GetPrimitiveType() != cfg->prim)
            {
                std::fprintf(stderr,
                    "[ShaderMaterialProgramManager] Material cache primitive mismatch on hit: key_hash=%llu requested_prim=%u cached_prim=%u material=%s\n",
                    static_cast<unsigned long long>(mkey.Hash()),
                    static_cast<unsigned>(cfg->prim),
                    static_cast<unsigned>(it->second->GetPrimitiveType()),
                    it->second->GetName().c_str());
            }

            return it->second;
        }
    }

    const auto *profile=GetPhysicalDeviceProfile();
    if(!profile)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterial(key/3D) warning: physical device profile is null (key_hash=%llu)\n",
            static_cast<unsigned long long>(key.Hash()));
    }

    // Pass original key (not cache_key) to CreateMaterialCreateInfo — the registry lookup
    // must use the clean key without the array bit injected for caching purposes.
    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo(profile,key,cfg);

    if(!mci)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterial(key/3D) failed: CreateMaterialCreateInfo returned null (key_hash=%llu surface=%u geom=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X cfg_hash=%s)\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits,
            cfg->ToHashStdString().c_str());
        return(nullptr);
    }

    ShaderMaterialProgram *mat = this->CreateMaterial(mtl_debug_name,mci);
    if (mat)
    {
        uint8_t flags = 0;
        for (uint8_t s = 0; s < uint8_t(mtl::SamplerSlot::RANGE_SIZE); ++s)
            if (cache_key.GetTextureSourceMode(mtl::SamplerSlot(s)) == mtl::TextureSourceMode::Array)
                flags |= (1u << s);
        mat->SetTextureArraySlotFlags(flags);
        // Dual-write to key map using cache_key-derived mkey for correct differentiation
        mat->SetMaterialKey(mkey);
        material_by_key[mkey] = mat;
    }
    return mat;
}

void ShaderMaterialProgramManager::DumpKeyMapDiagnostics() const
{
    std::fprintf(stderr,
        "[ShaderMaterialProgramManager] KeyMap: by_key=%zu hits=%llu\n",
        material_by_key.size(),
        static_cast<unsigned long long>(by_key_hits.load()));
}

MaterialBindingInstance *ShaderMaterialProgramManager::AcquireMaterialInstance(const MaterialInstanceSpec &spec, MaterialInstanceSpecKey *out_key)
{
    acquire_mi_requests.fetch_add(1);

    if(!spec.IsValid())
        return nullptr;

    ShaderMaterialProgram *mtl = spec.material;
    if(!mtl)
        return nullptr;

    MaterialBindingInstance *mi = CreateMaterialInstance(mtl,
                                                         spec.domain,
                                                         spec.instance_data,
                                                         spec.instance_data_size);

    if(!mi)
        return nullptr;

    acquire_mi_created.fetch_add(1);

    mi->SetRenderPreset(spec.preset);

    if(out_key)
    {
        out_key->material = spec.material;
        out_key->preset = spec.preset;
        out_key->domain = spec.domain;
    }

    return mi;
}

bool ShaderMaterialProgramManager::UpdateInstanceData(MaterialBindingInstance *mi, const void *data, const uint32 data_size)
{
    if(!mi || !data || data_size == 0)
        return false;

    mi->WriteMIData(data, data_size);
    return true;
}

// ============================================================================
// ResourceDomain — Phase 1
// ============================================================================

DomainResourceBinding *ShaderMaterialProgramManager::CreateDomainMaterialBinding(ResourceDomain *domain, ShaderMaterialProgram *mtl)
{
    if (!domain || !mtl)
        return nullptr;

    if (domain->GetShaderDataSchema() != mtl->GetShaderDataSchema())
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateDomainMaterialBinding: schema mismatch "
            "domain=%u mtl=%u\n",
            static_cast<unsigned>(domain->GetShaderDataSchema()),
            static_cast<unsigned>(mtl->GetShaderDataSchema()));
        return nullptr;
    }

    VulkanDevice *device = GetDevice();
    if (!device)
        return nullptr;

    MaterialParameters *mp_per_material = nullptr;
    if (mtl->hasSet(DescriptorSetType::PerMaterial))
        mp_per_material = CreateMaterialMP(mtl->GetName(), mtl->desc_manager,
                                           mtl->pipeline_layout_data, DescriptorSetType::PerMaterial);

    MaterialParameters *mp_per_object = nullptr;
    if (mtl->hasSet(DescriptorSetType::PerObject))
        mp_per_object = CreateMaterialMP(mtl->GetName(), mtl->desc_manager,
                                         mtl->pipeline_layout_data, DescriptorSetType::PerObject);

    DomainResourceBinding *binding = new DomainResourceBinding(domain, mtl, mp_per_material, mp_per_object);

    // Phase 3: register binding for lifecycle tracking
    domain_bindings_map[domain].push_back(binding);

    return binding;
}

// Phase 3 -- domain lifecycle management

void ShaderMaterialProgramManager::ReleaseDomainMaterialBinding(DomainResourceBinding *binding)
{
    if (!binding)
        return;

    ResourceDomain *d = binding->GetDomain();
    auto it = domain_bindings_map.find(d);
    if (it != domain_bindings_map.end())
    {
        auto &vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), binding), vec.end());
        if (vec.empty())
            domain_bindings_map.erase(it);
    }

    delete binding;
}

DomainResourceBinding *ShaderMaterialProgramManager::FindDomainMaterialBinding(ResourceDomain *domain, ShaderMaterialProgram *mtl) const
{
    if (!domain || !mtl)
        return nullptr;

    auto it = domain_bindings_map.find(domain);
    if (it == domain_bindings_map.end())
        return nullptr;

    const auto &vec = it->second;
    for (DomainResourceBinding *binding : vec)
    {
        if (binding && binding->GetShaderMaterialProgram() == mtl)
            return binding;
    }

    return nullptr;
}

MaterialBindingInstance *ShaderMaterialProgramManager::CreateMaterialInstance(ShaderMaterialProgram *mtl, ResourceDomain *domain)
{
    if (!domain || !mtl)
        return nullptr;

    if (domain->GetShaderDataSchema() != mtl->GetShaderDataSchema())
        return nullptr;

    int mi_id = domain->AllocMISlot();
    MaterialBindingInstance *mi = new MaterialBindingInstance(mtl, domain, mi_id);
    mi->InitMITLayout(mtl->GetTextureArraySlotFlags());
    Add(mi);
    return mi;
}

MaterialBindingInstance *ShaderMaterialProgramManager::CreateMaterialInstance(ShaderMaterialProgram *mtl, ResourceDomain *domain, const void *data, const uint32 data_size)
{
    if(!domain || !mtl) return nullptr;

    if (domain->GetShaderDataSchema() != mtl->GetShaderDataSchema())
        return nullptr;

    int mi_id = domain->AllocMISlot();
    MaterialBindingInstance *mi = new MaterialBindingInstance(mtl, domain, mi_id);
    mi->InitMITLayout(mtl->GetTextureArraySlotFlags());
    Add(mi);

    if(data && data_size > 0)
        mi->WriteMIData(data, data_size);

    return mi;
}

}//namespace hgl::graph
