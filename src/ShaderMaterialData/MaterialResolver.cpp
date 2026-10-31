#include<hgl/mtl/MaterialResolver.h>
#include<hgl/graph/ShaderBufferSource.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/ScenePassProfileResolver.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/common/DebugChannel.h>
#include <hgl/log/Log.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>

namespace hgl::graph::mtl{

namespace
{
    bool AddMaterialProviderRoot(
        PassProfileRequest &request,
        const ShaderModuleSlotRole role,
        const char *module_name) noexcept
    {
        if (!module_name || !module_name[0])
            return true;
        return request.AddModuleRoot(role, AnsiString(module_name));
    }
}

bool AppendMaterialPassProfileRoots(
    const ResolvedMaterial &definition,
    PassProfileRequest &request) noexcept
{
    bool appended = true;
    switch (request.profile_id)
    {
    case PassProfileID::ForwardLitShadowedAO:
    case PassProfileID::ForwardLitShadowedIdentityAO:
    case PassProfileID::ForwardLitUnshadowedAO:
        appended = AddMaterialProviderRoot(
                       request, ShaderModuleSlotRole::MaterialSourceProvider,
                      definition.material_source_module_name)
            && AddMaterialProviderRoot(
                   request, ShaderModuleSlotRole::NTBProvider,
                   definition.ntb_module_name);
        break;
    case PassProfileID::ForwardUnlit:
    case PassProfileID::ShadowCasterOpaque:
    case PassProfileID::ShadowCasterMasked:
        appended = AddMaterialProviderRoot(
            request, ShaderModuleSlotRole::MaterialSourceProvider,
            definition.material_source_module_name);
        break;
    case PassProfileID::Sky:
        if (!definition.surface_module_name.IsEmpty())
        {
            for (uint32 index = 0; index < request.module_root_count; ++index)
            {
                PassProfileModuleRoot &root = request.module_roots[index];
                if (root.role == ShaderModuleSlotRole::SurfaceProvider)
                {
                    root.module_name = definition.surface_module_name;
                    root.include_path.Clear();
                    break;
                }
            }
        }
        break;
    default:
        break;
    }

    PassProfileValidationDiagnostic diagnostic{};
    return appended && ValidatePassProfileRequest(request, diagnostic);
}

bool SelectScenePassProfileRequest(
    const ResolvedMaterial &definition,
    const ShaderProgramPurpose purpose,
    const MaterialRecipe &recipe,
    PassProfileRequest &out_request,
    const char *ambient_module_name)
{
    const bool depth_purpose =
        purpose == ShaderProgramPurpose::DepthOnly
     || purpose == ShaderProgramPurpose::ShadowDepth;

    // 深度/阴影模板的 masked 判定必须与 BuildMaterialCoverageContract 的
    // `requires_alpha_evaluation` **逐位同义**：coverage mode 只要不是 None
    // （AlphaTest 或 Dither 或两者）就需要在片元里评估 alpha 后再决定是否丢弃。
    //
    // 这里曾经只看 alpha_test，于是「纯 dither」材质在深度/阴影 pass 上会被路由
    // 到 ShadowCasterOpaque，而 MaterialProgramAssembler 的模板一致性断言要求
    // ShadowCasterMasked ⇒ 构建直接被拒绝（dither 材质一旦进阴影就报错）。
    const ResolvedMaterialRenderState resolved_state =
        ResolveMaterialRenderState(definition, recipe);
    const bool masked = resolved_state.alpha_test || resolved_state.dither;
    const bool has_material_source =
        definition.material_source_module_name
     && definition.material_source_module_name[0];
    const bool has_ntb_provider =
        definition.ntb_module_name && definition.ntb_module_name[0];

    // 「无 material_source 且无 ntb」只有 `no_material_source` 领域（Sky）可以走
    // Sky 模板。其他领域落到这里等于「该领域还没有主着色器入口」——静默渲染成
    // 一片天空，比直接失败更难发现。
    if (!depth_purpose && !has_ntb_provider && !has_material_source)
    {
        const MaterialDomainContract *contract =
            GetMaterialDomainContract(definition.domain);
        if (!contract || !contract->no_material_source)
        {
            GLogError(
                "[MaterialResolver] 领域没有主着色器入口，拒绝回退到 Sky 模板: "
                "id=%s domain=%s",
                definition.domain_name.c_str(),
                contract ? contract->name : "(无契约)");
            return false;
        }
    }

    const PassProfileID profile_id = depth_purpose
        ? (masked ? PassProfileID::ShadowCasterMasked
                  : PassProfileID::ShadowCasterOpaque)
        : (has_ntb_provider ? PassProfileID::ForwardLitShadowedAO
           : (has_material_source ? PassProfileID::ForwardUnlit
                                  : PassProfileID::Sky));
    ScenePassProfile scene_profile = depth_purpose
        ? MakeShadowCasterProfile()
        : (has_ntb_provider ? MakeIdentityForwardLitProfile()
           : (has_material_source ? MakeForwardUnlitProfile()
                                  : MakeSkyProfile()));

    // 引擎/环境层选定的 ambient provider 模块覆盖 Lit profile 默认的
    // indirect_sky_ambient（程序化天空环境光）——如 IBL 环境光照。
    // 该选择属于**环境**（`EnvironmentSystem::SetAmbientProvider`），不属于材质：
    // 它与 `ambient_env` 参数块（IBL 句柄）必须由同一个所有者配对。
    // include 路径不来自配置：清空后由模板校验按注册表来源规范化
    // （PassProfile.cpp ResolveModuleRoot），与 surface_module_name 同规则。
    if (!depth_purpose
     && ambient_module_name
     && ambient_module_name[0])
    {
        bool ambient_applied = false;
        for (uint32 index = 0; index < scene_profile.module_count; ++index)
        {
            if (scene_profile.roles[index]
                != ShaderModuleSlotRole::AmbientLightProvider)
                continue;
            scene_profile.module_names[index] = ambient_module_name;
            scene_profile.include_paths[index].Clear();
            ambient_applied = true;
            break;
        }
        if (!ambient_applied)
            GLogWarning(
                "[MaterialResolver] ambient provider ignored: id=%s template=%s "
                "has no ambient_light_provider slot",
                definition.domain_name.c_str(),
                GetPassProfileName(profile_id));
    }

    // [SkyRoute 诊断] 材质 → 渲染模板路由决策
    {
        AnsiString slots;
        for (uint32 index = 0; index < scene_profile.module_count; ++index)
        {
            if (index)
                slots += ",";
            slots += AnsiString(GetShaderModuleSlotRoleName(
                          scene_profile.roles[index]))
                   + "=" + scene_profile.module_names[index];
        }
        if (ULRE_DEBUG_ON(Route))
            GLogInfo("[SkyRoute] id=%s depth=%d masked=%d has_mtl_src=%d has_ntb=%d -> template=%s slots(%s)",
                     definition.domain_name.c_str(),
                     int(depth_purpose), int(masked),
                     int(has_material_source), int(has_ntb_provider),
                     GetPassProfileName(profile_id),
                     slots.c_str());
    }

    PassProfileValidationDiagnostic diagnostic{};
    if (scene_profile.module_count > 0
     && ResolveScenePassProfileRequest(
            profile_id, ShaderStage::Fragment, scene_profile,
            out_request, diagnostic))
        return true;

    GLogError(
        "[MaterialResolver] Scene render route selection failed: id=%s error=%s",
        definition.domain_name.c_str(),
        GetPassProfileValidationErrorName(diagnostic.error));
    return false;
}

VertexShaderNodeConfig ResolveMaterialVertexNodeConfig(
    const ResolvedMaterial &definition,
    const MaterialBuildRequest &request) noexcept
{
    // 1. request 显式覆盖
    if (request.has_vertex_node_config_override)
        return request.vertex_node_config_override;

    // 2. recipe 显式设置
    if (!IsDefault3DNodeConfig(request.recipe.vertex_node_config))
        return request.recipe.vertex_node_config;

    // 3. definition 非默认
    if (!IsDefault3DNodeConfig(definition.vertex_node_config))
        return definition.vertex_node_config;

    // 4. fallback
    return request.recipe.vertex_node_config;
}

uint64 HashMaterialProgramBuildContext(
    const PrimitiveType primitive_type,
    const GeometryVertexFormat *geometry_vertex_format,
    const contract::PhysicalDeviceProfileLite *profile,
    const mtl::ShaderProgramPurpose purpose,
    const char *ambient_module_name) noexcept
{
    hgl::hash::FNV1aHasher64 h;

    h << primitive_type
      << (geometry_vertex_format
            ? geometry_vertex_format->GetVertexInputHash() : 0)
      // 统一用编译目标超集哈希（设备能力 + 解析后的目标版本）——
      // 此前与 compiler_hash 双轨并存（L4 N5），两处口径不一致
      << contract::GetShaderCompilerProfileHash(profile)
      << static_cast<uint32>(purpose);

    // ambient provider 是前向着色的 program 身份维度（不同模块 ⇒ 不同 fragment 源码）。
    // depth/shadow 不消费该槽 ⇒ 不折入，避免阴影变体无谓分裂；
    // 空指针与空串归一化为同一取值，且**不追加任何字节**，保证引入本维度前
    // 已缓存的阴影 program 身份逐位不变。
    const bool depth_purpose =
        purpose == mtl::ShaderProgramPurpose::DepthOnly
     || purpose == mtl::ShaderProgramPurpose::ShadowDepth;
    if (!depth_purpose && ambient_module_name && ambient_module_name[0])
        h << ambient_module_name;

    return h;
}

MaterialVertexVaryingConfig ResolveMaterialVertexVaryingConfig(
    const ResolvedMaterial &definition,
    const mtl::ShaderProgramPurpose purpose,
    const mtl::MaterialCoverageContract &coverage) noexcept
{
    MaterialVertexVaryingConfig varying =
        definition.vertex_varying;
    const bool depth_purpose =
        purpose == ShaderProgramPurpose::DepthOnly
     || purpose == ShaderProgramPurpose::ShadowDepth;
    if (!depth_purpose)
        return varying;

    // depth/shadow 不带颜色输出：varying 只服务于 alpha 求值。
    //
    // 裁剪**不再按语义逐项手写**：coverage.required_semantics 是唯一判据，
    // 由 varying ↔ 语义定义表统一裁剪（MaterialStageInterface.h）。手写白名单
    // 曾在枚举新增语义时静默漏项 —— StyleID / WorldPosition / WorldNormal /
    // FragDirection 永远拿不到，而 coverage 仍在要求它们，于是 shadow program
    // 既不满足自己的 coverage 契约，也不报错（FS 侧 si 字段恒为零值）。
    // use_transform_id_attr 是顶点属性选择，不在裁剪范围内。
    const mtl::InterStageSemanticMask required_mask =
        coverage.requires_alpha_evaluation
            ? coverage.required_semantics
            : mtl::InterStageSemanticMask(0);
    mtl::RestrictMaterialVertexVaryingToSemanticMask(
        varying, required_mask);
    return varying;
}

bool BuildFallbackMaterialRecipe(MaterialRecipe &out_recipe,
                                 const MaterialErrorKind kind)
{
    if (kind == MaterialErrorKind::None)
        return false;

    const FallbackMaterialRule &rule = GetFallbackMaterialRule(kind);

    // 回退配方**从零合成**：不继承调用方残留字段 —— 调用点常是“构建失败后改走回退”，
    // 残留会让回退材质带上半份旧状态（旧纹理绑定 / 旧 SSBO 行）。
    out_recipe = MaterialRecipe{};
    out_recipe.recipe_name = rule.marker_name;
    out_recipe.domain      = rule.domain;
    out_recipe.fallback_error_kind = kind;
    for (uint32_t i = 0; i < 4; ++i)
        out_recipe.fallback_marker_color[i] = rule.marker_color[i];

    // 与普通配方**同一条**规范化路径（定义默认值 + 解析后的渲染状态）。
    NormalizeRecipe(out_recipe);
    return true;
}

bool ResolveMaterialFromRecipe(const MaterialRecipe &recipe,
                               ResolvedMaterial &out_definition) noexcept
{
    const MaterialDomainContract *contract = GetMaterialDomainContract(recipe.domain);
    if (!contract)
        return false;

    // 从零合成，绝不继承调用方残留字段（同 BuildFallbackMaterialRecipe 的理由）。
    out_definition = ResolvedMaterial{};

    out_definition.domain = recipe.domain;

    // 身份用领域名：领域内所有配方共享同一顶点 ABI 与片元模板，领域名才是稳定键。
    // 注意：`domain_name` 会被 `ComputeMaterialProgramManifestKey` 与
    // `MaterialTextureReferencePool::MakePoolKey` 当作哈希输入，**不是**纯日志字段；
    // 改动取值等于改动烘焙缓存身份，必须同步重烘并跑发行形态一致性门禁。
    if (const char *resolved_domain_name = GetMaterialDomainName(recipe.domain))
        out_definition.domain_name = resolved_domain_name;

    out_definition.vertex_provider_policy =
        contract->require_allow_derived
            ? MaterialVertexProviderPolicy::AllowDerived
            : MaterialVertexProviderPolicy::GeometryOnly;

    GetMaterialDomainCanonicalVertexNodeConfig(
        recipe.domain, out_definition.vertex_node_config);

    out_definition.material_private_data = contract->material_data_type;

    GetMaterialDomainVertexSemanticRequirements(
        recipe.domain, out_definition.vertex_semantic_requirements);

    const InterStageSemanticMask default_mask =
        GetMaterialDomainDefaultVaryingMask(recipe.domain);
    if (default_mask != 0)
    {
        BuildMaterialVertexVaryingFromSemanticMask(
            default_mask, out_definition.vertex_varying);

        // 顶点色来源由契约钉死：调色板一族查表出色、并需要 TransformID 属性。
        // 这里**不按领域名判断**，否则新增同类领域就得回来改这段合成代码。
        if (contract->emit_vertex_color_from_palette)
        {
            out_definition.vertex_varying.emit_vertex_color = false;
            out_definition.vertex_varying.emit_vertex_color_from_palette = true;
        }
        if (contract->use_transform_id_attr)
            out_definition.vertex_varying.use_transform_id_attr = true;
    }

    out_definition.vertex_normal_mode = contract->vertex_normal_mode;
    out_definition.mesh_shader_mode = contract->mesh_shader_mode;

    if (!contract->no_material_source)
        out_definition.material_source_module_name =
            GetMaterialDomainReferenceMaterialSource(recipe.domain);
    out_definition.ntb_module_name =
        GetMaterialDomainReferenceNtb(recipe.domain);

    // 表面模块：同一领域可以有多种表面形态（Sky 的 sky_minimal / sky_cube），
    // 这是配方唯一能自选的模块槽。**拷贝**配方字符串（不自持借用），合成结果
    // 因此可以安全地活得比 `recipe` 更久，也不受其移动/改写影响。
    if (!recipe.surface_module_name.empty())
        out_definition.surface_module_name =
            recipe.surface_module_name.c_str();

    // 纹理槽：保留领域白名单全集，解析式天空不采样立方纹理，但立方天空仍要求绑定。
    for (int i = 0; i < contract->texture_slot_count; ++i)
    {
        const MaterialDomainTextureSlot &slot = contract->texture_slots[i];
        const bool procedural_sky =
            recipe.domain == MaterialDomain::Sky
            && (recipe.surface_module_name.empty()
                || recipe.surface_module_name == "surface_sky_minimal");
        const bool required = slot.binding_required
            && !(procedural_sky && std::strcmp(slot.name, "sky_cube") == 0);
        out_definition.texture_declarations.push_back(
            {slot.name, slot.sampler, required, slot.channels});
    }

    // 采样器预设 / 编译宏：契约给惯例预设，配方可**整表替换**（不是追加 ——
    // 「同一领域不同形态」的差别正在于替换，例如 Text2D 的 SDF 形态给 Linear +
    // TEXT_SDF_ENABLED，位图形态必须换成 Nearest + 空表）。
    int entry_count = 0;
    const char *const *presets =
        GetMaterialDomainSamplerPresets(recipe.domain, entry_count);

    if (recipe.has_sampler_names)
        out_definition.sampler_names = recipe.sampler_names;
    else
        for (int i = 0; i < entry_count; ++i)
            out_definition.sampler_names.emplace_back(presets[i]);

    entry_count = 0;
    const char *const *defines =
        GetMaterialDomainCompileDefines(recipe.domain, entry_count);

    if (recipe.has_compile_defines)
        out_definition.compile_defines = recipe.compile_defines;
    else
        for (int i = 0; i < entry_count; ++i)
            out_definition.compile_defines.emplace_back(defines[i]);

    // 渲染状态默认值来自契约；配方的覆盖项由 NormalizeRecipe 落定，这里不预合并。
    out_definition.default_render_state = contract->default_render_state;
    return true;
}

void NormalizeRecipe(MaterialRecipe &recipe)
{
    const MaterialDomainContract *contract = GetMaterialDomainContract(recipe.domain);

    if (!contract)
    {
        // 无契约只有两种成因，都必须出声并把配方改写为保底领域配方：
        //   · `Custom`（未声明领域）= 无材质来源       ⇒ MissingMaterialSource
        //   · 其它非法枚举值       = 领域不可解析       ⇒ UnknownMaterialDomain
        // 改写后配方与普通配方走同一条合成路径（`BuildFallbackMaterialRecipe` 内部
        // 会再次调用本函数，届时领域已有契约，递归一层即终止）。
        const MaterialErrorKind error_kind =
            (recipe.domain == MaterialDomain::Custom)
                ? MaterialErrorKind::MissingMaterialSource
                : MaterialErrorKind::UnknownMaterialDomain;

        // 报错口径（2026-10-14 用户）：材质未知/写错 **直接报错**，不静默。本行是这条
        // 路径的唯一出声点 —— ECS 侧把它当"构建成功"处理（只计数，不再报一次），
        // 否则同一事件会出两条日志。
        {
            const FallbackMaterialRule &fallback_rule = GetFallbackMaterialRule(error_kind);
            const char *reason = (error_kind == MaterialErrorKind::MissingMaterialSource)
                                     ? "未声明材质领域（domain 缺失）"
                                     : "材质领域无契约（domain 非法）";

            GLogError(u8"[MaterialFallback] %s: \"%s\" => 回退错误材质 %s"
                      u8"（领域；错误种类 %s，颜色=(%.2f,%.2f,%.2f,%.2f)）",
                      reason, recipe.recipe_name.c_str(),
                      GetMaterialDomainName(fallback_rule.domain),
                      fallback_rule.marker_name,
                      fallback_rule.marker_color[0], fallback_rule.marker_color[1],
                      fallback_rule.marker_color[2], fallback_rule.marker_color[3]);
        }

        if (!BuildFallbackMaterialRecipe(recipe, error_kind))
            return;

        contract = GetMaterialDomainContract(recipe.domain);
        if (!contract)
            return;     // 保底领域本身没契约 = 引擎内部错误，规则表 static_assert 已拦
    }

    const ResolvedMaterialRenderState resolved =
        ResolveMaterialRenderState(contract->default_render_state, recipe);

    // Write resolved values back to render_state_overrides as authoritative.
    recipe.render_state_overrides.has_alpha_test = true;
    recipe.render_state_overrides.alpha_test = resolved.alpha_test;
    recipe.render_state_overrides.has_alpha_cutoff = true;
    recipe.render_state_overrides.alpha_cutoff = resolved.alpha_cutoff;
    recipe.render_state_overrides.has_dither = true;
    recipe.render_state_overrides.dither = resolved.dither;
    recipe.render_state_overrides.has_pipeline_config = true;
    recipe.render_state_overrides.pipeline_config = resolved.pipeline_config;
    // 回写的是**合完的整份**配置，局部掩码到此失效（否则会被再合一次）。
    recipe.render_state_overrides.pipeline_config_mask = 0;
}

}//namespace hgl::graph::mtl
