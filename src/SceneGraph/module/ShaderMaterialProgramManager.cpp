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
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/RecipeToKey.h>
#include<hgl/object/ObjectTracker.h>
#include<cstdio>
#include<cstdint>
#include<vector>
#include<algorithm>
#include<cassert>
#include<hgl/mtl/MaterialKeyToolchainVersion.h>

namespace hgl::graph{


namespace
{
    enum : uint32_t
    {
        MATERIAL_KEY_MISMATCH_DEF_ID       = 1u << 0,
        MATERIAL_KEY_MISMATCH_SCHEMA       = 1u << 1,
        MATERIAL_KEY_MISMATCH_GLSL_VERSION = 1u << 2,
        MATERIAL_KEY_MISMATCH_VK_VERSION   = 1u << 3,
        MATERIAL_KEY_MISMATCH_SPV_VERSION  = 1u << 4,
    };

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

    bool BuildShaderModulesFromStageMap(ShaderMaterialProgramManager *manager,
                                        const AnsiString &mtl_name,
                                        const ShaderStageMap &sci_map,
                                        ShaderModuleMap *shader_maps)
    {
        if (!manager || !shader_maps)
        {
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] BuildShaderModulesFromStageMap failed for '%s': manager=%p shader_maps=%p\n",
                mtl_name.c_str(),
                manager,
                shader_maps);
            return false;
        }

        if (sci_map.GetCount() < 2)
        {
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] BuildShaderModulesFromStageMap failed for '%s': shader count=%d (expected >= 2)\n",
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
                    "[ShaderMaterialProgramManager] BuildShaderModulesFromStageMap failed for '%s': shader create info is null\n",
                    mtl_name.c_str());
                return false;
            }

            const ShaderModule *module = manager->CreateShaderModule(mtl_name, sci_ptr);
            if (!module)
            {
                std::fprintf(stderr,
                    "[ShaderMaterialProgramManager] BuildShaderModulesFromStageMap failed for '%s': CreateShaderModule returned null for stage=%u\n",
                    mtl_name.c_str(),
                    static_cast<unsigned>(sci_ptr->GetShaderStage()));
                return false;
            }

            shader_maps->Add(module);
        }

        return true;
    }

    static mtl::MaterialKey BuildMaterialKeyFromVariantKey(const mtl::MaterialVariantKey &vk) noexcept
    {
        mtl::MaterialKey k{};
        k.variant = vk;
        k.pass    = vk.pass_hint;

        // Baseline runtime key for variant-driven cache lookup.
        k.def_id       = mtl::kInvalidStaticMaterialDefId;
        k.schema       = mtl::ShaderDataSchema::None;
        k.glsl_version = mtl::kMaterialKeyGLSLVersion;
        k.vk_version   = mtl::kMaterialKeyVulkanVersion;
        k.spv_version  = mtl::kMaterialKeySpvVersion;

        return k;
    }

    static void EnrichMaterialKeyWithCreateInfoAxes(mtl::MaterialKey &k,
                                                    const mtl::MaterialCreateInfo &mci) noexcept
    {
        // def_id is unresolved at this call site for now.
        k.schema = mci.GetMaterialInstance().schema;
    }

    static void MergeMaterialKeyAxesFromRequestIfDefault(mtl::MaterialKey &dst,
                                                          const mtl::MaterialKey &request,
                                                          uint32_t *out_mismatch_mask = nullptr) noexcept
    {
        uint32_t mismatch_mask = 0;

        if (dst.def_id == mtl::kInvalidStaticMaterialDefId)
        {
            if (request.def_id != mtl::kInvalidStaticMaterialDefId)
                dst.def_id = request.def_id;
        }
        else
        if (request.def_id != mtl::kInvalidStaticMaterialDefId && dst.def_id != request.def_id)
        {
            mismatch_mask |= MATERIAL_KEY_MISMATCH_DEF_ID;
        }

        if (dst.schema == mtl::ShaderDataSchema::None)
        {
            if (request.schema != mtl::ShaderDataSchema::None)
                dst.schema = request.schema;
        }
        else
        if (request.schema != mtl::ShaderDataSchema::None && dst.schema != request.schema)
        {
            mismatch_mask |= MATERIAL_KEY_MISMATCH_SCHEMA;
        }

        if (dst.glsl_version == 0)
        {
            if (request.glsl_version != 0)
                dst.glsl_version = request.glsl_version;
        }
        else
        if (request.glsl_version != 0 && dst.glsl_version != request.glsl_version)
        {
            mismatch_mask |= MATERIAL_KEY_MISMATCH_GLSL_VERSION;
        }

        if (dst.vk_version == 0)
        {
            if (request.vk_version != 0)
                dst.vk_version = request.vk_version;
        }
        else
        if (request.vk_version != 0 && dst.vk_version != request.vk_version)
        {
            mismatch_mask |= MATERIAL_KEY_MISMATCH_VK_VERSION;
        }

        if (dst.spv_version == 0)
        {
            if (request.spv_version != 0)
                dst.spv_version = request.spv_version;
        }
        else
        if (request.spv_version != 0 && dst.spv_version != request.spv_version)
        {
            mismatch_mask |= MATERIAL_KEY_MISMATCH_SPV_VERSION;
        }

        if (out_mismatch_mask)
            *out_mismatch_mask = mismatch_mask;
    }

    static void LogEffectiveFeatureMaskConsistency(const ShaderMaterialProgram *prog,
                                                   const mtl::MaterialKey &request_key,
                                                   const char *phase)
    {
        if(!prog)
            return;

        const uint64_t program_mask = prog->GetEffectiveFeatureMask();
        const uint64_t request_mask = request_key.variant.effective_feature_mask;

        if(program_mask != request_mask)
        {
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] effective_feature_mask drift (%s): program=0x%016llx request=0x%016llx material='%s'\n",
                phase ? phase : "unknown",
                static_cast<unsigned long long>(program_mask),
                static_cast<unsigned long long>(request_mask),
                prog->GetName().c_str());
        }
    }

}//namespace

void ShaderMaterialProgramManager::RecordMaterialKeyAxisMismatch(uint32_t mismatch_mask)
{
    if(mismatch_mask==0)
        return;

    key_axis_mismatch_total.fetch_add(1, std::memory_order_relaxed);

    if(mismatch_mask & MATERIAL_KEY_MISMATCH_DEF_ID)
        key_axis_mismatch_def_id.fetch_add(1, std::memory_order_relaxed);
    if(mismatch_mask & MATERIAL_KEY_MISMATCH_SCHEMA)
        key_axis_mismatch_schema.fetch_add(1, std::memory_order_relaxed);
    if(mismatch_mask & MATERIAL_KEY_MISMATCH_GLSL_VERSION)
        key_axis_mismatch_glsl.fetch_add(1, std::memory_order_relaxed);
    if(mismatch_mask & MATERIAL_KEY_MISMATCH_VK_VERSION)
        key_axis_mismatch_vk.fetch_add(1, std::memory_order_relaxed);
    if(mismatch_mask & MATERIAL_KEY_MISMATCH_SPV_VERSION)
        key_axis_mismatch_spv.fetch_add(1, std::memory_order_relaxed);
}

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

    if(!BuildShaderModulesFromStageMap(this,
                                       mtl_name,
                                       sci_map,
                                       mtl->shader_maps))
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] ExecuteMaterialBuildPipeline failed for '%s': BuildShaderModulesFromStageMap returned false\n",
            mtl_name.c_str());
        return false;
    }

    CreateShaderStageList(mtl->shader_stage_list,mtl->shader_maps);

    const ShaderCreateInfoVertex *vert = mci->GetVertexShader();
    mtl->vertex_input = vert ? GetVertexInput(vert->GetInput()) : nullptr;

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

    std::fprintf(stderr,
        "[ShaderMaterialProgramManager] CreateMaterialFromRecord: preset=%u dim=%u prim=%u l2w=%u pipeline=%u key_hash=0x%llx\n",
        static_cast<unsigned>(rec.preset),
        static_cast<unsigned>(rec.dim),
        static_cast<unsigned>(rec.prim),
        rec.l2w ? 1u : 0u,
        static_cast<unsigned>(rec.pipeline),
        static_cast<unsigned long long>(mtl::ResolveRecipePrimaryKey(rec).Hash()));

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
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterialFromRecord: 2D cfg.prim=%u preset=%u\n",
            static_cast<unsigned>(cfg.prim),
            static_cast<unsigned>(rec.preset));
        return mm->ResolveOrCreateProgram(rec.preset, &cfg);
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

        cfg.effective_feature_mask = feature_mask;

        if (rec.pos_format.Check())
            cfg.position_format = rec.pos_format;
        for (const auto &tc : rec.textures)
            if (tc.source_mode != TextureSourceMode::None)
                cfg.SetTextureSourceModeOverride(tc.slot, tc.source_mode);
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterialFromRecord: 3D cfg.prim=%u preset=%u include_camera=%u include_sky=%u\n",
            static_cast<unsigned>(cfg.prim),
            static_cast<unsigned>(rec.preset),
            include_camera ? 1u : 0u,
            include_sky ? 1u : 0u);
        return mm->ResolveOrCreateProgram(rec.preset, &cfg);
    }
}

ShaderMaterialProgram *ShaderMaterialProgramManager::GetOrCreateProgramByKey(
    const mtl::MaterialKey &key,
    const mtl::MaterialRecipe &recipe)
{
    std::fprintf(stderr,
        "[ShaderMaterialProgramManager] GetOrCreateProgramByKey: key_hash=0x%llx recipe_prim=%u preset=%u pipeline=%u\n",
        static_cast<unsigned long long>(key.Hash()),
        static_cast<unsigned>(recipe.prim),
        static_cast<unsigned>(recipe.preset),
        static_cast<unsigned>(recipe.pipeline));

    // Fast path: key already in cache (populated by ResolveOrCreateProgram on first call)
    auto it = material_by_key.find(key);
    if (it != material_by_key.end())
    {
        by_key_hits.fetch_add(1, std::memory_order_relaxed);

        if (it->second && it->second->HasMaterialKey())
        {
            mtl::MaterialKey merged_key = it->second->GetMaterialKey();
            const mtl::MaterialKey old_key = merged_key;
            uint32_t mismatch_mask = 0;
            MergeMaterialKeyAxesFromRequestIfDefault(merged_key, key, &mismatch_mask);

            if (mismatch_mask != 0)
            {
                RecordMaterialKeyAxisMismatch(mismatch_mask);

                if (mismatch_mask & MATERIAL_KEY_MISMATCH_DEF_ID)
                    std::fprintf(stderr,
                        "[ShaderMaterialProgramManager] MaterialKey axis mismatch: def_id cached=%u request=%u\n",
                        static_cast<unsigned>(old_key.def_id),
                        static_cast<unsigned>(key.def_id));

                if (mismatch_mask & MATERIAL_KEY_MISMATCH_SCHEMA)
                    std::fprintf(stderr,
                        "[ShaderMaterialProgramManager] MaterialKey axis mismatch: schema cached=%u request=%u\n",
                        static_cast<unsigned>(old_key.schema),
                        static_cast<unsigned>(key.schema));

                if (mismatch_mask & MATERIAL_KEY_MISMATCH_GLSL_VERSION)
                    std::fprintf(stderr,
                        "[ShaderMaterialProgramManager] MaterialKey axis mismatch: glsl_version cached=%u request=%u\n",
                        static_cast<unsigned>(old_key.glsl_version),
                        static_cast<unsigned>(key.glsl_version));

                if (mismatch_mask & MATERIAL_KEY_MISMATCH_VK_VERSION)
                    std::fprintf(stderr,
                        "[ShaderMaterialProgramManager] MaterialKey axis mismatch: vk_version cached=%u request=%u\n",
                        static_cast<unsigned>(old_key.vk_version),
                        static_cast<unsigned>(key.vk_version));

                if (mismatch_mask & MATERIAL_KEY_MISMATCH_SPV_VERSION)
                    std::fprintf(stderr,
                        "[ShaderMaterialProgramManager] MaterialKey axis mismatch: spv_version cached=%u request=%u\n",
                        static_cast<unsigned>(old_key.spv_version),
                        static_cast<unsigned>(key.spv_version));
            }

            if (!(merged_key == old_key))
            {
                it->second->SetMaterialKey(merged_key);
                material_by_key[merged_key] = it->second;
            }
        }

        if (it->second && it->second->GetEffectiveFeatureMask() == 0
         && key.variant.effective_feature_mask != 0)
        {
            it->second->effective_feature_mask = key.variant.effective_feature_mask;
        }

        LogEffectiveFeatureMaskConsistency(it->second, key, "cache_hit");
#ifndef NDEBUG
        assert(it->second != nullptr);

        // Alias-aware check: lookup key may be enriched (schema/version axes)
        // while program keeps a baseline alias for variant-key fast paths.
        auto it_verify = material_by_key.find(key);
        assert(it_verify != material_by_key.end() && it_verify->second == it->second);
#endif
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] GetOrCreateProgramByKey: cache_hit material=%p name='%s' material_prim=%u\n",
            it->second,
            it->second->GetName().c_str(),
            static_cast<unsigned>(it->second->GetPrimitiveType()));
        return it->second;
    }

    // Miss — fall back to recipe-based creation.  CreateMaterialFromRecord calls
    // ResolveOrCreateProgram which populates material_by_key as a side-effect (Step 3).
    ShaderMaterialProgram *prog = CreateMaterialFromRecord(this, recipe);

    if (prog)
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] GetOrCreateProgramByKey: created material=%p name='%s' material_prim=%u\n",
            prog,
            prog->GetName().c_str(),
            static_cast<unsigned>(prog->GetPrimitiveType()));

        // If program key still carries unresolved default axes, merge richer axes
        // from the incoming request key (def/schema/toolchain versions).
        if (prog->HasMaterialKey())
        {
            mtl::MaterialKey merged_key = prog->GetMaterialKey();
            uint32_t mismatch_mask = 0;
            MergeMaterialKeyAxesFromRequestIfDefault(merged_key, key, &mismatch_mask);
            if (mismatch_mask != 0)
                RecordMaterialKeyAxisMismatch(mismatch_mask);

            prog->SetMaterialKey(merged_key);
            material_by_key[merged_key] = prog;
        }

        if (prog->GetEffectiveFeatureMask() == 0 && key.variant.effective_feature_mask != 0)
            prog->effective_feature_mask = key.variant.effective_feature_mask;

        LogEffectiveFeatureMaskConsistency(prog, key, "cache_miss_created");

        // Alias requested key to the created program so callers that already hold
        // enriched MaterialKey(def/schema/version axes) can hit cache on next lookup.
        material_by_key[key] = prog;

        // Also alias current program baseline key to keep variant-key based lookup
        // and key-transparent lookup converging to the same cache entry.
        if (prog->HasMaterialKey())
        {
            mtl::MaterialKey baseline_alias = prog->GetMaterialKey();
            baseline_alias.def_id       = mtl::kInvalidStaticMaterialDefId;
            baseline_alias.schema       = mtl::ShaderDataSchema::None;
            baseline_alias.glsl_version = mtl::kMaterialKeyGLSLVersion;
            baseline_alias.vk_version   = mtl::kMaterialKeyVulkanVersion;
            baseline_alias.spv_version  = mtl::kMaterialKeySpvVersion;
            material_by_key[baseline_alias] = prog;
#ifndef NDEBUG
            auto it_base_alias = material_by_key.find(baseline_alias);
            assert(it_base_alias != material_by_key.end() && it_base_alias->second == prog);
#endif
        }
#ifndef NDEBUG
        auto it_alias = material_by_key.find(key);
        assert(it_alias != material_by_key.end() && it_alias->second == prog);
#endif
    }

#ifndef NDEBUG
    if (prog)
    {
        auto it2 = material_by_key.find(key);
        if (it2 == material_by_key.end() || it2->second != prog)
        {
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] WARN GetOrCreateProgramByKey: "
                "key.Hash=0x%llx not in material_by_key after creation — "
                "MaterialKey derivation may be inconsistent.\n",
                static_cast<unsigned long long>(key.Hash()));
        }
    }
#endif

    return prog;
}


ShaderMaterialProgram *ShaderMaterialProgramManager::ResolveOrCreateProgram(const mtl::MaterialPreset mtl_id, mtl::Material2DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderMaterialProgram *mtl = CreateMaterial(mtl_id, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

ShaderMaterialProgram *ShaderMaterialProgramManager::ResolveOrCreateProgram(const mtl::MaterialPreset mtl_id, mtl::Material3DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderMaterialProgram *mtl = CreateMaterial(mtl_id, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

ShaderMaterialProgram *ShaderMaterialProgramManager::ResolveOrCreateProgram(const mtl::MaterialVariantKey &key, mtl::Material2DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderMaterialProgram *mtl = CreateMaterial(key, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

ShaderMaterialProgram *ShaderMaterialProgramManager::ResolveOrCreateProgram(const mtl::MaterialVariantKey &key, mtl::Material3DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderMaterialProgram *mtl = CreateMaterial(key, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

void ShaderMaterialProgramManager::ResetShaderGenProfiler()
{
    // Current pipeline path keeps ShaderGen debug APIs as no-op for compatibility.
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

    const mtl::MaterialKey baseline_key = BuildMaterialKeyFromVariantKey(key);

    // Fast path: material_by_key
    {
        auto it = material_by_key.find(baseline_key);
        if (it != material_by_key.end())
        {
            by_key_hits.fetch_add(1);
#ifndef NDEBUG
            assert(it->second != nullptr);
            auto it_verify = material_by_key.find(baseline_key);
            assert(it_verify != material_by_key.end() && it_verify->second == it->second);
#endif
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
        mtl::MaterialKey enriched_key = baseline_key;
        EnrichMaterialKeyWithCreateInfoAxes(enriched_key, *mci);

        uint8_t flags = 0;
        for (uint8_t s = 0; s < uint8_t(mtl::SamplerSlot::RANGE_SIZE); ++s)
            if (key.GetTextureSourceMode(mtl::SamplerSlot(s)) == mtl::TextureSourceMode::Array)
                flags |= (1u << s);
        mat->SetTextureArraySlotFlags(flags);
        mat->effective_feature_mask = key.effective_feature_mask;

        // Primary key uses enriched axes; keep baseline alias for fast variant-key path.
        mat->SetMaterialKey(enriched_key);
        material_by_key[baseline_key] = mat;
        material_by_key[enriched_key] = mat;
#ifndef NDEBUG
        auto it_verify_base = material_by_key.find(baseline_key);
        assert(it_verify_base != material_by_key.end() && it_verify_base->second == mat);
        auto it_verify_enriched = material_by_key.find(enriched_key);
        assert(it_verify_enriched != material_by_key.end() && it_verify_enriched->second == mat);
        assert(mat->HasMaterialKey() && mat->GetMaterialKey() == enriched_key);
#endif
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

    const mtl::MaterialKey baseline_key = BuildMaterialKeyFromVariantKey(cache_key);

    // Fast path: material_by_key
    {
        auto it = material_by_key.find(baseline_key);
        if (it != material_by_key.end())
        {
            by_key_hits.fetch_add(1);
#ifndef NDEBUG
            assert(it->second != nullptr);
            auto it_verify = material_by_key.find(baseline_key);
            assert(it_verify != material_by_key.end() && it_verify->second == it->second);
#endif
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
        mtl::MaterialKey enriched_key = baseline_key;
        EnrichMaterialKeyWithCreateInfoAxes(enriched_key, *mci);

        uint8_t flags = 0;
        for (uint8_t s = 0; s < uint8_t(mtl::SamplerSlot::RANGE_SIZE); ++s)
            if (cache_key.GetTextureSourceMode(mtl::SamplerSlot(s)) == mtl::TextureSourceMode::Array)
                flags |= (1u << s);
        mat->SetTextureArraySlotFlags(flags);
        mat->effective_feature_mask = cache_key.effective_feature_mask;

        // Primary key uses enriched axes; keep baseline alias for fast variant-key path.
        mat->SetMaterialKey(enriched_key);
        material_by_key[baseline_key] = mat;
        material_by_key[enriched_key] = mat;
#ifndef NDEBUG
        auto it_verify_base = material_by_key.find(baseline_key);
        assert(it_verify_base != material_by_key.end() && it_verify_base->second == mat);
        auto it_verify_enriched = material_by_key.find(enriched_key);
        assert(it_verify_enriched != material_by_key.end() && it_verify_enriched->second == mat);
        assert(mat->HasMaterialKey() && mat->GetMaterialKey() == enriched_key);
#endif
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

    const VIL *resolved_vil = spec.vil_cfg ? mtl->CreateVIL(spec.vil_cfg)
                                           : (spec.vil ? spec.vil : mtl->GetDefaultVIL());

    MaterialBindingInstance *mi = CreateMaterialInstance(mtl,
                                                         spec.domain,
                                                         resolved_vil,
                                                         spec.instance_data,
                                                         spec.instance_data_size);

    if(!mi)
        return nullptr;

    acquire_mi_created.fetch_add(1);

    mi->SetRenderPreset(spec.preset);

    if(out_key)
    {
        out_key->material = spec.material;
        out_key->vil = resolved_vil;
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

MaterialBindingInstance *ShaderMaterialProgramManager::CreateMaterialInstance(ShaderMaterialProgram *mtl, ResourceDomain *domain, const VIL *vil)
{
    if (!domain || !mtl)
        return nullptr;

    if (domain->GetShaderDataSchema() != mtl->GetShaderDataSchema())
        return nullptr;

    const VIL *use_vil = vil ? vil : mtl->GetDefaultVIL();
    (void)use_vil;   // VIL no longer stored in MI; kept for potential local validation
    int mi_id = domain->AllocMISlot();
    MaterialBindingInstance *mi = new MaterialBindingInstance(mtl, domain, mi_id);
    mi->InitMITLayout(mtl->GetTextureArraySlotFlags());
    Add(mi);
    return mi;
}

MaterialBindingInstance *ShaderMaterialProgramManager::CreateMaterialInstance(ShaderMaterialProgram *mtl, ResourceDomain *domain, const VILConfig *vil_cfg)
{
    if(!domain || !mtl) return nullptr;

    if (domain->GetShaderDataSchema() != mtl->GetShaderDataSchema())
        return nullptr;

    // VIL no longer stored in MI; compute only for potential validation
    int mi_id = domain->AllocMISlot();
    MaterialBindingInstance *mi = new MaterialBindingInstance(mtl, domain, mi_id);
    mi->InitMITLayout(mtl->GetTextureArraySlotFlags());
    Add(mi);
    return mi;
}

MaterialBindingInstance *ShaderMaterialProgramManager::CreateMaterialInstance(ShaderMaterialProgram *mtl, ResourceDomain *domain, const VIL *vil, const void *data, const uint32 data_size)
{
    if(!domain || !mtl) return nullptr;

    if (domain->GetShaderDataSchema() != mtl->GetShaderDataSchema())
        return nullptr;

    const VIL *use_vil = vil ? vil : mtl->GetDefaultVIL();
    (void)use_vil;
    int mi_id = domain->AllocMISlot();
    MaterialBindingInstance *mi = new MaterialBindingInstance(mtl, domain, mi_id);
    mi->InitMITLayout(mtl->GetTextureArraySlotFlags());
    Add(mi);

    if(data && data_size > 0)
        mi->WriteMIData(data, data_size);

    return mi;
}

MaterialBindingInstance *ShaderMaterialProgramManager::CreateMaterialInstance(ShaderMaterialProgram *mtl, ResourceDomain *domain, const VILConfig *vil_cfg, const void *data, const uint32 data_size)
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
