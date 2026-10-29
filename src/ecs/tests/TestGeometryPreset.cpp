// TestGeometryPreset.cpp —— 预设库判据：示例用到的每个形状名都必须能从
// res/geo/presets/<name>.geo.toml 解析出来。这条用例不需要设备/窗口，
// 因此可以在 CI 里挡住「示例改了预设名、TOML 没跟上」这类静默失效。
#include <hgl/graph/asset/GeometryPreset.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    // 与 example/ 下 CreateGeometryPreset(pc, "...") 的实参一一对应；
    // 新增示例形状时改这里，用例会强制保证资产存在。
    constexpr const char *kPresetNames[] =
    {
        "plane_square",
        "sphere_12",
        "sphere_24",
        "sphere_48",
        "sphere_64",
        "dome_64",
        "cube",
        "cube_2x2x2",
        "cube_2x3x4",
        "cone",
        "cone_low",
        "cylinder",
        "cylinder_big",
        "cylinder_thin",
        "torus",
        "torus_ring",
        "torus_small",
        "torus_low",
        "hollow_cylinder",
        "hollow_cylinder_big",
        "hex_sphere",
        "hex_sphere_sky",
        "hex_sphere_far",
        "capsule",
        "tapered_capsule",
        "tapered_capsule_wide",
        "frustum",
        "plane_grid_lod",
        "plane_grid_pick",
        "bounding_box",
    };

    bool TestAllPresetsLoad()
    {
        GeometryPresetLibrary &library = GeometryPresetLibrary::Instance();

        bool ok = true;
        int count = 0;

        for (const char *name : kPresetNames)
        {
            const GeometryAssetSpec *spec = library.Get(name);

            if (!spec)
            {
                GLogError(u8"TestAllPresetsLoad: 预设缺失或解析失败：%s",
                          name);
                ok = false;
                continue;
            }

            if (spec->kind != GeometryAssetKind::Builtin)
            {
                GLogError(u8"TestAllPresetsLoad: 预设不是内置几何体：%s", name);
                ok = false;
                continue;
            }

            ++count;
        }

        if (count == 0)
            return false;

        GLogInfo(u8"TestAllPresetsLoad: %d 个预设全部解析成功", count);
        return ok;
    }
}

int main(int, char **)
{
    hgl::logger::InitLogger(OS_TEXT("TestGeometryPreset"));

    if (!TestAllPresetsLoad())
    {
        GLogError(u8"=== TestGeometryPreset: 存在失败项 ===");
        return 1;
    }

    GLogInfo(u8"=== TestGeometryPreset: 全部测试通过 ===");
    return 0;
}
