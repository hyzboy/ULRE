// TestMaterialDataBlockCodec.cpp —— 材质数据块（`*.material_data.toml` / `MDB1`）的契约门禁：
//
//   1. 元数据接线：字段数、结构大小、`rows` 的 `struct_meta` 递归指向行规约；
//   2. 授权行 → 物理行：逐字节等于 `MaterialDataRows.h` 的结构体（含缺省值）；
//   3. 三类内容行的行宽（PBR 32B / Emissive 16B / Transmission 16B）与打包口径
//      （`trans_color` 走 `packUnorm4x8`，保留位必须清零）；
//   4. 文本 → 规约 → 文本 往返后 payload 逐字节不变（往返不改变 GPU 字节）；
//   5. `MDB1` 二进制往返 + 视图零拷贝（`rows` 指向调用方缓冲区内部）；
//   6. 二进制坏输入（magic / 版本 / 行宽 / 截断 / 行数）必须被拒绝；
//   7. 文本坏输入（非内容行 target / 缺 target / schema 不符 / 跨类型行键 / 空 rows）
//      必须报错而不是静默忽略。
//
// 纯数据往返，不依赖设备 / 窗口，因此在 CI 里也能跑。
#include <hgl/asset/AssetObject.h>
#include <hgl/asset/generated/MaterialDataBlockMeta.gen.h>
#include <hgl/asset/generated/MaterialDataBlockSerialize.gen.h>
#include <hgl/graph/asset/MaterialDataBlockSpec.h>
#include <hgl/graph/ssbo/MaterialDataRows.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/mtl/MaterialDataBlock.h>
#include <hgl/mtl/MaterialDataBlockFile.h>
#include <hgl/mtl/MaterialDataBlockFlat.h>
#include <hgl/serialize/Archive.h>
#include <hgl/serialize/TomlCodec.h>

#include <cstring>

using namespace hgl;
using namespace hgl::graph;
namespace codec = hgl::asset::generated;
namespace mtl = hgl::graph::mtl;
namespace ssbo = hgl::graph::ssbo;

namespace
{
    int g_failed = 0;

    void Check(const bool condition, const char *what)
    {
        if (condition)
            return;

        GLogError(u8"TestMaterialDataBlockCodec 失败: %s", what);
        ++g_failed;
    }

    bool Parse(const char *text, mtl::MaterialDataBlockFileData &out,
               MaterialDataBlockSpec *spec = nullptr)
    {
        const auto size = static_cast<int>(strlen(text));
        return mtl::ParseMaterialDataBlockFile(text, size, out, spec)
            == mtl::MaterialDataBlockParseResult::OK;
    }

    /// 期望某段文本解析失败并得到指定结果类型。
    bool Rejected(const char *text, const mtl::MaterialDataBlockParseResult expected)
    {
        mtl::MaterialDataBlockFileData data;
        const auto size = static_cast<int>(strlen(text));
        const auto result = mtl::ParseMaterialDataBlockFile(text, size, data);
        return result == expected;
    }

    const char *kPbrBlock =
        "schema = 1\n"
        "name = \"pbr_grid\"\n"
        "target = \"PBRSurface\"\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.0, 0.0, 0.0, 1.0]\n"
        "metallic = 0.0\n"
        "roughness = 0.05\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.5, 0.5, 0.5, 1.0]\n"
        "metallic = 0.5\n"
        "roughness = 0.5\n"
        "\n"
        "[[rows]]\n"
        "base_color = [1.0, 1.0, 1.0, 1.0]\n"
        "metallic = 1.0\n"
        "roughness = 1.0\n";

    bool TestMetaWiring()
    {
        // 行规约是 7 个并集字段；块规约是 schema/name/target/rows 四个。
        if (codec::Class_MaterialDataRowSpec.property_count != 7)
            return false;
        if (codec::Class_MaterialDataBlockSpec.property_count != 4)
            return false;

        if (codec::Class_MaterialDataRowSpec.struct_size != sizeof(MaterialDataRowSpec))
            return false;
        if (codec::Class_MaterialDataBlockSpec.struct_size != sizeof(MaterialDataBlockSpec))
            return false;

        // rows 的 struct_meta 必须递归指向行规约：严格键校验与 FlatBuffers 元素
        // 类别判定都靠它。
        const auto &rows_meta = codec::Meta_MaterialDataBlockSpec[3];
        return rows_meta.kind == hgl::asset::PropertyKind::ValueArray
            && rows_meta.struct_meta == &codec::Class_MaterialDataRowSpec
            && rows_meta.elem_kind == hgl::asset::PropertyKind::Struct;
    }

    bool TestStrideTable()
    {
        return GetGlobalSSBOTypeStructStride(GlobalSSBOType::PBRSurface) == sizeof(ssbo::PBRSurfaceRow)
            && GetGlobalSSBOTypeStructStride(GlobalSSBOType::EmissiveSurface) == sizeof(ssbo::EmissiveSurfaceRow)
            && GetGlobalSSBOTypeStructStride(GlobalSSBOType::TransmissionSurface) == sizeof(ssbo::TransmissionSurfaceRow)
            // 组件行与绘制参数行都不是「可摊平的内容行」。
            && GetGlobalSSBOTypeStructStride(GlobalSSBOType::MaterialData) == 0
            && GetGlobalSSBOTypeStructStride(GlobalSSBOType::MeshDrawParams) == 0
            && sizeof(ssbo::PBRSurfaceRow) % 16 == 0
            && sizeof(ssbo::EmissiveSurfaceRow) % 16 == 0
            && sizeof(ssbo::TransmissionSurfaceRow) % 16 == 0;
    }

    bool TestPbrBlock()
    {
        mtl::MaterialDataBlockFileData data;
        if (!Parse(kPbrBlock, data))
            return false;

        const auto &block = data.block;
        if (block.type != GlobalSSBOType::PBRSurface)
            return false;
        if (block.row_bytes != 32)
            return false;
        if (block.GetRowCount() != 3 || block.GetByteSize() != 96)
            return false;

        const auto *rows = reinterpret_cast<const ssbo::PBRSurfaceRow *>(block.GetData());

        // 第 1 行：显式全写。
        if (rows[0].metallic != 0.0f || rows[0].roughness != 0.05f)
            return false;
        if (rows[2].metallic != 1.0f || rows[2].roughness != 1.0f)
            return false;
        if (rows[1].base_color.r != 0.5f || rows[1].base_color.a != 1.0f)
            return false;

        // 第 1、2 行没写 normal_scale / fresnel ⇒ 必须落到物理行的类内缺省，
        // 与 C++ 直接构造该行逐字节相同。
        const ssbo::PBRSurfaceRow reference;
        for (int i = 0; i < 3; ++i)
        {
            if (rows[i].normal_scale != reference.normal_scale)
                return false;
            if (rows[i].fresnel != reference.fresnel)
                return false;
        }

        return true;
    }

    bool TestEmissiveAndTransmission()
    {
        const char *emissive =
            "schema = 1\n"
            "name = \"glow\"\n"
            "target = \"EmissiveSurface\"\n"
            "\n"
            "[[rows]]\n"
            "color = [1.0, 0.5, 0.25, 1.0]\n";

        mtl::MaterialDataBlockFileData emissive_data;
        if (!Parse(emissive, emissive_data))
            return false;
        if (emissive_data.block.row_bytes != 16 || emissive_data.block.GetRowCount() != 1)
            return false;

        const auto *erow = reinterpret_cast<const ssbo::EmissiveSurfaceRow *>(
            emissive_data.block.GetData());
        if (erow[0].color.r != 1.0f || erow[0].color.g != 0.5f)
            return false;

        const char *transmission =
            "schema = 1\n"
            "name = \"glass\"\n"
            "target = \"TransmissionSurface\"\n"
            "\n"
            "[[rows]]\n"
            "trans_color = [0.25, 0.5, 0.75, 1.0]\n";

        mtl::MaterialDataBlockFileData transmission_data;
        if (!Parse(transmission, transmission_data))
            return false;
        if (transmission_data.block.row_bytes != 16
            || transmission_data.block.GetRowCount() != 1)
            return false;

        const auto *trow = reinterpret_cast<const ssbo::TransmissionSurfaceRow *>(
            transmission_data.block.GetData());

        // 授权侧是 `Color4f`，物理侧是打包后的 `uint32`：转换只在这一处发生。
        const Color4f authored(0.25f, 0.5f, 0.75f, 1.0f);
        if (trow[0].trans_color != authored.packUnorm4x8())
            return false;

        // 保留位必须清零（否则跨平台/跨版本比较会漂）。
        return trow[0].reserved0[0] == 0
            && trow[0].reserved0[1] == 0
            && trow[0].reserved0[2] == 0;
    }

    bool TestTextRoundTrip()
    {
        mtl::MaterialDataBlockFileData first;
        MaterialDataBlockSpec spec;
        if (!Parse(kPbrBlock, first, &spec))
            return false;

        // 规约 → 文本 → 规约：往返不得改变落到 GPU 的字节。
        AnsiString text;
        if (!mtl::WriteMaterialDataBlockToml(spec, text))
            return false;

        mtl::MaterialDataBlockFileData second;
        if (!Parse(text.c_str(), second))
        {
            GLogError(u8"往返文本无法解析:\n%s", text.c_str());
            return false;
        }

        if (second.block.type != first.block.type
            || second.block.row_bytes != first.block.row_bytes
            || second.block.GetRowCount() != first.block.GetRowCount())
        {
            GLogError(u8"往返文本结构漂移:\n%s", text.c_str());
            return false;
        }

        if (memcmp(second.block.GetData(), first.block.GetData(),
                   first.block.GetByteSize()) != 0)
        {
            const float *a = reinterpret_cast<const float *>(first.block.GetData());
            const float *b = reinterpret_cast<const float *>(second.block.GetData());
            const int count = first.block.GetByteSize() / 4;
            for (int i = 0; i < count; ++i)
            {
                if (a[i] != b[i])
                    GLogError(u8"第 %d 个 float 漂移: %.9g -> %.9g\n%s",
                              i, a[i], b[i], text.c_str());
            }
            return false;
        }

        return true;
    }

    bool TestBinaryRoundTrip()
    {
        mtl::MaterialDataBlockFileData data;
        if (!Parse(kPbrBlock, data))
            return false;

        ValueArray<uint8> bytes;
        if (!mtl::WriteMaterialDataBlockBinary(data.block, bytes))
            return false;

        if (bytes.GetCount() != static_cast<int>(mtl::kMaterialDataBlockBinaryHeaderSize)
                                    + data.block.GetByteSize())
            return false;

        // 视图是零拷贝：行区指针必须落在调用方缓冲区内部。
        mtl::MaterialDataBlockView view;
        AnsiString error;
        if (!mtl::ViewMaterialDataBlockBinary(bytes.GetData(),
                                              static_cast<uint64>(bytes.GetCount()),
                                              view, error))
            return false;

        if (view.rows != bytes.GetData() + mtl::kMaterialDataBlockBinaryHeaderSize)
            return false;
        if (view.type != data.block.type || view.row_count != data.block.GetRowCount())
            return false;

        mtl::MaterialDataBlock decoded;
        if (!mtl::ReadMaterialDataBlockBinary(bytes.GetData(),
                                              static_cast<uint64>(bytes.GetCount()),
                                              decoded, error))
            return false;

        return decoded.type == data.block.type
            && decoded.row_bytes == data.block.row_bytes
            && decoded.GetByteSize() == data.block.GetByteSize()
            && memcmp(decoded.GetData(), data.block.GetData(),
                      data.block.GetByteSize()) == 0;
    }

    bool TestBinaryRejectsBadHeader()
    {
        mtl::MaterialDataBlockFileData data;
        if (!Parse(kPbrBlock, data))
            return false;

        ValueArray<uint8> good;
        if (!mtl::WriteMaterialDataBlockBinary(data.block, good))
            return false;

        const auto size = static_cast<uint64>(good.GetCount());
        mtl::MaterialDataBlockView view;
        AnsiString error;

        // magic 损坏。
        ValueArray<uint8> broken = good;
        broken.GetData()[0] = 'X';
        if (mtl::ViewMaterialDataBlockBinary(broken.GetData(), size, view, error))
            return false;

        // 版本不受支持。
        broken = good;
        broken.GetData()[4] = 2;
        if (mtl::ViewMaterialDataBlockBinary(broken.GetData(), size, view, error))
            return false;

        // 行宽与类型的物理行宽不符（32 → 16）。
        broken = good;
        broken.GetData()[8] = 16;
        if (mtl::ViewMaterialDataBlockBinary(broken.GetData(), size, view, error))
            return false;

        // 行区被截断。
        if (mtl::ViewMaterialDataBlockBinary(good.GetData(), size - 1, view, error))
            return false;

        // 行数为 0。
        broken = good;
        broken.GetData()[12] = 0;
        broken.GetData()[13] = 0;
        broken.GetData()[14] = 0;
        broken.GetData()[15] = 0;
        return !mtl::ViewMaterialDataBlockBinary(broken.GetData(), size, view, error);
    }

    bool TestTextRejects()
    {
        // 组件行 / 绘制行不是内容行，不能作为数据块 target。
        if (!Rejected("schema = 1\nname = \"x\"\ntarget = \"MaterialData\"\n"
                      "\n[[rows]]\nmetallic = 0.5\n",
                      mtl::MaterialDataBlockParseResult::InvalidValue))
            return false;

        // target 必须显式声明：一个「什么都没写就默认 PBR」的块错起来无声无息。
        if (!Rejected("schema = 1\nname = \"x\"\n\n[[rows]]\nmetallic = 0.5\n",
                      mtl::MaterialDataBlockParseResult::InvalidValue))
            return false;

        // schema 只接受 1。
        if (!Rejected("schema = 2\nname = \"x\"\ntarget = \"PBRSurface\"\n"
                      "\n[[rows]]\nmetallic = 0.5\n",
                      mtl::MaterialDataBlockParseResult::InvalidValue))
            return false;

        // 块级未知键。
        if (!Rejected("schema = 1\nname = \"x\"\ntarget = \"PBRSurface\"\n"
                      "rows_count = 3\n",
                      mtl::MaterialDataBlockParseResult::UnknownKey))
            return false;

        // 跨类型行键：给 PBR 块写 trans_color 必须报错，不能静默忽略。
        if (!Rejected("schema = 1\nname = \"x\"\ntarget = \"PBRSurface\"\n"
                      "\n[[rows]]\ntrans_color = [1.0, 1.0, 1.0, 1.0]\n",
                      mtl::MaterialDataBlockParseResult::UnknownKey))
            return false;

        // 行级未知键。
        if (!Rejected("schema = 1\nname = \"x\"\ntarget = \"PBRSurface\"\n"
                      "\n[[rows]]\nmetallic_v2 = 0.5\n",
                      mtl::MaterialDataBlockParseResult::UnknownKey))
            return false;

        // 空 rows / 空 name。
        if (!Rejected("schema = 1\nname = \"x\"\ntarget = \"PBRSurface\"\n",
                      mtl::MaterialDataBlockParseResult::InvalidValue))
            return false;

        // 类型不符（metallic 写成字符串）必须失败。
        return Rejected("schema = 1\nname = \"x\"\ntarget = \"PBRSurface\"\n"
                        "\n[[rows]]\nmetallic = \"high\"\n",
                        mtl::MaterialDataBlockParseResult::InvalidValue);
    }

    bool TestBuildRejectsNonContentRow()
    {
        MaterialDataBlockSpec spec;
        spec.schema = 1;
        spec.name = "bad";
        spec.target = GlobalSSBOType::MaterialData;
        spec.rows.Resize(1);

        mtl::MaterialDataBlock block;
        return !mtl::BuildMaterialDataBlock(spec, block);
    }

    /// 双后端一致性：TOML 直通得到的行字节，必须与「TOML → 规约 → FlatBuffers
    /// → 规约 → 摊平」逐字节相同。这是「批量生成用 FB、调试用手写 TOML」两条
    /// 路可以互相替换的前提。
    bool TestFlatBuffersParity()
    {
        mtl::MaterialDataBlockFileData direct;
        MaterialDataBlockSpec spec;
        if (!Parse(kPbrBlock, direct, &spec))
            return false;

        ValueArray<uint8> flat;
        AnsiString error;
        if (!mtl::EncodeMaterialDataBlockFlat(spec, flat, &error))
            return false;
        if (flat.GetCount() <= 0)
            return false;

        MaterialDataBlockSpec decoded;
        if (!mtl::DecodeMaterialDataBlockFlat(flat.GetData(),
                                              static_cast<uint64>(flat.GetCount()),
                                              decoded, &error))
            return false;

        // 规约层必须无损（含块名与 target）。
        if (decoded.name != spec.name || decoded.target != spec.target)
            return false;
        if (decoded.rows.GetCount() != spec.rows.GetCount())
            return false;

        // 物理层必须逐字节相同。
        mtl::MaterialDataBlock via_flat;
        if (!mtl::BuildMaterialDataBlock(decoded, via_flat))
            return false;

        if (via_flat.row_bytes != direct.block.row_bytes
            || via_flat.GetRowCount() != direct.block.GetRowCount())
            return false;

        return memcmp(via_flat.GetData(), direct.block.GetData(),
                      direct.block.GetByteSize()) == 0;
    }

    /// 一步到位入口：缓冲 → 物理块，结果与两段式完全相同。
    bool TestFlatBuffersDirectBuild()
    {
        mtl::MaterialDataBlockFileData direct;
        MaterialDataBlockSpec spec;
        if (!Parse(kPbrBlock, direct, &spec))
            return false;

        ValueArray<uint8> flat;
        if (!mtl::EncodeMaterialDataBlockFlat(spec, flat))
            return false;

        mtl::MaterialDataBlock block;
        AnsiString error;
        if (!mtl::BuildMaterialDataBlockFromFlat(flat.GetData(),
                                                 static_cast<uint64>(flat.GetCount()),
                                                 block, &error))
            return false;

        return block.type == direct.block.type
            && memcmp(block.GetData(), direct.block.GetData(),
                      direct.block.GetByteSize()) == 0;
    }

    bool TestFlatBuffersRejects()
    {
        mtl::MaterialDataBlockFileData good;
        MaterialDataBlockSpec spec;
        if (!Parse(kPbrBlock, good, &spec))
            return false;

        ValueArray<uint8> flat;
        if (!mtl::EncodeMaterialDataBlockFlat(spec, flat))
            return false;

        MaterialDataBlockSpec decoded;
        AnsiString error;

        // 截断缓冲：校验阶段必须挡住，不能读到越界。
        const uint64 truncated = static_cast<uint64>(flat.GetCount()) / 2;
        if (mtl::DecodeMaterialDataBlockFlat(flat.GetData(), truncated, decoded, &error))
            return false;

        // 根偏移被破坏。
        ValueArray<uint8> broken = flat;
        broken.GetData()[0] = 0xFF;
        broken.GetData()[1] = 0xFF;
        broken.GetData()[2] = 0xFF;
        broken.GetData()[3] = 0x7F;
        if (mtl::DecodeMaterialDataBlockFlat(broken.GetData(),
                                             static_cast<uint64>(broken.GetCount()),
                                             decoded, &error))
            return false;

        // 组装行不是内容行 ⇒ 写出口就拒绝，不把问题推到运行时。
        MaterialDataBlockSpec bad;
        bad.schema = 1;
        bad.name = "bad";
        bad.target = GlobalSSBOType::MaterialData;
        bad.rows.Resize(1);

        return !mtl::EncodeMaterialDataBlockFlat(bad, flat, &error);
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestMaterialDataBlockCodec"));

    Check(TestMetaWiring(), "元数据接线");
    Check(TestStrideTable(), "内容行行宽表");
    Check(TestPbrBlock(), "PBR 块摊平");
    Check(TestEmissiveAndTransmission(), "Emissive / Transmission 行");
    Check(TestTextRoundTrip(), "文本往返不改变 GPU 字节");
    Check(TestBinaryRoundTrip(), "MDB1 二进制往返 + 视图零拷贝");
    Check(TestBinaryRejectsBadHeader(), "MDB1 坏头必须被拒绝");
    Check(TestTextRejects(), "文本坏输入必须报错");
    Check(TestBuildRejectsNonContentRow(), "非内容行不能构建数据块");
    Check(TestFlatBuffersParity(), "TOML / FlatBuffers 双后端行字节一致");
    Check(TestFlatBuffersDirectBuild(), "缓冲一步摊平");
    Check(TestFlatBuffersRejects(), "FlatBuffers 坏输入必须被拒绝");

    if (g_failed > 0)
    {
        GLogError(u8"=== TestMaterialDataBlockCodec: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestMaterialDataBlockCodec: 全部测试通过 ===");
    return 0;
}
