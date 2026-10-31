// TestPbrGridDataBlock.cpp —— 「示例把批量数据行搬进文件」这件事的门禁（无需图形设备）：
//
//   1. `PBRSpheres` 用的那份 `.material_data.toml` 真能读（文件名 / 块名 / 行数）；
//   2. 每一行的数值都与原先生成公式一致（col→metallic，row→roughness，统一底色，
//      统一 normal_scale）—— 搬家不许改画面；
//   3. `PBRSpheres.cpp` 里已经没有逐行硬编码，只剩一条 URI 引用。
//
// 第 2 条是重点：文件是程序生成的，一旦有人手改数字，这里立刻红。

#include <hgl/filesystem/FileSystem.h>
#include <hgl/graph/asset/MaterialDataBlockAsset.h>
#include <hgl/graph/asset/MaterialDataBlockSpec.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/mtl/MaterialDataBlockFile.h>

#include <cmath>
#include <cstring>

using namespace hgl;

namespace mtl = hgl::graph::mtl;

namespace
{
    int g_failed = 0;

    void Check(const bool condition, const char *what)
    {
        if (condition)
            return;

        GLogError(u8"TestPbrGridDataBlock 失败: %s", what);
        ++g_failed;
    }

    /// 示例与数据文件之间的**唯一**约定，改这里就要改演示代码。
    const char *kGridDataPath =
        "ShaderLibrary/asset/material_data/pbr_10x10_grid.material_data.toml";

    const char *kDemoSourcePath = "example/Material/PBRSpheres.cpp";

    constexpr int   kGridSize       = 10;
    constexpr int   kRowCount       = kGridSize * kGridSize;
    constexpr float kBaseColor      = 0.72f;
    constexpr float kNormalScale    = 0.35f;
    constexpr float kEpsilon        = 1.0e-5f;

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

    /// 原 `CreateLitMaterialData()` 的公式，逐字搬过来当参照物。
    float ExpectedMetallic(const int col)
    {
        return float(col) / float(kGridSize - 1);
    }

    float ExpectedRoughness(const int row)
    {
        return 0.05f + float(row) / float(kGridSize - 1) * 0.95f;
    }

    bool LoadGridSpec(hgl::graph::MaterialDataBlockSpec &out_spec)
    {
        AnsiString text;
        if (!ReadText(ToOSString(kGridDataPath), text))
        {
            GLogError(u8"TestPbrGridDataBlock: 读不到数据文件（需在仓库根目录运行）: %s",
                      kGridDataPath);
            return false;
        }

        mtl::MaterialDataBlockFileData data;
        if (mtl::ParseMaterialDataBlockFile(text.c_str(), static_cast<int>(text.Length()),
                                            data, &out_spec)
            != mtl::MaterialDataBlockParseResult::OK)
        {
            GLogError(u8"TestPbrGridDataBlock: 数据文件解析失败");
            return false;
        }

        return true;
    }

    /// 1. 块的形状：类型、名字、行数。
    bool TestShape()
    {
        hgl::graph::MaterialDataBlockSpec spec;
        if (!LoadGridSpec(spec))
            return false;

        Check(spec.schema == 1, "schema 为 1");
        Check(spec.name == "pbr_10x10_grid", "块名为 pbr_10x10_grid");
        Check(spec.target == hgl::graph::GlobalSSBOType::PBRSurface, "行类型为 PBRSurface");
        Check(spec.rows.GetCount() == kRowCount, "行数为 100（10x10）");

        return true;
    }

    /// 2. 每一行都是生成公式的值（行优先：index = row * 10 + col）。
    bool TestRowValues()
    {
        hgl::graph::MaterialDataBlockSpec spec;
        if (!LoadGridSpec(spec))
            return false;

        int bad_index = -1;
        const char *bad_field = nullptr;

        for (int row = 0; row < kGridSize && !bad_field; ++row)
        {
            for (int col = 0; col < kGridSize && !bad_field; ++col)
            {
                const int index = row * kGridSize + col;
                const auto &r   = spec.rows[index];

                if (!Near(r.base_color.x, kBaseColor)
                 || !Near(r.base_color.y, kBaseColor)
                 || !Near(r.base_color.z, kBaseColor)
                 || !Near(r.base_color.w, 1.0f))
                    bad_field = "base_color";
                else if (!Near(r.metallic, ExpectedMetallic(col)))
                    bad_field = "metallic";
                else if (!Near(r.roughness, ExpectedRoughness(row)))
                    bad_field = "roughness";
                else if (!Near(r.normal_scale, kNormalScale))
                    bad_field = "normal_scale";

                if (bad_field)
                    bad_index = index;
            }
        }

        if (bad_field)
        {
            GLogError(u8"TestPbrGridDataBlock: 第 %d 行的 %s 与生成公式不符",
                      bad_index, bad_field);
            return false;
        }

        return true;
    }

    /// 去掉行注释后再做「不得出现 X」的断言 —— 注释里可以留历史说明，代码里不行。
    AnsiString StripLineComments(const AnsiString &src)
    {
        AnsiString out;
        const char *p = src.c_str();

        while (*p)
        {
            if (p[0] == '/' && p[1] == '/')
            {
                while (*p && *p != '\n')
                    ++p;

                continue;
            }

            out.Strcat(p, 1);
            ++p;
        }

        return out;
    }

    /// 3. 演示代码不再有逐行硬编码，只留一条 URI 引用。
    bool TestDemoSourceContracts()
    {
        AnsiString src;
        if (!ReadText(ToOSString(kDemoSourcePath), src))
        {
            GLogError(u8"TestPbrGridDataBlock: 读不到演示源码（需在仓库根目录运行）: %s",
                      kDemoSourcePath);
            return false;
        }

        const AnsiString code = StripLineComments(src);
        const char *text = code.c_str();

        Check(strstr(text, "CreateLitMaterialData") == nullptr,
              "演示里已删除逐行生成的 CreateLitMaterialData()");

        Check(strstr(text, "sphere_material_data") == nullptr,
              "演示里已删除 100 行的 sphere_material_data 数组");

        Check(strstr(text, "sphere_slot_accessors") == nullptr,
              "演示里已删除 100 个 MaterialDataAccessor");

        // 数据文件路径与 URI 引用都在注释外（真代码）。
        const char *raw = src.c_str();

        Check(strstr(raw, kGridDataPath) != nullptr,
              "演示引用了数据文件路径");

        Check(strstr(text, ".WithMaterialDataRow(") != nullptr,
              "演示走 WithMaterialDataRow（URI + 行下标）");

        // 行下标必须是 row * GRID_SIZE + col —— 与文件的「行优先」约定对齐。
        Check(strstr(text, "row * GRID_SIZE + col") != nullptr,
              "行下标 = row * GRID_SIZE + col");

        Check(strstr(text, ".WithMaterialRow(") == nullptr,
              "演示不再逐格传访问器 + 行数据");

        return true;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestPbrGridDataBlock"));

    Check(TestShape(), "数据文件的基本形状");
    Check(TestRowValues(), "100 行数值与生成公式一致");
    Check(TestDemoSourceContracts(), "演示源码契约");

    if (g_failed > 0)
    {
        GLogError(u8"TestPbrGridDataBlock: %d 项失败", g_failed);
        return 1;
    }

    GLogInfo(u8"TestPbrGridDataBlock: 全部通过");
    return 0;
}
