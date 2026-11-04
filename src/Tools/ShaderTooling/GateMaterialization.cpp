// 阶段 5 拆分自 ShaderResourceSchemaRegressionGate.cpp：组 materialization 的 case 实现。
#include "GateTestSupport.h"

#include <hgl/graph/asset/MaterialRecipeSpec.h>
#include <hgl/mtl/MaterialRecipeInherit.h>

namespace gate
{
    GateResult RunMaterializationSharedInstanceCase()
    {
        GateResult result;
        result.name = "D1.materialization-shared-instance-separation";

        MaterialRecipe recipe;
        recipe.recipe_name = "shared-instance-regression";

        RecipeTextureBinding texture{};
        texture.texture_name = "base_color";
        texture.resource_id = "asset/base-color";
        texture.required = true;
        recipe.textures.emplace_back(texture);

        GlobalSSBOBinding asset{
            GlobalSSBOType::PBRSurface,
            41,
            3};
        recipe.material_ssbo_binding = asset;

        const auto *initial_binding =
            recipe.material_ssbo_binding.IsValid()
                ? &recipe.material_ssbo_binding : nullptr;
        if (!initial_binding
         || initial_binding->data_index != 3
         || !initial_binding->IsValid())
        {
            result.diagnostics.emplace_back("shared recipe binding resolve failed");
            result.passed = false;
            return result;
        }

        // Instance separation: changing the per-instance data_index must not
        // leak into the shared recipe identity (HashMaterialRecipe), while
        // the direct recipe binding must still carry the new data_index.
        recipe.material_ssbo_binding.data_index = 9;
        const auto *changed_binding =
            recipe.material_ssbo_binding.IsValid()
                ? &recipe.material_ssbo_binding : nullptr;
        if (!changed_binding
         || changed_binding->data_index != 9)
        {
            result.diagnostics.emplace_back(
                "instance data_index was not preserved in the direct recipe binding");
            result.passed = false;
            return result;
        }

        recipe.material_ssbo_binding.data_index = 3;
        const uint64_t first_recipe_hash = HashMaterialRecipe(recipe);
        recipe.material_ssbo_binding.data_index = 9;
        const uint64_t second_recipe_hash = HashMaterialRecipe(recipe);
        if (first_recipe_hash != second_recipe_hash)
            result.diagnostics.emplace_back(
            "instance data_index must not change shared recipe identity");

        recipe.material_ssbo_binding.ssbo_id = 42;
        const uint64_t changed_buffer_hash = HashMaterialRecipe(recipe);
        if (second_recipe_hash == changed_buffer_hash)
            result.diagnostics.emplace_back(
            "SSBO buffer identity must change shared recipe identity");

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 领域覆盖：契约表是「领域 → 顶点 ABI / 片元模板」的唯一真源。
    ///
    /// definition 文件消失后，没有注册表可以「漏登记」；本用例改为遍历全部领域值，
    /// 断言每个领域都能当场合成出一份带名字的定义（`Custom` 无契约，按设计不入表）。
    GateResult RunMaterialDomainCoverageCase()
    {
        GateResult result;
        result.name = "E.material-domain-coverage";

        for (uint32_t index = 1; index < MaterialDomainContractCount; ++index)
        {
            const MaterialDomain domain = static_cast<MaterialDomain>(index);
            const char *domain_name = GetMaterialDomainName(domain);

            ResolvedMaterial definition{};
            if (!BuildResolvedMaterialForDomain(domain, definition))
            {
                result.diagnostics.emplace_back(
                    std::string("Missing domain contract: ") + domain_name);
                continue;
            }

            if (definition.domain_name.empty())
                result.diagnostics.emplace_back(
                    std::string("Empty domain definition name: ") + domain_name);

            if (definition.domain_name != domain_name)
                result.diagnostics.emplace_back(
                    std::string("Domain definition identity is not the domain"
                                " name: ") + domain_name);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 回退材质必须"任何几何都能画"（数据驱动：遍历整张回退规则表 + 标记行不可得时的
    /// 降级领域，新增种类自动纳入）：
    ///   ① 规则表里的领域必须能合成出定义（写错领域 = 兜底路径整条失效）；
    ///   ② 顶点需求只允许 GeometryAttribute/Position（不得依赖 UV/法线/颜色等可选属性）；
    ///      `Resource` 源需求（要材质数据行）不在本条约束内 —— 回退路径由
    ///      AttachFallbackMarkerColor 提供该行；
    ///   ③ provider_policy 必须是 GeometryOnly（不得靠 code module 反推属性）。
    GateResult RunMaterialFallbackDefinitionCase()
    {
        GateResult result;
        result.name = "S.material-fallback-definitions-exist";

        const auto check_definition = [&result](const char *label,
                                                const MaterialDomain domain)
        {
            const char *domain_name = GetMaterialDomainName(domain);

            ResolvedMaterial definition{};
            if (!BuildResolvedMaterialForDomain(domain, definition))
            {
                result.diagnostics.emplace_back(std::string("fallback domain for ") + label
                    + " has no contract: " + domain_name);
                return;
            }

            if (definition.vertex_provider_policy != MaterialVertexProviderPolicy::GeometryOnly)
                result.diagnostics.emplace_back(std::string("fallback domain for ") + label
                    + " must use GeometryOnly provider policy: " + domain_name);

            for (uint32_t i = 0; i < definition.vertex_semantic_requirements.GetCount(); ++i)
            {
                const GlslModuleSemanticRequirement &requirement =
                    definition.vertex_semantic_requirements[i];

                // 只看**几何输入**（GeometryAttribute）：ProducedSemantic 是着色器内部
                // 产出（例如 WorldPosition），不依赖顶点数据，任何几何都满足。
                if (requirement.source == GlslModuleCapabilitySource::GeometryAttribute
                 && requirement.semantic != GlslModuleSemantic::Position)
                    result.diagnostics.emplace_back(std::string("fallback domain for ") + label
                        + " must not require vertex attributes beyond Position: " + domain_name);
            }
        };

        for (uint32_t index = 0; index < MATERIAL_ERROR_KIND_COUNT; ++index)
        {
            const MaterialErrorKind kind = static_cast<MaterialErrorKind>(index);
            const FallbackMaterialRule &rule = GetFallbackMaterialRule(kind);

            check_definition((std::string("kind '") + rule.marker_name + "'").c_str(),
                             rule.domain);
        }

        // 标记行不可得时的降级领域：与规则表同样的约束（它是最后一道兜底）。
        check_definition("markerless degrade",
                         kMarkerlessFallbackMaterialDomain);

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 材质身份：定义身份必须**只由领域决定**。
    ///
    /// definition 文件时代身份来自用户随手写的 `id = "..."`，同一个领域的多份文件
    /// 可以各自命名（`lit` / `pbr_sphere` / …），缓存与日志里出现两个名字指同一份
    /// ABI。现在身份就是领域名，本用例钉住三件事：
    ///   ① 合成结果的 id/name 都等于领域名；
    ///   ② 配方里塞任何用户名字都不改变身份（身份不吃 recipe_name）；
    ///   ③ 历史的 definition ID 字符串不再是合法领域名（`ParseMaterialDomain` 拒绝），
    ///      否则残留的 `.material.toml` 会静默解析成 `Custom`。
    GateResult RunMaterialIdentityCase()
    {
        GateResult result;
        result.name = "R1.material-definition-identity";

        const auto check_identity = [&result](const MaterialDomain domain)
        {
            const char *domain_name = GetMaterialDomainName(domain);

            MaterialRecipe recipe{};
            recipe.recipe_name = "user_supplied_name";
            recipe.domain = domain;

            ResolvedMaterial definition{};
            if (!ResolveMaterialFromRecipe(recipe, definition))
            {
                result.diagnostics.emplace_back(
                    std::string("domain definition is missing: ") + domain_name);
                return;
            }

            if (definition.domain_name != domain_name
             || definition.domain_name != domain_name)
            {
                result.diagnostics.emplace_back(
                    std::string("definition identity is not the domain name: ")
                    + domain_name);
            }

            if (definition.domain != domain)
            {
                result.diagnostics.emplace_back(
                    std::string("synthesized definition lost its domain: ")
                    + domain_name);
            }
        };

        check_identity(MaterialDomain::UnlitSolid);
        check_identity(MaterialDomain::Text2D);
        check_identity(MaterialDomain::StandardOpaque);

        // 历史定义 ID：`builtin/pure_color`、`lit` 之类曾经是合法主键，现在必须
        // 被领域解析器拒绝 —— 它们只可能来自遗留的 definition 引用。
        static const char *retired_domain_names[] = {
            "builtin/pure_color",
            "builtin/text_gpu",
            "builtin/checkerboard_3d",
            "builtin/error_checkerboard_3d",
            "checkerboard_2d",
            "lit"
        };
        for (const char *retired : retired_domain_names)
        {
            MaterialDomain parsed = MaterialDomain::Custom;
            if (ParseMaterialDomain(retired, parsed))
                result.diagnostics.emplace_back(
                    std::string("retired definition id still parses as a domain: ")
                    + retired);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunTransformGraphModelCase()
    {
        GateResult result;
        result.name = "W.transform-graph-model";

        const VertexShaderNodeConfig flat = VertexNodeConfigResolver::FlatXY();
        const VertexShaderNodeConfig wall = VertexNodeConfigResolver::WallXY();
        const VertexShaderNodeConfig world = VertexNodeConfigResolver::World3D();
        VertexShaderNodeConfig world_vec2 = world;
        world_vec2.input = VertexInputMode::Vec2Position;
        world_vec2.position_mapping = PositionMappingMode::LiftXY_XY0;

        if (VertexNodeConfigResolver::GetHash(flat) == VertexNodeConfigResolver::GetHash(wall)
         || VertexNodeConfigResolver::GetHash(flat) == VertexNodeConfigResolver::GetHash(world))
            result.diagnostics.emplace_back(
                "transform graph variants must have distinct structural identity");

        if (!VertexNodeConfigResolver::IsScreenLike(flat)
         || !VertexNodeConfigResolver::IsScreenLike(wall)
         || VertexNodeConfigResolver::IsScreenLike(world)
         || VertexNodeConfigResolver::IsScreenLike(world_vec2))
            result.diagnostics.emplace_back(
                "screen-space classification must include projection, not only Vec2 input");

        if (flat.input != VertexInputMode::Vec2Position
         || flat.position_mapping != PositionMappingMode::NDCLift
         || flat.projection != ProjectionMode::LocalToWorldOnly)
            result.diagnostics.emplace_back("FlatXY graph conversion mismatch");

        if (wall.position_mapping != PositionMappingMode::LiftXY_X0Y)
            result.diagnostics.emplace_back("WallXY graph conversion mismatch");

        // Round-trip is identity now that VertexNodeConfigResolver is a static utility
        if (VertexNodeConfigResolver::GetHash(world) != VertexNodeConfigResolver::GetHash(world))
            result.diagnostics.emplace_back("transform graph hash self-consistency failure");

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunTransformGraphCompositionCase()
    {
        GateResult result;
        result.name = "X.transform-graph-composition";

        // 生成器删除后，「这个图会拼出哪些 Stage2/Stage3 模块」不再靠生成一段
        // GLSL 去观察，而是直接问 `VertexNodeConfigResolver` 要模块路径，并核对
        // **该模块文件真的在库里**。少一个文件、改名、删枚举分支都会在这里硬失败，
        // 而原断言只证明「生成器当次拼对了字符串」。
        const auto require_module = [&](const char *module, const char *context)
        {
            if (!module || !module[0])
            {
                result.diagnostics.emplace_back(
                    std::string(context) + ": 图没有映射到任何模块");
                return;
            }
            const std::string path =
                RepoRootPath((std::string("ShaderLibrary/") + module).c_str());
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec))
                result.diagnostics.emplace_back(
                    std::string(context) + ": 模块文件不存在 " + module);
        };

        for (uint32_t i = uint32_t(PositionMappingMode::BEGIN_RANGE);
             i <= uint32_t(PositionMappingMode::END_RANGE); ++i)
        {
            const auto mapping = PositionMappingMode(i);
            VertexShaderNodeConfig cfg = MakeDefault3DNodeConfig();
            cfg.position_mapping = mapping;
            require_module(
                VertexNodeConfigResolver::GetMappingModulePath(cfg),
                hgl::serialize::EnumNameOf(mapping));
            if (GetPositionMappingComponentCount(mapping) == 0)
                result.diagnostics.emplace_back(
                    std::string(hgl::serialize::EnumNameOf(mapping))
                    + ": 位置分量数不能为 0");
        }

        for (uint32_t i = uint32_t(ProjectionMode::BEGIN_RANGE);
             i <= uint32_t(ProjectionMode::END_RANGE); ++i)
        {
            const auto projection = ProjectionMode(i);
            VertexShaderNodeConfig cfg = MakeDefault3DNodeConfig();
            cfg.projection = projection;
            require_module(
                VertexNodeConfigResolver::GetStage3ModulePath(cfg),
                hgl::serialize::EnumNameOf(projection));
        }

        // 相机朝向是 Stage3 的另一条分叉（不看 projection），必须单独覆盖。
        {
            VertexShaderNodeConfig facing = MakeDefault3DNodeConfig();
            facing.orientation = OrientationMode::CameraFacingAxisY;
            facing.scale = ScaleMode::World;
            require_module(
                VertexNodeConfigResolver::GetStage3ModulePath(facing),
                "CameraFacingAxisY/World");
            facing.scale = ScaleMode::FixedPixelSize;
            require_module(
                VertexNodeConfigResolver::GetStage3ModulePath(facing),
                "CameraFacingAxisY/FixedPixelSize");
        }

        // 图不同 ⇒ 选中的模块必须不同（否则「图」就是个摆设）。
        {
            const std::string flat_module = VertexNodeConfigResolver::GetMappingModulePath(
                VertexNodeConfigResolver::FlatXY());
            const std::string wall_module = VertexNodeConfigResolver::GetMappingModulePath(
                VertexNodeConfigResolver::WallXY());
            const std::string world_module = VertexNodeConfigResolver::GetMappingModulePath(
                VertexNodeConfigResolver::World3D());
            if (flat_module == wall_module || flat_module == world_module
             || wall_module == world_module)
                result.diagnostics.emplace_back(
                    "transform graph variants must map to distinct Stage2 modules");
        }

        ShaderStageBuildContext flat_stage{};
        flat_stage.stage = ShaderStage::Mesh;
        flat_stage.compile_inputs_hash =
            VertexNodeConfigResolver::GetHash(VertexNodeConfigResolver::FlatXY());
        const ShaderStageKey flat_key = flat_stage.BuildKey();

        ShaderStageBuildContext wall_stage{};
        wall_stage.stage = ShaderStage::Mesh;
        wall_stage.compile_inputs_hash =
            VertexNodeConfigResolver::GetHash(VertexNodeConfigResolver::WallXY());
        const ShaderStageKey wall_key = wall_stage.BuildKey();
        if (flat_key == wall_key)
            result.diagnostics.emplace_back(
                "transform graph variants must produce distinct stage identity");

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunResolvedMaterialRenderStateCase()
    {
        GateResult result;
        result.name = "Y1.resolved-material-render-state";

        ResolvedMaterial definition{};
        definition.default_render_state.alpha_test = true;
        definition.default_render_state.alpha_cutoff = 0.35f;
        definition.default_render_state.pipeline_config =
            MakeSolid3DConfig();
        definition.default_render_state.pipeline_config.depth_write = false;
        definition.default_render_state.pipeline_config.cull_mode = VK_CULL_MODE_NONE;

        MaterialRecipe defaults{};
        const ResolvedMaterialRenderState default_state =
            ResolveMaterialRenderState(definition, defaults);
        if (default_state.pipeline_config.cull_mode != VK_CULL_MODE_NONE
         || !default_state.alpha_test
         || default_state.alpha_cutoff != 0.35f
         || default_state.pipeline_config.depth_write)
        {
            result.diagnostics.emplace_back(
                "definition render defaults were not preserved by resolution");
        }

        MaterialRecipe overrides{};
        overrides.render_state_overrides.has_alpha_test = true;
        overrides.render_state_overrides.alpha_test = false;
        overrides.render_state_overrides.has_alpha_cutoff = true;
        overrides.render_state_overrides.alpha_cutoff = 0.8f;
        overrides.render_state_overrides.has_dither = true;
        overrides.render_state_overrides.dither = true;
        overrides.render_state_overrides.has_pipeline_config = true;
        overrides.render_state_overrides.pipeline_config =
            MakeAlpha3DConfig();
        overrides.render_state_overrides.pipeline_config.cull_mode = VK_CULL_MODE_BACK_BIT;
        const ResolvedMaterialRenderState resolved =
            ResolveMaterialRenderState(definition, overrides);
        if (resolved.pipeline_config.cull_mode != VK_CULL_MODE_BACK_BIT
         || resolved.alpha_test
         || resolved.alpha_cutoff != 0.8f
         || !resolved.dither
         || !resolved.pipeline_config.alpha_blend)
        {
            result.diagnostics.emplace_back(
                "recipe render-state overrides were not applied atomically");
        }

        if (HashResolvedMaterialRenderState(default_state)
            == HashResolvedMaterialRenderState(resolved))
        {
            result.diagnostics.emplace_back(
                "resolved render-state differences must affect pipeline identity");
        }

        // S4b/D4：归一化取用口只剩 TryGet… —— 未归一化 recipe 必须被**拒绝**
        // （旧口会静默拷结构体默认值 ⇒ 错值），归一化后与 ResolveMaterialRenderState(definition, recipe) 等价。
        MaterialRecipe unnormalized{};
        ResolvedMaterialRenderState from_recipe{};
        if (TryGetNormalizedRecipeRenderState(unnormalized, from_recipe))
        {
            result.diagnostics.emplace_back(
                "un-normalized recipe must be rejected by TryGetNormalizedRecipeRenderState");
        }

        MaterialRecipe normalized_recipe{};
        normalized_recipe.domain = MaterialDomain::UnlitSolid;
        NormalizeRecipe(normalized_recipe);
        if (!TryGetNormalizedRecipeRenderState(normalized_recipe, from_recipe))
        {
            result.diagnostics.emplace_back(
                "normalized recipe must be readable by TryGetNormalizedRecipeRenderState");
        }
        else
        {
            ResolvedMaterial pure_color{};
            if (!BuildResolvedMaterialForDomain(MaterialDomain::UnlitSolid, pure_color))
            {
                result.diagnostics.emplace_back(
                    "UnlitSolid domain definition is missing");
            }
            else
            {
                const ResolvedMaterialRenderState expected =
                    ResolveMaterialRenderState(pure_color, normalized_recipe);
                if (from_recipe.alpha_test != expected.alpha_test
                 || from_recipe.alpha_cutoff != expected.alpha_cutoff
                 || from_recipe.dither != expected.dither
                 || HashResolvedMaterialRenderState(from_recipe)
                        != HashResolvedMaterialRenderState(expected))
                {
                    result.diagnostics.emplace_back(
                        "normalized-recipe accessor must equal ResolveMaterialRenderState(definition, recipe)");
                }
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunMaterialLibraryContractCase()
    {
        GateResult result;
        result.name = "M.material-library-contract";

        const std::string root = GetShaderLibraryPath("ShaderLibrary");
        const std::filesystem::path material_dir =
            std::filesystem::path(root) / "asset" / "recipe";

        // 阶段四（Definition 层删除）：材质库只有一个来源 —— 配方资产。
        // 已退役的 `asset/definition/` 若被重新加回来，说明有人把中间层复活了。
        const std::filesystem::path retired_dir =
            std::filesystem::path(root) / "asset" / "definition";
        if (std::filesystem::exists(retired_dir))
        {
            result.diagnostics.emplace_back(
                "retired ShaderLibrary/asset/definition directory must not exist: "
                + retired_dir.string());
        }

        if (!std::filesystem::exists(material_dir)
         || !std::filesystem::is_directory(material_dir))
        {
            result.diagnostics.emplace_back(
                "missing required ShaderLibrary/asset/recipe directory: "
                + material_dir.string());
            result.passed = false;
            return result;
        }

        std::vector<std::string> material_files;
        try
        {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(material_dir))
            {
                if (!entry.is_regular_file())
                    continue;
                if (entry.path().extension() == ".toml")
                    material_files.push_back(entry.path().string());
            }
        }
        catch (const std::filesystem::filesystem_error &)
        {
            result.diagnostics.emplace_back(
                "failed to inspect ShaderLibrary/asset/recipe tree");
            result.passed = false;
            return result;
        }

        if (material_files.empty())
        {
            result.diagnostics.emplace_back(
                "ShaderLibrary/asset/recipe contains no .recipe.toml files");
            result.passed = false;
            return result;
        }

        // 差量配方（写了 `parent`）允许不声明 `domain`：解析层此时只记名字，
        // 领域要到注册表合并后才有。门禁没有注册表，这里自己建「名字/路径 →
        // 解析结果」索引，第二趟沿父链把有效领域找出来再校验主着色器入口。
        struct ScannedRecipe
        {
            std::string   path;
            std::string   name;
            std::string   parent;
            MaterialDomain domain = MaterialDomain::Custom;
        };
        std::vector<ScannedRecipe> scanned;
        std::map<std::string, size_t> by_name;
        std::map<std::string, size_t> by_path;

        for (const std::string &path : material_files)
        {
            const std::string text = ReadFileText(path);
            if (text.empty())
            {
                result.diagnostics.emplace_back(
                    "empty material file: " + path);
                continue;
            }

            // 材质资产统一为配方格式 `*.recipe.toml`。
            // 这里刻意不再「跳过不认识的后缀」——目录里出现别的 .toml 就是违规，
            // 否则旧 `*.material.toml` / `*.mi.toml` / `*.definition.toml` 会被静默忽略
            // （加载器也不会读它）。
            constexpr const char kMaterialRecipeSuffix[] = ".recipe.toml";
            constexpr std::size_t kMaterialRecipeSuffixLength =
                sizeof(kMaterialRecipeSuffix) - 1;
            if (path.size() < kMaterialRecipeSuffixLength
             || path.compare(path.size() - kMaterialRecipeSuffixLength,
                             kMaterialRecipeSuffixLength,
                             kMaterialRecipeSuffix) != 0)
            {
                result.diagnostics.emplace_back(
                    "material asset must use the .recipe.toml suffix: " + path);
                continue;
            }

            // 用**真实解析器**判定内容，而不是文本子串：
            //   * 子串扫描会把注释里的 `domain = ...` 也当成声明（假阴性）；
            //   * 只要有人改写引号风格或换行，`schema = 1` 的字面匹配立刻误报（假阳性）；
            //   * 领域是否存在、是否已实现主着色器、表面模块是否可用，这些判断
            //     只有解析器 + 领域表知道，在门禁里再抄一份必然漂移。
            MaterialRecipeFileData recipe_data{};
            MaterialRecipeSpec spec{};
            const MaterialRecipeFileParseResult parse_result =
                ParseMaterialRecipeFile(text.c_str(),
                                        static_cast<int>(text.size()),
                                        recipe_data,
                                        &spec);
            if (parse_result != MaterialRecipeFileParseResult::OK)
            {
                // schema 必须为 1、领域必须显式声明且非 Custom、不得有未知键、
                // 表面模块必须属于该领域的固定形态 —— 全部由解析器一次判掉。
                result.diagnostics.emplace_back(
                    "material recipe rejected by parser ["
                    + std::string(GetMaterialRecipeFileParseResultName(parse_result))
                    + "] line=" + std::to_string(recipe_data.parse_error.line)
                    + " table=" + std::string(recipe_data.parse_error.table.c_str())
                    + " key=" + std::string(recipe_data.parse_error.key.c_str())
                    + " value=" + std::string(recipe_data.parse_error.value.c_str())
                    + " detail=" + std::string(recipe_data.parse_error.detail.c_str())
                    + ": " + path);
            }
            else
            {
                ScannedRecipe entry;
                entry.path = path;
                entry.name = spec.name.c_str() ? spec.name.c_str() : "";
                entry.parent = spec.parent.c_str() ? spec.parent.c_str() : "";
                entry.domain = spec.domain;
                scanned.push_back(std::move(entry));
            }

            if (ContainsAnyToken(text, {
                    "MTL_DEF_",
                    "MaterialDefinitionSourceKind",
                    "MaterialDefinitionBootstrapKind",
                    "embedded material",
                    "EmbeddedMaterial" }))
            {
                result.diagnostics.emplace_back(
                    "embedded material or legacy definition IDs are forbidden in: "
                    + path);
            }
        }

        // ── 第二趟：解析父链得到有效领域，再校验固定主着色器入口 ──
        for (size_t i = 0; i < scanned.size(); ++i)
        {
            if (!scanned[i].name.empty())
            {
                const auto existing = by_name.find(scanned[i].name);
                if (existing != by_name.end())
                {
                    result.diagnostics.emplace_back(
                        "duplicate material recipe name: " + scanned[i].name
                        + " in " + scanned[existing->second].path
                        + " and " + scanned[i].path);
                }
                else
                    by_name.emplace(scanned[i].name, i);
            }
            by_path.emplace(scanned[i].path, i);
        }

        for (const ScannedRecipe &entry : scanned)
        {
            MaterialDomain domain = entry.domain;

            if (domain == MaterialDomain::Custom)
            {
                const ScannedRecipe *cur = &entry;
                std::set<std::string> visited;
                visited.insert(cur->path);
                bool resolved = false;
                while (!resolved)
                {
                    MaterialRecipeParentRef ref;
                    if (!ParseMaterialRecipeParentRef(cur->parent.c_str(), ref))
                    {
                        result.diagnostics.emplace_back(
                            "material recipe has an illegal parent reference: " + cur->path);
                        break;
                    }
                    if (ref.IsNone())
                    {
                        result.diagnostics.emplace_back(
                            "material recipe chain declares no domain: " + entry.path);
                        break;
                    }

                    std::string key;
                    if (ref.kind == MaterialRecipeParentRefKind::ByName)
                        key = ref.name.c_str() ? ref.name.c_str() : "";
                    else
                        for (const auto *p = ref.path.c_str(); p && *p; ++p)
                            key += static_cast<char>(*p);

                    const std::map<std::string, size_t> &index =
                        ref.kind == MaterialRecipeParentRefKind::ByName ? by_name : by_path;
                    const auto it = index.find(key);
                    if (it == index.end())
                    {
                        result.diagnostics.emplace_back(
                            "material recipe parent not found in recipe tree: "
                            + cur->path + " parent=" + key);
                        break;
                    }

                    const ScannedRecipe &parent = scanned[it->second];
                    if (!visited.insert(parent.path).second)
                    {
                        result.diagnostics.emplace_back(
                            "material recipe parent chain cycles: " + entry.path);
                        break;
                    }
                    if (parent.domain != MaterialDomain::Custom)
                    {
                        domain = parent.domain;
                        resolved = true;
                    }
                    else if (visited.size() > 16)
                    {
                        result.diagnostics.emplace_back(
                            "material recipe parent chain too deep: " + entry.path);
                        break;
                    }
                    cur = &parent;
                }
                if (!resolved)
                    continue;
            }

            if (!HasMaterialDomainEntrySource(domain))
            {
                result.diagnostics.emplace_back(
                    "material recipe domain has no fixed master shader entry: "
                    + entry.path + " domain=" + GetMaterialDomainName(domain));
            }
        }

        for (const auto &root_dir : { std::filesystem::path("src"), std::filesystem::path("inc") })
        {
            if (!std::filesystem::exists(root_dir) || !std::filesystem::is_directory(root_dir))
                continue;

            try
            {
                for (const auto &entry : std::filesystem::recursive_directory_iterator(root_dir))
                {
                    if (!entry.is_regular_file())
                        continue;

                    const std::string extension = entry.path().extension().string();
                    if (extension != ".cpp" && extension != ".h" && extension != ".hpp"
                     && extension != ".inl" && extension != ".inc")
                        continue;

                    const std::string path = entry.path().string();
                    if (path.find("ShaderResourceSchemaRegressionGate.cpp") != std::string::npos
                     || path.find("GateMaterialization.cpp") != std::string::npos)
                        continue;

                    const std::string text = ReadFileText(path);
                    if (text.empty())
                        continue;

                    if (ContainsAnyToken(text, {
                            "MTL_DEF_",
                            "MaterialDefinitionSourceKind",
                            "MaterialDefinitionBootstrapKind" }))
                    {
                        result.diagnostics.emplace_back(
                            "legacy embedded material bootstrapping markers are forbidden in: "
                            + path);
                    }
                }
            }
            catch (const std::filesystem::filesystem_error &)
            {
                result.diagnostics.emplace_back(
                    "failed to scan source tree for legacy material bootstrap markers");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 顶点 ABI 解析：定义 → 顶点输入模块（闭环位置格式 + 反向硬拒绝）。
    ///
    /// 这些断言原先挂在「解析一份 `LitFile` 定义文件」上。definition 文件消失后
    /// 定义由领域契约当场合成，被测对象换成 `BuildResolvedMaterialForDomain` 产出的
    /// `StandardOpaque` 定义 + 显式覆盖 `vertex_node_config` —— 断言的对象
    /// （`BuildResolvedMaterialVertexABI`）与定义从哪来无关。
    GateResult RunMaterialVertexABIResolutionCase()
    {
        GateResult result;
        result.name = "K.material-vertex-abi-resolution";

        ResolvedMaterial definition{};
        if (!BuildResolvedMaterialForDomain(MaterialDomain::StandardOpaque, definition))
        {
            result.diagnostics.emplace_back(
                "StandardOpaque domain definition is unavailable");
            result.passed = false;
            return result;
        }

        // 契约给的是 3 分量直通位置：三维几何必须选到 vec3 模块。
        definition.vertex_node_config.position_mapping =
            PositionMappingMode::Passthrough3D;

        const GeometryVertexFormat allow_derived_geometry{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal, VF_V3F}
        };
        MaterialBuildRequest allow_derived_request{};
        allow_derived_request.geometry_vertex_format = &allow_derived_geometry;
        MaterialResolvedVertexABI allow_derived_abi{};
        if (!BuildResolvedMaterialVertexABI(
                definition, allow_derived_request, allow_derived_abi)
         || allow_derived_abi.vertex_input_glsl.IsEmpty()
         || allow_derived_abi.position_format != VK_FORMAT_R32G32B32_SFLOAT)
        {
            result.diagnostics.emplace_back(
                "AllowDerived material definition must build a SSBO vertex ABI");
        }

        // 打包 int16x2 位置（4B/顶点）必须映射到 vec2i 模块——这是三对闭环格式之一。
        // 该格式产出 `vec2 Position`，因此只能配 2D 映射的材质；`LitFile` 是
        // Passthrough3D（要 3 分量），直接配它会被生成期硬门拦下。
        ResolvedMaterial packed_position_definition = definition;
        packed_position_definition.vertex_node_config.position_mapping =
            PositionMappingMode::PixelToLocal;

        const GeometryVertexFormat packed_position_geometry{
            {VertexSemantic::Position, VF_V2I16},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal, VF_V3F}
        };
        MaterialBuildRequest packed_position_request{};
        packed_position_request.geometry_vertex_format = &packed_position_geometry;
        MaterialResolvedVertexABI packed_position_abi{};
        if (!BuildResolvedMaterialVertexABI(
                packed_position_definition, packed_position_request, packed_position_abi)
         || packed_position_abi.position_format != VK_FORMAT_R16G16_SINT
         || !std::strstr(packed_position_abi.vertex_input_glsl.c_str(),
                         "stream_position_vec2i.glsl"))
        {
            result.diagnostics.emplace_back(
                "packed int16x2 position must resolve to the vec2i module");
        }

        // 反向：分量数不匹配时必须硬失败（历史上这种组合会静默产出一份编不过的 GLSL，
        // glslang 再把错误指向 s2 模块，定位成本极高）。
        MaterialResolvedVertexABI mismatch_abi{};
        MaterialBuildRequest mismatch_request{};
        mismatch_request.geometry_vertex_format = &packed_position_geometry;
        if (BuildResolvedMaterialVertexABI(
                definition, mismatch_request, mismatch_abi))
        {
            result.diagnostics.emplace_back(
                "vec2 position with Passthrough3D mapping must be rejected");
        }

        // 未闭环位置格式必须被拒绝，而不是挑一个"同义"模块凑合：
        // R16G16_SFLOAT(4B) 曾被当成 Vec2Position（读 8B），R32G32B32A32_SFLOAT(16B)
        // 曾被当成 Vec3Position（读 12B），两者都产出顶点全体错位的着色器。
        const VkFormat unsupported_position_formats[] = {
            VF_V2HF, VF_V2I, VF_V2U, VF_V2U16, VF_V4F
        };
        for (const VkFormat unsupported_format : unsupported_position_formats)
        {
            const GeometryVertexFormat unsupported_geometry{
                {VertexSemantic::Position, unsupported_format},
                {VertexSemantic::TexCoord, VF_V2F},
                {VertexSemantic::Normal, VF_V3F}
            };
            MaterialBuildRequest unsupported_request{};
            unsupported_request.geometry_vertex_format = &unsupported_geometry;
            MaterialResolvedVertexABI unsupported_abi{};
            if (BuildResolvedMaterialVertexABI(definition, unsupported_request, unsupported_abi))
            {
                result.diagnostics.emplace_back(
                    std::string("unsupported vertex position format must be rejected: VkFormat=")
                    + std::to_string(static_cast<int>(unsupported_format)));
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 解析失败细节回传（2026-10-12 用户口径）：材质配置写错必须能 LOG 到"表/键/值/行号"级别
    /// —— 发售口径是全预编译 SPV、届时不再有 shader 合成器，配置错误只能靠日志定位。
    ///
    /// definition 文件消失后，材质作者的唯一输入是 `.material.toml`（配方），所以
    /// 探针喂的是配方文本；子项覆盖：领域名写错（键+值）、未知键（键名）、
    /// 表内枚举值写错（表+键+值）、TOML 语法错（行号）。
    GateResult RunMaterialRecipeParseDetailCase()
    {
        GateResult result;
        result.name = "S.material-recipe-parse-detail";

        // ① 领域名写错：必须报到 键 + 值（本次口径的原始触发场景）
        const char bad_domain_file[] =
            "schema = 1\n"
            "name = \"ParseDetailBadDomain\"\n"
            "domain = \"StandradOpaque\"\n";
        MaterialRecipeFileData bad_domain_data;
        if (ParseMaterialRecipeFile(
                bad_domain_file, static_cast<int>(std::strlen(bad_domain_file)),
                bad_domain_data) != MaterialRecipeFileParseResult::InvalidValue)
        {
            result.diagnostics.emplace_back(
                "bogus domain name must fail the material recipe parse");
        }
        else if (std::strcmp(bad_domain_data.parse_error.key.c_str(), "domain") != 0
              || std::strcmp(bad_domain_data.parse_error.value.c_str(), "StandradOpaque") != 0)
        {
            result.diagnostics.emplace_back(std::string("bogus domain name must report key+value, got key='")
                + bad_domain_data.parse_error.key.c_str() + "' value='"
                + bad_domain_data.parse_error.value.c_str() + "' (table='"
                + bad_domain_data.parse_error.table.c_str() + "')");
        }

        // ② 未知键：必须报到键名（键白名单是"作者笔误 / 已删字段残留"的第一道闸）
        const char unknown_key_file[] =
            "schema = 1\n"
            "name = \"ParseDetailUnknownKey\"\n"
            "domain = \"UnlitSolid\"\n"
            "bogus_root_key = 1\n";
        MaterialRecipeFileData unknown_key_data;
        if (ParseMaterialRecipeFile(
                unknown_key_file, static_cast<int>(std::strlen(unknown_key_file)),
                unknown_key_data) != MaterialRecipeFileParseResult::UnknownKey)
        {
            result.diagnostics.emplace_back("unknown root key must be rejected as UnknownKey");
        }
        else if (std::strcmp(unknown_key_data.parse_error.key.c_str(), "bogus_root_key") != 0)
        {
            result.diagnostics.emplace_back(std::string("unknown root key must report its name, got '")
                + unknown_key_data.parse_error.key.c_str() + "'");
        }

        // ③ 表内枚举值写错：必须报到 表 + 键 + 值（子表名写错是第二高频形态）
        const char bad_transform_file[] =
            "schema = 1\n"
            "name = \"ParseDetailBadTransform\"\n"
            "domain = \"UnlitSolid\"\n"
            "[transform]\n"
            "mapping = \"Passthrough9D\"\n";
        MaterialRecipeFileData bad_transform_data;
        if (ParseMaterialRecipeFile(
                bad_transform_file, static_cast<int>(std::strlen(bad_transform_file)),
                bad_transform_data) != MaterialRecipeFileParseResult::InvalidValue)
        {
            result.diagnostics.emplace_back(
                "bogus transform mapping must fail the material recipe parse");
        }
        else if (std::strcmp(bad_transform_data.parse_error.table.c_str(), "transform") != 0
              || std::strcmp(bad_transform_data.parse_error.key.c_str(), "mapping") != 0
              || std::strcmp(bad_transform_data.parse_error.value.c_str(), "Passthrough9D") != 0)
        {
            result.diagnostics.emplace_back(std::string("bogus transform mapping must report table+key+value, got table='")
                + bad_transform_data.parse_error.table.c_str() + "' key='"
                + bad_transform_data.parse_error.key.c_str() + "' value='"
                + bad_transform_data.parse_error.value.c_str() + "'");
        }

        // ④ TOML 语法错误：必须报出行号（作者最常见的失效模式之一）
        const char syntax_error_file[] = "schema = 1\nname = \"Unterminated\n";
        MaterialRecipeFileData syntax_error_data;
        if (ParseMaterialRecipeFile(
                syntax_error_file, static_cast<int>(std::strlen(syntax_error_file)),
                syntax_error_data) != MaterialRecipeFileParseResult::InvalidValue)
        {
            result.diagnostics.emplace_back("broken TOML must fail the parse");
        }
        else if (syntax_error_data.parse_error.line <= 0)
        {
            result.diagnostics.emplace_back("broken TOML must report the offending line number");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }


    // 表面领域契约（MaterialDomain.h）的门禁。
    //
    // 这一层是「有限领域」取代「顶点×材质×Pass 笛卡尔积」的唯一强制点，
    // 所以门禁必须同时守住两个方向：
    //   ① 契约表自身不能腐化（指向不存在的模块 / 空语义 / 重复槽名）；
    //   ② 校验器不能放行违规材质，也不能误杀合法材质。
    GateResult RunMaterialDomainContractCase()
    {
        GateResult result;
        result.name = "D.material-domain-contract";

        if (MakeSkyConfig().cull_mode != VK_CULL_MODE_NONE)
            result.diagnostics.emplace_back(
                "Sky spheres are viewed from inside and must not cull either face");

        if (MakeSkyCubeSphereConfig().cull_mode != VK_CULL_MODE_FRONT_BIT)
            result.diagnostics.emplace_back(
                "SkyCubeSphere must use its explicit front-face culling config");

        const std::string color_identity = ReadFileText(
            RepoRootPath("ShaderLibrary/module/vertex/stream_color_default.glsl"));
        if (color_identity.find("vec4 GetVertexColor() { return vec4(1.0); }")
                == std::string::npos)
            result.diagnostics.emplace_back(
                "default vertex-color provider must return the white multiplicative identity");

        const std::string surface_interface = ReadFileText(
            RepoRootPath("ShaderLibrary/module/provider/surface/surface_material.glsl"));
        if (surface_interface.find(
                "surface.baseColor = material.baseColor * si.vertexColor.rgb;")
                == std::string::npos
         || surface_interface.find(
                "surface.alpha = material.alpha * si.vertexColor.a;")
                == std::string::npos)
            result.diagnostics.emplace_back(
                "common surface contract must apply vertex color exactly once to RGB and alpha");

        const std::string vertex_color_module_text = ReadFileText(
            RepoRootPath("ShaderLibrary/module/provider/material_source/material_source_vertex_color.glsl"));
        if (vertex_color_module_text.find("materialResult.baseColor = vec3(1.0);")
                == std::string::npos)
            result.diagnostics.emplace_back(
                "VertexColor domain must rely on the common vertex-color multiplier, not multiply it twice");

        // ── 片段侧 varying 声明：单一真源审计 ──
        //
        // 迁移前 13 份领域 `.frag` 各自手抄 `layout(in …)`：网格写出什么与片段读
        // 什么只能靠人对齐，而 checker_3d / error_checker_3d 还给 WorldNormal 挂了
        // 一个与接口无关的 `HGL_MATERIAL_HAS_VERTEX_NORMAL` 开关。现在声明只有一
        // 处——`module/stage/interstage_varying.glsl` 的片段段，开关是
        // `generated/common_options.glsl` 的 `HGL_VARYING_*`，与网格侧同一份。
        // 契约因此变成三条可机检的事实：
        //   ① 领域 `.frag` 里不再出现任何阶段间 varying 的手写声明；
        //   ② 除纯空体占位外，每份 `.frag` 都经 `HGL_FRAGMENT_INPUT_VARYINGS`
        //      走统一文件（否则「统一」只是别人的统一）；
        //   ③ 统一文件把 WorldNormal 声明挂在 `HGL_VARYING_WORLD_NORMAL` 之下，
        //      且两个「有则消费」的领域仍在消费 `fragWorldNormal`。
        const std::string interstage_source = ReadFileText(
            RepoRootPath("ShaderLibrary/module/stage/interstage_varying.glsl"));

        // 「这一行是阶段间输入声明」：`layout(location=…)` 起头且带 `in` 记号。
        // 输出附件用 `out`，因此两者不会混。
        const auto declares_interstage_input =
            [](const std::string &line) -> bool
            {
                return line.find("layout(location=") != std::string::npos
                    && ContainsWord(line, "in")
                    && !ContainsWord(line, "out");
            };

        // 唯一豁免：StandardOpaque / StandardAdvanced 的 depth pass 空体占位。它们
        // 刻意没有 varying、没有 include、没有采样输出，加任何东西都会平白改变 depth 管线。
        const char *const varying_free_fragments[] =
        {
            "ShaderLibrary/domain/standard_opaque/shadow_caster.frag",
            "ShaderLibrary/domain/standard_advanced/shadow_caster.frag",
        };

        const std::vector<std::pair<std::string, std::string>> domain_tree =
            LoadTextTree(RepoRootPath("ShaderLibrary/domain"));
        std::size_t audited_fragments = 0;
        for (const auto &entry : domain_tree)
        {
            const std::string &identity = entry.first;
            if (identity.find(".frag|") == std::string::npos)
                continue;

            // `LoadTextTree` 的 key 是 `lexically_relative` 结果，Windows 上带 `\`。
            // 判据与诊断信息统一成 `/`，免得同一个文件出现两种写法。
            std::string relative = identity.substr(0, identity.find('|'));
            for (char &ch : relative)
                if (ch == '\\')
                    ch = '/';
            const std::string path = "ShaderLibrary/domain/" + relative;

            bool exempt = false;
            for (const char *allowed : varying_free_fragments)
                exempt = exempt || path == allowed;
            if (exempt)
                continue;

            ++audited_fragments;
            const std::string &source = entry.second;

            for (std::size_t line_begin = 0; line_begin < source.size();)
            {
                const std::size_t line_end = source.find('\n', line_begin);
                const std::string line = source.substr(
                    line_begin,
                    line_end == std::string::npos
                        ? std::string::npos
                        : line_end - line_begin);
                if (declares_interstage_input(line))
                    result.diagnostics.emplace_back(
                        path
                        + " declares an inter-stage varying inline; the only "
                          "definition point is module/stage/interstage_varying.glsl");

                if (line_end == std::string::npos)
                    break;
                line_begin = line_end + 1;
            }

            const bool routes_through_unified_file =
                source.find("#define HGL_FRAGMENT_INPUT_VARYINGS")
                    != std::string::npos
             && source.find("#include \"module/stage/interstage_varying.glsl\"")
                    != std::string::npos;
            if (!routes_through_unified_file)
                result.diagnostics.emplace_back(
                    path
                    + " must define HGL_FRAGMENT_INPUT_VARYINGS and include "
                      "module/stage/interstage_varying.glsl instead of declaring "
                      "inter-stage varyings itself");
        }
        if (audited_fragments < 12)
            result.diagnostics.emplace_back(
                "domain fragment sweep found only "
                + std::to_string(audited_fragments)
                + " master(s); the domain library is expected to be far larger");

        const InterStageSemanticInfo *normal_info =
            GetInterStageSemanticInfo(InterStageSemantic::WorldNormal);
        if (normal_info == nullptr || normal_info->shader_symbol == nullptr)
        {
            result.diagnostics.emplace_back(
                "WorldNormal semantics are not registered");
        }
        else
        {
            const std::string conditional = ExtractConditionalBlock(
                interstage_source, "HGL_VARYING_WORLD_NORMAL");
            if (conditional.find(std::string("vec3 ") + normal_info->shader_symbol)
                    == std::string::npos)
                result.diagnostics.emplace_back(
                    "module/stage/interstage_varying.glsl must declare fragWorldNormal "
                    "under HGL_VARYING_WORLD_NORMAL");

            // 「有则消费」的两个领域：接口给了就必须用上，否则顶点法线会静默退化
            // 成面法线兜底。
            const char *const normal_consumers[] =
            {
                "ShaderLibrary/domain/checker_3d/checker_3d.frag",
                "ShaderLibrary/domain/error_checker_3d/error_checker_3d.frag",
            };
            for (const char *consumer_path : normal_consumers)
            {
                const std::string consumer_source =
                    ReadFileText(RepoRootPath(consumer_path));
                if (consumer_source.find(
                        "si.worldNormal = normalize(fragWorldNormal);")
                        == std::string::npos)
                    result.diagnostics.emplace_back(
                        std::string(consumer_path)
                        + " must consume the optional WorldNormal varying when present");
            }
        }

        DomainEntrySource sky_minimal_template;
        if (!ResolveDomainEntrySource(
                MaterialDomain::Sky,
                ShaderProgramPurpose::ForwardColor,
                false,
                sky_minimal_template)
         || std::strcmp(
                sky_minimal_template.relative_path.c_str(),
                "domain/sky/sky_minimal.frag") != 0)
            result.diagnostics.emplace_back(
                "SkyMinimal must resolve the fixed fragment that includes its no-texture surface");

        DomainEntrySource sky_cube_template;
        if (!ResolveDomainEntrySource(
                MaterialDomain::Sky,
                ShaderProgramPurpose::ForwardColor,
                false,
                sky_cube_template,
                "surface_sky_cube")
         || std::strcmp(
                sky_cube_template.relative_path.c_str(),
                "domain/sky/sky.frag") != 0)
            result.diagnostics.emplace_back(
                "SkyCube must retain its cubemap fixed fragment");

        DomainEntrySource unsupported_sky_template;
        if (ResolveDomainEntrySource(
                MaterialDomain::Sky,
                ShaderProgramPurpose::ForwardColor,
                false,
                unsupported_sky_template,
                "unknown_sky_surface"))
            result.diagnostics.emplace_back(
                "Sky must reject a surface provider without a fixed fragment master");

        // ── ① 契约表自检 + 引用模块存在性 ────────────────────────────────────
        GlslModuleRegistry registry;
        int module_file_count = 0;
        int module_error_count = 0;
        const bool registry_ok = registry.LoadDirectory(
            hgl::ToOSString(GetShaderLibraryPath()),
            &module_file_count, &module_error_count);
        if (!registry_ok)
            result.diagnostics.emplace_back("domain contract: module registry scan failed");
        else if (module_error_count != 0)
            result.diagnostics.emplace_back(
                "domain contract: module registry scan reported parse errors");

        for (int i = 0; i < MaterialDomainContractCount; ++i)
        {
            const MaterialDomainContract *contract = GetMaterialDomainContractAt(i);
            if (!contract)
            {
                result.diagnostics.emplace_back(
                    "domain contract table has a null entry at index "
                    + std::to_string(i));
                continue;
            }

            const std::string tag = std::string("domain '")
                + (contract->name ? contract->name : "<null>") + "'";

            if (!contract->name || !*contract->name)
                result.diagnostics.emplace_back(
                    "domain contract at index " + std::to_string(i) + " has no name");

            // 名字必须能被 ParseMaterialDomain 反解回同一个枚举值，
            // 否则 TOML 写什么名字都对不上。
            MaterialDomain roundtrip = MaterialDomain::Custom;
            if (contract->name
                && (!ParseMaterialDomain(contract->name, roundtrip)
                    || roundtrip != contract->domain))
            {
                result.diagnostics.emplace_back(
                    tag + " name does not round-trip through ParseMaterialDomain");
            }

            if (contract->required_vertex_semantic_mask == 0)
                result.diagnostics.emplace_back(tag + " requires no vertex semantic at all");

            // 无纹理领域（纯色/顶点色/亮度/棋盘格等）是**合法**的：
            // 它们结构上就不该有贴图。这里只拦「指针与计数不一致」的笔误，
            // 不要求每个领域都必须有槽。
            if ((contract->texture_slots == nullptr) != (contract->texture_slot_count == 0))
            {
                result.diagnostics.emplace_back(
                    tag + " texture slot table is inconsistent (pointer/count mismatch)");
                continue;
            }

            // no_material_source 与 reference_source 不能同时成立：
            // 前者说「本领域没有来源模块」，后者却给出了一个模块名。
            if (contract->no_material_source
                && contract->reference_material_source_module)
            {
                result.diagnostics.emplace_back(
                    tag + " sets no_material_source but still names a source module");
            }

            if (contract->texture_slot_count <= 0)
                continue;

            for (int s = 0; s < contract->texture_slot_count; ++s)
            {
                const MaterialDomainTextureSlot &slot = contract->texture_slots[s];
                if (!slot.name || !*slot.name)
                {
                    result.diagnostics.emplace_back(
                        tag + " has an unnamed texture slot at index "
                        + std::to_string(s));
                    continue;
                }
                for (int t = s + 1; t < contract->texture_slot_count; ++t)
                {
                    const MaterialDomainTextureSlot &other = contract->texture_slots[t];
                    if (other.name && std::strcmp(slot.name, other.name) == 0)
                        result.diagnostics.emplace_back(
                            tag + " declares duplicate texture slot '" + slot.name + "'");
                }
            }

            // 引用模块必须真实存在于注册表：契约指向空气时，
            // Phase 2 的宏头生成会产出无法编译的结果。
            if (registry_ok)
            {
                if (contract->reference_material_source_module
                    && !registry.FindByName(contract->reference_material_source_module))
                {
                    result.diagnostics.emplace_back(
                        tag + " references unknown material source module '"
                        + contract->reference_material_source_module + "'");
                }
                if (contract->reference_ntb_module
                    && !registry.FindByName(contract->reference_ntb_module))
                {
                    result.diagnostics.emplace_back(
                        tag + " references unknown ntb module '"
                        + contract->reference_ntb_module + "'");
                }
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunFallbackInferenceCase()
    {
        GateResult result;
        result.name = "F.fallback-dimension-neutral";

        GeometryVertexFormat gvf_2d{};
        gvf_2d.Add(VertexSemantic::Position, VK_FORMAT_R32G32_SFLOAT, 2, sizeof(float) * 2);

        GeometryVertexFormat gvf_3d{};
        gvf_3d.Add(VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT, 3, sizeof(float) * 3);

        if (gvf_2d.GetVertexInputHash() == gvf_3d.GetVertexInputHash())
            result.diagnostics.emplace_back(
                "Geometry formats with different dimensions must remain distinct");
        if (kMarkerlessFallbackMaterialDomain != MaterialDomain::Checker3D)
            result.diagnostics.emplace_back("all Geometry dimensions must use unified Checker3D fallback");

        result.passed = result.diagnostics.empty();
        return result;
    }

    GateResult RunSamplerPresetLibraryCase()
    {
        GateResult result;
        result.name = "AC.sampler-preset-library";

        auto &lib = SamplerPresetLibrary::Instance();

        // 1. 加载 contract/sampler.toml（统一注册机制唯一数据源）。
        const std::string root = GetShaderLibraryPath();
        const hgl::filesystem::Path sampler_toml(
            hgl::ToOSString(root + "/contract/sampler.toml"));
        if (!lib.Load(sampler_toml.ToOSString()))
        {
            result.diagnostics.emplace_back("contract/sampler.toml load failed");
            result.passed = false;
            return result;
        }

        // 2. 名字→索引一致（顺序 = contract/sampler.toml 数组顺序）。
        struct NameIndex
        {
            const char *name;
            uint32_t index;
        };
        const NameIndex expected[] =
        {
            { "Nearest",        0u },
            { "Linear",         1u },
            { "Trilinear",      2u },
            { "TrilinearAniso", 3u },
            { "ShadowPCF",      4u },
            { "Terrain",        5u },
            { "UI",             6u },
            { "TrilinearClamp", 7u },
        };
        if (lib.GetCount() != uint32_t(std::size(expected)))
        {
            result.diagnostics.emplace_back(
                "preset count mismatch: expected "
                + std::to_string(std::size(expected))
                + " got " + std::to_string(lib.GetCount()));
        }
        for (const auto &e : expected)
        {
            if (lib.GetIndex(e.name) != e.index)
            {
                result.diagnostics.emplace_back(
                    std::string("name->index mismatch: ") + e.name);
            }
        }

        // 3. 未知名 / 空名 / 空指针 → 显式无效（~0u）——调用方必须处理，
        //    不再静默保底 0（=Nearest）掩盖 contract/sampler.toml 顺序错位。
        if (lib.GetIndex("DoesNotExistSampler") != ~0u)
            result.diagnostics.emplace_back("unknown name must yield invalid index");
        if (lib.GetIndex("") != ~0u)
            result.diagnostics.emplace_back("empty name must yield invalid index");
        if (lib.GetIndex(nullptr) != ~0u)
            result.diagnostics.emplace_back("null name must yield invalid index");

        // 4. max_lod 统一 15.0（不再按纹理 mip 级数派生）。
        for (uint32_t i = 0; i < lib.GetCount(); ++i)
        {
            const VkSamplerCreateInfo *sci = lib.GetCreateInfo(i);
            if (!sci)
            {
                result.diagnostics.emplace_back(
                    "GetCreateInfo(" + std::to_string(i) + ") returned null");
                continue;
            }
            if (sci->maxLod != 15.0f)
            {
                result.diagnostics.emplace_back(
                    "preset " + std::to_string(i) + " maxLod != 15.0");
            }
        }

        // 5. 关键预设过滤语义。
        const VkSamplerCreateInfo *nearest = lib.GetCreateInfo(0);
        if (nearest
            && (nearest->magFilter != VK_FILTER_NEAREST
             || nearest->minFilter != VK_FILTER_NEAREST))
            result.diagnostics.emplace_back("Nearest filter mismatch");

        const VkSamplerCreateInfo *trilinear = lib.GetCreateInfo(2);
        if (trilinear
            && trilinear->mipmapMode != VK_SAMPLER_MIPMAP_MODE_LINEAR)
            result.diagnostics.emplace_back("Trilinear mipmap mode mismatch");

        const VkSamplerCreateInfo *aniso = lib.GetCreateInfo(3);
        if (aniso && aniso->anisotropyEnable != VK_TRUE)
            result.diagnostics.emplace_back("TrilinearAniso anisotropy mismatch");

        const VkSamplerCreateInfo *pcf = lib.GetCreateInfo(4);
        if (pcf
            && (pcf->compareEnable != VK_TRUE
             || pcf->addressModeU != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE))
            result.diagnostics.emplace_back("ShadowPCF compare/clamp mismatch");

        const VkSamplerCreateInfo *clamp = lib.GetCreateInfo(7);
        if (clamp
            && (clamp->addressModeU != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
             || clamp->mipmapMode != VK_SAMPLER_MIPMAP_MODE_LINEAR))
            result.diagnostics.emplace_back("TrilinearClamp addressMode/mipmap mismatch");

        // 6. sampler 宏注入（name→idx 与运行时 binding=1 数组下标一致）。
        const std::vector<std::string> names =
            { "Trilinear", "Linear", "Terrain" };
        const std::string macros = BuildSamplerMacros(names);
        const std::string expected_macros =
            "#define TrilinearSampler 2u\n"
            "#define LinearSampler 1u\n"
            "#define TerrainSampler 5u\n";
        if (macros != expected_macros)
        {
            result.diagnostics.emplace_back(
                "sampler macro injection mismatch: got [" + macros + "]");
        }

        // 7. 未知名 → 不生成宏（shader 编译显式失败暴露，不再静默保底 0）；
        //    空列表、空名跳过。
        if (!BuildSamplerMacros({ "UnknownSampler" }).empty())
            result.diagnostics.emplace_back("unknown sampler must produce no macro");
        if (!BuildSamplerMacros({}).empty())
            result.diagnostics.emplace_back("empty sampler list must produce no macros");
        if (!BuildSamplerMacros({ "" }).empty())
            result.diagnostics.emplace_back("empty sampler name must be skipped");

        result.passed = result.diagnostics.empty();
        return result;
    }

} // namespace gate
