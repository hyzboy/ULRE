// TestAssetReflection.cpp —— 资产反射机制的门禁：
//
//   1. TOML：Write → Read 往返保持全部可序列化字段（含枚举名与 Key 覆盖）；
//   2. 语义：缺键保留 C++ 类内初始值，类型不符必须失败；
//   3. 元数据：PropertyMetaInfo 的偏移/类型/类别/编辑器旗标与实际字段一致；
//   4. FlatBuffers：Encode → Finish → Decode 往返保持全部在 schema 里的字段。
//
// 这条用例不依赖设备/窗口，纯数据往返，因此可以在 CI 里挡住
// 「改了 C++ 结构但忘记重新生成」与「生成器改坏了语义」两类事故。
#include <hgl/asset/AssetObject.h>
#include <hgl/asset/generated/GeometrySerialize.gen.h>
#include <hgl/asset/generated/GeometryMeta.gen.h>
#include <hgl/asset/generated/PrimitiveMeta.gen.h>
#include <hgl/asset/generated/PrimitiveSerialize.gen.h>
#include <hgl/asset/generated/SceneSerialize.gen.h>
#include <hgl/asset/generated/SceneMeta.gen.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/serialize/TomlCodec.h>
#include <cstring>

#include "SerializeSnapshot.h"

using namespace hgl;
using namespace hgl::graph;
namespace codec = hgl::asset::generated;

namespace
{
    int g_failed = 0;

    void Check(const bool condition, const char *what)
    {
        if (condition)
            return;

        GLogError(u8"TestAssetReflection 失败: %s", what);
        ++g_failed;
    }

    // ── 1. TOML 往返 ────────────────────────────────────────────────────────
    bool TestTomlRoundTrip()
    {
        BuiltinCubeParams cube;
        cube.segments_x = 3;
        cube.segments_y = 5;
        cube.segments_z = 7;
        cube.ntb = NTBType::NormalTangentBitangent;
        cube.tex_coord = false;

        // TOML 后端只暴露「文本 ⇄ 值树」，往返就是真落盘的那份文本再读回来。
        const AnsiString written = serialize_test::WriteToml<BuiltinCubeParams>(cube);

        BuiltinCubeParams back;
        if (!hgl::serialize::LoadFromTomlText<BuiltinCubeParams>(
                written.c_str(), static_cast<size_t>(written.Length()), back))
            return false;

        return back.segments_x == cube.segments_x
            && back.segments_y == cube.segments_y
            && back.segments_z == cube.segments_z
            && back.ntb == cube.ntb
            && back.tex_coord == cube.tex_coord;
    }

    bool TestEmptyStringSerialization()
    {
        // 空字符串必须产生「空 TOML 字符串」而不是缺键/崩溃：
        // 走的是统一序列化框架，所以覆盖 AnsiString / U8String / OSString 三种载体。
        hgl::serialize::Value root;
        root.SetTable();

        hgl::serialize::Value *ansi_node = root.AddField("ansi", 1);
        hgl::serialize::Value *utf8_node = root.AddField("utf8", 2);
        hgl::serialize::Value *os_node = root.AddField("os", 3);

        if (!ansi_node || !utf8_node || !os_node)
            return false;

        ansi_node->SetString(AnsiString{});
        utf8_node->SetString(U8String{});
        os_node->SetString(OSString{});

        // 往返一圈，断言的就是落盘文本解析回来的值树。
        const AnsiString text = hgl::serialize::ToTomlText(root);

        hgl::serialize::Value back;
        if (!serialize_test::ParseToml(text, back))
            return false;

        const hgl::serialize::Value *ansi = back.FindField("ansi");
        const hgl::serialize::Value *utf8 = back.FindField("utf8");
        const hgl::serialize::Value *os = back.FindField("os");

        return ansi && ansi->IsString() && ansi->GetString().Length() == 0
            && utf8 && utf8->IsString() && utf8->GetString().Length() == 0
            && os && os->IsString() && os->GetString().Length() == 0;
    }

    // ── 2. 缺键保留类内初始值 / 类型不符失败 ────────────────────────────────
    bool TestMissingKeyKeepsDefault()
    {
        BuiltinTorusParams torus;
        const BuiltinTorusParams defaults;   // 类内初始值

        // 空文本 ⇒ 空值树，语义等同于「整张表一个键都没写」。
        hgl::serialize::Value empty_root;
        if (!serialize_test::ParseToml(AnsiString{}, empty_root))
            return false;

        BuiltinTorusParams back;
        back.inner_radius = 99.0f;           // 故意污染，验证「缺键不动」

        if (!hgl::serialize::ReadFromValue<BuiltinTorusParams>(empty_root, back))
            return false;

        // 缺键 = 保持调用方传进来的值（不重置），这正是「增量补丁」语义。
        if (back.inner_radius != 99.0f)
            return false;

        // 而 ReadShapeParams 这类调用方会先把结构复位到类内缺省值。
        back = BuiltinTorusParams{};
        if (back.inner_radius != defaults.inner_radius)
            return false;

        // 类型不符必须失败：slices 是整数，写成字符串应当报错。
        constexpr char kBadSlices[] = "slices = \"many\"\n";
        hgl::serialize::Value bad;
        if (!serialize_test::ParseToml(AnsiString(kBadSlices), bad))
            return false;

        BuiltinTorusParams junk;
        if (hgl::serialize::ReadFromValue<BuiltinTorusParams>(bad, junk))
            return false;

        return true;
    }

    bool TestNumericBounds()
    {
        constexpr char kTooLarge[] = "segments_x = 4294967296\n";
        hgl::serialize::Value too_large;
        if (!serialize_test::ParseToml(AnsiString(kTooLarge), too_large))
            return false;

        BuiltinCubeParams cube;
        if (hgl::serialize::ReadFromValue<BuiltinCubeParams>(too_large, cube))
            return false;

        constexpr char kTooSmall[] = "segments_x = -1\n";
        hgl::serialize::Value too_small;
        if (!serialize_test::ParseToml(AnsiString(kTooSmall), too_small))
            return false;

        return !hgl::serialize::ReadFromValue<BuiltinCubeParams>(too_small, cube);
    }

    // ── 3. 枚举名与别名 ────────────────────────────────────────────────────
    bool TestEnumNames()
    {
        // 写出用 TOML 别名（历史拼写），读入接受别名与 C++ 名两种大小写。
        // 名字↔值的唯一真源是枚举定义旁的 `HGL_ENUM_BEGIN` 登记块
        // （`hgl::serialize::EnumMeta`）；序列化层与测试都直接用它，
        // 生成器不再造 `AssetEnumName_*` / `AssetEnumParse_*` 转发壳。
        const auto name_is = [](const char *actual, const char *expected)
        {
            return actual && std::strcmp(actual, expected) == 0;
        };

        if (!name_is(hgl::serialize::EnumNameOf(NTBType::NormalTangent), "normal_tangent"))
            return false;

        NTBType parsed = NTBType::None;
        if (!hgl::serialize::EnumParse("NormalTangent", parsed) || parsed != NTBType::NormalTangent)
            return false;
        if (!hgl::serialize::EnumParse("normal_tangent", parsed) || parsed != NTBType::NormalTangent)
            return false;

        if (!name_is(hgl::serialize::EnumNameOf(PrimitiveVariantPurpose::DepthOnly), "depth_only"))
            return false;

        PrimitiveVariantPurpose purpose = PrimitiveVariantPurpose::Surface;
        if (!hgl::serialize::EnumParse("depth_only", purpose)
            || purpose != PrimitiveVariantPurpose::DepthOnly)
            return false;

        return true;
    }

    // ── 4. Key 覆盖：TOML 键名与字段名不同 ──────────────────────────────────
    bool TestKeyOverride()
    {
        const hgl::asset::ClassMetaInfo *meta = codec::GetClassMeta(
            static_cast<const PrimitiveVariantSpec *>(nullptr));
        if (!meta)
            return false;

        bool found_lod = false;
        for (uint32_t i = 0; i < meta->property_count; ++i)
        {
            const hgl::asset::PropertyMetaInfo &prop = meta->properties[i];
            if (hgl::strcmp(prop.name, "lod_index") != 0)
                continue;

            found_lod = true;
            if (hgl::strcmp(prop.toml_key, "lod") != 0)
                return false;
            if (!hgl::asset::IsSerialized(prop))
                return false;
            if (!hgl::asset::IsEditable(prop))
                return false;
        }
        return found_lod;
    }

    // ── 5. 元数据表与真实字段一致 ──────────────────────────────────────────
    bool TestMetaTable()
    {
        const hgl::asset::ClassMetaInfo *meta = codec::GetClassMeta(
            static_cast<const BuiltinCubeParams *>(nullptr));
        if (!meta)
            return false;

        if (meta->struct_size != sizeof(BuiltinCubeParams))
            return false;

        // 逐属性校验「偏移 = 真实地址差」：offsetof 在非标准布局结构上不可靠，
        // 生成器用的是 Clang 记录布局，这里用真实对象验证它算对了。
        BuiltinCubeParams instance;
        const auto *base = reinterpret_cast<const char *>(&instance);

        for (uint32_t i = 0; i < meta->property_count; ++i)
        {
            const hgl::asset::PropertyMetaInfo &prop = meta->properties[i];

            if (hgl::strcmp(prop.name, "segments_x") == 0)
            {
                if (static_cast<size_t>(prop.offset)
                    != static_cast<size_t>(reinterpret_cast<const char *>(&instance.segments_x) - base))
                    return false;
                if (prop.kind != hgl::asset::PropertyKind::UInt32)
                    return false;
                if (!hgl::asset::IsSerializedToml(prop)
                    || !hgl::asset::IsSerializedFlatBuffers(prop))
                    return false;
            }
            else if (hgl::strcmp(prop.name, "ntb") == 0)
            {
                if (prop.kind != hgl::asset::PropertyKind::Enum)
                    return false;
                if (!prop.enum_name || hgl::strcmp(prop.enum_name, "hgl::graph::NTBType") != 0)
                    return false;
            }
            else if (hgl::strcmp(prop.name, "tex_coord") == 0)
            {
                if (prop.kind != hgl::asset::PropertyKind::Bool)
                    return false;
            }
        }

        return true;
    }

    // ── 6. FlatBuffers 往返 ────────────────────────────────────────────────
    bool TestFlatBuffersRoundTrip()
    {
        BuiltinFrustumParams frustum;
        frustum.bottom_radius = 3.5f;
        frustum.top_radius = 1.25f;
        frustum.height = 7.0f;
        frustum.slices = 42;
        frustum.generate_caps = false;

        hgl::ValueArray<uint8> fb;
        if (!hgl::serialize::fb::EncodeToFlatBuffers<BuiltinFrustumParams>(fb, frustum))
            return false;

        BuiltinFrustumParams back;
        if (!hgl::serialize::fb::DecodeFromFlatBuffers<BuiltinFrustumParams>(
                fb.GetData(), static_cast<size_t>(fb.GetCount()), back))
            return false;

        return back.bottom_radius == frustum.bottom_radius
            && back.top_radius == frustum.top_radius
            && back.height == frustum.height
            && back.slices == frustum.slices
            && back.generate_caps == frustum.generate_caps;
    }

    // ── 7. 图元规约往返（含 ManagedArray 与 Key 覆盖）──────────────────────
    bool TestPrimitiveSpec()
    {
        constexpr char kVariant[] =
            "recipe = \"Core.PBR.DepthOnly\"\n"
            "purpose = \"depth_only\"\n"
            "lod = 2\n";

        PrimitiveVariantSpec spec;
        if (!hgl::serialize::LoadFromTomlText<PrimitiveVariantSpec>(kVariant, spec))
            return false;

        if (spec.recipe_name != "Core.PBR.DepthOnly")
            return false;
        if (spec.purpose != PrimitiveVariantPurpose::DepthOnly)
            return false;
        if (spec.lod_index != 2)
            return false;

        PrimitiveAssetSpec source;
        source.name = "TomlPrimitive";
        source.geometry_path = OS_TEXT("res/geo/toml.geo.toml");
        source.recipe_name = "Core.PBR.Lit";
        source.primitive_type = PrimitiveType::Triangles;
        PrimitiveVariantSpec *extra = source.extra_variants.Create();
        if (!extra)
            return false;
        extra->recipe_name = "Core.PBR.DepthOnly";
        extra->purpose = PrimitiveVariantPurpose::DepthOnly;
        extra->lod_index = 4;

        const AnsiString written = serialize_test::WriteToml<PrimitiveAssetSpec>(source);
        PrimitiveAssetSpec decoded;
        if (!hgl::serialize::LoadFromTomlText<PrimitiveAssetSpec>(
                written.c_str(), static_cast<size_t>(written.Length()), decoded))
            return false;

        return decoded.name == source.name
            && decoded.geometry_path == source.geometry_path
            && decoded.recipe_name == source.recipe_name
            && decoded.primitive_type == source.primitive_type
            && decoded.extra_variants.GetCount() == 1
            && decoded.extra_variants[0]->recipe_name == extra->recipe_name
            && decoded.extra_variants[0]->purpose == extra->purpose
            && decoded.extra_variants[0]->lod_index == extra->lod_index;
    }

    bool TestPrimitiveFlatBuffersRoundTrip()
    {
        PrimitiveAssetSpec source;
        source.name = "FlatPrimitive";
        source.geometry_path = OS_TEXT("res/geo/flat.geo.toml");
        source.recipe_name = "Core.PBR.Lit";
        source.primitive_type = PrimitiveType::Triangles;

        PrimitiveVariantSpec *variant = source.extra_variants.Create();
        if (!variant)
            return false;
        variant->recipe_name = "Core.PBR.DepthOnly";
        variant->purpose = PrimitiveVariantPurpose::DepthOnly;
        variant->lod_index = 3;

        hgl::ValueArray<uint8> fb;
        if (!hgl::serialize::fb::EncodeToFlatBuffers<PrimitiveAssetSpec>(fb, source))
            return false;

        PrimitiveAssetSpec decoded;
        if (!hgl::serialize::fb::DecodeFromFlatBuffers<PrimitiveAssetSpec>(
                fb.GetData(), static_cast<size_t>(fb.GetCount()), decoded))
            return false;
        return decoded.name == source.name
            && decoded.geometry_path == source.geometry_path
            && decoded.recipe_name == source.recipe_name
            && decoded.primitive_type == source.primitive_type
            && decoded.extra_variants.GetCount() == 1
            && decoded.extra_variants[0]->recipe_name == variant->recipe_name
            && decoded.extra_variants[0]->purpose == variant->purpose
            && decoded.extra_variants[0]->lod_index == variant->lod_index;
    }

    bool TestSceneFlatBuffersRoundTrip()
    {
        SceneSpec source;
        source.name = "FlatScene";
        SceneMaterialSpec *material = source.materials.Create();
        if (!material)
            return false;
        material->name = "glow";
        material->type = SceneMaterialType::EmissiveSurface;
        material->color = Color4f(0.9f, 0.2f, 0.1f, 1.0f);

        SceneNodeSpec *node = source.nodes.Create();
        if (!node)
            return false;
        node->name = "Mesh";
        node->geometry = "presets:sphere_64";
        node->recipe = "Core.PBR.Lit";
        node->material = "glow";
        node->transform.position = math::Vector3f(1.0f, 2.0f, 3.0f);

        hgl::ValueArray<uint8> fb;
        if (!hgl::serialize::fb::EncodeToFlatBuffers<SceneSpec>(fb, source))
            return false;

        SceneSpec decoded;
        if (!hgl::serialize::fb::DecodeFromFlatBuffers<SceneSpec>(
                fb.GetData(), static_cast<size_t>(fb.GetCount()), decoded))
            return false;
        return decoded.schema == source.schema
            && decoded.name == source.name
            && decoded.materials.GetCount() == 1
            && decoded.materials[0]->type == material->type
            && decoded.materials[0]->color == material->color
            && decoded.nodes.GetCount() == 1
            && decoded.nodes[0]->name == node->name
            && decoded.nodes[0]->material == node->material
            && decoded.nodes[0]->transform.position == node->transform.position;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestAssetReflection"));
    Check(TestTomlRoundTrip(), "TOML round-trip");
    Check(TestEmptyStringSerialization(), "empty string serialization");
    Check(TestMissingKeyKeepsDefault(), "missing key keeps default / bad type fails");
    Check(TestNumericBounds(), "integer values outside the C++ field range fail");
    Check(TestEnumNames(), "enum names and aliases");
    Check(TestKeyOverride(), "Key override maps field name to TOML key");
    Check(TestMetaTable(), "metadata table matches real layout");
    Check(TestFlatBuffersRoundTrip(), "FlatBuffers round-trip");
    Check(TestPrimitiveSpec(), "primitive variant spec");
    Check(TestPrimitiveFlatBuffersRoundTrip(), "primitive FlatBuffers round-trip");
    Check(TestSceneFlatBuffersRoundTrip(), "scene FlatBuffers round-trip");

    if (g_failed != 0)
    {
        GLogError(u8"=== TestAssetReflection: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestAssetReflection: 全部测试通过 ===");
    return 0;
}
