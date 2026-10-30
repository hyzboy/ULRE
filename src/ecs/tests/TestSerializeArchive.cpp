// TestSerializeArchive.cpp —— 统一序列化框架（Unified Archive）门禁：
//
//   1. Value 树：标量 / 字符串 / 数组 / 表的读写与深拷贝；
//   2. Archive：标量、字符串、枚举、数学向量、嵌套结构、Sequence / Mapping /
//      Set / Tree 的双向遍历；
//   3. TOML 后端：Value ↔ toml::value 往返，文本输出可再解析；
//   4. 缺键语义：读方向缺字段必须保留 C++ 类内缺省值而不是报错；
//   5. 类型不符：必须失败并给出字段名。
//
// 本用例纯数据往返，不依赖设备/窗口，可在 CI 阻止「框架改坏语义」。

#include <hgl/serialize/Archive.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/serialize/TomlCodec.h>

#include <hgl/type/FlatTreeContainer.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/OrderedMap.h>
#include <hgl/type/OrderedSet.h>
#include <hgl/type/ValueArray.h>

#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cstring>

using namespace hgl;
using namespace hgl::serialize;

namespace
{
    int g_failed = 0;

    void Check(const bool condition, const char *what)
    {
        if (condition)
            return;

        GLogError(u8"TestSerializeArchive 失败: %s", what);
        ++g_failed;
    }

    // ── 受测类型（等价于 AssetGen 会生成的形态）────────────────────────────

    enum class SampleKind : uint8_t
    {
        First,
        Second,
        Third,
    };

    struct SampleLeaf
    {
        float scalar = 1.5f;
        hgl::math::Vector3f direction{0.0f, 1.0f, 0.0f};
    };

    struct SampleNode
    {
        AnsiString name;
        int32_t parent = -1;
    };

    struct SampleTreeData
    {
        int32_t id = 0;
    };

    struct SampleAsset
    {
        uint32_t schema = 1;
        AnsiString name;
        OSString uri;
        bool enabled = true;
        int32_t count = 7;
        uint64_t big = 1234567890123ull;
        double ratio = 0.25;
        SampleKind kind = SampleKind::Second;
        hgl::math::Vector3f position{1.0f, 2.0f, 3.0f};
        Color4f tint{0.1f, 0.2f, 0.3f, 0.4f};
        SampleLeaf leaf;
        ValueArray<float> weights;
        ManagedArray<SampleNode> nodes;
        OrderedMap<AnsiString, int32_t> counters;
        OrderedSet<int32_t> tags;
        FlatTreeContainer<SampleTreeData> tree;
    };
}//namespace

// ── 生成的遍历（手工书写，等价于 AssetGen 产物）────────────────────────────

namespace hgl::serialize
{
    template <>
    struct EnumTraits<SampleKind>
    {
        static constexpr bool defined = true;

        static const char *ToName(SampleKind v)
        {
            switch (v)
            {
            case SampleKind::First: return "first";
            case SampleKind::Second: return "second";
            case SampleKind::Third: return "third";
            }
            return nullptr;
        }

        static bool FromName(const char *s, SampleKind &out)
        {
            if (!s)
                return false;
            if (hgl::stricmp(s, "first") == 0) { out = SampleKind::First; return true; }
            if (hgl::stricmp(s, "second") == 0) { out = SampleKind::Second; return true; }
            if (hgl::stricmp(s, "third") == 0) { out = SampleKind::Third; return true; }
            return false;
        }
    };

    template <>
    struct StructSerializer<SampleLeaf>
    {
        static constexpr bool defined = true;

        template <typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "scalar", v.scalar);
            ar.Field(2, "direction", v.direction);
        }
    };

    template <>
    struct StructSerializer<SampleNode>
    {
        static constexpr bool defined = true;

        template <typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "name", v.name);
            ar.Field(2, "parent", v.parent);
        }
    };

    template <>
    struct StructSerializer<SampleTreeData>
    {
        static constexpr bool defined = true;

        template <typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "id", v.id);
        }
    };

    template <>
    struct StructSerializer<SampleAsset>
    {
        static constexpr bool defined = true;

        template <typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "name", v.name);
            ar.Field(3, "uri", v.uri);
            ar.Field(4, "enabled", v.enabled);
            ar.Field(5, "count", v.count);
            ar.Field(6, "big", v.big);
            ar.Field(7, "ratio", v.ratio);
            ar.Field(8, "kind", v.kind);
            ar.Field(9, "position", v.position);
            ar.Field(10, "tint", v.tint);
            ar.Field(11, "leaf", v.leaf);
            ar.Field(12, "weights", v.weights);
            ar.Field(13, "nodes", v.nodes);
            ar.Field(14, "counters", v.counters);
            ar.Field(15, "tags", v.tags);
            ar.Field(16, "tree", v.tree);
        }
    };
}//namespace hgl::serialize

namespace
{
    // ── 用例 1：Value 基础 ──────────────────────────────────────────────────

    bool TestValueBasics()
    {
        Value v;
        Check(v.IsNull(), "Value 初始应为 Null");

        v.SetBool(true);
        bool b = false;
        Check(v.GetBool(b) && b, "Bool 往返");

        v.SetInt(-42);
        int64_t i = 0;
        Check(v.GetInt(i) && i == -42, "Int 往返");

        v.SetUInt(42);
        uint64_t u = 0;
        Check(v.GetUInt(u) && u == 42, "UInt 往返");

        v.SetDouble(1.25);
        double d = 0.0;
        Check(v.GetDouble(d) && d == 1.25, "Double 往返");

        v.SetString(AnsiString("hello"));
        Check(v.IsString()
                  && hgl::stricmp(reinterpret_cast<const char *>(v.GetString().c_str()),
                                  "hello") == 0,
              "String 往返");

        // 表：顺序 + 按名查找 + 深拷贝
        v.SetTable();
        Value *a = v.AddField("a", 1);
        Value *c = v.AddField("c", 3);
        Value *bb = v.AddField("b", 2);
        Check(a && c && bb, "AddField 分配");
        Check(v.GetFieldCount() == 3, "表字段数");
        Check(hgl::stricmp(v.GetFieldName(0), "a") == 0, "表保持插入顺序（0）");
        Check(hgl::stricmp(v.GetFieldName(1), "c") == 0, "表保持插入顺序（1）");
        Check(v.GetFieldId(2) == 2, "表字段 id");

        a->SetInt(11);
        Value copy;
        copy.CopyFrom(v);
        const Value *copied = copy.FindField("a");
        int64_t copied_value = 0;
        Check(copied && copied->GetInt(copied_value) && copied_value == 11, "深拷贝");

        // 同一名字重复 AddField 必须返回既有节点而不是新增
        Value *again = v.AddField("a", 1);
        Check(again == a, "同名 AddField 复用节点");
        Check(v.GetFieldCount() == 3, "同名 AddField 不增加字段");

        return true;
    }

    // ── 用例 2：完整结构的 Archive 往返（内存树，不经文本）──────────────────

    struct Fixture
    {
        SampleAsset asset;

        Fixture()
        {
            asset.schema = 3;
            asset.name = "Demo";
            asset.uri = OS_TEXT("res/demo.geo.toml");
            asset.enabled = false;
            asset.count = -17;
            asset.big = 9007199254740993ull;
            asset.ratio = 0.125;
            asset.kind = SampleKind::Third;
            asset.position = hgl::math::Vector3f(4.0f, 5.0f, 6.0f);
            asset.tint = Color4f(0.5f, 0.25f, 0.125f, 1.0f);
            asset.leaf.scalar = 2.5f;
            asset.leaf.direction = hgl::math::Vector3f(0.0f, 0.0f, 1.0f);

            asset.weights.Add(1.0f);
            asset.weights.Add(2.0f);
            asset.weights.Add(3.5f);

            auto *n0 = asset.nodes.Create();
            n0->name = "root";
            n0->parent = -1;
            auto *n1 = asset.nodes.Create();
            n1->name = "child";
            n1->parent = 0;

            asset.counters.Add(AnsiString("hp"), 100);
            asset.counters.Add(AnsiString("mp"), 50);

            asset.tags.Add(3);
            asset.tags.Add(9);

            const int32_t root = asset.tree.CreateNode(SampleTreeData{1}, -1);
            const int32_t child = asset.tree.CreateNode(SampleTreeData{2}, root);
            asset.tree.CreateNode(SampleTreeData{3}, child);
        }
    };

    bool SameAsset(const SampleAsset &a, const SampleAsset &b)
    {
        if (a.schema != b.schema) return false;
        if (hgl::stricmp(a.name.c_str(), b.name.c_str()) != 0) return false;
        if (hgl::stricmp(a.uri.c_str(), b.uri.c_str()) != 0) return false;
        if (a.enabled != b.enabled) return false;
        if (a.count != b.count) return false;
        if (a.big != b.big) return false;
        if (a.ratio != b.ratio) return false;
        if (a.kind != b.kind) return false;
        if (a.position.x != b.position.x || a.position.y != b.position.y
            || a.position.z != b.position.z) return false;
        if (a.tint.x != b.tint.x || a.tint.w != b.tint.w) return false;
        if (a.leaf.scalar != b.leaf.scalar) return false;
        if (a.leaf.direction.z != b.leaf.direction.z) return false;

        if (a.weights.GetCount() != b.weights.GetCount()) return false;
        for (int i = 0; i < a.weights.GetCount(); ++i)
        {
            if (a.weights[i] != b.weights[i])
                return false;
        }

        if (a.nodes.GetCount() != b.nodes.GetCount()) return false;
        for (int i = 0; i < a.nodes.GetCount(); ++i)
        {
            if (hgl::stricmp(a.nodes[i]->name.c_str(), b.nodes[i]->name.c_str()) != 0)
                return false;
            if (a.nodes[i]->parent != b.nodes[i]->parent)
                return false;
        }

        if (a.counters.GetCount() != b.counters.GetCount()) return false;
        int32_t hp = 0;
        int32_t bhp = 0;
        if (!a.counters.Get(AnsiString("hp"), hp) || !b.counters.Get(AnsiString("hp"), bhp))
            return false;
        if (hp != bhp) return false;

        if (a.tags.GetCount() != b.tags.GetCount()) return false;

        if (a.tree.GetCount() != b.tree.GetCount()) return false;
        for (int i = 0; i < a.tree.GetCount(); ++i)
        {
            if (a.tree.GetData()[i].id != b.tree.GetData()[i].id)
                return false;
            const auto *ta = a.tree.GetHierarchyTopologyData();
            const auto *tb = b.tree.GetHierarchyTopologyData();
            if (!ta || !tb || ta[i].parent != tb[i].parent)
                return false;
        }

        return true;
    }

    bool TestArchiveRoundTrip()
    {
        Fixture fixture;

        Value tree;
        Archive writer = Archive::ForWrite(tree);
        StructSerializer<SampleAsset>::Do(writer, fixture.asset);
        Check(writer.Ok(), "写入方向不应失败");
        if (!writer.Ok())
        {
            GLogError(u8"  失败字段=%s 原因=%s",
                      writer.FailedField(), writer.ErrorMessage());
            return false;
        }

        Check(tree.IsTable(), "写入根节点应为表");
        Check(tree.GetFieldCount() == 16, "写入应包含全部 16 个字段");

        SampleAsset loaded;
        // 把类内缺省值清成可区分的样子，确认读方向真的写入了每个字段。
        loaded.name.Clear();
        loaded.schema = 0;

        Archive reader = Archive::ForRead(tree);
        StructSerializer<SampleAsset>::Do(reader, loaded);
        Check(reader.Ok(), "读取方向不应失败");
        if (!reader.Ok())
        {
            GLogError(u8"  失败字段=%s 原因=%s",
                      reader.FailedField(), reader.ErrorMessage());
            return false;
        }

        Check(SameAsset(fixture.asset, loaded), "结构往返后应完全一致");
        return true;
    }

    // ── 用例 3：TOML 文本往返 ───────────────────────────────────────────────

    bool TestTomlRoundTrip()
    {
        Fixture fixture;

        Value tree;
        Archive writer = Archive::ForWrite(tree);
        StructSerializer<SampleAsset>::Do(writer, fixture.asset);
        if (!writer.Ok())
            return false;

        const toml::value toml_value = ToToml(tree);
        Check(toml_value.is_table(), "TOML 根应为 table");

        Value restored;
        FromToml(toml_value, restored);

        SampleAsset loaded;
        Archive reader = Archive::ForRead(restored);
        StructSerializer<SampleAsset>::Do(reader, loaded);
        Check(reader.Ok(), "TOML 往返读取不应失败");
        if (!reader.Ok())
        {
            GLogError(u8"  失败字段=%s 原因=%s",
                      reader.FailedField(), reader.ErrorMessage());
            return false;
        }

        Check(SameAsset(fixture.asset, loaded), "TOML 往返后应完全一致");

        // 文本可再解析（format → parse 闭环）
        const AnsiString text = ToTomlText(tree);
        Check(text.Length() > 0, "TOML 文本不应为空");

        Value reparsed;
        Check(FromTomlText(text.c_str(), reparsed), "TOML 文本应可再解析");
        SampleAsset second;
        Archive reader2 = Archive::ForRead(reparsed);
        StructSerializer<SampleAsset>::Do(reader2, second);
        Check(reader2.Ok(), "文本再解析后读取不应失败");
        Check(SameAsset(fixture.asset, second), "文本闭环应完全一致");

        return true;
    }

    // ── 用例 4：缺键保留缺省值 ─────────────────────────────────────────────

    bool TestMissingKeyKeepsDefault()
    {
        Value tree;
        Check(FromTomlText("schema = 9\n", tree), "缺键样本应可解析");

        SampleAsset loaded;
        loaded.name = "keep-me";
        loaded.count = 42;

        Archive reader = Archive::ForRead(tree);
        StructSerializer<SampleAsset>::Do(reader, loaded);
        Check(reader.Ok(), "缺键不应导致失败");
        Check(loaded.schema == 9, "存在的字段应被写入");
        Check(hgl::stricmp(loaded.name.c_str(), "keep-me") == 0, "缺失的字段应保留缺省值");
        Check(loaded.count == 42, "缺失的整型字段应保留缺省值");

        return true;
    }

    // ── 用例 5：类型不符必须失败并给出字段名 ───────────────────────────────

    bool TestTypeMismatchFails()
    {
        Value tree;
        Check(FromTomlText("count = \"not-an-int\"\n", tree), "类型错误样本应可解析");

        SampleAsset loaded;
        Archive reader = Archive::ForRead(tree);
        StructSerializer<SampleAsset>::Do(reader, loaded);
        Check(!reader.Ok(), "类型不符必须失败");
        Check(hgl::stricmp(reader.FailedField(), "count") == 0, "失败字段名应为 count");

        return true;
    }

    // ── 用例 6：枚举名写文本、读回文本 ─────────────────────────────────────

    bool TestEnumUsesName()
    {
        SampleAsset asset;
        asset.kind = SampleKind::Third;

        Value tree;
        Archive writer = Archive::ForWrite(tree);
        StructSerializer<SampleAsset>::Do(writer, asset);

        const Value *kind = tree.FindField("kind");
        Check(kind != nullptr, "应写出 kind 字段");
        if (kind)
        {
            Check(kind->IsString(), "枚举应写成字符串名");
            if (kind->IsString())
            {
                Check(hgl::stricmp(
                          reinterpret_cast<const char *>(kind->GetString().c_str()),
                          "third") == 0,
                      "枚举名拼写");
            }
        }

        // 整数写法的旧数据仍可读
        Value legacy;
        Check(FromTomlText("kind = 1\n", legacy), "整数枚举样本应可解析");
        SampleAsset loaded;
        Archive reader = Archive::ForRead(legacy);
        StructSerializer<SampleAsset>::Do(reader, loaded);
        Check(reader.Ok(), "整数枚举应可读");
        Check(loaded.kind == SampleKind::Second, "整数枚举映射");

        return true;
    }

    // ── 用例 7：FlatBuffers 后端往返（通用描述表驱动）─────────────────────
    //
    // 这里手工写出 AssetGen 会生成的等价描述表，验证通用 FB 编解码器
    // （两趟构建、voffset、内联 struct、标量向量、表向量）行为正确。
    // 未出现在描述表里的字段（Mapping / Set / Tree）在二进制快照中被忽略。

    namespace desc
    {
        using hgl::serialize::fb::FieldDesc;
        using hgl::serialize::fb::FbType;
        using hgl::serialize::fb::TableDesc;

        inline bool KindFromName(const char *name, int64_t &out)
        {
            SampleKind parsed{};
            if (!EnumTraits<SampleKind>::FromName(name, parsed))
                return false;
            out = static_cast<int64_t>(parsed);
            return true;
        }

        inline const char *KindToName(const int64_t value)
        {
            return EnumTraits<SampleKind>::ToName(static_cast<SampleKind>(value));
        }

        inline const FieldDesc leaf_fields[] = {
            {1, "scalar", FbType::Float, 4},
            {2, "direction", FbType::Struct, 6, FbType::None, 3},
        };
        inline const TableDesc leaf = {"SampleLeaf", leaf_fields, 2};

        inline const FieldDesc node_fields[] = {
            {1, "name", FbType::String, 4},
            {2, "parent", FbType::Int32, 6},
        };
        inline const TableDesc node = {"SampleNode", node_fields, 2};

        inline const FieldDesc asset_fields[] = {
            {1, "schema", FbType::UInt32, 4},
            {2, "name", FbType::String, 6},
            {3, "uri", FbType::String, 8},
            {4, "enabled", FbType::Bool, 10},
            {5, "count", FbType::Int32, 12},
            {6, "big", FbType::UInt64, 14},
            {7, "ratio", FbType::Double, 16},
            {8, "kind", FbType::Enum, 18, FbType::None, 0, nullptr,
             &KindFromName, &KindToName},
            {9, "position", FbType::Struct, 20, FbType::None, 3},
            {10, "tint", FbType::Struct, 22, FbType::None, 4},
            {11, "leaf", FbType::Table, 24, FbType::None, 0, &leaf},
            {12, "weights", FbType::VectorScalar, 26, FbType::Float},
            {13, "nodes", FbType::VectorTable, 28, FbType::None, 0, &node},
        };
        inline const TableDesc asset = {"SampleAsset", asset_fields, 13};
    }//namespace desc

    /// 只比较 FB 描述表里覆盖到的字段。
    bool SameFlatFields(const SampleAsset &a, const SampleAsset &b)
    {
        if (a.schema != b.schema) { GLogError(u8"  diff: schema"); return false; }
        if (hgl::stricmp(a.name.c_str(), b.name.c_str()) != 0) { GLogError(u8"  diff: name"); return false; }
        if (hgl::stricmp(a.uri.c_str(), b.uri.c_str()) != 0) { GLogError(u8"  diff: uri"); return false; }
        if (a.enabled != b.enabled) { GLogError(u8"  diff: enabled"); return false; }
        if (a.count != b.count) { GLogError(u8"  diff: count"); return false; }
        if (a.big != b.big) { GLogError(u8"  diff: big"); return false; }
        if (a.ratio != b.ratio) { GLogError(u8"  diff: ratio"); return false; }
        if (a.kind != b.kind) { GLogError(u8"  diff: kind"); return false; }
        if (a.position.x != b.position.x || a.position.y != b.position.y
            || a.position.z != b.position.z) { GLogError(u8"  diff: position"); return false; }
        if (a.tint.x != b.tint.x || a.tint.y != b.tint.y
            || a.tint.z != b.tint.z || a.tint.w != b.tint.w) { GLogError(u8"  diff: tint"); return false; }
        if (a.leaf.scalar != b.leaf.scalar) { GLogError(u8"  diff: leaf.scalar"); return false; }
        if (a.leaf.direction.y != b.leaf.direction.y) { GLogError(u8"  diff: leaf.direction"); return false; }

        if (a.weights.GetCount() != b.weights.GetCount()) { GLogError(u8"  diff: weights count"); return false; }
        for (int i = 0; i < a.weights.GetCount(); ++i)
        {
            if (a.weights[i] != b.weights[i])
            { GLogError(u8"  diff: weights[%d]", i); return false; }
        }

        if (a.nodes.GetCount() != b.nodes.GetCount()) { GLogError(u8"  diff: nodes count"); return false; }
        for (int i = 0; i < a.nodes.GetCount(); ++i)
        {
            if (hgl::stricmp(a.nodes[i]->name.c_str(), b.nodes[i]->name.c_str()) != 0)
            { GLogError(u8"  diff: nodes[%d].name", i); return false; }
            if (a.nodes[i]->parent != b.nodes[i]->parent)
            { GLogError(u8"  diff: nodes[%d].parent", i); return false; }
        }

        return true;
    }

    bool TestFlatBuffersRoundTrip()
    {
        Fixture fixture;

        Value tree;
        Archive writer = Archive::ForWrite(tree);
        StructSerializer<SampleAsset>::Do(writer, fixture.asset);
        Check(writer.Ok(), "FB 写入前的树构建不应失败");

        flatbuffers::FlatBufferBuilder fbb;
        hgl::serialize::fb::EncodeToBuffer(fbb, tree, desc::asset);
        Check(fbb.GetSize() > 0, "FlatBuffers 编码应产出非空缓冲");

        Value decoded_tree;
        Check(hgl::serialize::fb::DecodeFromBuffer(fbb.GetBufferPointer(),
                                                  fbb.GetSize(), desc::asset,
                                                  decoded_tree),
              "FlatBuffers 解码应成功");

        SampleAsset loaded;
        Archive reader = Archive::ForRead(decoded_tree);
        StructSerializer<SampleAsset>::Do(reader, loaded);
        Check(reader.Ok(), "FB 解码后的树读入结构不应失败");
        if (!reader.Ok())
        {
            GLogError(u8"  失败字段=%s 原因=%s",
                      reader.FailedField(), reader.ErrorMessage());
            return false;
        }

        Check(SameFlatFields(fixture.asset, loaded), "FlatBuffers 往返后字段应一致");

        // 过短的缓冲必须被拒绝（真正的 schema 级校验由调用方用 flatc 生成的
        // 根类型跑 Verifier，见 DecodeFromBuffer 文档）。
        Value junk;
        const uint8_t bogus[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        Check(!hgl::serialize::fb::DecodeFromBuffer(bogus, sizeof(bogus),
                                                   desc::asset, junk),
              "过短缓冲必须被拒绝");

        return true;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestSerializeArchive"));

    TestValueBasics();
    TestArchiveRoundTrip();
    TestTomlRoundTrip();
    TestMissingKeyKeepsDefault();
    TestTypeMismatchFails();
    TestEnumUsesName();
    TestFlatBuffersRoundTrip();

    if (g_failed != 0)
    {
        GLogError(u8"=== TestSerializeArchive: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestSerializeArchive: 全部测试通过 ===");
    return 0;
}
