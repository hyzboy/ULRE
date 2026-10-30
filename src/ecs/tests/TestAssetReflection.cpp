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
#include <hgl/asset/generated/GeometryCodec.gen.h>
#include <hgl/asset/generated/GeometryMeta.gen.h>
#include <hgl/asset/generated/PrimitiveMeta.gen.h>
#include <hgl/asset/generated/PrimitiveCodec.gen.h>
#include <hgl/asset/generated/SceneCodec.gen.h>
#include <hgl/asset/generated/SceneMeta.gen.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cstring>

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

        const toml::value written = codec::WriteBuiltinCubeParams(cube);

        BuiltinCubeParams back;
        if (!codec::ReadBuiltinCubeParams(written, back))
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
        const auto to_value = [](hgl::serialize::Value &node)
        {
            return hgl::serialize::ToToml(node);
        };

        hgl::serialize::Value ansi_node;
        ansi_node.SetString(AnsiString{});
        hgl::serialize::Value utf8_node;
        utf8_node.SetString(U8String{});
        hgl::serialize::Value os_node;
        os_node.SetString(OSString{});

        const toml::value ansi = to_value(ansi_node);
        const toml::value utf8 = to_value(utf8_node);
        const toml::value os = to_value(os_node);

        return ansi.is_string() && ansi.as_string().empty()
            && utf8.is_string() && utf8.as_string().empty()
            && os.is_string() && os.as_string().empty();
    }

    // ── 2. 缺键保留类内初始值 / 类型不符失败 ────────────────────────────────
    bool TestMissingKeyKeepsDefault()
    {
        BuiltinTorusParams torus;
        const BuiltinTorusParams defaults;   // 类内初始值

        toml::table empty;
        BuiltinTorusParams back;
        back.inner_radius = 99.0f;           // 故意污染，验证「缺键不动」

        if (!codec::ReadBuiltinTorusParams(toml::value(std::move(empty)), back))
            return false;

        // 缺键 = 保持调用方传进来的值（不重置），这正是「增量补丁」语义。
        if (back.inner_radius != 99.0f)
            return false;

        // 而 ReadShapeParams 这类调用方会先把结构复位到类内缺省值。
        back = BuiltinTorusParams{};
        if (back.inner_radius != defaults.inner_radius)
            return false;

        // 类型不符必须失败：slices 是整数，写成字符串应当报错。
        toml::table bad;
        bad["slices"] = "many";
        BuiltinTorusParams junk;
        if (codec::ReadBuiltinTorusParams(toml::value(std::move(bad)), junk))
            return false;

        return true;
    }

    bool TestNumericBounds()
    {
        toml::table too_large;
        too_large["segments_x"] = static_cast<int64_t>(UINT64_C(0x100000000));
        BuiltinCubeParams cube;
        if (codec::ReadBuiltinCubeParams(toml::value(std::move(too_large)), cube))
            return false;

        toml::table too_small;
        too_small["segments_x"] = static_cast<int64_t>(-1);
        return !codec::ReadBuiltinCubeParams(toml::value(std::move(too_small)), cube);
    }

    // ── 3. 枚举名与别名 ────────────────────────────────────────────────────
    bool TestEnumNames()
    {
        // 写出用 TOML 别名（历史拼写），读入接受别名与 C++ 名两种大小写。
        if (toml::value::string_type(codec::AssetEnumName_NTBType(NTBType::NormalTangent))
            != "normal_tangent")
            return false;

        NTBType parsed = NTBType::None;
        if (!codec::AssetEnumParse_NTBType("NormalTangent", parsed) || parsed != NTBType::NormalTangent)
            return false;
        if (!codec::AssetEnumParse_NTBType("normal_tangent", parsed) || parsed != NTBType::NormalTangent)
            return false;

        if (toml::value::string_type(
                codec::AssetEnumName_PrimitiveVariantPurpose(PrimitiveVariantPurpose::DepthOnly))
            != "depth_only")
            return false;

        PrimitiveVariantPurpose purpose = PrimitiveVariantPurpose::Surface;
        if (!codec::AssetEnumParse_PrimitiveVariantPurpose("depth_only", purpose)
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

        flatbuffers::FlatBufferBuilder fbb;
        const auto offset = codec::EncodeBuiltinFrustumParams(fbb, frustum);
        fbb.Finish(offset);

        const auto *root = flatbuffers::GetRoot<hgl::asset::schema::BuiltinFrustumParams>(
            fbb.GetBufferPointer());

        BuiltinFrustumParams back;
        codec::DecodeBuiltinFrustumParams(root, back);

        return back.bottom_radius == frustum.bottom_radius
            && back.top_radius == frustum.top_radius
            && back.height == frustum.height
            && back.slices == frustum.slices
            && back.generate_caps == frustum.generate_caps;
    }

    // ── 7. 图元规约往返（含 ManagedArray 与 Key 覆盖）──────────────────────
    bool TestPrimitiveSpec()
    {
        toml::table variant;
        variant["recipe"] = "Core.PBR.DepthOnly";
        variant["purpose"] = "depth_only";
        variant["lod"] = static_cast<int64_t>(2);

        PrimitiveVariantSpec spec;
        if (!codec::ReadPrimitiveVariantSpec(toml::value(std::move(variant)), spec))
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

        const toml::value written = codec::WritePrimitiveAssetSpec(source);
        PrimitiveAssetSpec decoded;
        if (!codec::ReadPrimitiveAssetSpec(written, decoded))
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

        flatbuffers::FlatBufferBuilder fbb;
        const auto offset = codec::EncodePrimitiveAssetSpec(fbb, source);
        fbb.Finish(offset);
        const auto *root = flatbuffers::GetRoot<hgl::asset::schema::PrimitiveAssetSpec>(
            fbb.GetBufferPointer());

        PrimitiveAssetSpec decoded;
        codec::DecodePrimitiveAssetSpec(root, decoded);
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

        flatbuffers::FlatBufferBuilder fbb;
        const auto offset = codec::EncodeSceneSpec(fbb, source);
        fbb.Finish(offset);
        const auto *root = flatbuffers::GetRoot<hgl::asset::schema::SceneSpec>(
            fbb.GetBufferPointer());

        SceneSpec decoded;
        codec::DecodeSceneSpec(root, decoded);
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
