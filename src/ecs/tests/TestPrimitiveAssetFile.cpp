#include <hgl/graph/asset/PrimitiveAssetFile.h>
#include <hgl/graph/asset/PrimitiveAssetSpec.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <cstring>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    constexpr char kValidSingleToml[] =
        "schema = 1\n"
        "name = \"GoldSphere\"\n"
        "geometry = \"../geo/unit_sphere.geo.toml\"\n"
        "recipe = \"PBRSpheres.Lit\"\n"
        "primitive_type = \"triangles\"\n";

    constexpr char kValidVariantToml[] =
        "schema = 1\n"
        "name = \"GridDebug\"\n"
        "geometry = \"../geo/ground_grid.geo.toml\"\n"
        "recipe = \"LoadGeometry.DebugNormalColor\"\n"
        "primitive_type = \"lines\"\n"
        "\n"
        "[[variants]]\n"
        "recipe = \"LoadGeometry.Wire\"\n"
        "purpose = \"debug\"\n"
        "lod = 1\n";

    constexpr char kInvalidSurfaceVariantToml[] =
        "schema = 1\n"
        "name = \"BadVariant\"\n"
        "geometry = \"../geo/unit_cube.geo.toml\"\n"
        "recipe = \"PBRSpheres.Lit\"\n"
        "\n"
        "[[variants]]\n"
        "recipe = \"PBRSpheres.Lit\"\n"
        "purpose = \"surface\"\n";

    constexpr char kInvalidMissingRecipeToml[] =
        "schema = 1\n"
        "name = \"NoRecipe\"\n"
        "geometry = \"../geo/unit_cube.geo.toml\"\n";

    constexpr char kInvalidMissingGeometryToml[] =
        "schema = 1\n"
        "name = \"NoGeometry\"\n"
        "recipe = \"PBRSpheres.Lit\"\n";

    constexpr char kInvalidPrimitiveTypeToml[] =
        "schema = 1\n"
        "name = \"BadType\"\n"
        "geometry = \"../geo/unit_cube.geo.toml\"\n"
        "recipe = \"PBRSpheres.Lit\"\n"
        "primitive_type = \"quads\"\n";

    constexpr char kInvalidSchemaToml[] =
        "schema = 2\n"
        "name = \"FutureSchema\"\n"
        "geometry = \"../geo/unit_cube.geo.toml\"\n"
        "recipe = \"PBRSpheres.Lit\"\n";

    bool TestSingleVariant()
    {
        PrimitiveAssetSpec spec;

        if (!ParsePrimitiveAssetSpec(kValidSingleToml, strlen(kValidSingleToml), OS_TEXT("SingleVariant"), spec))
        {
            GLogError(u8"TestSingleVariant: 解析失败");
            return false;
        }

        if (spec.name != "GoldSphere") return false;
        if (spec.geometry_path != OS_TEXT("../geo/unit_sphere.geo.toml")) return false;
        if (spec.recipe_name != "PBRSpheres.Lit") return false;
        if (spec.primitive_type != PrimitiveType::Triangles) return false;
        if (spec.extra_variants.GetCount() != 0) return false;
        if (!spec.IsValid()) return false;

        return true;
    }

    bool TestExtraVariant()
    {
        PrimitiveAssetSpec spec;

        if (!ParsePrimitiveAssetSpec(kValidVariantToml, strlen(kValidVariantToml), OS_TEXT("ExtraVariant"), spec))
        {
            GLogError(u8"TestExtraVariant: 解析失败");
            return false;
        }

        if (spec.primitive_type != PrimitiveType::Lines) return false;
        if (spec.extra_variants.GetCount() != 1) return false;

        const PrimitiveVariantSpec *variant = spec.extra_variants.At(0);
        if (!variant) return false;
        if (variant->recipe_name != "LoadGeometry.Wire") return false;
        if (variant->purpose != PrimitiveVariantPurpose::Debug) return false;
        if (variant->lod_index != 1) return false;

        return true;
    }

    bool TestInvalidCases()
    {
        PrimitiveAssetSpec spec;

        if (ParsePrimitiveAssetSpec(kInvalidSurfaceVariantToml, strlen(kInvalidSurfaceVariantToml), OS_TEXT("SurfaceVariant"), spec))
        {
            GLogError(u8"TestInvalidCases: 额外变体用 surface 用途预期应失败");
            return false;
        }

        if (ParsePrimitiveAssetSpec(kInvalidMissingRecipeToml, strlen(kInvalidMissingRecipeToml), OS_TEXT("MissingRecipe"), spec))
        {
            GLogError(u8"TestInvalidCases: 缺 recipe 预期应失败");
            return false;
        }

        if (ParsePrimitiveAssetSpec(kInvalidMissingGeometryToml, strlen(kInvalidMissingGeometryToml), OS_TEXT("MissingGeometry"), spec))
        {
            GLogError(u8"TestInvalidCases: 缺 geometry 预期应失败");
            return false;
        }

        if (ParsePrimitiveAssetSpec(kInvalidPrimitiveTypeToml, strlen(kInvalidPrimitiveTypeToml), OS_TEXT("BadPrimitiveType"), spec))
        {
            GLogError(u8"TestInvalidCases: 未知 primitive_type 预期应失败");
            return false;
        }

        if (ParsePrimitiveAssetSpec(kInvalidSchemaToml, strlen(kInvalidSchemaToml), OS_TEXT("FutureSchema"), spec))
        {
            GLogError(u8"TestInvalidCases: 未知 schema 预期应失败");
            return false;
        }

        return true;
    }

    bool TestPrimitiveTypeNames()
    {
        // 名字表是 TOML 与诊断日志的单一真源，双向必须自洽。
        for (int i = int(PrimitiveType::BEGIN_RANGE); i <= int(PrimitiveType::END_RANGE); ++i)
        {
            const PrimitiveType pt = static_cast<PrimitiveType>(i);
            const char *name = GetPrimitiveTypeName(pt);

            if (!name || !*name)
                return false;

            if (ParsePrimitiveType(name) != pt)
            {
                GLogError(u8"TestPrimitiveTypeNames: '%s' 无法回读为原值", name);
                return false;
            }
        }

        return true;
    }
}

int main(int, char **)
{
    hgl::logger::InitLogger(OS_TEXT("TestPrimitiveAssetFile"));

    bool ok = true;
    ok = ok && TestSingleVariant();
    ok = ok && TestExtraVariant();
    ok = ok && TestInvalidCases();
    ok = ok && TestPrimitiveTypeNames();

    if (ok)
    {
        GLogInfo(u8"=== TestPrimitiveAssetFile: 全部测试通过 ===");
        return 0;
    }

    GLogError(u8"=== TestPrimitiveAssetFile: 存在测试失败 ===");
    return 1;
}
