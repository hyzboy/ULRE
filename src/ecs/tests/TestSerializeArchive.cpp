// TestSerializeArchive.cpp —— 统一序列化框架（Unified Archive）门禁：
//
//   1. Value 树：标量 / 字符串 / 数组 / 表的读写与深拷贝；
//   2. Archive：标量、字符串、枚举、数学向量、嵌套结构、Sequence / Mapping /
//      Set / Tree 的双向遍历；
//   3. TOML 后端：Value 树 ⇄ 文本往返，文本输出可再解析；
//   4. 缺键语义：读方向缺字段必须保留 C++ 类内缺省值而不是报错；
//   5. 类型不符：必须失败并给出字段名；
//   6. 严格键校验：`ValidateKeysByMeta` 按 `ClassMetaInfo` 递归判键名合法性
//      （大小写敏感、数组不进路径）；
//   7. 整体入口：`ReadFromValue` / `WriteToValue` 把一个节点整体当成 `T` 读写。
//
// 本用例纯数据往返，不依赖设备/窗口，可在 CI 阻止「框架改坏语义」。

#include <hgl/serialize/Archive.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/serialize/MetaSchema.h>
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

// ── HGL_ENUM_* / HGL_FLAGS_* 登记样张 ──────────────────────────────────────
//
// 刻意声明在具名命名空间里，再用完全限定名在全局作用域登记 —— 与引擎里
// `hgl::graph::SkyMode` 这类真实用法一致，顺带验证「登记块必须在全局作用域、
// TYPE 必须写完全限定名」这条硬约定在本工程里确实可用。

namespace archive_test
{
    enum class WeatherMode : uint8_t
    {
        Clear,
        Rain,
        Snow,
    };

    enum class PostFxFlags : uint32_t
    {
        None     = 0,
        Bloom    = 1u << 0,
        Tonemap  = 1u << 1,
        Vignette = 1u << 2,
    };

    /// 序列化名与 C++ 子名刻意不一致：验证 `HGL_ENUM_ITEM_NAME`。
    enum class ShapeKind : uint8_t
    {
        PlaneGrid,
        BoundingBox,
        HexSphere,
    };

    /// 同样使用显式序列化名的按位标记。
    enum class DirtFlags : uint32_t
    {
        None        = 0,
        DoubleSided = 1u << 0,
        AlphaTest   = 1u << 1,
    };
}// namespace archive_test

HGL_ENUM_BEGIN(archive_test::WeatherMode)
    HGL_ENUM_ITEM(Clear, "晴天")
    HGL_ENUM_ITEM(Rain, "雨天")
    HGL_ENUM_ITEM(Snow, "雪天")
HGL_ENUM_END(archive_test::WeatherMode)

HGL_FLAGS_BEGIN(archive_test::PostFxFlags)
    HGL_FLAG_ITEM(None, "无后期")
    HGL_FLAG_ITEM(Bloom, "泛光")
    HGL_FLAG_ITEM(Tonemap, "色调映射")
    HGL_FLAG_ITEM(Vignette, "暗角")
HGL_FLAGS_END(archive_test::PostFxFlags)

HGL_ENUM_BEGIN(archive_test::ShapeKind)
    HGL_ENUM_ITEM_NAME(PlaneGrid, "plane_grid", "网格平面")
    HGL_ENUM_ITEM_NAME(BoundingBox, "bounding_box", "包围盒")
    HGL_ENUM_ITEM_NAME(HexSphere, "hex_sphere", "六边形球")
HGL_ENUM_END(archive_test::ShapeKind)

HGL_FLAGS_BEGIN(archive_test::DirtFlags)
    HGL_FLAG_ITEM(None, "无标记")
    HGL_FLAG_ITEM_NAME(DoubleSided, "double_sided", "关闭背面剔除")
    HGL_FLAG_ITEM_NAME(AlphaTest, "alpha_test", "Alpha 截断测试")
HGL_FLAGS_END(archive_test::DirtFlags)

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

        // Value 树 → 文本 → Value 树，闭环一次：断言的就是真正落盘的那份文本。
        const AnsiString text = ToTomlText(tree);
        Check(text.Length() > 0, "TOML 文本不应为空");

        Value restored;
        Check(FromTomlText(text.c_str(), restored), "TOML 文本应可再解析");

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

    // ── 用例 9/10 的元数据样板（等价于 AssetGen 产物里的 `Meta.gen.h`）──────

    namespace meta_fixture
    {
        using namespace hgl::asset;

        const PropertyMetaInfo kLeafProps[] = {
            { "scalar",    nullptr, "Scalar",    "Default", "", "float",
              nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml, 0, 4, 0, 0.0, 0.0, 0.0, nullptr },
            { "direction", nullptr, "Direction", "Default", "", "hgl::math::Vector3f",
              nullptr, nullptr, PropertyKind::Vector3f, PF_Serialized | PF_SerializedToml, 4, 12, 0, 0.0, 0.0, 0.0, nullptr },
        };

        const ClassMetaInfo kLeafMeta = {
            "SampleLeaf", "", "", kLeafProps, 2,
            static_cast<uint32_t>(sizeof(SampleLeaf)),
            static_cast<uint32_t>(alignof(SampleLeaf)),
        };

        const PropertyMetaInfo kRootProps[] = {
            { "name",  nullptr, "Name",  "Default", "", "AnsiString",
              nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml, 0, 0, 0, 0.0, 0.0, 0.0, nullptr },
            { "count", nullptr, "Count", "Default", "", "int32_t",
              nullptr, nullptr, PropertyKind::Int32, PF_Serialized | PF_SerializedToml, 0, 4, 0, 0.0, 0.0, 1.0, nullptr },
            { "leaf",  nullptr, "Leaf",  "Default", "", "SampleLeaf",
              nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml, 0, 0, 0, 0.0, 0.0, 0.0, &kLeafMeta },
            { "items", "variants", "Items", "Default", "", "hgl::ManagedArray<SampleLeaf>",
              nullptr, nullptr, PropertyKind::PtrArray, PF_Serialized | PF_SerializedToml, 0, 0, 0, 0.0, 0.0, 0.0, &kLeafMeta },
        };

        const ClassMetaInfo kRootMeta = {
            "SampleRoot", "", "", kRootProps, 4,
            static_cast<uint32_t>(sizeof(SampleAsset)),
            static_cast<uint32_t>(alignof(SampleAsset)),
        };
    }//namespace meta_fixture

    // ── 用例 9：`FindFieldExact` / `FindByPath` ─────────────────────────────

    bool TestFindByPathAndExact()
    {
        Value root;
        root.SetTable();

        Value *render_state = root.AddField("render_state", 0);
        render_state->SetTable();
        Value *pipeline = render_state->AddField("pipeline", 0);
        pipeline->SetTable();
        Value *cull_mode = pipeline->AddField("cull_mode", 0);
        cull_mode->SetString(AnsiString("Back"));

        Value *textures = root.AddField("textures", 0);
        textures->SetArray();
        Value *first = textures->AddElement();
        first->SetTable();
        Value *texture_name = first->AddField("name", 0);
        texture_name->SetString(AnsiString("albedo"));

        Check(root.FindByPath("render_state.pipeline.cull_mode") == cull_mode,
              "FindByPath 应逐层下钻");
        Check(root.FindByPath("render_state") == render_state, "FindByPath 单段");
        Check(root.FindByPath("textures.name") == texture_name,
              "FindByPath 遇到表数组不下标，取首个命中的元素");
        Check(root.FindByPath("") == &root, "FindByPath 空路径返回自身");
        Check(root.FindByPath("render_state.missing") == nullptr,
              "FindByPath 缺键返回 nullptr");
        Check(root.FindByPath("missing.cull_mode") == nullptr,
              "FindByPath 首段缺键返回 nullptr");

        Check(root.FindFieldExact("textures") == textures, "FindFieldExact 应命中");
        Check(root.FindFieldExact("Textures") == nullptr,
              "FindFieldExact 必须大小写敏感");
        Check(root.FindField("Textures") == textures,
              "FindField 保持大小写不敏感的既有语义");

        return true;
    }

    // ── 用例 10：`ValidateKeysByMeta` 严格键校验 ────────────────────────────

    bool TestValidateKeysByMeta()
    {
        AnsiString path;
        AnsiString key;

        {
            // 全部键都被认领：嵌套表与表数组都要下钻成功。
            Value root;
            root.SetTable();
            root.AddField("name", 0)->SetString(AnsiString("Demo"));
            root.AddField("count", 0)->SetInt(3);

            Value *leaf = root.AddField("leaf", 0);
            leaf->SetTable();
            leaf->AddField("scalar", 0)->SetFloat(1.0f);

            Value *items = root.AddField("variants", 0);
            items->SetArray();
            Value *item = items->AddElement();
            item->SetTable();
            item->AddField("scalar", 0)->SetFloat(2.0f);

            Check(ValidateKeysByMeta(root, meta_fixture::kRootMeta, AnsiString(""), path, key),
                  "合法树必须通过严格键校验");
            Check(path.IsEmpty() && key.IsEmpty(), "通过时不给诊断信息");
        }

        {
            // 嵌套表里的拼写错误：报错路径必须指向那张子表。
            Value root;
            root.SetTable();
            Value *leaf = root.AddField("leaf", 0);
            leaf->SetTable();
            leaf->AddField("scalr", 0)->SetFloat(1.0f);

            Check(!ValidateKeysByMeta(root, meta_fixture::kRootMeta, AnsiString(""), path, key),
                  "嵌套表里的未知键必须失败");
            Check(hgl::stricmp(key.c_str(), "scalr") == 0, "必须报出未知键名");
            Check(hgl::stricmp(path.c_str(), "leaf") == 0, "必须报出出错表路径");
        }

        {
            // 表数组元素里的未知键：数组不进路径。
            Value root;
            root.SetTable();
            Value *items = root.AddField("variants", 0);
            items->SetArray();
            Value *item = items->AddElement();
            item->SetTable();
            item->AddField("scalar_typo", 0)->SetFloat(1.0f);

            Check(!ValidateKeysByMeta(root, meta_fixture::kRootMeta, AnsiString(""), path, key),
                  "表数组元素里的未知键必须失败");
            Check(hgl::stricmp(key.c_str(), "scalar_typo") == 0, "必须报出未知键名");
            Check(hgl::stricmp(path.c_str(), "variants") == 0, "数组不进路径");
        }

        {
            // 键名大小写不符：`Name` 不等于 `name`。
            Value root;
            root.SetTable();
            root.AddField("Name", 0)->SetString(AnsiString("Demo"));

            Check(!ValidateKeysByMeta(root, meta_fixture::kRootMeta, AnsiString(""), path, key),
                  "键名大小写不符必须失败");
            Check(hgl::stricmp(key.c_str(), "Name") == 0, "必须报出原始键名");
            Check(path.IsEmpty(), "顶层失败时表路径为空");
        }

        {
            // 顶层键名走 `toml_key` 覆盖（`items` → `variants`）也要被认出。
            Value root;
            root.SetTable();
            root.AddField("variants", 0)->SetArray();

            Check(ValidateKeysByMeta(root, meta_fixture::kRootMeta, AnsiString(""), path, key),
                  "toml_key 覆盖的键必须被认领");
        }

        return true;
    }

    // ── 用例 11：`ReadFromValue` / `WriteToValue` 整体入口 ───────────────────

    bool TestWholeValueEntry()
    {
        SampleAsset source;
        source.schema = 42;
        source.name = "Whole";
        source.count = -9;
        source.kind = SampleKind::Third;
        source.position = hgl::math::Vector3f(7.0f, 8.0f, 9.0f);

        Value tree;
        Check(WriteToValue(source, tree), "WriteToValue 应成功");
        Check(tree.IsTable(), "WriteToValue 产出应为表");

        SampleAsset loaded;
        Archive read_archive;
        Check(ReadFromValue(tree, loaded, &read_archive), "ReadFromValue 应成功");
        Check(read_archive.Ok(), "ReadFromValue 的 Archive 应无失败");
        Check(loaded.schema == source.schema && loaded.count == source.count,
              "整体入口标量往返");
        Check(loaded.kind == source.kind, "整体入口枚举往返");
        Check(hgl::stricmp(loaded.name.c_str(), source.name.c_str()) == 0,
              "整体入口字符串往返");

        // 类型不符必须失败，并把字段名带回诊断。
        tree.FindField("count")->SetString(AnsiString("nope"));
        SampleAsset broken;
        Archive failed_archive;
        Check(!ReadFromValue(tree, broken, &failed_archive), "类型不符必须失败");
        Check(hgl::stricmp(failed_archive.FailedField(), "count") == 0,
              "必须报出失败字段名");

        return true;
    }

    bool TestFlatBuffersRoundTrip()
    {
        Fixture fixture;

        Value tree;
        Archive writer = Archive::ForWrite(tree);
        StructSerializer<SampleAsset>::Do(writer, fixture.asset);
        Check(writer.Ok(), "FB 写入前的树构建不应失败");

        hgl::ValueArray<uint8> fb_bytes;
        Check(hgl::serialize::fb::EncodeToBuffer(tree, desc::asset, nullptr, fb_bytes),
              "FlatBuffers 编码应成功");
        Check(fb_bytes.GetCount() > 0, "FlatBuffers 编码应产出非空缓冲");

        Value decoded_tree;
        Check(hgl::serialize::fb::DecodeFromBuffer(fb_bytes.GetData(),
                                                  fb_bytes.GetCount(), desc::asset,
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

    // ── 用例 13：`ClassMetaInfo` 驱动的 FlatBuffers（反射即布局）──────────────
    //
    // 与用例 12 的区别：这里**不写任何描述表**，字段 id / 键名 / voffset 全部由
    // 反射元数据推导 —— 这正是 `HGL_ASSET_STRUCT` + AssetGen 产物的真实路径。
    // 用同一份值树分别走「手写描述表」和「反射推导」两条路，缓冲必须逐字节相同。

    namespace fbmeta
    {
        using namespace hgl::asset;

        /// 「既进 TOML 又进 FlatBuffers」的常规标志组合。
        constexpr uint32_t kBoth = PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers;

        /// 「只进 TOML」：值树里有键，但不写进二进制（映射 / 集合 / 树）。
        constexpr uint32_t kTomlOnly = PF_Serialized | PF_SerializedToml;

        const PropertyMetaInfo kLeafProps[] = {
            { "scalar",    nullptr, "scalar",    "Default", "", "float",
              nullptr, nullptr, PropertyKind::Float, kBoth, 0, 4, 0, 0.0, 0.0, 0.0, nullptr },
            { "direction", nullptr, "direction", "Default", "", "hgl::math::Vector3f",
              nullptr, nullptr, PropertyKind::Vector3f, kBoth, 4, 12, 0, 0.0, 0.0, 0.0, nullptr },
        };

        const ClassMetaInfo kLeafMeta = {
            "SampleLeaf", "", "", kLeafProps, 2,
            static_cast<uint32_t>(sizeof(SampleLeaf)),
            static_cast<uint32_t>(alignof(SampleLeaf)),
        };

        const PropertyMetaInfo kNodeProps[] = {
            { "name",   nullptr, "name",   "Default", "", "AnsiString",
              nullptr, nullptr, PropertyKind::String, kBoth, 0, 0, 0, 0.0, 0.0, 0.0, nullptr },
            { "parent", nullptr, "parent", "Default", "", "int32_t",
              nullptr, nullptr, PropertyKind::Int32, kBoth, 0, 4, 0, 0.0, 0.0, 0.0, nullptr },
        };

        const ClassMetaInfo kNodeMeta = {
            "SampleNode", "", "", kNodeProps, 2,
            static_cast<uint32_t>(sizeof(SampleNode)),
            static_cast<uint32_t>(alignof(SampleNode)),
        };

        const PropertyMetaInfo kAssetProps[] = {
            { "schema",   nullptr,  "schema",   "Default", "", "uint32_t",
              nullptr, nullptr, PropertyKind::UInt32, kBoth, 0, 4, 0, 0.0, 0.0, 0.0, nullptr },
            { "name",     nullptr,  "name",     "Default", "", "AnsiString",
              nullptr, nullptr, PropertyKind::String, kBoth, 0, 0, 0, 0.0, 0.0, 0.0, nullptr },
            { "uri",      nullptr,  "uri",      "Default", "", "OSString",
              nullptr, nullptr, PropertyKind::String, kBoth, 0, 0, 0, 0.0, 0.0, 0.0, nullptr },
            { "enabled",  nullptr,  "enabled",  "Default", "", "bool",
              nullptr, nullptr, PropertyKind::Bool, kBoth, 0, 1, 0, 0.0, 0.0, 0.0, nullptr },
            { "count",    nullptr,  "count",    "Default", "", "int32_t",
              nullptr, nullptr, PropertyKind::Int32, kBoth, 0, 4, 0, 0.0, 0.0, 0.0, nullptr },
            { "big",      nullptr,  "big",      "Default", "", "uint64_t",
              nullptr, nullptr, PropertyKind::UInt64, kBoth, 0, 8, 0, 0.0, 0.0, 0.0, nullptr },
            { "ratio",    nullptr,  "ratio",    "Default", "", "double",
              nullptr, nullptr, PropertyKind::Double, kBoth, 0, 8, 0, 0.0, 0.0, 0.0, nullptr },
            { "kind",     nullptr,  "kind",     "Default", "", "SampleKind", "SampleKind",
              nullptr, PropertyKind::Enum, kBoth, 0, 4, 0, 0.0, 0.0, 0.0, nullptr,
              PropertyKind::None, &desc::KindFromName, &desc::KindToName },
            { "position", nullptr,  "position", "Default", "", "hgl::math::Vector3f",
              nullptr, nullptr, PropertyKind::Vector3f, kBoth, 0, 12, 0, 0.0, 0.0, 0.0, nullptr },
            { "tint",     nullptr,  "tint",     "Default", "", "Color4f",
              nullptr, nullptr, PropertyKind::Vector4f, kBoth, 0, 16, 0, 0.0, 0.0, 0.0, nullptr },
            { "leaf",     nullptr,  "leaf",     "Default", "", "SampleLeaf",
              nullptr, nullptr, PropertyKind::Struct, kBoth, 0, 0, 0, 0.0, 0.0, 0.0, &kLeafMeta },
            { "weights",  nullptr,  "weights",  "Default", "", "hgl::ValueArray<float>",
              nullptr, nullptr, PropertyKind::ValueArray, kBoth, 0, 0, 0, 0.0, 0.0, 0.0, nullptr,
              PropertyKind::Float },
            { "nodes",    nullptr,  "nodes",    "Default", "", "hgl::ManagedArray<SampleNode>",
              nullptr, nullptr, PropertyKind::PtrArray, kBoth, 0, 0, 0, 0.0, 0.0, 0.0, &kNodeMeta,
              PropertyKind::Struct },
            { "counters", nullptr,  "counters", "Default", "", "hgl::OrderedMap<AnsiString,int32_t>",
              nullptr, nullptr, PropertyKind::Struct, kTomlOnly, 0, 0, 0, 0.0, 0.0, 0.0, nullptr },
            { "tags",     nullptr,  "tags",     "Default", "", "hgl::OrderedSet<int32_t>",
              nullptr, nullptr, PropertyKind::Struct, kTomlOnly, 0, 0, 0, 0.0, 0.0, 0.0, nullptr },
            { "tree",     nullptr,  "tree",     "Default", "", "hgl::FlatTreeContainer<SampleTreeData>",
              nullptr, nullptr, PropertyKind::Struct, kTomlOnly, 0, 0, 0, 0.0, 0.0, 0.0, nullptr },
        };

        const ClassMetaInfo kAssetMeta = {
            "SampleAsset", "", "", kAssetProps, 16,
            static_cast<uint32_t>(sizeof(SampleAsset)),
            static_cast<uint32_t>(alignof(SampleAsset)),
        };
    }//namespace fbmeta

    bool TestFlatBuffersMetaDriven()
    {
        Fixture fixture;

        // ① 反射推导编码
        hgl::ValueArray<uint8> meta_fb;
        AnsiString encode_error;
        Check(hgl::serialize::fb::EncodeToFlatBuffers<SampleAsset>(
                  meta_fb, fixture.asset, fbmeta::kAssetMeta, nullptr, &encode_error),
              "反射驱动编码应成功");
        if (!encode_error.IsEmpty())
            GLogError(u8"  编码诊断=%s", encode_error.c_str());

        // ② 与手写描述表的产物逐字节对比：两条路必须推导出同一布局
        hgl::ValueArray<uint8> desc_fb;
        Value tree;
        Archive writer = Archive::ForWrite(tree);
        StructSerializer<SampleAsset>::Do(writer, fixture.asset);
        hgl::serialize::fb::EncodeToBuffer(tree, desc::asset, nullptr, desc_fb);

        Check(meta_fb.GetCount() == desc_fb.GetCount(), "反射与描述表产出的缓冲长度应一致");
        if (meta_fb.GetCount() == desc_fb.GetCount())
            Check(memcmp(meta_fb.GetData(), desc_fb.GetData(),
                         meta_fb.GetCount()) == 0,
                  "反射与描述表产出的缓冲应逐字节相同");

        // ③ 校验通过 + 解码往返
        const size_t size = static_cast<size_t>(meta_fb.GetCount());
        Check(hgl::serialize::fb::VerifyBuffer(meta_fb.GetData(), size,
                                               fbmeta::kAssetMeta),
              "合法缓冲应通过反射校验");

        SampleAsset loaded;
        AnsiString decode_error;
        Check(hgl::serialize::fb::VerifyAndDecodeFlatBuffers<SampleAsset>(
                  meta_fb.GetData(), size, loaded, fbmeta::kAssetMeta,
                  nullptr, &decode_error),
              "校验并解码应成功");
        if (!decode_error.IsEmpty())
            GLogError(u8"  解码诊断=%s", decode_error.c_str());
        Check(SameFlatFields(fixture.asset, loaded), "反射驱动往返后字段应一致");

        // ④ 坏缓冲：越界根偏移必须被拒，且不得解码出半个结构
        uint8_t broken[2048];
        Check(size > 0 && size < sizeof(broken), "缓冲应装得进本地副本");
        memcpy(broken, meta_fb.GetData(), size);
        broken[0] = 0xFF;
        broken[1] = 0xFF;
        broken[2] = 0xFF;
        broken[3] = 0x7F;
        Check(!hgl::serialize::fb::VerifyBuffer(broken, size, fbmeta::kAssetMeta),
              "越界根偏移必须校验失败");
        SampleAsset rejected;
        Check(!hgl::serialize::fb::VerifyAndDecodeFlatBuffers<SampleAsset>(
                  broken, size, rejected, fbmeta::kAssetMeta),
              "校验失败的缓冲不得解码");

        // ⑤ 截断缓冲同样必须被拒
        Check(!hgl::serialize::fb::VerifyBuffer(meta_fb.GetData(), 8,
                                                fbmeta::kAssetMeta),
              "截断缓冲必须校验失败");

        return true;
    }

    // ── 用例 14：TOML 文本入口 —— 错误行号 + 类型化往返 ─────────────────────
    //
    // `FromTomlText` 之前只报「解析失败」；现在必须带 1 起的行号，否则用户在几百行
    // 的材质文件里根本定位不到错在哪。`SaveToTomlText` / `LoadFromTomlText` 则把
    // 「结构 ⇄ 文本」压成一个调用，调用方不再自己 `Archive` + `Value` 两头跑。

    bool TestTomlTextEntry()
    {
        // ① 语法错误必须带行号（1 起）
        const char *broken = "schema = 1\nname = \"ok\"\nbad = = 2\n";
        Value broken_tree;
        TomlParseError parse_error;
        Check(!FromTomlText(broken, std::strlen(broken), broken_tree, &parse_error),
              "语法错误必须解析失败");
        Check(parse_error.line == 3, "语法错误行号应为第 3 行");
        Check(!parse_error.title.IsEmpty() || !parse_error.detail.IsEmpty(),
              "语法错误应带描述");
        GLogInfo(u8"  TOML 语法错误诊断: %s", parse_error.Format().c_str());

        // ② 结构 → 文本 → 结构，字段双向一致
        SampleAsset source;
        source.schema = 7;
        source.name = "FromText";
        source.count = -3;
        source.kind = SampleKind::Second;
        source.position = hgl::math::Vector3f(1.5f, 2.5f, 3.5f);
        source.weights.Add(0.25f);
        source.weights.Add(0.75f);

        const AnsiString text = SaveToTomlText<SampleAsset>(source);
        Check(!text.IsEmpty(), "SaveToTomlText 应产出非空文本");

        SampleAsset loaded;
        Archive read_archive;
        Check(LoadFromTomlText<SampleAsset>(text.c_str(), loaded, &read_archive),
              "LoadFromTomlText 应成功");
        if (!read_archive.Ok())
            GLogError(u8"  失败字段=%s 原因=%s", read_archive.FailedField(),
                      read_archive.ErrorMessage());

        Check(loaded.schema == source.schema, "文本往返 schema");
        Check(hgl::stricmp(loaded.name.c_str(), source.name.c_str()) == 0, "文本往返 name");
        Check(loaded.count == source.count, "文本往返 count");
        Check(loaded.kind == source.kind, "文本往返枚举");
        Check(loaded.position.z == source.position.z, "文本往返向量");
        Check(loaded.weights.GetCount() == source.weights.GetCount(), "文本往返数组长度");
        Check(loaded.weights.GetCount() == 2 && loaded.weights[1] == 0.75f,
              "文本往返数组内容");

        // ③ 坏文本走类型化入口也必须失败，且行号仍可用
        TomlParseError entry_error;
        SampleAsset untouched;
        Check(!LoadFromTomlText<SampleAsset>("===", untouched, nullptr, &entry_error),
              "坏文本的类型化入口必须失败");
        Check(entry_error.line >= 1, "类型化入口也应给出行号");

        return true;
    }

    // ── 用例 N：HGL_ENUM_* 登记 —— 名字 / 说明 / 序列化一次写好 ────────────

    bool TestEnumMacroRegistration()
    {
        using archive_test::WeatherMode;

        // ① 名字与 GUI 说明都由登记表直接给出，没有第二份硬编码表
        Check(hgl::stricmp(EnumNameOf(WeatherMode::Rain), "Rain") == 0,
              "HGL_ENUM_ITEM 应自动生成登记名");
        Check(hgl::stricmp(EnumDescOf(WeatherMode::Rain), "雨天") == 0,
              "HGL_ENUM_ITEM 应自动保存 GUI 说明");
        Check(EnumNameOf(static_cast<WeatherMode>(200)) == nullptr,
              "未登记的值不应有名字");

        // ② 名字 → 值：大小写不敏感，数字写法也接受
        WeatherMode parsed = WeatherMode::Clear;
        Check(EnumParse("snow", parsed) && parsed == WeatherMode::Snow,
              "EnumParse 应大小写不敏感");
        Check(EnumParse("1", parsed) && parsed == WeatherMode::Rain,
              "EnumParse 应接受十进制数字");
        Check(!EnumParse("Nope", parsed), "未知名字应解析失败");

        // ③ GUI 项表：下拉框直接遍历，顺序即登记顺序
        Check(EnumItemCount<WeatherMode>() == 3, "登记项数量");
        const EnumItemMeta *item = EnumItemAt<WeatherMode>(2);
        Check(item != nullptr && hgl::stricmp(item->name, "Snow") == 0,
              "EnumItemAt 顺序");
        Check(EnumItemAt<WeatherMode>(3) == nullptr, "EnumItemAt 越界应返回 nullptr");

        // ④ Archive 往返：写名字、读名字
        Value tree;
        Archive writer = Archive::ForWrite(tree);
        WeatherMode mode = WeatherMode::Snow;
        writer.Field(1, "mode", mode);
        Check(writer.Ok(), "写入宏登记的枚举");

        const Value *node = tree.FindField("mode");
        Check(node != nullptr && node->IsString(), "枚举应写成字符串名");

        WeatherMode loaded = WeatherMode::Clear;
        Archive reader = Archive::ForRead(tree);
        reader.Field(1, "mode", loaded);
        Check(reader.Ok() && loaded == WeatherMode::Snow, "枚举名读回");

        // ⑤ TOML 文本往返
        const AnsiString text = ToTomlText(tree);
        Value reparsed;
        Check(FromTomlText(text.c_str(), reparsed), "宏登记枚举的 TOML 文本应可再解析");
        WeatherMode from_text = WeatherMode::Clear;
        Archive text_reader = Archive::ForRead(reparsed);
        text_reader.Field(1, "mode", from_text);
        Check(text_reader.Ok() && from_text == WeatherMode::Snow, "TOML 文本往返枚举");

        return true;
    }

    // ── 用例 N+1：HGL_FLAGS_* 登记 —— 名字数组 / 位运算 / 宽松读入 ─────────

    bool TestFlagsMacroRegistration()
    {
        using archive_test::PostFxFlags;

        const PostFxFlags combo = PostFxFlags::Bloom | PostFxFlags::Vignette;

        // ① 位运算：标记枚举的运算符由 EnumMeta.h 统一提供
        Check(HasFlag(combo, PostFxFlags::Bloom), "HasFlag 命中");
        Check(!HasFlag(combo, PostFxFlags::Tonemap), "HasFlag 未命中");
        Check((combo & PostFxFlags::Vignette) == PostFxFlags::Vignette, "按位与");
        Check((combo ^ PostFxFlags::Bloom) == PostFxFlags::Vignette, "按位异或");
        Check((combo | PostFxFlags::Tonemap) == static_cast<PostFxFlags>(0x7u),
              "按位或");

        // ② 说明同样来自登记表
        Check(hgl::stricmp(EnumDescOf(PostFxFlags::Tonemap), "色调映射") == 0,
              "标记项也应带 GUI 说明");

        // ③ 写：展开成名字数组
        Value tree;
        Archive writer = Archive::ForWrite(tree);
        PostFxFlags flags = combo;
        writer.Field(1, "post_fx", flags);
        Check(writer.Ok(), "写入按位标记");

        const Value *node = tree.FindField("post_fx");
        Check(node != nullptr && node->IsArray(), "按位标记应写成名字数组");
        if (node != nullptr && node->IsArray())
        {
            Check(node->GetElementCount() == 2, "标记数组元素数量");

            const Value *first = node->GetElement(0);
            Check(first != nullptr && first->IsString()
                      && hgl::stricmp(
                             reinterpret_cast<const char *>(first->GetString().c_str()),
                             "Bloom")
                             == 0,
                  "标记数组首项应按登记顺序为 Bloom");
        }

        // ④ 读：名字数组
        PostFxFlags loaded = PostFxFlags::None;
        Archive reader = Archive::ForRead(tree);
        reader.Field(1, "post_fx", loaded);
        Check(reader.Ok() && loaded == combo, "标记数组读回");

        // ⑤ 读：管道分隔文本（作者手写配置的常见形态）
        Value piped;
        piped.SetString("Bloom | Vignette");
        PostFxFlags from_pipe = PostFxFlags::None;
        Check(LoadEnum(piped, from_pipe) && from_pipe == combo, "应接受 | 分隔文本");

        // ⑥ 读：原始位掩码
        Value raw;
        raw.SetUInt(static_cast<uint64_t>(combo));
        PostFxFlags from_number = PostFxFlags::None;
        Check(LoadEnum(raw, from_number) && from_number == combo, "应接受原始位掩码");

        // ⑦ 0 值写成它的登记名，而不是空数组
        Value zero;
        StoreEnum(zero, PostFxFlags::None);
        Check(zero.IsArray() && zero.GetElementCount() == 1, "0 值应写成单项数组");
        if (zero.IsArray() && zero.GetElementCount() == 1)
        {
            const Value *only = zero.GetElement(0);
            Check(only != nullptr && only->IsString()
                      && hgl::stricmp(
                             reinterpret_cast<const char *>(only->GetString().c_str()),
                             "None")
                             == 0,
                  "0 值应写成登记名 None");
        }

        // ⑧ 含未登记位时必须退回整数，绝不能静默丢位
        Value unknown;
        StoreEnum(unknown, static_cast<PostFxFlags>(1u << 7));
        Check(unknown.IsInteger(), "未登记位应退回整数形式");

        return true;
    }

    // ── 用例 N+2：显式序列化名 + 下划线/大小写容错 ────────────────────────

    bool TestEnumExplicitWireName()
    {
        using archive_test::DirtFlags;
        using archive_test::ShapeKind;

        // ① 写出的是显式指定的蛇形名，不是 C++ 子名
        Check(hgl::stricmp(EnumNameOf(ShapeKind::PlaneGrid), "plane_grid") == 0,
              "HGL_ENUM_ITEM_NAME 应让 EnumNameOf 返回显式序列化名");

        // ② C++ 子名仍可作为备用拼写读回
        ShapeKind parsed = ShapeKind::HexSphere;
        Check(EnumParse("plane_grid", parsed) && parsed == ShapeKind::PlaneGrid,
              "应接受蛇形拼写");
        Check(EnumParse("PlaneGrid", parsed) && parsed == ShapeKind::PlaneGrid,
              "应接受 C++ 驼峰拼写");
        Check(EnumParse("PLANE-GRID", parsed) && parsed == ShapeKind::PlaneGrid,
              "应忽略大小写与 - 分隔符");
        Check(EnumParse("plane grid", parsed) == false,
              "空格不是分隔符，不应被接受");

        // ③ 写进树里的字面量就是蛇形名
        Value tree;
        Archive writer = Archive::ForWrite(tree);
        ShapeKind kind = ShapeKind::BoundingBox;
        writer.Field(1, "shape", kind);
        Check(writer.Ok(), "写入显式命名枚举");

        const Value *node = tree.FindField("shape");
        Check(node != nullptr && node->IsString(), "显式命名枚举应写成字符串");
        if (node != nullptr && node->IsString())
            Check(hgl::stricmp(reinterpret_cast<const char *>(node->GetString().c_str()),
                               "bounding_box")
                      == 0,
                  "写出的文本应为 bounding_box");

        ShapeKind loaded = ShapeKind::PlaneGrid;
        Archive reader = Archive::ForRead(tree);
        reader.Field(1, "shape", loaded);
        Check(reader.Ok() && loaded == ShapeKind::BoundingBox, "显式命名枚举读回");

        // ④ 按位标记同样支持显式序列化名
        const DirtFlags combo = DirtFlags::DoubleSided | DirtFlags::AlphaTest;

        Value flag_tree;
        Archive flag_writer = Archive::ForWrite(flag_tree);
        DirtFlags flags = combo;
        flag_writer.Field(1, "dirt", flags);
        Check(flag_writer.Ok(), "写入显式命名标记");

        const Value *flag_node = flag_tree.FindField("dirt");
        Check(flag_node != nullptr && flag_node->IsArray(), "显式命名标记应为数组");
        if (flag_node != nullptr && flag_node->IsArray()
            && flag_node->GetElementCount() == 2)
        {
            const Value *first = flag_node->GetElement(0);
            Check(first != nullptr && first->IsString()
                      && hgl::stricmp(
                             reinterpret_cast<const char *>(first->GetString().c_str()),
                             "double_sided")
                             == 0,
                  "标记项应写出显式序列化名");
        }
        else
        {
            Check(false, "标记项数组长度应为 2");
        }

        DirtFlags flag_loaded = DirtFlags::None;
        Archive flag_reader = Archive::ForRead(flag_tree);
        flag_reader.Field(1, "dirt", flag_loaded);
        Check(flag_reader.Ok() && flag_loaded == combo, "显式命名标记读回");

        // ⑤ 手写文本用 C++ 子名也应该能读
        Value cpp_named;
        cpp_named.SetString("DoubleSided | AlphaTest");
        DirtFlags from_cpp = DirtFlags::None;
        Check(LoadEnum(cpp_named, from_cpp) && from_cpp == combo,
              "标记读入应同时接受 C++ 子名");

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
    TestFindByPathAndExact();
    TestValidateKeysByMeta();
    TestWholeValueEntry();
    TestFlatBuffersRoundTrip();
    TestFlatBuffersMetaDriven();
    TestTomlTextEntry();
    TestEnumMacroRegistration();
    TestFlagsMacroRegistration();
    TestEnumExplicitWireName();

    if (g_failed != 0)
    {
        GLogError(u8"=== TestSerializeArchive: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestSerializeArchive: 全部测试通过 ===");
    return 0;
}
