// TestMaterialRecipeCodec.cpp —— 材质配方规约（`*.recipe.toml`）的字段契约门禁：
//
//   1. 完整字段「文本 → 规约 → 文本」往返稳定（含 `Key` 覆盖、嵌套表、表数组）；
//   2. `Presence<T>` 三态：缺键 / 显式写缺省值 / 显式写非缺省值；
//   3. `[render_state.pipeline]` 逐字段覆盖：只写一个键时其余 10 个必须不 present；
//   4. 字符串数组（`samplers` / `defines`）：`[]` 与「不写这一行」语义不同；
//   5. 枚举拼写：内部枚举走 `stricmp`，转发到 `VkEnumText` 的外部枚举走 `strcmp`；
//   6. 类型不符必须失败（不能静默吞掉）；
//   7. FlatBuffers Encode → Finish → Decode 往返保持 presence 与字符串数组；
//   8. 元数据 `struct_meta` 接线正确（解析层的严格键校验靠它递归）。
//
// 纯数据往返，不依赖设备 / 窗口，因此在 CI 里也能跑。
#include <hgl/asset/AssetObject.h>
#include <hgl/asset/generated/MaterialRecipeCodec.gen.h>
#include <hgl/asset/generated/MaterialRecipeMeta.gen.h>
#include <hgl/graph/asset/MaterialRecipeSpec.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/serialize/Archive.h>
#include <hgl/serialize/TomlCodec.h>

using namespace hgl;
using namespace hgl::asset;
using namespace hgl::graph::mtl;
namespace codec = hgl::asset::generated;

namespace
{
    int g_failed = 0;

    void Check(const bool condition, const char *what)
    {
        if (condition)
            return;

        GLogError(u8"TestMaterialRecipeCodec 失败: %s", what);
        ++g_failed;
    }

    /// 文本 → 规约。走与生成代码完全相同的路径（Value 树 + Archive），
    /// 因此这里测到的 `Presence` 行为就是解析器将来拿到的行为。
    bool ReadText(const char *text, MaterialRecipeSpec &out)
    {
        hgl::serialize::Value node;
        if (!hgl::serialize::FromTomlText(text, node))
            return false;

        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<MaterialRecipeSpec>::Do(ar, out);
        return ar.Ok();
    }

    /// `[render_state.pipeline]` 里除 `cull_mode` 之外的 10 个字段必须都没出现。
    bool RestOfPipelineAbsent(const RecipePipelineSpec &p)
    {
        return !p.depth_test.present
            && !p.depth_write.present
            && !p.depth_compare_op.present
            && !p.alpha_blend.present
            && !p.blend_src.present
            && !p.blend_dst.present
            && !p.dynamic_line_width.present
            && !p.overlay.present
            && !p.wireframe.present
            && !p.line_width.present;
    }

    // ── 1. 完整字段文本往返 ─────────────────────────────────────────────────
    constexpr char kFullRecipe[] =
        "schema = 1\n"
        "name = \"RecipeCodec.Full\"\n"
        "domain = \"UnlitTexture\"\n"
        "\n"
        "[fragment]\n"
        "surface_module_name = \"custom_surface\"\n"
        "samplers = [\"Sampler2D\", \"Sampler2D\"]\n"
        "defines = [\"NORMAL_MAP\", \"ALPHA_TEST\"]\n"
        "\n"
        "[transform]\n"
        "source = \"Vec2Position\"\n"
        "mapping = \"LiftXY_XY0\"\n"
        "orientation = \"World\"\n"
        "scale = \"World\"\n"
        "projection = \"OrthoViewport\"\n"
        "\n"
        "[render_state]\n"
        "alpha_test = true\n"
        "alpha_cutoff = 0.25\n"
        "dither = true\n"
        "\n"
        "[render_state.pipeline]\n"
        "cull_mode = \"FrontAndBack\"\n"
        "depth_test = false\n"
        "depth_write = false\n"
        "depth_compare_op = \"Always\"\n"
        "alpha_blend = true\n"
        "blend_src = \"SrcAlpha\"\n"
        "blend_dst = \"OneMinusSrcAlpha\"\n"
        "dynamic_line_width = true\n"
        "overlay = true\n"
        "wireframe = true\n"
        "line_width = 2.5\n"
        "\n"
        "[[textures]]\n"
        "name = \"base_color\"\n"
        "asset = \"res/image/lena.Tex2D\"\n"
        "required = true\n"
        "layer = 2\n"
        "auto_mipmaps = true\n"
        "\n"
        "[[textures]]\n"
        "name = \"normal\"\n"
        "asset = \"res/image/normal.Tex2D\"\n"
        "required = false\n";

    bool TestFullTextRoundTrip()
    {
        MaterialRecipeSpec spec;
        if (!ReadText(kFullRecipe, spec))
            return false;

        const RecipePipelineSpec &pipeline = spec.render_state.pipeline.value;
        const bool read_ok =
               spec.schema == 1
            && spec.name == "RecipeCodec.Full"
            && spec.domain == MaterialDomain::UnlitTexture
            && spec.fragment.surface_module_name == "custom_surface"
            && spec.fragment.samplers.present
            && spec.fragment.samplers.value.GetCount() == 2
            && *spec.fragment.samplers.value[0] == "Sampler2D"
            && *spec.fragment.samplers.value[1] == "Sampler2D"
            && spec.fragment.defines.present
            && spec.fragment.defines.value.GetCount() == 2
            && *spec.fragment.defines.value[1] == "ALPHA_TEST"
            && spec.transform.input.present
            && spec.transform.input.value == VertexInputMode::Vec2Position
            && spec.transform.position_mapping.value == PositionMappingMode::LiftXY_XY0
            && spec.transform.orientation.value == OrientationMode::World
            && spec.transform.scale.value == ScaleMode::World
            && spec.transform.projection.value == ProjectionMode::OrthoViewport
            && spec.render_state.alpha_test.present
            && spec.render_state.alpha_test.value
            && spec.render_state.alpha_cutoff.present
            && spec.render_state.alpha_cutoff.value == 0.25f
            && spec.render_state.dither.value
            && spec.render_state.pipeline.present
            && pipeline.cull_mode.present
            && pipeline.cull_mode.value == VK_CULL_MODE_FRONT_AND_BACK
            && pipeline.depth_test.present
            && !pipeline.depth_test.value
            && pipeline.depth_compare_op.value == VK_COMPARE_OP_ALWAYS
            && pipeline.alpha_blend.value
            && pipeline.blend_src.value == VK_BLEND_FACTOR_SRC_ALPHA
            && pipeline.blend_dst.value == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
            && pipeline.dynamic_line_width.present
            && pipeline.overlay.value
            && pipeline.wireframe.value
            && pipeline.line_width.present
            && pipeline.line_width.value == 2.5f
            && spec.textures.GetCount() == 2
            && spec.textures[0]->texture_name == "base_color"
            && spec.textures[0]->resource_id == "res/image/lena.Tex2D"
            && spec.textures[0]->required
            && spec.textures[0]->layer == 2
            && spec.textures[0]->auto_mipmaps
            && spec.textures[1]->texture_name == "normal"
            && !spec.textures[1]->required
            && spec.textures[1]->layer == 0;

        if (!read_ok)
            return false;

        // 写成文本快照后再次读回：值、presence、`Key` 覆盖、表数组都要原样。
        toml::value written = codec::WriteMaterialRecipeSpec(spec);

        MaterialRecipeSpec back;
        if (!codec::ReadMaterialRecipeSpec(written, back))
            return false;

        const RecipePipelineSpec &back_pipeline = back.render_state.pipeline.value;
        return back.schema == spec.schema
            && back.name == spec.name
            && back.domain == spec.domain
            && back.fragment.surface_module_name == "custom_surface"
            && back.fragment.samplers.present
            && back.fragment.samplers.value.GetCount() == 2
            && back.fragment.defines.present
            && back.fragment.defines.value.GetCount() == 2
            && back.transform.input.value == VertexInputMode::Vec2Position
            && back.transform.projection.value == ProjectionMode::OrthoViewport
            && back.render_state.alpha_cutoff.value == 0.25f
            && back.render_state.pipeline.present
            && back_pipeline.cull_mode.value == VK_CULL_MODE_FRONT_AND_BACK
            && back_pipeline.line_width.value == 2.5f
            && back_pipeline.depth_write.present
            && back.textures.GetCount() == 2
            && back.textures[0]->texture_name == "base_color"
            && back.textures[1]->texture_name == "normal"
            // `Key` 覆盖：字段名是 `input`，写出去必须是 `source`。
            && written.at("transform").as_table().count("source") == 1
            && written.at("transform").as_table().count("input") == 0
            && written.at("textures").as_array().size() == 2;
    }

    // ── 2. Presence 三态 ────────────────────────────────────────────────────
    bool TestPresenceTriState()
    {
        // 缺键：整个 `[render_state.pipeline]` 都没有。
        constexpr char kAbsent[] =
            "schema = 1\n"
            "name = \"RecipeCodec.Absent\"\n"
            "domain = \"UnlitTexture\"\n";

        MaterialRecipeSpec absent;
        if (!ReadText(kAbsent, absent))
            return false;

        if (absent.render_state.pipeline.present)
            return false;
        if (absent.render_state.alpha_test.present || absent.render_state.alpha_cutoff.present
            || absent.render_state.dither.present)
            return false;
        if (absent.fragment.samplers.present || absent.fragment.defines.present)
            return false;
        if (absent.textures.GetCount() != 0)
            return false;

        // 显式写缺省值：`depth_test` / `alpha_blend` / `line_width` 的类内缺省
        // 分别是 false / false / 0.0，写出来必须 `present == true`；
        // 其余 8 个键没写就只能是 `present == false`。
        constexpr char kExplicitDefault[] =
            "schema = 1\n"
            "name = \"RecipeCodec.ExplicitDefault\"\n"
            "domain = \"UnlitTexture\"\n"
            "[render_state.pipeline]\n"
            "depth_test = false\n"
            "alpha_blend = false\n"
            "line_width = 0.0\n";

        MaterialRecipeSpec explicit_default;
        if (!ReadText(kExplicitDefault, explicit_default))
            return false;

        if (!explicit_default.render_state.pipeline.present)
            return false;

        const RecipePipelineSpec &p = explicit_default.render_state.pipeline.value;
        if (!p.depth_test.present || p.depth_test.value)
            return false;
        if (!p.alpha_blend.present || p.alpha_blend.value)
            return false;
        if (!p.line_width.present || p.line_width.value != 0.0f)
            return false;
        if (p.cull_mode.present
            || p.depth_write.present
            || p.depth_compare_op.present
            || p.blend_src.present
            || p.blend_dst.present
            || p.dynamic_line_width.present
            || p.overlay.present
            || p.wireframe.present)
            return false;

        // 写方向：未 present 的键不能出现在快照里，否则往返会膨胀成整份声明。
        toml::value written = codec::WriteMaterialRecipeSpec(explicit_default);
        return written.at("render_state").as_table().at("pipeline").as_table().size() == 3;
    }

    bool TestPartialPipeline()
    {
        constexpr char kPartial[] =
            "schema = 1\n"
            "name = \"RecipeCodec.Partial\"\n"
            "domain = \"StandardOpaque\"\n"
            "[render_state.pipeline]\n"
            "cull_mode = \"None\"\n";

        MaterialRecipeSpec spec;
        if (!ReadText(kPartial, spec))
            return false;

        if (!spec.render_state.pipeline.present)
            return false;
        if (!RestOfPipelineAbsent(spec.render_state.pipeline.value))
            return false;

        const RecipePipelineSpec &p = spec.render_state.pipeline.value;
        if (!p.cull_mode.present || p.cull_mode.value != VK_CULL_MODE_NONE)
            return false;

        // 局部覆盖往返后仍只有 1 个键。
        toml::value written = codec::WriteMaterialRecipeSpec(spec);
        return written.at("render_state").as_table().at("pipeline").as_table().size() == 1;
    }

    bool TestEmptyPipelineTable()
    {
        // 「有这张表但一个键都没写」是作者的书写错误，规约层只如实记录
        // 「表出现过」，判定留给解析层的跨字段校验。
        constexpr char kEmpty[] =
            "schema = 1\n"
            "name = \"RecipeCodec.EmptyPipeline\"\n"
            "domain = \"UnlitTexture\"\n"
            "[render_state.pipeline]\n";

        MaterialRecipeSpec spec;
        if (!ReadText(kEmpty, spec))
            return false;

        if (!spec.render_state.pipeline.present)
            return false;
        if (!RestOfPipelineAbsent(spec.render_state.pipeline.value))
            return false;
        if (spec.render_state.pipeline.value.cull_mode.present)
            return false;

        // 空表往返仍是「表存在、零个键」。
        toml::value written = codec::WriteMaterialRecipeSpec(spec);
        MaterialRecipeSpec back;
        if (!codec::ReadMaterialRecipeSpec(written, back))
            return false;

        return back.render_state.pipeline.present
            && RestOfPipelineAbsent(back.render_state.pipeline.value)
            && !back.render_state.pipeline.value.cull_mode.present;
    }

    // ── 3. 字符串数组：`[]` ≠ 缺键 ─────────────────────────────────────────
    bool TestStringArrayPresence()
    {
        constexpr char kEmptyArray[] =
            "schema = 1\n"
            "name = \"RecipeCodec.EmptyArray\"\n"
            "domain = \"UnlitTexture\"\n"
            "[fragment]\n"
            "samplers = []\n";

        MaterialRecipeSpec spec;
        if (!ReadText(kEmptyArray, spec))
            return false;

        // `samplers = []`：出现过、内容为空 ⇒ 整表清空。
        if (!spec.fragment.samplers.present)
            return false;
        if (spec.fragment.samplers.value.GetCount() != 0)
            return false;
        // `defines` 没写 ⇒ 保留领域预设。
        if (spec.fragment.defines.present)
            return false;

        toml::value written = codec::WriteMaterialRecipeSpec(spec);
        const toml::table &fragment = written.at("fragment").as_table();
        if (fragment.count("samplers") != 1 || fragment.count("defines") != 0)
            return false;

        MaterialRecipeSpec back;
        if (!codec::ReadMaterialRecipeSpec(written, back))
            return false;

        return back.fragment.samplers.present
            && back.fragment.samplers.value.GetCount() == 0
            && !back.fragment.defines.present;
    }

    // ── 4. 枚举拼写 ────────────────────────────────────────────────────────
    bool TestEnumSpelling()
    {
        // 枚举拼写表只有一份真源：`AssetGen` 的 `[[enum]]` 配置把引擎枚举
        // **转发**到 `ParseMaterialDomain` / `ParseVertexInputMode` /
        // `ParseProjectionMode` / `ParseVkCullMode` 等真源函数，生成器不再自带
        // 字面量表。因此口径与真源一致：`strcmp`，大小写敏感。
        constexpr char kLowerCase[] =
            "schema = 1\n"
            "name = \"RecipeCodec.LowerCase\"\n"
            "domain = \"unlittexture\"\n"
            "[transform]\n"
            "source = \"vec2position\"\n"
            "projection = \"orthoviewport\"\n";

        MaterialRecipeSpec rejected_case;
        if (ReadText(kLowerCase, rejected_case))
            return false;

        // 规范拼写必须通过；未写的可选字段保持缺席（presence 只由「写没写」决定）。
        constexpr char kCanonical[] =
            "schema = 1\n"
            "name = \"RecipeCodec.Canonical\"\n"
            "domain = \"UnlitTexture\"\n"
            "[transform]\n"
            "source = \"Vec2Position\"\n"
            "projection = \"OrthoViewport\"\n";

        MaterialRecipeSpec spec;
        if (!ReadText(kCanonical, spec))
            return false;
        if (spec.domain != MaterialDomain::UnlitTexture)
            return false;
        if (spec.transform.input.value != VertexInputMode::Vec2Position)
            return false;
        if (spec.transform.projection.value != ProjectionMode::OrthoViewport)
            return false;
        if (spec.transform.orientation.present || spec.transform.scale.present)
            return false;

        // 外部枚举（转发到 `VkEnumText`）走的也是 `strcmp`：拼写必须与真源逐字一致。
        constexpr char kBadCase[] =
            "schema = 1\n"
            "name = \"RecipeCodec.BadCase\"\n"
            "domain = \"UnlitTexture\"\n"
            "[render_state.pipeline]\n"
            "cull_mode = \"frontandback\"\n";

        MaterialRecipeSpec rejected;
        if (ReadText(kBadCase, rejected))
            return false;

        // 带下划线的枚举内部名同样不是合法拼写。
        constexpr char kEnumTag[] =
            "schema = 1\n"
            "name = \"RecipeCodec.EnumTag\"\n"
            "domain = \"UnlitTexture\"\n"
            "[render_state.pipeline]\n"
            "cull_mode = \"VK_CULL_MODE_NONE\"\n";

        MaterialRecipeSpec rejected_tag;
        return !ReadText(kEnumTag, rejected_tag);
    }

    // ── 5. 类型不符必须失败 ────────────────────────────────────────────────
    bool TestTypeMismatchRejected()
    {
        constexpr char kSchemaAsText[] =
            "schema = \"one\"\n"
            "name = \"RecipeCodec.BadType\"\n";

        MaterialRecipeSpec a;
        if (ReadText(kSchemaAsText, a))
            return false;

        constexpr char kLayerAsText[] =
            "schema = 1\n"
            "name = \"RecipeCodec.BadLayer\"\n"
            "[[textures]]\n"
            "name = \"base_color\"\n"
            "asset = \"res/image/lena.Tex2D\"\n"
            "layer = \"2\"\n";

        MaterialRecipeSpec b;
        if (ReadText(kLayerAsText, b))
            return false;

        // `line_width` 是 float，写布尔必须被拒。
        constexpr char kLineWidthAsBool[] =
            "schema = 1\n"
            "name = \"RecipeCodec.BadLineWidth\"\n"
            "[render_state.pipeline]\n"
            "line_width = true\n";

        MaterialRecipeSpec c;
        if (ReadText(kLineWidthAsBool, c))
            return false;

        // `samplers` 是字符串数组，给单个字符串必须被拒。
        constexpr char kSamplersAsString[] =
            "schema = 1\n"
            "name = \"RecipeCodec.BadSamplers\"\n"
            "[fragment]\n"
            "samplers = \"Sampler2D\"\n";

        MaterialRecipeSpec d;
        return !ReadText(kSamplersAsString, d);
    }

    // ── 6. FlatBuffers 往返 ────────────────────────────────────────────────
    bool TestFlatBuffersRoundTrip()
    {
        MaterialRecipeSpec source;
        source.schema = 1;
        source.name = "RecipeCodec.Flat";
        source.domain = MaterialDomain::StandardOpaque;
        source.fragment.surface_module_name = "flat_surface";

        AnsiString *sampler = source.fragment.samplers.value.Create();
        if (!sampler)
            return false;
        *sampler = "Sampler2D";
        sampler = source.fragment.samplers.value.Create();
        if (!sampler)
            return false;
        *sampler = "Sampler2DArray";
        source.fragment.samplers.present = true;

        // `defines` 故意留成「出现过但为空」，用来验证空数组不被当成缺键。
        source.fragment.defines.present = true;

        source.transform.input = VertexInputMode::Vec3Position;
        source.transform.projection = ProjectionMode::LocalToWorldOnly;

        // 局部覆盖：只写 2 个管线键。
        source.render_state.pipeline.present = true;
        source.render_state.pipeline.value.cull_mode = VK_CULL_MODE_BACK_BIT;
        source.render_state.pipeline.value.line_width = 1.5f;

        RecipeTextureSpec *tex = source.textures.Create();
        if (!tex)
            return false;
        tex->texture_name = "base_color";
        tex->resource_id = "res/image/lena.Tex2D";
        tex->layer = 3;
        tex->auto_mipmaps = true;

        tex = source.textures.Create();
        if (!tex)
            return false;
        tex->texture_name = "normal";
        tex->resource_id = "res/image/normal.Tex2D";
        tex->required = false;

        flatbuffers::FlatBufferBuilder fbb;
        const auto offset = codec::EncodeMaterialRecipeSpec(fbb, source);
        fbb.Finish(offset);

        const auto *root = flatbuffers::GetRoot<::hgl::asset::schema::MaterialRecipeSpec>(
            fbb.GetBufferPointer());

        MaterialRecipeSpec back;
        codec::DecodeMaterialRecipeSpec(root, back);

        const RecipePipelineSpec &pipeline = back.render_state.pipeline.value;
        return back.schema == source.schema
            && back.name == source.name
            && back.domain == MaterialDomain::StandardOpaque
            && back.fragment.surface_module_name == "flat_surface"
            && back.fragment.samplers.present
            && back.fragment.samplers.value.GetCount() == 2
            && *back.fragment.samplers.value[0] == "Sampler2D"
            && *back.fragment.samplers.value[1] == "Sampler2DArray"
            && back.fragment.defines.present
            && back.fragment.defines.value.GetCount() == 0
            && back.transform.input.present
            && back.transform.input.value == VertexInputMode::Vec3Position
            && back.transform.projection.present
            && back.transform.projection.value == ProjectionMode::LocalToWorldOnly
            && !back.transform.orientation.present
            && back.render_state.pipeline.present
            && pipeline.cull_mode.present
            && pipeline.cull_mode.value == VK_CULL_MODE_BACK_BIT
            && pipeline.line_width.present
            && pipeline.line_width.value == 1.5f
            && !pipeline.depth_test.present
            && !pipeline.wireframe.present
            && back.textures.GetCount() == 2
            && back.textures[0]->texture_name == "base_color"
            && back.textures[0]->layer == 3
            && back.textures[0]->auto_mipmaps
            && back.textures[1]->texture_name == "normal"
            && !back.textures[1]->required;
    }

    // ── 7. 元数据：struct_meta 递归接线 ────────────────────────────────────
    const PropertyMetaInfo *FindProperty(const ClassMetaInfo &meta, const char *name)
    {
        for (uint32_t i = 0; i < meta.property_count; ++i)
        {
            if (meta.properties[i].name && AnsiString(meta.properties[i].name) == name)
                return &meta.properties[i];
        }
        return nullptr;
    }

    bool TestMetaStructLinks()
    {
        const ClassMetaInfo &recipe = codec::Class_MaterialRecipeSpec;
        if (recipe.property_count != 7)
            return false;

        const PropertyMetaInfo *fragment = FindProperty(recipe, "fragment");
        const PropertyMetaInfo *transform = FindProperty(recipe, "transform");
        const PropertyMetaInfo *render_state = FindProperty(recipe, "render_state");
        const PropertyMetaInfo *textures = FindProperty(recipe, "textures");
        if (!fragment || !transform || !render_state || !textures)
            return false;

        if (fragment->struct_meta != &codec::Class_RecipeFragmentSpec)
            return false;
        if (transform->struct_meta != &codec::Class_RecipeTransformSpec)
            return false;
        if (render_state->struct_meta != &codec::Class_RecipeRenderStateSpec)
            return false;
        if (textures->struct_meta != &codec::Class_RecipeTextureSpec)
            return false;

        const PropertyMetaInfo *pipeline = FindProperty(
            *render_state->struct_meta, "pipeline");
        if (!pipeline || pipeline->struct_meta != &codec::Class_RecipePipelineSpec)
            return false;

        // `Presence<T>` 的标量字段没有子结构，必须留空。
        const PropertyMetaInfo *cull_mode = FindProperty(*pipeline->struct_meta, "cull_mode");
        if (!cull_mode || cull_mode->struct_meta != nullptr)
            return false;

        return recipe.struct_size == sizeof(MaterialRecipeSpec)
            && codec::Class_RecipePipelineSpec.property_count == 11;
    }
}//namespace

int main()
{
    Check(TestFullTextRoundTrip(), "完整字段文本往返");
    Check(TestPresenceTriState(), "Presence 三态");
    Check(TestPartialPipeline(), "pipeline 局部覆盖");
    Check(TestEmptyPipelineTable(), "空 pipeline 表");
    Check(TestStringArrayPresence(), "字符串数组 presence");
    Check(TestEnumSpelling(), "枚举拼写");
    Check(TestTypeMismatchRejected(), "类型不符必须失败");
    Check(TestFlatBuffersRoundTrip(), "FlatBuffers 往返");
    Check(TestMetaStructLinks(), "元数据 struct_meta 接线");

    if (g_failed > 0)
    {
        GLogError(u8"=== TestMaterialRecipeCodec: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestMaterialRecipeCodec: 全部测试通过 ===");
    return 0;
}
