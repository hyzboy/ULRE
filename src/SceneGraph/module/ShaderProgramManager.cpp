#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/vk/pipeline/VKPipelineLayoutData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/common/ShaderStageDef.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramCreatePrecheckAdapter.h>
#include<hgl/graph/module/ShaderProgramFinalizeFlowAdapter.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/ShaderBuildContext.h>
#include<hgl/mtl/MaterialShaderCompiler.h>
#include<hgl/mtl/ShaderArtifactStore.h>
#include<hgl/mtl/ShaderCreateInfo.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/MaterialDefinitionFile.h>
#include<hgl/mtl/ShaderCacheRoot.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/utf.h>
#include<cstdint>
#include<vector>
#include<cstdlib>
#include<cwchar>

namespace hgl::graph{

namespace
{
    bool ResolveMaterialDefinitionForRequest(const mtl::MaterialDefinitionBuildRequest &request,
                                             mtl::MaterialDefinition &out_bmi)
    {
        const std::string &mtl_def_id = request.recipe.mtl_def_id;

        if (mtl::TryGetMaterialDefinitionByID(mtl_def_id, out_bmi))
            return true;

        // 显式诊断：材质 ID 配置错误不应静默消失（fallback 只是运行时安全网）
        GLogError(u8"[ShaderProgramManager] material definition not found: %s — falling back to %s",
                  mtl_def_id.c_str(), mtl::GetFallbackMaterialDefinitionID());

        return mtl::TryGetMaterialDefinitionByID(
            mtl::GetFallbackMaterialDefinitionID(), out_bmi);
    }

    void CreateShaderStageList(ValueArray<VkPipelineShaderStageCreateInfo> &shader_stage_list,ShaderModuleMap *shader_maps)
    {
        const ShaderModule *sm;

        const int shader_count=shader_maps->GetCount();
        shader_stage_list.Resize(shader_count);

        VkPipelineShaderStageCreateInfo *p=shader_stage_list.GetData();

        for(auto [stage, module] : *shader_maps)
        {
            sm = module;
            mem_copy(p,sm->GetCreateInfo(),1);

            ++p;
        }
    }

    bool BuildShaderModulesFromCreateInfoMap(ShaderProgramManager *manager,
                                             const AnsiString &mtl_name,
                                             const mtl::ShaderCreateInfoMap &sci_map,
                                             const mtl::ShaderBuildContext *build_spec,
                                             ShaderModuleMap *shader_maps)
    {
        if (!manager || !shader_maps)
            return false;

        if (sci_map.GetCount() < 2)
            return false;

        for (auto [stage, sci_ptr] : sci_map)
        {
            (void)stage;

            if (!sci_ptr)
                return false;

            const ShaderModule *module = nullptr;
            mtl::ShaderArtifactStore *artifact_store =
                build_spec ? build_spec->GetArtifactStore() : nullptr;
            const mtl::ShaderStageKey *cache_key = nullptr;
            if (build_spec && build_spec->HasProgramLink())
            {
                const auto &link = build_spec->GetProgramLink();
                cache_key = stage == ShaderStage::Mesh
                    ? &link.mesh_stage : stage == ShaderStage::Fragment
                        ? &link.fragment_stage : nullptr;
            }

            if (artifact_store && cache_key)
            {
                hgl::ValueArray<hgl::uint8> cached_spv;
                if (artifact_store->LoadStageSPV(*cache_key, cached_spv))
                {
                    module = manager->CreateShaderModuleFromSPV(
                        *cache_key,
                        reinterpret_cast<const uint32_t *>(cached_spv.GetData()),
                        static_cast<size_t>(cached_spv.GetCount()));
                }
            }

            if (!module)
            {
                if (artifact_store
                 && artifact_store->GetCacheMode() == mtl::ShaderCacheMode::ReadOnly)
                {
                    GLogError(u8"[ShaderProgramManager] readonly shader artifact missing");
                    return false;
                }
                module = manager->CreateShaderModule(mtl_name, sci_ptr);
            }
            if (!module)
                return false;

            shader_maps->Add(module);
        }

        return true;
    }

    std::vector<ShaderDescriptor> CollectDescriptorsFromBuildContext(const mtl::ShaderBuildContext *ctx)
    {
        std::vector<ShaderDescriptor> descriptors;
        if (!ctx)
            return descriptors;

        const auto &allocator = ctx->GetDescriptorAllocator();
        if (allocator.GetCount() == 0)
            return descriptors;

        const auto &sds_array = allocator.Get();
        descriptors.reserve(allocator.GetCount());

        for (size_t i = 0; i < DESCRIPTOR_SET_TYPE_COUNT; i++)
        {
            std::vector<ShaderDescriptor *> values;
            sds_array[i].descriptor_map.GetValueArray(values);

            for (auto *sd : values)
            {
                if (sd)
                    descriptors.emplace_back(*sd);
            }
        }

        return descriptors;
    }

    // 进程级单例（与 GetMaterialDefinitionFileRegistry 同一模式）。
    // 模式由环境变量 ULRE_SHADER_CACHE_MODE 控制：
    //   readonly —— 只读（发布形态：配合 ShaderCooker 离线 cook 的产物分发，
    //              缓存 miss 即材质构建失败，用于验证 cook 覆盖完整性）；
    //   默认 BuildIfMissing（开发形态：命中免编译，未命中编译后回填）。
    // 缓存 key 含 GLSL 源码/资源契约/编译器 profile 哈希，任何输入变化自然
    // miss 并覆盖旧条目；损坏文件因 header/payload 哈希校验失败按 miss 处理。
    // ReadOnly 的 miss 硬失败（FinalizeShaderBuildContext 返回 false）是设计
    // 行为——发布前必须用 ShaderCooker 完成全变体 cook。
    mtl::ShaderArtifactStore *GetRuntimeShaderArtifactStore()
    {
        static const OSString cache_root = mtl::GetShaderCacheRootPath();
        static const bool readonly_mode = []()
        {
            const wchar_t *mode = _wgetenv(L"ULRE_SHADER_CACHE_MODE");
            return mode
                && (wcscmp(mode, L"readonly") == 0
                 || wcscmp(mode, L"ro") == 0);
        }();
        static mtl::ShaderArtifactStore store(
            cache_root,
            readonly_mode
                ? mtl::ShaderCacheMode::ReadOnly
                : mtl::ShaderCacheMode::BuildIfMissing);
        static bool logged = false;
        if (!logged)
        {
            logged = true;
            if (cache_root.IsEmpty())
            {
                GLogWarning(u8"[ShaderProgramManager] shader SPV cache disabled: cannot resolve cache root");
            }
            else
            {
                const U8String root_utf8 = ToU8String(cache_root);
                GLogInfo(u8"[ShaderProgramManager] shader SPV cache root=%s (mode=%s)",
                         root_utf8.c_str(),
                         readonly_mode ? "ReadOnly" : "BuildIfMissing");
            }
        }
        return &store;
    }

}//namespace

GRAPH_MODULE_CONSTRUCT(ShaderProgramManager)
{
    (void)mtl::GetMaterialDefinitionFileRegistry();
}

const ShaderModule *ShaderProgramManager::CreateShaderModule(const AnsiString &sm_name,const mtl::ShaderCreateInfo *sci)
{
    VulkanDevice *device = GetDevice();
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);

    ShaderModule *sm = shader_module_cache.FindName(sci->GetShaderStage(), sm_name);
    if(sm)
        return sm;

    sm=device->CreateShaderModule((VkShaderStageFlagBits)sci->GetShaderStage(),sci->GetSPVData(),sci->GetSPVSize());

    if(!sm)
        return(nullptr);

    shader_module_cache.AddName(sci->GetShaderStage(), sm_name, sm);

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

const ShaderModule *ShaderProgramManager::CreateShaderModule(const mtl::ShaderStageKey &key,
                                                             const mtl::ShaderCreateInfo *sci)
{
    if (!sci || key.stage != sci->GetShaderStage())
        return nullptr;

    return CreateShaderModule(key.ToString(), sci);
}

const ShaderModule *ShaderProgramManager::CreateShaderModuleFromSPV(const AnsiString &sm_name,
                                                                const VkShaderStageFlagBits stage,
                                                                const uint32_t *spv_data,
                                                                const size_t spv_size)
{
    VulkanDevice *device = GetDevice();
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);
    if(!spv_data||spv_size==0)return(nullptr);

    ShaderModule *sm = shader_module_cache.FindName(static_cast<ShaderStage>(stage), sm_name);
    if(sm)
        return sm;

    sm=device->CreateShaderModule(stage,spv_data,spv_size);

    if(!sm)
        return(nullptr);

    shader_module_cache.AddName(static_cast<ShaderStage>(stage), sm_name, sm);

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

const ShaderModule *ShaderProgramManager::CreateShaderModuleFromSPV(const mtl::ShaderStageKey &key,
                                                                     const uint32_t *spv_data,
                                                                     const size_t spv_size)
{
    return CreateShaderModuleFromSPV(key.ToString(),
                                     static_cast<VkShaderStageFlagBits>(key.stage),
                                     spv_data,
                                     spv_size);
}

PipelineLayoutData *ShaderProgramManager::CreateMaterialPipelineLayoutData(const AnsiString &mtl_name, const MaterialDescriptorManager *desc_manager)
{
    VulkanDevice *device = GetDevice();
    if(!device) return nullptr;

    PipelineLayoutData *pld = device->CreatePipelineLayoutData(desc_manager, bindless_layout_, scene_layout_);

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

MaterialParameters *ShaderProgramManager::CreateMaterialMP(const AnsiString &mtl_name, const MaterialDescriptorManager *desc_manager, const PipelineLayoutData *pld, const DescriptorSetType &desc_set_type)
{
    VulkanDevice *device = GetDevice();
    if(!device) return nullptr;

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

void ShaderProgramManager::ApplyMaterialFinalizePlan(ShaderProgram *mtl, const AnsiString &mtl_name, const mtl::ShaderBuildContext &ctx)
{
    if(!mtl)
        return;

    ShaderProgramFinalizePlan finalize_plan;
    BuildShaderProgramFinalizePlan(mtl->desc_manager, ctx, finalize_plan);

    mtl->pipeline_layout_data = CreateMaterialPipelineLayoutData(mtl_name, mtl->desc_manager);

    for(const auto set_type : finalize_plan.mp_set_types)
    {
        // Scene（Set 0）已全局化（P1）：不再生成 per-material MP，
        // 由设备级 GlobalSceneUBOSet 一帧写/绑一次。
        if(scene_layout_ != VK_NULL_HANDLE && set_type == DescriptorSetType::Scene)
            continue;

        mtl->mp_array[(int)set_type] = CreateMaterialMP(mtl_name, mtl->desc_manager, mtl->pipeline_layout_data, set_type);
    }

}

ShaderProgram *ShaderProgramManager::TryGetCachedShaderProgram(
    const mtl::ShaderProgramKey &key)
{
    return shader_program_cache.Find(key);
}

bool ShaderProgramManager::ExecuteRuntimeMaterialBuildPipeline(ShaderProgram *mtl,
                                                          const AnsiString &mtl_name,
                                                          const mtl::ShaderBuildContext *ctx,
                                                          const mtl::ShaderCreateInfoMap &sci_map)
{
    if(!mtl || !ctx)
        return false;

    if(!BuildRuntimeShaderProgramState(mtl, mtl_name, ctx, sci_map))
        return false;

    if(!BuildRuntimeDescriptorState(mtl, mtl_name, ctx))
        return false;

    ApplyMaterialFinalizePlan(mtl, mtl_name, *ctx);

    return true;
}

bool ShaderProgramManager::BuildRuntimeShaderProgramState(ShaderProgram *mtl,
                                                     const AnsiString &mtl_name,
                                                     const mtl::ShaderBuildContext *ctx,
                                                     const mtl::ShaderCreateInfoMap &sci_map)
{
    if(!mtl || !ctx)
        return false;

    if(!BuildShaderModulesFromCreateInfoMap(this,
                                            mtl_name,
                                            sci_map,
                                            ctx,
                                            mtl->shader_maps))
    {
        return false;
    }

    CreateShaderStageList(mtl->shader_stage_list,mtl->shader_maps);

    // mesh 化后顶点输入统一走 SSBO，无 VBO 顶点输入布局（VS 遗留 vertex_input 已删）

    return true;
}

bool ShaderProgramManager::BuildRuntimeDescriptorState(ShaderProgram *mtl,
                                                  const AnsiString &mtl_name,
                                                  const mtl::ShaderBuildContext *ctx)
{
    if(!mtl || !ctx)
        return false;

    std::vector<ShaderDescriptor> descriptors = CollectDescriptorsFromBuildContext(ctx);
    if(!descriptors.empty())
        mtl->desc_manager = new MaterialDescriptorManager(mtl_name, descriptors.data(), static_cast<uint>(descriptors.size()));
    else
        mtl->desc_manager = nullptr;

    return true;
}

ShaderProgram *ShaderProgramManager::AcquireShaderProgram(
    const mtl::ShaderProgramKey &program_key,
    const mtl::ShaderBuildContext *ctx)
{
    HGL_CAPTURE_SCOPE();

    if (!ctx
     || !ctx->HasProgramLink()
     || !(ctx->GetProgramLink().BuildKey() == program_key))
        return(nullptr);

    // B9：program 级缓存先查——此前只 Add 不查（TryGetCachedShaderProgram
    // 无调用者，缓存从不命中，同 key 每次全新构建）。命中返回共享实例
    // （调用方不释放——manager 拥有；同 key = 同构建输入——P1-7 后
    // build_context_hash 含 purpose/profile/vertex_format，材质差异走
    // batch 级 descriptor override/bindless 索引）。
    if (auto *cached = TryGetCachedShaderProgram(program_key))
        return cached;

    const AnsiString mtl_name = program_key.ToString();
    ShaderProgramCreatePrecheckResult precheck_result;
    const ShaderProgramCreatePrecheckDecision precheck_decision = RunShaderProgramCreatePrecheck(
        ctx,
        mtl_name,
        precheck_result);

    if(precheck_decision != ShaderProgramCreatePrecheckDecision::Proceed)
    {
        GLogError("[ShaderProgramManager] shader program precheck rejected: name=%s decision=%u",
                  mtl_name.c_str(),
                  static_cast<uint32>(precheck_decision));
        return nullptr;
    }

    const mtl::ShaderCreateInfoMap &sci_map = *precheck_result.shader_map;

    AutoDelete<ShaderProgram> mtl=new ShaderProgram(mtl_name,ctx);
    if(!ExecuteRuntimeMaterialBuildPipeline(mtl,
                                            mtl_name,
                                            ctx,
                                            sci_map))
        return nullptr;

    Add(mtl);

    shader_program_cache.Add(program_key, mtl);
    // ShaderProgram is a C++ object managed by ShaderProgramManager, not a Vulkan object
    // No need to track with ObjectTracker
    return mtl.Finish();
}

namespace
{
    // AcquireShaderProgram(request) 与 BuildShaderResourceSchema 的公共前半段：
    // 解析 definition → 创建构建上下文。失败返回 nullptr 并写日志。
    mtl::ShaderBuildContext *BuildContextFromRequest(
        const mtl::contract::PhysicalDeviceProfileLite *profile,
        const mtl::MaterialDefinitionBuildRequest &request,
        mtl::MaterialDefinition &out_definition)
    {
        if (!ResolveMaterialDefinitionForRequest(request, out_definition))
            return nullptr;

        AutoDelete<mtl::ShaderBuildContext> ctx =
            mtl::CreateMaterialFromDefinition(profile, out_definition, request);
        if (!ctx)
        {
            GLogError("[ShaderProgramManager] Material definition build failed: id=%s name=%s",
                      out_definition.definition_id.c_str(),
                      out_definition.definition_name.c_str());
            return nullptr;
        }
        return ctx.Finish();
    }
}//namespace

bool ShaderProgramManager::BuildShaderResourceSchema(const mtl::MaterialDefinitionBuildRequest &request,
                                                  mtl::ShaderResourceSchema &out_schema)
{
    mtl::MaterialDefinition definition{};
    AutoDelete<mtl::ShaderBuildContext> ctx =
        BuildContextFromRequest(GetPhysicalDeviceProfile(), request, definition);
    if (!ctx)
        return false;

    out_schema = ctx->GetShaderResourceSchema();
    return true;
}

ShaderProgram *ShaderProgramManager::AcquireShaderProgram(
    const mtl::MaterialDefinitionBuildRequest &request)
{
    // Ensure recipe defaults are filled in before lookup, in case the caller skipped normalization.
    // This call is idempotent; PrimitiveComponent-initiated paths will have already normalized.
    mtl::MaterialRecipe normalized_recipe = request.recipe;
    mtl::NormalizeRecipe(normalized_recipe);

    mtl::MaterialDefinitionBuildRequest normalized_request = request;
    normalized_request.recipe = normalized_recipe;
    normalized_request.defer_finalize = true;
    // 接通跨进程 SPV 磁盘缓存：FinalizeShaderBuildContext 先查缓存，未命中
    // 才走 glslang 编译并回填（BuildIfMissing）。FinalizeShaderBuildContext 在
    // store 非空时要求 program link + artifact metadata 齐备——生产路径的
    // BuildGenericMaterial 两者恒备（回归门已全变体验证）。
    normalized_request.shader_artifact_store = GetRuntimeShaderArtifactStore();

    mtl::MaterialDefinition definition{};
    AutoDelete<mtl::ShaderBuildContext> ctx =
        BuildContextFromRequest(GetPhysicalDeviceProfile(), normalized_request, definition);
    if (!ctx)
        return nullptr;

    if (!ctx->HasProgramLink())
    {
        GLogError(
            "[ShaderProgramManager] Material build produced incomplete program identity: id=%s",
            definition.definition_id.c_str());
        return nullptr;
    }
    const mtl::ShaderProgramKey program_key =
        ctx->GetProgramLink().BuildKey();
    if (ShaderProgram *cached = TryGetCachedShaderProgram(program_key))
        return cached;

    if (!mtl::FinalizeShaderBuildContext(ctx))
    {
        GLogError(
            "[ShaderProgramManager] Material build finalization failed: id=%s",
            definition.definition_id.c_str());
        return nullptr;
    }

    return this->AcquireShaderProgram(program_key, ctx);
}

}//namespace hgl::graph
