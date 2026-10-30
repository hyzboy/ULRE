#include <hgl/graph/asset/SceneAssetFile.h>
#include <hgl/graph/asset/SceneAssetRegistry.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cstring>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    constexpr char kValidScene[] =
        "schema = 1\n"
        "name = \"SceneTest\"\n"
        "\n"
        "[[materials]]\n"
        "name = \"lit\"\n"
        "type = \"PBRSurface\"\n"
        "base_color = [0.8, 0.7, 0.6, 1.0]\n"
        "metallic = 0.3\n"
        "\n"
        "[materials.textures]\n"
        "base_color = \"res/image/pbr/albedo.png\"\n"
        "normal = \"res/image/pbr/normal.png\"\n"
        "\n"
        "[[materials]]\n"
        "name = \"glow\"\n"
        "type = \"EmissiveSurface\"\n"
        "color = [1.0, 0.4, 0.2, 1.0]\n"
        "\n"
        "[[materials]]\n"
        "name = \"glass\"\n"
        "type = \"TransmissionSurface\"\n"
        "trans_color = [0.9, 0.95, 1.0, 0.6]\n"
        "\n"
        "[[nodes]]\n"
        "name = \"Root\"\n"
        "geometry = \"presets:sphere_64\"\n"
        "recipe = \"06b.BasicLit.Lit\"\n"
        "material = \"lit\"\n"
        "mobility = \"static\"\n"
        "\n"
        "[[nodes]]\n"
        "name = \"Child\"\n"
        "parent = \"Root\"\n"
        "geometry = \"presets:cube\"\n"
        "recipe = \"06b.BasicLit.Lit\"\n"
        "material = \"glow\"\n"
        "transform.position = [1.0, 2.0, 3.0]\n"
        "transform.rotation_axis = [0.0, 0.0, 1.0]\n"
        "transform.rotation_angle_deg = 30.0\n";

    bool Parse(const char *text, SceneSpec &spec)
    {
        return ParseSceneSpec(text, std::strlen(text), OS_TEXT("TestSceneFile"), spec);
    }

    bool TestValidScene()
    {
        SceneSpec spec;
        if (!Parse(kValidScene, spec))
            return false;

        if (spec.name != "SceneTest" || spec.schema != 1
            || spec.materials.GetCount() != 3 || spec.nodes.GetCount() != 2)
            return false;
        if (spec.materials[0]->type != SceneMaterialType::PBRSurface
            || spec.materials[1]->type != SceneMaterialType::EmissiveSurface
            || spec.materials[2]->type != SceneMaterialType::TransmissionSurface)
            return false;
        if (spec.materials[0]->textures.GetCount() != 2
            || spec.materials[0]->textures[0]->name != "base_color"
            || spec.materials[0]->textures[0]->uri != OS_TEXT("res/image/pbr/albedo.png")
            || !spec.materials[0]->textures[0]->required)
            return false;
        if (spec.nodes[0]->mobility != "static"
            || spec.nodes[1]->parent != "Root"
            || spec.nodes[1]->transform.position.y != 2.0f
            || spec.nodes[1]->transform.rotation_angle_deg != 30.0f)
            return false;
        return true;
    }

    bool TestInvalidScenes()
    {
        constexpr char kUnknownRootKey[] =
            "schema = 1\nname = \"Bad\"\nunknown = true\n"
            "[[nodes]]\nname = \"N\"\ngeometry = \"presets:cube\"\nrecipe = \"R\"\n";
        constexpr char kWrongMaterialKey[] =
            "schema = 1\nname = \"Bad\"\n"
            "[[materials]]\nname = \"m\"\ntype = \"EmissiveSurface\"\nmetallic = 0.5\n"
            "[[nodes]]\nname = \"N\"\ngeometry = \"presets:cube\"\nrecipe = \"R\"\nmaterial = \"m\"\n";
        constexpr char kDuplicateTextureSlot[] =
            "schema = 1\nname = \"Bad\"\n"
            "[[materials]]\nname = \"m\"\ntype = \"PBRSurface\"\n"
            "textures = [{ name = \"normal\", uri = \"a\" }, { name = \"normal\", uri = \"b\" }]\n"
            "[[nodes]]\nname = \"N\"\ngeometry = \"presets:cube\"\nrecipe = \"R\"\nmaterial = \"m\"\n";
        constexpr char kMissingReference[] =
            "schema = 1\nname = \"Bad\"\n"
            "[[nodes]]\nname = \"N\"\ngeometry = \"presets:cube\"\nrecipe = \"R\"\nmaterial = \"missing\"\n";
        constexpr char kUnsupportedGeometry[] =
            "schema = 1\nname = \"Bad\"\n"
            "[[nodes]]\nname = \"N\"\ngeometry = \"sphere_64\"\nrecipe = \"R\"\n";
        constexpr char kParentCycle[] =
            "schema = 1\nname = \"Bad\"\n"
            "[[nodes]]\nname = \"A\"\nparent = \"B\"\ngeometry = \"presets:cube\"\nrecipe = \"R\"\n"
            "[[nodes]]\nname = \"B\"\nparent = \"A\"\ngeometry = \"presets:cube\"\nrecipe = \"R\"\n";
        constexpr char kWrongVersion[] =
            "schema = 2\nname = \"Bad\"\n"
            "[[nodes]]\nname = \"N\"\ngeometry = \"presets:cube\"\nrecipe = \"R\"\n";

        SceneSpec spec;
        return !Parse(kUnknownRootKey, spec)
            && !Parse(kWrongMaterialKey, spec)
            && !Parse(kDuplicateTextureSlot, spec)
            && !Parse(kMissingReference, spec)
            && !Parse(kUnsupportedGeometry, spec)
            && !Parse(kParentCycle, spec)
            && !Parse(kWrongVersion, spec);
    }

    bool TestBasicLitMeshesSceneFile()
    {
        SceneSpec spec;
        if (!LoadSceneSpecFromTOML(
                OS_TEXT("example/Material/BasicLitMeshes.scene.toml"),
                spec))
            return false;

        return spec.name == "BasicLitMeshes"
            && spec.materials.GetCount() == 1
            && spec.materials[0]->textures.GetCount() == 3
            && spec.nodes.GetCount() == 12;
    }

    bool TestSceneAssetRegistry()
    {
        SceneAssetRegistry registry;
        SceneAsset *first = registry.GetOrLoad(
            OS_TEXT("example/Material/BasicLitMeshes.scene.toml"));
        SceneAsset *second = registry.GetOrLoad(
            OS_TEXT("example\\Material\\BasicLitMeshes.scene.toml"));

        return first
            && first == second
            && registry.GetCount() == 1
            && registry.FindByFilename(
                   OS_TEXT("example/Material/BasicLitMeshes.scene.toml")) == first;
    }

    bool TestSceneFlatBuffersFile()
    {
        const OSString filename =
            OS_TEXT("build/Testing/Temporary/scene_asset_roundtrip.scene.fb");
        hgl::filesystem::FileDelete(filename);

        SceneSpec source;
        if (!Parse(kValidScene, source))
            return false;
        if (!SaveSceneSpecToFlatBuffers(source, filename))
            return false;

        SceneSpec loaded;
        const bool load_ok = LoadSceneSpecFromFile(filename, loaded);
        hgl::filesystem::FileDelete(filename);
        return load_ok
            && loaded.name == source.name
            && loaded.materials.GetCount() == source.materials.GetCount()
            && loaded.materials[0]->textures.GetCount() == 2
            && loaded.nodes.GetCount() == source.nodes.GetCount();
    }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestSceneFile"));
    if (!TestValidScene()
        || !TestInvalidScenes()
        || !TestBasicLitMeshesSceneFile()
        || !TestSceneAssetRegistry()
        || !TestSceneFlatBuffersFile())
    {
        GLogError(u8"=== TestSceneFile: 测试失败 ===");
        return 1;
    }

    GLogInfo(u8"=== TestSceneFile: 全部测试通过 ===");
    return 0;
}
