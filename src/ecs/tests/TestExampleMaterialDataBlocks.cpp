// TestExampleMaterialDataBlocks.cpp —— 阶段 A 的门禁：「示例的材质 SSBO 数据行搬进文件」。
//
// 不需要图形设备（只解析 + 校验，不上传显存）。检查三件事：
//
//   1. `ShaderLibrary/asset/material_data/` 下的每一个 `.material_data.toml` 都能解析，
//      且块名 == 文件名主干、行类型与行数 == 台账（本文件末尾的 kBlocks[]）；
//   2. 目录里没有多出/缺失的块文件（多一个少一个都会红 —— 示例与文件的对应关系是契约）；
//   3. 若干抽样行逐字段对值（搬家不许改画面上任何一位）。
//
// 第 3 条是重点：数据是脚本从原示例公式生成的，一旦有人手改数字，这里立刻红。

#include <hgl/filesystem/FileSystem.h>
#include <hgl/graph/asset/MaterialDataBlockSpec.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/mtl/MaterialDataBlockFile.h>
#include <hgl/type/String.h>

#include <cmath>

using namespace hgl;

namespace mtl = hgl::graph::mtl;
namespace graph = hgl::graph;

namespace
{
    int g_failed = 0;

    void Check(const bool condition, const char *what)
    {
        if (condition)
            return;

        GLogError(u8"TestExampleMaterialDataBlocks 失败: %s", what);
        ++g_failed;
    }

    const char *kDataDir = "ShaderLibrary/asset/material_data";

    constexpr float kEpsilon = 1.0e-6f;

    constexpr size_t kExpectedBlockCount = 29;  // 28 个示例块 + pbr_10x10_grid

    /// 阶段 A 的台账：文件名主干 → (行类型, 行数)。
    struct BlockExpect
    {
        const char *          stem;
        graph::GlobalSSBOType target;
        int                   rows;
    };

    const BlockExpect kBlocks[] =
    {
        { "simple_sphere",                 graph::GlobalSSBOType::PBRSurface,       1 },
        { "simple_cube",                   graph::GlobalSSBOType::EmissiveSurface, 1 },
        { "atmosphere_sky_ambient",        graph::GlobalSSBOType::PBRSurface,       1 },
        { "basic_lit_sun_direction",       graph::GlobalSSBOType::PBRSurface,       1 },
        { "mixed_buffer_meshes",           graph::GlobalSSBOType::PBRSurface,       1 },
        { "full_precision_lit_meshes",     graph::GlobalSSBOType::PBRSurface,       1 },
        { "ibl_environment",               graph::GlobalSSBOType::PBRSurface,       5 },
        { "compute_indirect_count",        graph::GlobalSSBOType::EmissiveSurface, 1 },
        { "debug_plane_grid_3d",           graph::GlobalSSBOType::EmissiveSurface, 3 },
        { "material_recipe_entry",         graph::GlobalSSBOType::EmissiveSurface, 1 },
        { "recursive_cube",                graph::GlobalSSBOType::EmissiveSurface, 1 },
        { "gizmo_usage",                   graph::GlobalSSBOType::EmissiveSurface, 2 },
        { "clock_use",                     graph::GlobalSSBOType::EmissiveSurface, 4 },
        { "ray_picking",                   graph::GlobalSSBOType::EmissiveSurface, 2 },
        { "alpha_test_shadow",             graph::GlobalSSBOType::PBRSurface,       1 },
        { "dual_world_shadows",            graph::GlobalSSBOType::PBRSurface,       1 },
        { "material_error_fallback",       graph::GlobalSSBOType::PBRSurface,       1 },
        { "single_sphere_material_switch", graph::GlobalSSBOType::PBRSurface,       2 },
        { "render_to_texture",             graph::GlobalSSBOType::PBRSurface,       2 },
        { "render_to_texture_color_depth", graph::GlobalSSBOType::PBRSurface,       2 },
        { "render_bound_box",              graph::GlobalSSBOType::EmissiveSurface, 16 },
        { "load_geometry",                 graph::GlobalSSBOType::EmissiveSurface, 9 },
        { "load_scene",                    graph::GlobalSSBOType::EmissiveSurface, 8 },
        { "auto_merge_material_instance",  graph::GlobalSSBOType::EmissiveSurface, 12 },
        { "gpu_driven_cull",               graph::GlobalSSBOType::EmissiveSurface, 64 },
        { "alpha_modes",                   graph::GlobalSSBOType::PBRSurface,      28 },
        { "coverage_variants",             graph::GlobalSSBOType::PBRSurface,      20 },
        { "cascade_shadow_map",            graph::GlobalSSBOType::PBRSurface,      11 },
        { "pbr_10x10_grid",                graph::GlobalSSBOType::PBRSurface,     100 },
    };

    bool ReadText(const OSString &path, AnsiString &out_text)
    {
        hgl::io::OpenFileInputStream fis(path);
        if (!fis || fis->GetSize() <= 0)
            return false;

        char chunk[4096];
        int64 got;
        while ((got = fis->Read(chunk, static_cast<int64>(sizeof(chunk)))) > 0)
            out_text.Strcat(chunk, static_cast<int>(got));

        return true;
    }

    bool Near(const float a, const float b)
    {
        return std::fabs(a - b) <= kEpsilon;
    }

    bool NearColor(const Color4f &c, const float r, const float g, const float b, const float a)
    {
        return Near(c.r, r) && Near(c.g, g) && Near(c.b, b) && Near(c.a, a);
    }

    OSString BlockPath(const char *stem)
    {
        OSString path;

        path  = ToOSString(kDataDir);
        path += OS_TEXT("/");
        path += ToOSString(stem);
        path += OS_TEXT(".material_data.toml");

        return path;
    }

    bool LoadSpec(const char *stem, graph::MaterialDataBlockSpec &out_spec)
    {
        AnsiString text;

        if (!ReadText(BlockPath(stem), text))
        {
            GLogError(u8"TestExampleMaterialDataBlocks: 读不到数据文件（需在仓库根目录运行）: %s",
                      stem);
            return false;
        }

        mtl::MaterialDataBlockFileData data;

        if (mtl::ParseMaterialDataBlockFile(text.c_str(), static_cast<int>(text.Length()),
                                            data, &out_spec)
            != mtl::MaterialDataBlockParseResult::OK)
        {
            GLogError(u8"TestExampleMaterialDataBlocks: 解析失败: %s", stem);
            return false;
        }

        return true;
    }

    /// 1. 目录里的块文件集合与台账一一对应（多一个少一个都算破坏契约）。
    bool TestDirectoryManifest()
    {
        ValueArray<filesystem::FileInfo> list;

        const int count = filesystem::GetFileInfoList(list, ToOSString(kDataDir),
                                                     false, true, false);

        if (count < 0)
        {
            GLogError(u8"TestExampleMaterialDataBlocks: 列不出数据目录（需在仓库根目录运行）: %s",
                      kDataDir);
            Check(false, "数据目录可枚举");
            return false;
        }

        const os_char *suffix = OS_TEXT(".material_data.toml");
        const int      suffix_len = static_cast<int>(hgl::strlen(suffix));

        size_t found = 0;

        for (int i = 0; i < count; ++i)
        {
            const os_char *name = list[i].name;
            const int      len  = static_cast<int>(hgl::strlen(name));

            if (len <= suffix_len || hgl::strcmp(name + len - suffix_len, suffix) != 0)
                continue;

            ++found;

            bool known = false;

            for (const BlockExpect &e : kBlocks)
            {
                OSString expected;

                expected  = ToOSString(e.stem);
                expected += OS_TEXT(".material_data.toml");

                if (hgl::strcmp(name, expected.c_str()) == 0)
                {
                    known = true;
                    break;
                }
            }

            if (!known)
                GLogError(u8"TestExampleMaterialDataBlocks: 台账里没有这个块文件");
        }

        Check(found == kExpectedBlockCount, "块文件数量与台账一致");

        return true;
    }

    /// 2. 每个块都能解析，且块名 / 行类型 / 行数与台账一致。
    bool TestEveryBlockShape()
    {
        for (const BlockExpect &e : kBlocks)
        {
            graph::MaterialDataBlockSpec spec;

            if (!LoadSpec(e.stem, spec))
            {
                Check(false, "块文件可解析");
                continue;
            }

            if (spec.name != e.stem)
                GLogError(u8"TestExampleMaterialDataBlocks: 块名与文件名不一致: %s -> %s",
                          e.stem, spec.name.c_str());

            Check(spec.name == e.stem, "块名 == 文件名主干");
            Check(spec.schema == 1, "schema 为 1");
            Check(spec.target == e.target, "行类型与台账一致");
            Check(int(spec.rows.GetCount()) == e.rows, "行数与台账一致");
        }

        return true;
    }

    /// 3a. 单行示例：逐字段对值。
    bool TestSingleRowValues()
    {
        {
            graph::MaterialDataBlockSpec spec;

            if (LoadSpec("simple_sphere", spec) && spec.rows.GetCount() == 1)
            {
                const graph::MaterialDataRowSpec &row = spec.rows[0];

                Check(NearColor(row.base_color, 1.0f, 1.0f, 1.0f, 1.0f), "simple_sphere 底色为白");
                Check(Near(row.metallic, 0.08f), "simple_sphere metallic 0.08");
                Check(Near(row.roughness, 0.92f), "simple_sphere roughness 0.92");
                Check(Near(row.normal_scale, 0.35f), "simple_sphere normal_scale 0.35");
            }
            else
            {
                Check(false, "simple_sphere 可读且单行");
            }
        }

        {
            graph::MaterialDataBlockSpec spec;

            // BlenderAxisBlue = (0x35,0x77,0xCD)
            if (LoadSpec("simple_cube", spec) && spec.rows.GetCount() == 1)
            {
                const graph::MaterialDataRowSpec &row = spec.rows[0];

                Check(NearColor(row.color, 53.0f / 255.0f, 119.0f / 255.0f,
                                205.0f / 255.0f, 1.0f),
                      "simple_cube 颜色为 Blender 轴蓝");
            }
            else
            {
                Check(false, "simple_cube 可读且单行");
            }
        }

        {
            graph::MaterialDataBlockSpec spec;

            if (LoadSpec("material_error_fallback", spec) && spec.rows.GetCount() == 1)
            {
                const graph::MaterialDataRowSpec &row = spec.rows[0];

                Check(NearColor(row.base_color, 0.85f, 0.72f, 0.45f, 1.0f),
                      "material_error_fallback 底色为暖砂");
                Check(Near(row.metallic, 0.05f), "material_error_fallback metallic 0.05");
                Check(Near(row.roughness, 0.85f), "material_error_fallback roughness 0.85");
            }
            else
            {
                Check(false, "material_error_fallback 可读且单行");
            }
        }

        return true;
    }

    /// 3b. 公式生成的块：抽样几行验证「不是手改出来的」。
    bool TestGeneratedBlockValues()
    {
        {
            graph::MaterialDataBlockSpec spec;

            if (LoadSpec("alpha_modes", spec) && spec.rows.GetCount() == 28)
            {
                // row * 7 + col，alpha = 1 - 0.15 * col
                for (int col = 0; col < 7; ++col)
                {
                    const graph::MaterialDataRowSpec &row = spec.rows[3 * 7 + col];

                    Check(Near(row.base_color.a, 1.0f - 0.15f * float(col)),
                          "alpha_modes alpha 逐列递减 0.15");
                    Check(Near(row.roughness, 0.75f), "alpha_modes roughness 0.75");
                    Check(Near(row.normal_scale, 0.0f), "alpha_modes normal_scale 0");
                }
            }
            else
            {
                Check(false, "alpha_modes 可读且 28 行");
            }
        }

        {
            graph::MaterialDataBlockSpec spec;

            if (LoadSpec("coverage_variants", spec) && spec.rows.GetCount() == 20)
            {
                Check(Near(spec.rows[0].roughness, 0.45f), "coverage_variants 行 0 粗糙度 0.45");
                Check(Near(spec.rows[10].roughness, 0.85f), "coverage_variants 行 1 粗糙度 0.85");
                Check(Near(spec.rows[9].metallic, 0.10f), "coverage_variants metallic 0.10");
                Check(Near(spec.rows[9].normal_scale, 0.35f), "coverage_variants normal_scale 0.35");
            }
            else
            {
                Check(false, "coverage_variants 可读且 20 行");
            }
        }

        {
            graph::MaterialDataBlockSpec spec;

            if (LoadSpec("cascade_shadow_map", spec) && spec.rows.GetCount() == 11)
            {
                for (int i = 0; i < 10; ++i)
                {
                    const graph::MaterialDataRowSpec &row = spec.rows[i];

                    Check(Near(row.metallic, 0.05f + 0.1f * float(i % 5)),
                          "cascade_shadow_map metallic = 0.05 + 0.1*(i%5)");
                    Check(Near(row.roughness, 0.2f + 0.08f * float(i)),
                          "cascade_shadow_map roughness = 0.2 + 0.08*i");
                    Check(Near(row.normal_scale, 0.5f), "cascade_shadow_map normal_scale 0.5");
                }

                const graph::MaterialDataRowSpec &ground = spec.rows[10];

                Check(NearColor(ground.base_color, 0.6f, 0.6f, 0.62f, 1.0f),
                      "cascade_shadow_map 地面底色 0.6/0.6/0.62");
                Check(Near(ground.roughness, 0.85f), "cascade_shadow_map 地面粗糙度 0.85");
            }
            else
            {
                Check(false, "cascade_shadow_map 可读且 11 行");
            }
        }

        {
            graph::MaterialDataBlockSpec spec;

            if (LoadSpec("gpu_driven_cull", spec) && spec.rows.GetCount() == 64)
            {
                // 颜色按 TestColor[8] 循环，第 0 项 = MozillaCharcoal (77,78,83)
                Check(NearColor(spec.rows[0].color, 77.0f / 255.0f, 78.0f / 255.0f,
                                83.0f / 255.0f, 1.0f),
                      "gpu_driven_cull 行 0 为 MozillaCharcoal");
                Check(NearColor(spec.rows[8].color, spec.rows[0].color.r, spec.rows[0].color.g,
                                spec.rows[0].color.b, 1.0f),
                      "gpu_driven_cull 颜色 8 行循环");
            }
            else
            {
                Check(false, "gpu_driven_cull 可读且 64 行");
            }
        }

        return true;
    }

    /// 3c. 多行示例的行序契约（行序错位画面上就是错的颜色）。
    bool TestRowOrder()
    {
        {
            graph::MaterialDataBlockSpec spec;

            if (LoadSpec("clock_use", spec) && spec.rows.GetCount() == 4)
            {
                Check(NearColor(spec.rows[0].color, 1.0f, 1.0f, 1.0f, 1.0f), "clock_use 行 0 为白");
                Check(NearColor(spec.rows[1].color, 1.0f, 0.0f, 0.0f, 1.0f), "clock_use 行 1 为红");
                Check(NearColor(spec.rows[2].color, 0.0f, 1.0f, 0.0f, 1.0f), "clock_use 行 2 为绿");
                Check(NearColor(spec.rows[3].color, 0.0f, 0.0f, 1.0f, 1.0f), "clock_use 行 3 为蓝");
            }
            else
            {
                Check(false, "clock_use 可读且 4 行");
            }
        }

        {
            graph::MaterialDataBlockSpec spec;

            if (LoadSpec("ray_picking", spec) && spec.rows.GetCount() == 2)
            {
                Check(NearColor(spec.rows[0].color, 1.0f, 1.0f, 1.0f, 1.0f), "ray_picking 行 0 为白");
                Check(NearColor(spec.rows[1].color, 1.0f, 1.0f, 0.0f, 1.0f), "ray_picking 行 1 为黄");
            }
            else
            {
                Check(false, "ray_picking 可读且 2 行");
            }
        }

        {
            graph::MaterialDataBlockSpec spec;

            if (LoadSpec("render_to_texture", spec) && spec.rows.GetCount() == 2)
            {
                // 行 0 = 球（SkyBlue 135,206,235），行 1 = 立方体（白）
                Check(NearColor(spec.rows[0].base_color, 135.0f / 255.0f, 206.0f / 255.0f,
                                235.0f / 255.0f, 1.0f),
                      "render_to_texture 行 0 为 SkyBlue");
                Check(NearColor(spec.rows[1].base_color, 1.0f, 1.0f, 1.0f, 1.0f),
                      "render_to_texture 行 1 为白");
            }
            else
            {
                Check(false, "render_to_texture 可读且 2 行");
            }
        }

        {
            graph::MaterialDataBlockSpec spec;

            if (LoadSpec("single_sphere_material_switch", spec) && spec.rows.GetCount() == 2)
            {
                for (int i = 0; i < 2; ++i)
                {
                    const graph::MaterialDataRowSpec &row = spec.rows[i];

                    Check(NearColor(row.base_color, 0.72f, 0.72f, 0.72f, 1.0f),
                          "single_sphere_material_switch 两行底色相同");
                    Check(Near(row.metallic, 0.15f), "single_sphere_material_switch metallic 0.15");
                    Check(Near(row.roughness, 0.25f), "single_sphere_material_switch roughness 0.25");
                }
            }
            else
            {
                Check(false, "single_sphere_material_switch 可读且 2 行");
            }
        }

        return true;
    }
}//namespace

int main(int, char **)
{
    TestDirectoryManifest();
    TestEveryBlockShape();
    TestSingleRowValues();
    TestGeneratedBlockValues();
    TestRowOrder();

    if (g_failed != 0)
    {
        GLogError(u8"TestExampleMaterialDataBlocks: %d 条断言失败", g_failed);
        return 1;
    }

    GLogInfo(u8"TestExampleMaterialDataBlocks: 全部通过");
    return 0;
}
