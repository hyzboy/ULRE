#include <hgl/graph/asset/GeometryAssetFile.h>
#include <hgl/graph/asset/GeometryAssetSpec.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <cstring>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    constexpr char kValidSphereToml[] =
        "schema = 1\n"
        "name = \"UnitSphere\"\n"
        "type = \"builtin\"\n"
        "shape = \"sphere\"\n"
        "\n"
        "[builtin.sphere]\n"
        "slices = 48\n"
        "\n"
        "[vertex_format]\n"
        "normal = \"VF_V2UN8\"\n";

    constexpr char kValidCubeToml[] =
        "schema = 1\n"
        "name = \"CustomCube\"\n"
        "type = \"builtin\"\n"
        "shape = \"cube\"\n"
        "\n"
        "[builtin.cube]\n"
        "segments_x = 2\n"
        "segments_y = 3\n"
        "segments_z = 4\n"
        "ntb = \"normal_tangent\"\n"
        "tex_coord = false\n";

    constexpr char kValidPlaneGridToml[] =
        "schema = 1\n"
        "name = \"GroundGrid\"\n"
        "type = \"builtin\"\n"
        "shape = \"plane_grid\"\n"
        "\n"
        "[builtin.plane_grid]\n"
        "grid_x = 20\n"
        "grid_y = 30\n"
        "sub_x = 2\n"
        "sub_y = 4\n"
        "lum = 200\n"
        "sub_lum = 100\n"
        "is_3d = true\n";

    constexpr char kValidFileToml[] =
        "schema = 1\n"
        "name = \"StanfordBunny\"\n"
        "type = \"file\"\n"
        "file = \"res/mesh/bunny.geo\"\n"
        "\n"
        "[vertex_format]\n"
        "normal = \"VF_V3SN16\"\n";

    constexpr char kValidTorusToml[] =
        "schema = 1\n"
        "name = \"Donut\"\n"
        "type = \"builtin\"\n"
        "shape = \"torus\"\n"
        "\n"
        "[builtin.torus]\n"
        "inner_radius = 0.8\n"
        "outer_radius = 2.5\n"
        "slices = 64\n"
        "stacks = 32\n"
        "uv_scale_x = 2.0\n"
        "uv_scale_y = 3.0\n";

    constexpr char kInvalidUnknownShapeToml[] =
        "schema = 1\n"
        "name = \"Unknown\"\n"
        "type = \"builtin\"\n"
        "shape = \"hypercube\"\n";

    constexpr char kInvalidMissingFileToml[] =
        "schema = 1\n"
        "name = \"MissingFile\"\n"
        "type = \"file\"\n";

    bool TestSphereParse()
    {
        GeometryAssetSpec spec;
        if (!ParseGeometryAssetSpec(kValidSphereToml, strlen(kValidSphereToml), OS_TEXT("SphereTest"), spec))
        {
            GLogError(u8"TestSphereParse: 解析失败");
            return false;
        }

        if (spec.name != "UnitSphere") return false;
        if (spec.kind != GeometryAssetKind::Builtin) return false;
        if (spec.shape != BuiltinShapeKind::Sphere) return false;
        if (spec.params.sphere.slices != 48) return false;
        if (spec.normal_format != VF_V2UN8) return false;

        return true;
    }

    bool TestCubeParse()
    {
        GeometryAssetSpec spec;
        if (!ParseGeometryAssetSpec(kValidCubeToml, strlen(kValidCubeToml), OS_TEXT("CubeTest"), spec))
        {
            GLogError(u8"TestCubeParse: 解析失败");
            return false;
        }

        if (spec.name != "CustomCube") return false;
        if (spec.kind != GeometryAssetKind::Builtin) return false;
        if (spec.shape != BuiltinShapeKind::Cube) return false;
        if (spec.params.cube.segments_x != 2) return false;
        if (spec.params.cube.segments_y != 3) return false;
        if (spec.params.cube.segments_z != 4) return false;
        if (spec.params.cube.ntb != NTBType::NormalTangent) return false;
        if (spec.params.cube.tex_coord != false) return false;

        return true;
    }

    bool TestPlaneGridParse()
    {
        GeometryAssetSpec spec;
        if (!ParseGeometryAssetSpec(kValidPlaneGridToml, strlen(kValidPlaneGridToml), OS_TEXT("PlaneGridTest"), spec))
        {
            GLogError(u8"TestPlaneGridParse: 解析失败");
            return false;
        }

        if (spec.name != "GroundGrid") return false;
        if (spec.kind != GeometryAssetKind::Builtin) return false;
        if (spec.shape != BuiltinShapeKind::PlaneGrid) return false;
        if (spec.params.plane_grid.grid_x != 20) return false;
        if (spec.params.plane_grid.grid_y != 30) return false;
        if (spec.params.plane_grid.sub_x != 2) return false;
        if (spec.params.plane_grid.sub_y != 4) return false;
        if (spec.params.plane_grid.lum != 200) return false;
        if (spec.params.plane_grid.sub_lum != 100) return false;
        if (!spec.params.plane_grid.is_3d) return false;

        return true;
    }

    bool TestTorusParse()
    {
        GeometryAssetSpec spec;
        if (!ParseGeometryAssetSpec(kValidTorusToml, strlen(kValidTorusToml), OS_TEXT("TorusTest"), spec))
        {
            GLogError(u8"TestTorusParse: 解析失败");
            return false;
        }

        if (spec.name != "Donut") return false;
        if (spec.shape != BuiltinShapeKind::Torus) return false;
        if (spec.params.torus.inner_radius < 0.79f || spec.params.torus.inner_radius > 0.81f) return false;
        if (spec.params.torus.outer_radius < 2.49f || spec.params.torus.outer_radius > 2.51f) return false;
        if (spec.params.torus.slices != 64) return false;
        if (spec.params.torus.stacks != 32) return false;

        return true;
    }

    bool TestFileParse()
    {
        GeometryAssetSpec spec;
        if (!ParseGeometryAssetSpec(kValidFileToml, strlen(kValidFileToml), OS_TEXT("FileTest"), spec))
        {
            GLogError(u8"TestFileParse: 解析失败");
            return false;
        }

        if (spec.name != "StanfordBunny") return false;
        if (spec.kind != GeometryAssetKind::File) return false;
        if (spec.file_path != OS_TEXT("res/mesh/bunny.geo")) return false;
        if (spec.normal_format != VF_V3SN16) return false;

        return true;
    }

    bool TestInvalidCases()
    {
        GeometryAssetSpec spec;
        if (ParseGeometryAssetSpec(kInvalidUnknownShapeToml, strlen(kInvalidUnknownShapeToml), OS_TEXT("InvalidUnknownShape"), spec))
        {
            GLogError(u8"TestInvalidCases: 未知形状预期应失败，但返回成功");
            return false;
        }

        if (ParseGeometryAssetSpec(kInvalidMissingFileToml, strlen(kInvalidMissingFileToml), OS_TEXT("InvalidMissingFile"), spec))
        {
            GLogError(u8"TestInvalidCases: 文件类型缺失 file 路径预期应失败，但返回成功");
            return false;
        }

        return true;
    }
}

int main(int, char **)
{
    hgl::logger::InitLogger(OS_TEXT("TestGeometryAssetFile"));

    bool ok = true;
    ok = ok && TestSphereParse();
    ok = ok && TestCubeParse();
    ok = ok && TestPlaneGridParse();
    ok = ok && TestTorusParse();
    ok = ok && TestFileParse();
    ok = ok && TestInvalidCases();

    if (ok)
    {
        GLogInfo(u8"=== TestGeometryAssetFile: 全部测试通过 ===");
        return 0;
    }
    else
    {
        GLogError(u8"=== TestGeometryAssetFile: 存在测试失败 ===");
        return 1;
    }
}
