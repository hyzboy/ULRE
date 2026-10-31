#include <hgl/mtl/MaterialRecipeFile.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cstring>

using namespace hgl;
using namespace hgl::graph::mtl;

namespace
{
    constexpr char kValidRecipe[] =
        "schema = 1\n"
        "name = \"RecipeParser.Valid\"\n"
        "domain = \"UnlitTexture\"\n"
        "\n"
        "[transform]\n"
        "source = \"Vec2Position\"\n"
        "mapping = \"LiftXY_XY0\"\n"
        "orientation = \"World\"\n"
        "scale = \"World\"\n"
        "projection = \"OrthoViewport\"\n"
        "\n"
        "[render_state]\n"
        "alpha_test = true\n"
        "alpha_cutoff = 0.25\n"
        "dither = false\n"
        "\n"
        "[render_state.pipeline]\n"
        "cull_mode = \"None\"\n"
        "depth_test = false\n"
        "depth_write = false\n"
        "depth_compare_op = \"Always\"\n"
        "alpha_blend = true\n"
        "blend_src = \"SrcAlpha\"\n"
        "blend_dst = \"OneMinusSrcAlpha\"\n"
        "dynamic_line_width = false\n"
        "overlay = true\n"
        "wireframe = false\n"
        "line_width = 1.0\n"
        "\n"
        "[[textures]]\n"
        "name = \"base_color\"\n"
        "asset = \"res/image/lena.Tex2D\"\n"
        "required = true\n"
        "layer = 0\n";

    bool Parse(const char *text, MaterialRecipeFileData &data)
    {
        return ParseMaterialRecipeFile(
            text, static_cast<int>(strlen(text)), data)
            == MaterialRecipeFileParseResult::OK;
    }

    bool ParseFile(const OSString &path, MaterialRecipeFileData &data)
    {
        hgl::io::OpenFileInputStream opener(path);
        if (!opener)
            return false;

        const int64 size = opener->GetSize();
        if (size <= 0 || size > hgl::HGL_U32_MAX)
            return false;

        hgl::AutoDeleteArray<char> buffer(static_cast<size_t>(size) + 1);
        if (!buffer || opener->Read(buffer.data(), size) != size)
            return false;
        buffer[static_cast<size_t>(size)] = '\0';

        return ParseMaterialRecipeFile(
            buffer.data(), static_cast<int>(size), data)
            == MaterialRecipeFileParseResult::OK;
    }

    bool CheckValidRecipe()
    {
        MaterialRecipeFileData data;
        if (!Parse(kValidRecipe, data))
            return false;

        const MaterialRecipe &recipe = data.recipe;
        return recipe.recipe_name == "RecipeParser.Valid"
            && recipe.domain == MaterialDomain::UnlitTexture
            && recipe.vertex_node_config.input == VertexInputMode::Vec2Position
            && recipe.vertex_node_config.position_mapping == PositionMappingMode::LiftXY_XY0
            && recipe.vertex_node_config.projection == ProjectionMode::OrthoViewport
            && recipe.render_state_overrides.has_alpha_test
            && recipe.render_state_overrides.alpha_test
            && recipe.render_state_overrides.has_pipeline_config
            && recipe.render_state_overrides.pipeline_config.cull_mode == VK_CULL_MODE_NONE
            && !recipe.render_state_overrides.pipeline_config.depth_test
            && recipe.render_state_overrides.pipeline_config.alpha_blend
            && recipe.textures.size() == 1
            && recipe.textures[0].texture_name == "base_color"
            && recipe.textures[0].resource_id == "res/image/lena.Tex2D"
            && recipe.textures[0].required
            && recipe.textures[0].array_layer == 0;
    }

    bool CheckUnknownKeyDiagnostic()
    {
        constexpr char kUnknownKeyRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.UnknownKey\"\n"
            "domain = \"UnlitTexture\"\n"
            "matreial_definition = \"typo\"\n";

        constexpr char kUnknownNestedKeyRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.UnknownNestedKey\"\n"
            "domain = \"UnlitTexture\"\n"
            "\n"
            "[transform]\n"
            "maping = \"LiftXY_XY0\"\n";

        MaterialRecipeFileData data;
        const MaterialRecipeFileParseResult root_result =
            ParseMaterialRecipeFile(
                kUnknownKeyRecipe,
                static_cast<int>(strlen(kUnknownKeyRecipe)),
                data);

        if (root_result != MaterialRecipeFileParseResult::UnknownKey
         || data.parse_error.table != "root"
         || data.parse_error.key != "matreial_definition")
            return false;

        const MaterialRecipeFileParseResult nested_result =
            ParseMaterialRecipeFile(
                kUnknownNestedKeyRecipe,
                static_cast<int>(strlen(kUnknownNestedKeyRecipe)),
                data);

        return nested_result == MaterialRecipeFileParseResult::UnknownKey
            && data.parse_error.table == "transform"
            && data.parse_error.key == "maping";
    }

    bool CheckInvalidTransformDiagnostic()
    {
        constexpr char kInvalidTransformRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.InvalidTransform\"\n"
            "domain = \"UnlitTexture\"\n"
            "\n"
            "[transform]\n"
            "projection = \"Orthographic2D\"\n";

        MaterialRecipeFileData data;
        const MaterialRecipeFileParseResult result =
            ParseMaterialRecipeFile(
                kInvalidTransformRecipe,
                static_cast<int>(strlen(kInvalidTransformRecipe)),
                data);

        return result == MaterialRecipeFileParseResult::InvalidValue
            && data.parse_error.table == "transform"
            && data.parse_error.key == "projection"
            && data.parse_error.value == "Orthographic2D";
    }

    // 值解析失败发生在**嵌套表内部**时，诊断必须带上完整路径（`render_state.pipeline`），
    // 而不是退化成空表名或上一层的陈旧表名 —— 否则报错会指错位置。这是 codec 读完字段
    // 才发现失败的那一类：键校验（按生成元数据递归）认得出 `cull_mode` 是合法键，
    // 只有把 `NotACullMode` 解析成 `VkCullModeFlagBits` 时才会失败，失败路径由
    // `Archive::FailedPath()` 在递归下钻时逐层拼出，不依赖任何粘性状态。
    bool CheckNestedPipelineDiagnostic()
    {
        constexpr char kNestedRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.NestedDiagnostic\"\n"
            "domain = \"StandardOpaque\"\n"
            "\n"
            "[render_state.pipeline]\n"
            "cull_mode = \"NotACullMode\"\n";

        MaterialRecipeFileData data;
        const MaterialRecipeFileParseResult result =
            ParseMaterialRecipeFile(
                kNestedRecipe,
                static_cast<int>(strlen(kNestedRecipe)),
                data);

        if (result != MaterialRecipeFileParseResult::InvalidValue)
            return false;

        if (data.parse_error.table != "render_state.pipeline"
         || data.parse_error.key != "cull_mode"
         || data.parse_error.value != "NotACullMode")
        {
            GLogError(u8"嵌套失败诊断路径不符: table=%s key=%s value=%s",
                      data.parse_error.table.c_str(),
                      data.parse_error.key.c_str(),
                      data.parse_error.value.c_str());
            return false;
        }

        return true;
    }

    // `[render_state.pipeline]` 是局部覆盖：只写与默认不同的键，未写的字段保持
    // 契约默认值。显式写出的 false / 0 / None 必须真正生效（不能当「没写」）。
    bool CheckPartialPipelineOverride()
    {
        constexpr char kPartialPipelineRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.PartialPipeline\"\n"
            "domain = \"UnlitTexture\"\n"
            "\n"
            "[transform]\n"
            "source = \"Vec2Position\"\n"
            "mapping = \"LiftXY_XY0\"\n"
            "orientation = \"World\"\n"
            "scale = \"World\"\n"
            "projection = \"OrthoViewport\"\n"
            "\n"
            "[render_state.pipeline]\n"
            "cull_mode = \"None\"\n";

        MaterialRecipeFileData data;
        if (!Parse(kPartialPipelineRecipe, data))
            return false;

        const MaterialRenderStateOverrides &parsed = data.recipe.render_state_overrides;
        if (!parsed.has_pipeline_config)
            return false;
        if (parsed.pipeline_config_mask
         != static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::CullMode))
            return false;

        // 契约默认（Back / depth_test=true / depth_write=true / GreaterOrEqual）只被 cull 一项覆盖。
        const ResolvedMaterialRenderState defaults{};
        const ResolvedMaterialRenderState merged =
            ResolveMaterialRenderState(defaults, data.recipe);
        if (merged.pipeline_config.cull_mode != VK_CULL_MODE_NONE
         || !merged.pipeline_config.depth_test
         || !merged.pipeline_config.depth_write
         || merged.pipeline_config.depth_compare_op != VK_COMPARE_OP_GREATER_OR_EQUAL
         || merged.pipeline_config.overlay)
            return false;

        // 显式 false / 非默认数值：与「没写」必须区分开。
        constexpr char kExplicitValues[] =
            "schema = 1\n"
            "name = \"RecipeParser.ExplicitPipelineValues\"\n"
            "domain = \"UnlitTexture\"\n"
            "\n"
            "[render_state.pipeline]\n"
            "depth_write = false\n"
            "alpha_blend = false\n"
            "overlay = true\n"
            "line_width = 2.5\n";

        MaterialRecipeFileData explicit_data;
        if (!Parse(kExplicitValues, explicit_data))
            return false;

        // 基准故意把 alpha_blend / depth_write 设成相反值，才能证明「写 false 真的盖下去了」。
        ResolvedMaterialRenderState base{};
        base.pipeline_config.alpha_blend = true;
        base.pipeline_config.depth_write = true;
        base.pipeline_config.line_width = 1.0f;

        const ResolvedMaterialRenderState explicit_merged =
            ResolveMaterialRenderState(base, explicit_data.recipe);
        return !explicit_merged.pipeline_config.depth_write
            && !explicit_merged.pipeline_config.alpha_blend
            && explicit_merged.pipeline_config.overlay
            && explicit_merged.pipeline_config.line_width == 2.5f
            && explicit_merged.pipeline_config.depth_test
            && explicit_merged.pipeline_config.cull_mode == VK_CULL_MODE_BACK_BIT;
    }

    bool CheckMaterialDataIsRuntimeOnly()
    {
        constexpr char kMaterialDataRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.MaterialData\"\n"
            "domain = \"StandardOpaque\"\n"
            "\n"
            "[material_data]\n"
            "roughness = 0.5\n";

        MaterialRecipeFileData data;
        return ParseMaterialRecipeFile(
                   kMaterialDataRecipe,
                   static_cast<int>(strlen(kMaterialDataRecipe)),
                   data)
            == MaterialRecipeFileParseResult::UnknownKey;
    }

    bool CheckFragmentOverrides()
    {
        // `[fragment]` 的 samplers/defines 覆盖必须能从数据表达，
        // 空数组表示「清空」而非「不覆盖」。
        constexpr char kOverrides[] =
            "schema = 1\n"
            "name = \"RecipeParser.FragmentOverrides\"\n"
            "domain = \"UnlitTexture\"\n"
            "\n"
            "[fragment]\n"
            "samplers = [\"nearest\"]\n"
            "defines = [\"TEXT_GPU_BITMAP\"]\n";

        MaterialRecipeFileData data;
        if (!Parse(kOverrides, data))
            return false;
        if (!data.recipe.has_sampler_names
         || data.recipe.sampler_names.size() != 1
         || data.recipe.sampler_names[0] != "nearest"
         || !data.recipe.has_compile_defines
         || data.recipe.compile_defines.size() != 1
         || data.recipe.compile_defines[0] != "TEXT_GPU_BITMAP")
            return false;

        constexpr char kCleared[] =
            "schema = 1\n"
            "name = \"RecipeParser.FragmentCleared\"\n"
            "domain = \"UnlitTexture\"\n"
            "\n"
            "[fragment]\n"
            "samplers = []\n";

        MaterialRecipeFileData cleared;
        if (!Parse(kCleared, cleared))
            return false;
        if (!cleared.recipe.has_sampler_names || !cleared.recipe.sampler_names.empty())
            return false;
        if (cleared.recipe.has_compile_defines)
            return false;

        // 表面模块只在 no_material_source 领域（当前仅 Sky）有效
        constexpr char kSurfaceOnWrongDomain[] =
            "schema = 1\n"
            "name = \"RecipeParser.SurfaceOnWrongDomain\"\n"
            "domain = \"UnlitTexture\"\n"
            "\n"
            "[fragment]\n"
            "surface_module_name = \"surface_sky_cube\"\n";

        const MaterialRecipeFileParseResult wrong_domain =
            ParseMaterialRecipeFile(
                kSurfaceOnWrongDomain,
                static_cast<int>(strlen(kSurfaceOnWrongDomain)),
                data);
        if (wrong_domain != MaterialRecipeFileParseResult::InvalidValue
         || data.parse_error.table != "fragment"
         || data.parse_error.key != "surface_module_name")
            return false;

        // 领域正确但形态名不存在也必须失败，而不是静默忽略
        constexpr char kUnknownSurface[] =
            "schema = 1\n"
            "name = \"RecipeParser.UnknownSurface\"\n"
            "domain = \"Sky\"\n"
            "\n"
            "[fragment]\n"
            "surface_module_name = \"surface_missing\"\n";

        const MaterialRecipeFileParseResult unknown_surface =
            ParseMaterialRecipeFile(
                kUnknownSurface,
                static_cast<int>(strlen(kUnknownSurface)),
                data);
        if (unknown_surface != MaterialRecipeFileParseResult::InvalidValue
         || data.parse_error.key != "surface_module_name")
            return false;

        constexpr char kKnownSurface[] =
            "schema = 1\n"
            "name = \"RecipeParser.KnownSurface\"\n"
            "domain = \"Sky\"\n"
            "\n"
            "[fragment]\n"
            "surface_module_name = \"surface_sky_cube\"\n";

        MaterialRecipeFileData sky;
        return Parse(kKnownSurface, sky)
            && sky.recipe.surface_module_name == "surface_sky_cube";
    }

    bool CheckRegistryNormalizationAndDuplicate()
    {
        constexpr char kRegistryRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.Registry\"\n"
            "domain = \"UnlitTexture\"\n";

        MaterialRecipeRegistry registry;
        if (!registry.RegisterRecipeSource(
                OS_TEXT("registry-test"),
                kRegistryRecipe,
                static_cast<int>(strlen(kRegistryRecipe)),
                nullptr))
            return false;

        const MaterialRecipe *recipe =
            registry.FindByName("RecipeParser.Registry");
        if (!recipe || !IsRecipeNormalized(*recipe))
            return false;

        // 句柄层：注册表表项是 `MaterialRecipeAsset`，同一份指针要能同时给出
        // 身份、规约与运行时结果（三个视角，一张表）。
        const MaterialRecipeAsset *asset =
            registry.FindAssetByName("RecipeParser.Registry");
        if (!asset
         || asset->GetState() != MaterialRecipeAssetState::Ready
         || !asset->IsReady()
         || &asset->GetRecipe() != recipe)
            return false;

        if (strcmp(asset->GetAssetName().c_str(), "RecipeParser.Registry") != 0
         || strcmp(asset->GetSpec().name.c_str(), "RecipeParser.Registry") != 0)
            return false;

        if (asset->GetResourceId() != U8String(u8"rcp:@RecipeParser.Registry"))
        {
            GLogError(u8"配方资产身份不符: %s",
                      reinterpret_cast<const char *>(asset->GetResourceId().c_str()));
            return false;
        }

        // 内存注册的标签就是字面量来源，可按来源反查。
        if (asset->GetSourcePath() != OS_TEXT("registry-test")
         || registry.FindAssetByPath(OS_TEXT("registry-test")) != asset
         || registry.FindAssetByPath(OS_TEXT("registry-nonexistent")) != nullptr)
            return false;

        // `AssetObject` 契约：反射描述取规约表，且必须真的带属性。
        const hgl::asset::ClassMetaInfo *meta = asset->GetClassMetaInfo();
        if (!meta || !meta->name || strcmp(meta->name, "MaterialRecipeSpec") != 0
         || meta->property_count <= 0
         || meta->struct_size != sizeof(MaterialRecipeSpec))
            return false;

        // 无纹理 ⇒ 就绪度聚合恒为真（异步轴在纹理句柄上，这里只做汇总）。
        if (!asset->AreTexturesResident() || asset->GetPendingTextureCount() != 0)
            return false;

        if (registry.RegisterRecipeSource(
                OS_TEXT("registry-duplicate"),
                kRegistryRecipe,
                static_cast<int>(strlen(kRegistryRecipe)),
                nullptr))
            return false;

        constexpr char kUnknownDomain[] =
            "schema = 1\n"
            "name = \"RecipeParser.UnknownDomain\"\n"
            "domain = \"MissingDomain\"\n";
        if (registry.RegisterRecipeSource(
                OS_TEXT("registry-unknown-domain"),
                kUnknownDomain,
                static_cast<int>(strlen(kUnknownDomain)),
                nullptr))
            return false;

        constexpr char kCustomDomain[] =
            "schema = 1\n"
            "name = \"RecipeParser.CustomDomain\"\n"
            "domain = \"Custom\"\n";
        if (registry.RegisterRecipeSource(
                OS_TEXT("registry-custom-domain"),
                kCustomDomain,
                static_cast<int>(strlen(kCustomDomain)),
                nullptr))
            return false;

        constexpr char kUnknownSlot[] =
            "schema = 1\n"
            "name = \"RecipeParser.UnknownSlot\"\n"
            "domain = \"UnlitTexture\"\n"
            "\n"
            "[[textures]]\n"
            "name = \"not_declared\"\n"
            "asset = \"res/image/lena.Tex2D\"\n";
        if (registry.RegisterRecipeSource(
                OS_TEXT("registry-unknown-slot"),
                kUnknownSlot,
                static_cast<int>(strlen(kUnknownSlot)),
                nullptr))
            return false;

        return registry.GetCount() == 1
            && registry.FindByName("RecipeParser.UnknownDomain") == nullptr
            && registry.FindByName("RecipeParser.CustomDomain") == nullptr
            && registry.FindByName("RecipeParser.UnknownSlot") == nullptr;
    }

    bool CheckExampleRecipeFiles()
    {
        MaterialRecipeFileData texture_quad;
        if (!ParseFile(
                OS_TEXT("ShaderLibrary/asset/recipe/TextureQuad.recipe.toml"),
                texture_quad)
         || texture_quad.recipe.recipe_name != "TextureQuad.UnlitTexture"
         || texture_quad.recipe.textures.size() != 1
         || texture_quad.recipe.textures[0].resource_id != "res/image/lena.Tex2D")
            return false;

        // overlay 配方只写与默认不同的键，其余保持默认。
        const MaterialPipelineConfig &quad_pipeline =
            texture_quad.recipe.render_state_overrides.pipeline_config;
        if (!texture_quad.recipe.render_state_overrides.has_pipeline_config
         || quad_pipeline.cull_mode != VK_CULL_MODE_NONE
         || quad_pipeline.depth_test
         || quad_pipeline.depth_write
         || quad_pipeline.depth_compare_op != VK_COMPARE_OP_ALWAYS
         || !quad_pipeline.overlay
         || quad_pipeline.alpha_blend)
            return false;

        // PBRSpheres 与契约/结构体默认完全一致 ⇒ 整块 `[render_state.pipeline]` 已删掉。
        MaterialRecipeFileData pbr_spheres;
        if (!ParseFile(
                OS_TEXT("ShaderLibrary/asset/recipe/PBRSpheres.recipe.toml"),
                pbr_spheres)
         || pbr_spheres.recipe.recipe_name != "PBRSpheres.Lit"
         || !pbr_spheres.recipe.textures.empty()
         || pbr_spheres.recipe.render_state_overrides.has_pipeline_config)
            return false;

        hgl::graph::TextureManager texture_manager(nullptr);
        MaterialRecipeRegistry example_registry;
        if (!example_registry.LoadFile(
                OS_TEXT("ShaderLibrary/asset/recipe/PBRSpheres.recipe.toml"), &texture_manager)
         || !example_registry.FindByName("PBRSpheres.Lit")
         || !example_registry.LoadFile(
                OS_TEXT("ShaderLibrary/asset/recipe/TextureQuad.recipe.toml"), &texture_manager)
         || !example_registry.FindByName("TextureQuad.UnlitTexture"))
        {
            example_registry.Clear();
            texture_manager.Release();
            return false;
        }

        int registered_file_count = 0;
        int error_count = 0;
        const bool loaded = example_registry.LoadDirectory(
                   OS_TEXT("ShaderLibrary/asset/recipe/Examples"),
                   &texture_manager,
                   &registered_file_count,
                   &error_count);
        const bool valid = loaded && registered_file_count > 0 && error_count == 0
                        && example_registry.GetCount() == registered_file_count + 2;
        if (!valid)
        {
            GLogError("example recipe registry mismatch: load=%d registered=%d errors=%d stored=%d",
                      loaded, registered_file_count, error_count,
                      example_registry.GetCount());
        }
        example_registry.Clear();
        texture_manager.Release();
        return valid;
    }

    bool CheckLookupFailures()
    {
        hgl::graph::GraphicsContext graphics(nullptr);
        if (graphics.LoadMaterialRecipeDirectory(
                OS_TEXT("ShaderLibrary/asset/recipe/__missing_recipe_directory__")))
            return false;
        if (graphics.FindOrLoadMaterialRecipe("") != nullptr)
            return false;
        if (!graphics.FindOrLoadMaterialRecipe("PBRSpheres.Lit"))
            return false;
        return graphics.FindOrLoadMaterialRecipe("RecipeParser.Nonexistent") == nullptr;
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    hgl::logger::InitLogger(OS_TEXT("TestMaterialRecipeFile"));

    if (!CheckValidRecipe())
    {
        GLogError(u8"TestMaterialRecipeFile failed: valid recipe parsing");
        return 1;
    }
    if (!CheckUnknownKeyDiagnostic())
    {
        GLogError(u8"TestMaterialRecipeFile failed: unknown-key diagnostic");
        return 2;
    }
    if (!CheckInvalidTransformDiagnostic())
    {
        GLogError(u8"TestMaterialRecipeFile failed: invalid transform diagnostic");
        return 3;
    }
    if (!CheckPartialPipelineOverride())
    {
        GLogError(u8"TestMaterialRecipeFile failed: partial pipeline override");
        return 4;
    }
    if (!CheckMaterialDataIsRuntimeOnly())
    {
        GLogError(u8"TestMaterialRecipeFile failed: material data runtime-only rule");
        return 5;
    }
    if (!CheckRegistryNormalizationAndDuplicate())
    {
        GLogError(u8"TestMaterialRecipeFile failed: registry normalization/duplicate guard");
        return 6;
    }
    if (!CheckExampleRecipeFiles())
    {
        GLogError(u8"TestMaterialRecipeFile failed: example TOML asset parsing");
        return 7;
    }
    if (!CheckLookupFailures())
    {
        GLogError(u8"TestMaterialRecipeFile failed: lookup failure diagnostics");
        return 8;
    }
    if (!CheckFragmentOverrides())
    {
        GLogError(u8"TestMaterialRecipeFile failed: [fragment] override parsing");
        return 9;
    }
    if (!CheckNestedPipelineDiagnostic())
    {
        GLogError(u8"TestMaterialRecipeFile failed: nested table diagnostic path");
        return 10;
    }

    GLogInfo(u8"TestMaterialRecipeFile passed");
    return 0;
}
