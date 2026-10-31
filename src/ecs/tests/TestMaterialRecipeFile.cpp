#include <hgl/mtl/MaterialRecipeFile.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>
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
        "material_definition = \"UnlitTexture\"\n"
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
            && recipe.mtl_def_id == "unlit_texture"
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
            "material_definition = \"UnlitTexture\"\n"
            "matreial_definition = \"typo\"\n";

        constexpr char kUnknownNestedKeyRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.UnknownNestedKey\"\n"
            "material_definition = \"UnlitTexture\"\n"
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
            "material_definition = \"UnlitTexture\"\n"
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

    bool CheckPartialPipelineRejected()
    {
        constexpr char kPartialPipelineRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.PartialPipeline\"\n"
            "material_definition = \"UnlitTexture\"\n"
            "\n"
            "[render_state.pipeline]\n"
            "cull_mode = \"None\"\n";

        MaterialRecipeFileData data;
        return ParseMaterialRecipeFile(
                   kPartialPipelineRecipe,
                   static_cast<int>(strlen(kPartialPipelineRecipe)),
                   data)
            == MaterialRecipeFileParseResult::InvalidValue
            && !data.parse_error.IsEmpty();
    }

    bool CheckMaterialDataIsRuntimeOnly()
    {
        constexpr char kMaterialDataRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.MaterialData\"\n"
            "material_definition = \"Lit\"\n"
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

    bool CheckRegistryNormalizationAndDuplicate()
    {
        constexpr char kRegistryRecipe[] =
            "schema = 1\n"
            "name = \"RecipeParser.Registry\"\n"
            "material_definition = \"UnlitTexture\"\n";

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

        if (registry.RegisterRecipeSource(
                OS_TEXT("registry-duplicate"),
                kRegistryRecipe,
                static_cast<int>(strlen(kRegistryRecipe)),
                nullptr))
            return false;

        constexpr char kUnknownDefinition[] =
            "schema = 1\n"
            "name = \"RecipeParser.UnknownDefinition\"\n"
            "material_definition = \"MissingDefinition\"\n";
        if (registry.RegisterRecipeSource(
                OS_TEXT("registry-unknown-definition"),
                kUnknownDefinition,
                static_cast<int>(strlen(kUnknownDefinition)),
                nullptr))
            return false;

        constexpr char kUnknownSlot[] =
            "schema = 1\n"
            "name = \"RecipeParser.UnknownSlot\"\n"
            "material_definition = \"UnlitTexture\"\n"
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
            && registry.FindByName("RecipeParser.UnknownDefinition") == nullptr
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

        MaterialRecipeFileData pbr_spheres;
        if (!ParseFile(
                OS_TEXT("ShaderLibrary/asset/recipe/PBRSpheres.recipe.toml"),
                pbr_spheres)
         || pbr_spheres.recipe.recipe_name != "PBRSpheres.Lit"
         || !pbr_spheres.recipe.textures.empty()
         || !pbr_spheres.recipe.render_state_overrides.has_pipeline_config)
            return false;

        MaterialRecipeRegistry example_registry;
        int registered_file_count = 0;
        int error_count = 0;
        const bool loaded = example_registry.LoadDirectory(
                   OS_TEXT("ShaderLibrary/asset/recipe/Examples"),
                   nullptr,
                   &registered_file_count,
                   &error_count);
        if (!loaded || registered_file_count <= 0 || error_count != 0
         || example_registry.GetCount() != registered_file_count)
        {
            GLogError("example recipe registry mismatch: load=%d registered=%d errors=%d stored=%d",
                      loaded, registered_file_count, error_count,
                      example_registry.GetCount());
            return false;
        }
        return true;
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
    if (!CheckPartialPipelineRejected())
    {
        GLogError(u8"TestMaterialRecipeFile failed: incomplete pipeline rejection");
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

    GLogInfo(u8"TestMaterialRecipeFile passed");
    return 0;
}
