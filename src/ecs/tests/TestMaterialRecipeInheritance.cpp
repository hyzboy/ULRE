/**
 * TestMaterialRecipeInheritance —— 配方差量继承（`parent`）的回归测试。
 *
 * 覆盖三层语义：
 *   1. 纯函数层：`MergeMaterialRecipeSpec` 的逐字段/整表/按名合并口径；
 *   2. 引用解析层：`ParseMaterialRecipeParentRef` 认哪些写法；
 *   3. 注册表层：两阶段加载（子先注册、父后注册）、父缺失、父链成环。
 *
 * 为什么这一层必须单独测：合并只能在**规约层**做（`MaterialRecipe` 是压平结果，
 * 没有「这一项写没写」的信息）。因此这里的断言全部打在 `MaterialRecipeSpec` 上，
 * 注册表用例才顺带验一次「合并后能构建出期望的运行时配方」。
 */
#include <hgl/graph/asset/MaterialRecipeSpec.h>
#include <hgl/asset/generated/MaterialRecipeMeta.gen.h>
#include <hgl/asset/generated/MaterialRecipeSerialize.gen.h>
#include <hgl/mtl/MaterialRecipeInherit.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/serialize/Archive.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/serialize/Value.h>
#include <hgl/utf.h>

#include <cstring>

using namespace hgl::graph::mtl;
using hgl::serialize::Archive;
using hgl::serialize::FromTomlText;
using hgl::serialize::StructSerializer;
using hgl::serialize::Value;

namespace
{
    int g_failed = 0;

    void Check(const bool ok, const char *label)
    {
        if (ok)
            return;

        ++g_failed;
        GLogError(u8"TestMaterialRecipeInheritance 失败: %s", label);
    }

    /// TOML → 规约。与真实解析器共用 `StructSerializer`，因此测到的就是线上的行为。
    bool ReadSpec(const char *text, MaterialRecipeSpec &out)
    {
        Value node;
        if (!FromTomlText(text, node))
            return false;

        Archive ar = Archive::ForRead(node);
        StructSerializer<MaterialRecipeSpec>::Do(ar, out);
        return ar.Ok();
    }

    bool SameArray(const hgl::ManagedArray<hgl::AnsiString> &array,
                   const char *const *expected,
                   const int expected_count)
    {
        if (array.GetCount() != expected_count)
            return false;

        for (int i = 0; i < expected_count; ++i)
        {
            if (!array[i] || *array[i] != expected[i])
                return false;
        }

        return true;
    }

    // ── 1. 基本继承 ─────────────────────────────────────────────────────────
    constexpr char kBaseRecipe[] =
        "schema = 1\n"
        "name = \"Inherit.Base\"\n"
        "domain = \"UnlitTexture\"\n"
        "\n"
        "[fragment]\n"
        "surface_module_name = \"base_surface\"\n"
        "samplers = [\"Sampler2D\"]\n"
        "defines = [\"BASE_DEFINE\"]\n"
        "\n"
        "[transform]\n"
        "source = \"Vec2Position\"\n"
        "mapping = \"LiftXY_XY0\"\n"
        "orientation = \"World\"\n"
        "\n"
        "[render_state]\n"
        "alpha_test = true\n"
        "alpha_cutoff = 0.5\n"
        "\n"
        "[[textures]]\n"
        "name = \"base_color\"\n"
        "asset = \"res/image/base.Tex2D\"\n"
        "layer = 2\n"
        "auto_mipmaps = true\n"
        "\n"
        "[[textures]]\n"
        "name = \"normal\"\n"
        "asset = \"res/image/normal.Tex2D\"\n"
        "required = false\n";

    void TestBasicInheritance()
    {
        MaterialRecipeSpec parent;
        if (!ReadSpec(kBaseRecipe, parent))
        {
            Check(false, "基本继承：父规约解析");
            return;
        }

        constexpr char kChild[] =
            "schema = 1\n"
            "name = \"Inherit.Child\"\n"
            "parent = \"rcp:@Inherit.Base\"\n"
            "\n"
            "[fragment]\n"
            "surface_module_name = \"child_surface\"\n"
            "\n"
            "[render_state]\n"
            "alpha_cutoff = 0.75\n";

        MaterialRecipeSpec child;
        if (!ReadSpec(kChild, child))
        {
            Check(false, "基本继承：子规约解析");
            return;
        }

        MaterialRecipeSpec merged;
        MergeMaterialRecipeSpec(parent, child, merged);

        Check(merged.name == "Inherit.Child", "基本继承：name 取子");
        Check(merged.parent == "rcp:@Inherit.Base", "基本继承：parent 取子");
        // 子没写 domain ⇒ 继承父（`Custom` 是「未声明」的哨兵，必须被父覆盖）。
        Check(merged.domain == MaterialDomain::UnlitTexture, "基本继承：domain 继承父");
        Check(merged.fragment.surface_module_name == "child_surface",
              "基本继承：子显式字段覆盖");
        Check(merged.fragment.samplers.present
              && merged.fragment.samplers.value.GetCount() == 1
              && *merged.fragment.samplers.value[0] == "Sampler2D",
              "基本继承：子未声明的整表沿用父");
        Check(merged.transform.input.present
              && merged.transform.input.value == VertexInputMode::Vec2Position
              && merged.transform.position_mapping.value == PositionMappingMode::LiftXY_XY0,
              "基本继承：transform 逐字段沿用父");
        Check(merged.render_state.alpha_test.present
              && merged.render_state.alpha_test.value,
              "基本继承：未覆盖的 Presence 沿用父");
        Check(merged.render_state.alpha_cutoff.present
              && merged.render_state.alpha_cutoff.value == 0.75f,
              "基本继承：覆盖值取子");

        // 纹理：子没提纹理表 ⇒ 父的两条原样保留（含 `layer` / `auto_mipmaps`）。
        Check(merged.textures.GetCount() == 2, "基本继承：纹理条数");
        Check(merged.textures[0]->texture_name == "base_color"
              && merged.textures[0]->resource_id.present
              && merged.textures[0]->resource_id.value == "res/image/base.Tex2D"
              && merged.textures[0]->layer.present
              && merged.textures[0]->layer.value == 2
              && merged.textures[0]->auto_mipmaps.present
              && merged.textures[0]->auto_mipmaps.value,
              "基本继承：父纹理槽逐字段保留");
    }

    // ── 2. 多级继承 ─────────────────────────────────────────────────────────
    void TestMultiLevelInheritance()
    {
        MaterialRecipeSpec grand;
        if (!ReadSpec(kBaseRecipe, grand))
        {
            Check(false, "多级继承：祖父规约解析");
            return;
        }

        constexpr char kMidText[] =
            "schema = 1\n"
            "name = \"Inherit.Mid\"\n"
            "parent = \"rcp:@Inherit.Base\"\n"
            "\n"
            "[transform]\n"
            "orientation = \"CameraFacingFree\"\n";

        MaterialRecipeSpec mid;
        if (!ReadSpec(kMidText, mid))
        {
            Check(false, "多级继承：中间规约解析");
            return;
        }

        MaterialRecipeSpec mid_merged;
        MergeMaterialRecipeSpec(grand, mid, mid_merged);

        constexpr char kLeafText[] =
            "schema = 1\n"
            "name = \"Inherit.Leaf\"\n"
            "parent = \"rcp:@Inherit.Mid\"\n"
            "\n"
            "[render_state]\n"
            "dither = true\n";

        MaterialRecipeSpec leaf;
        if (!ReadSpec(kLeafText, leaf))
        {
            Check(false, "多级继承：叶子规约解析");
            return;
        }

        MaterialRecipeSpec leaf_merged;
        MergeMaterialRecipeSpec(mid_merged, leaf, leaf_merged);

        Check(leaf_merged.domain == MaterialDomain::UnlitTexture,
              "多级继承：domain 透传自祖父");
        Check(leaf_merged.fragment.surface_module_name == "base_surface",
              "多级继承：祖父字段透传");
        Check(leaf_merged.transform.orientation.present
              && leaf_merged.transform.orientation.value == OrientationMode::CameraFacingFree,
              "多级继承：中间层覆盖保留");
        Check(leaf_merged.transform.input.value == VertexInputMode::Vec2Position,
              "多级继承：祖父字段在中间层未动");
        Check(leaf_merged.render_state.alpha_cutoff.value == 0.5f
              && leaf_merged.render_state.dither.present
              && leaf_merged.render_state.dither.value,
              "多级继承：分层的 render_state 合并");
    }

    // ── 3. 显式写默认值 vs 缺键 ─────────────────────────────────────────────
    void TestExplicitDefaultOverrides()
    {
        constexpr char kParentText[] =
            "schema = 1\n"
            "name = \"Inherit.PipelineBase\"\n"
            "domain = \"UnlitTexture\"\n"
            "\n"
            "[render_state.pipeline]\n"
            "depth_test = true\n"
            "alpha_blend = true\n"
            "line_width = 2.0\n";

        MaterialRecipeSpec parent;
        if (!ReadSpec(kParentText, parent))
        {
            Check(false, "显式默认：父规约解析");
            return;
        }

        // 子把 `alpha_blend` 显式改成 false —— 与「缺键」必须区分开。
        constexpr char kChildText[] =
            "schema = 1\n"
            "name = \"Inherit.PipelineChild\"\n"
            "parent = \"rcp:@Inherit.PipelineBase\"\n"
            "\n"
            "[render_state.pipeline]\n"
            "alpha_blend = false\n";

        MaterialRecipeSpec child;
        if (!ReadSpec(kChildText, child))
        {
            Check(false, "显式默认：子规约解析");
            return;
        }

        MaterialRecipeSpec merged;
        MergeMaterialRecipeSpec(parent, child, merged);

        const RecipePipelineSpec &pipeline = merged.render_state.pipeline.value;
        Check(merged.render_state.pipeline.present, "显式默认：pipeline 表存在");
        Check(pipeline.alpha_blend.present && !pipeline.alpha_blend.value,
              "显式默认：显式 false 覆盖父的 true");
        Check(pipeline.depth_test.present && pipeline.depth_test.value,
              "显式默认：子未写的字段沿用父");
        Check(pipeline.line_width.present && pipeline.line_width.value == 2.0f,
              "显式默认：数值字段沿用父");
    }

    // ── 4. pipeline 双层合并（父无表 / 子无表） ─────────────────────────────
    void TestPipelineTablePresence()
    {
        constexpr char kNoPipeline[] =
            "schema = 1\n"
            "name = \"Inherit.NoPipeline\"\n"
            "domain = \"UnlitTexture\"\n";

        MaterialRecipeSpec no_pipeline;
        if (!ReadSpec(kNoPipeline, no_pipeline))
        {
            Check(false, "pipeline 双层：无表规约解析");
            return;
        }

        constexpr char kWithPipeline[] =
            "schema = 1\n"
            "name = \"Inherit.WithPipeline\"\n"
            "parent = \"rcp:@Inherit.NoPipeline\"\n"
            "\n"
            "[render_state.pipeline]\n"
            "cull_mode = \"None\"\n";

        MaterialRecipeSpec with_pipeline;
        if (!ReadSpec(kWithPipeline, with_pipeline))
        {
            Check(false, "pipeline 双层：有表规约解析");
            return;
        }

        // 父没有表、子有表 ⇒ 结果有表，只带子写的字段。
        MaterialRecipeSpec merged;
        MergeMaterialRecipeSpec(no_pipeline, with_pipeline, merged);
        Check(merged.render_state.pipeline.present
              && merged.render_state.pipeline.value.cull_mode.present
              && merged.render_state.pipeline.value.cull_mode.value == VK_CULL_MODE_NONE,
              "pipeline 双层：父无表时取子表");
        Check(!merged.render_state.pipeline.value.depth_test.present,
              "pipeline 双层：父无表时子表其余字段仍缺键");

        // 反过来：父有表、子无表 ⇒ 结果有表，内容来自父。
        MaterialRecipeSpec back;
        MergeMaterialRecipeSpec(with_pipeline, no_pipeline, back);
        Check(back.render_state.pipeline.present
              && back.render_state.pipeline.value.cull_mode.value == VK_CULL_MODE_NONE,
              "pipeline 双层：子无表时保留父表");
    }

    // ── 5. 纹理表按名合并 ───────────────────────────────────────────────────
    void TestTextureMerge()
    {
        MaterialRecipeSpec parent;
        if (!ReadSpec(kBaseRecipe, parent))
        {
            Check(false, "纹理合并：父规约解析");
            return;
        }

        // 只换 `asset`，不写 `layer` / `auto_mipmaps` —— 正是「一个配方、不同贴图」的用法。
        constexpr char kChildText[] =
            "schema = 1\n"
            "name = \"Inherit.TextureChild\"\n"
            "parent = \"rcp:@Inherit.Base\"\n"
            "\n"
            "[[textures]]\n"
            "name = \"base_color\"\n"
            "asset = \"res/image/other.Tex2D\"\n"
            "\n"
            "[[textures]]\n"
            "name = \"emissive\"\n"
            "asset = \"res/image/emissive.Tex2D\"\n";

        MaterialRecipeSpec child;
        if (!ReadSpec(kChildText, child))
        {
            Check(false, "纹理合并：子规约解析");
            return;
        }

        MaterialRecipeSpec merged;
        MergeMaterialRecipeSpec(parent, child, merged);

        Check(merged.textures.GetCount() == 3, "纹理合并：同名合并且新增槽追加");
        Check(merged.textures[0]->texture_name == "base_color",
              "纹理合并：输出顺序沿用父声明顺序");
        Check(merged.textures[0]->resource_id.present
              && merged.textures[0]->resource_id.value == "res/image/other.Tex2D",
              "纹理合并：asset 取子");
        Check(merged.textures[0]->layer.present
              && merged.textures[0]->layer.value == 2,
              "纹理合并：子未写的 layer 保持父值");
        Check(merged.textures[0]->auto_mipmaps.present
              && merged.textures[0]->auto_mipmaps.value,
              "纹理合并：子未写的 auto_mipmaps 保持父值");
        Check(merged.textures[1]->texture_name == "normal"
              && merged.textures[1]->resource_id.value == "res/image/normal.Tex2D"
              && merged.textures[1]->required.present
              && !merged.textures[1]->required.value,
              "纹理合并：父未提及的槽原样保留");
        Check(merged.textures[2]->texture_name == "emissive"
              && merged.textures[2]->resource_id.value == "res/image/emissive.Tex2D",
              "纹理合并：新槽追加在尾部");
    }

    // ── 6. 字符串数组整表替换 ───────────────────────────────────────────────
    void TestStringArrayReplace()
    {
        MaterialRecipeSpec parent;
        if (!ReadSpec(kBaseRecipe, parent))
        {
            Check(false, "数组替换：父规约解析");
            return;
        }

        constexpr char kChildText[] =
            "schema = 1\n"
            "name = \"Inherit.ArrayChild\"\n"
            "parent = \"rcp:@Inherit.Base\"\n"
            "\n"
            "[fragment]\n"
            "samplers = [\"Sampler2DArray\"]\n";

        MaterialRecipeSpec child;
        if (!ReadSpec(kChildText, child))
        {
            Check(false, "数组替换：子规约解析");
            return;
        }

        MaterialRecipeSpec merged;
        MergeMaterialRecipeSpec(parent, child, merged);

        static constexpr const char *kExpected[] = {"Sampler2DArray"};
        Check(SameArray(merged.fragment.samplers.value, kExpected, 1),
              "数组替换：出现即整表替换而不是逐项合并");

        // `defines` 子没提 ⇒ 沿用父整表。
        static constexpr const char *kBaseDefines[] = {"BASE_DEFINE"};
        Check(SameArray(merged.fragment.defines.value, kBaseDefines, 1),
              "数组替换：未出现的数组沿用父");
    }

    // ── 7. parent 引用写法 ──────────────────────────────────────────────────
    void TestParentRefParsing()
    {
        MaterialRecipeParentRef ref;

        Check(ParseMaterialRecipeParentRef("rcp:@Inherit.Base", ref)
              && ref.kind == MaterialRecipeParentRefKind::ByName
              && ref.name == "Inherit.Base",
              "引用解析：rcp:@Name");

        Check(ParseMaterialRecipeParentRef("@Inherit.Base", ref)
              && ref.kind == MaterialRecipeParentRefKind::ByName
              && ref.name == "Inherit.Base",
              "引用解析：@Name");

        Check(ParseMaterialRecipeParentRef("Inherit.Base", ref)
              && ref.kind == MaterialRecipeParentRefKind::ByName
              && ref.name == "Inherit.Base",
              "引用解析：裸名字");

        Check(ParseMaterialRecipeParentRef("rcp:file:ShaderLibrary/asset/recipe/Base.recipe.toml", ref)
              && ref.kind == MaterialRecipeParentRefKind::ByPath
              && ref.path == OS_TEXT("ShaderLibrary/asset/recipe/Base.recipe.toml"),
              "引用解析：rcp:file:<path>");

        Check(ParseMaterialRecipeParentRef("ShaderLibrary/asset/recipe/Base.recipe.toml", ref)
              && ref.kind == MaterialRecipeParentRefKind::ByPath,
              "引用解析：带分隔符的裸路径");

        Check(ParseMaterialRecipeParentRef("", ref) && ref.kind == MaterialRecipeParentRefKind::None,
              "引用解析：空串等价于没有父配方");
    }

    // ── 8. 注册表：子先注册、父后注册（两阶段加载） ─────────────────────────
    void TestRegistryChildBeforeParent()
    {
        constexpr char kParentText[] =
            "schema = 1\n"
            "name = \"Reg.Base\"\n"
            "domain = \"UnlitTexture\"\n"
            "\n"
            "[render_state]\n"
            "alpha_cutoff = 0.5\n"
            "\n"
            "[[textures]]\n"
            "name = \"base_color\"\n"
            "asset = \"res/image/base.Tex2D\"\n"
            "layer = 2\n"
            "auto_mipmaps = true\n";

        constexpr char kChildText[] =
            "schema = 1\n"
            "name = \"Reg.Child\"\n"
            "parent = \"rcp:@Reg.Base\"\n"
            "\n"
            "[render_state]\n"
            "dither = true\n"
            "\n"
            "[[textures]]\n"
            "name = \"base_color\"\n"
            "asset = \"res/image/other.Tex2D\"\n";

        hgl::graph::TextureManager texture_manager(nullptr);
        MaterialRecipeRegistry registry;

        // 先注册子：父还不存在 ⇒ 留在 `Parsed` 态等待，注册本身不算失败。
        if (!registry.RegisterRecipeSource(OS_TEXT("reg-child"),
                                           kChildText,
                                           static_cast<int>(strlen(kChildText)),
                                           &texture_manager))
        {
            Check(false, "注册表：子配方先注册应成功登记（等待父）");
            texture_manager.Release();
            return;
        }

        Check(registry.FindAssetByName("Reg.Child") != nullptr
              && !registry.FindAssetByName("Reg.Child")->IsReady(),
              "注册表：父未到之前子配方不可用");
        Check(registry.FindByName("Reg.Child") == nullptr,
              "注册表：等待中的配方不出现在查询结果里");

        if (!registry.RegisterRecipeSource(OS_TEXT("reg-parent"),
                                           kParentText,
                                           static_cast<int>(strlen(kParentText)),
                                           &texture_manager))
        {
            Check(false, "注册表：父配方注册失败");
            texture_manager.Release();
            return;
        }

        const MaterialRecipeAsset *child = registry.FindAssetByName("Reg.Child");
        Check(child && child->IsReady(), "注册表：父到齐后子配方自动就绪");
        Check(registry.FinalizePending() == 0, "注册表：无悬空配方");

        if (child && child->IsReady())
        {
            const MaterialRecipe &recipe = child->GetRecipe();
            Check(recipe.domain == MaterialDomain::UnlitTexture,
                  "注册表：domain 继承父");
            Check(recipe.textures.size() == 1, "注册表：纹理槽数量");
            // `ResolveAndValidate` 会用 `TextureAsset` 的身份改写 `resource_id`
            // （`texfile:` 之类前缀由 URI 决定），所以这里只校验路径本身还在。
            Check(strstr(recipe.textures[0].resource_id.c_str(),
                         "res/image/other.Tex2D") != nullptr,
                  "注册表：纹理 asset 覆盖为子值");
            // 子没写 `layer` / `auto_mipmaps` ⇒ 沿用父。`required` 两侧都没写 ⇒
            // 读侧默认 true（`ApplyTextures` 的缺键语义）。
            Check(recipe.textures[0].array_layer == 2
                  && recipe.textures[0].auto_mipmaps
                  && recipe.textures[0].required,
                  "注册表：纹理未覆盖字段沿用父 + 缺键默认");
            Check(recipe.render_state_overrides.has_alpha_cutoff
                  && recipe.render_state_overrides.alpha_cutoff == 0.5f
                  && recipe.render_state_overrides.has_dither
                  && recipe.render_state_overrides.dither,
                  "注册表：两层的 render_state 覆盖都进入运行时配方");
            Check(child->GetSpec().render_state.alpha_cutoff.present
                  && child->GetSpec().render_state.alpha_cutoff.value == 0.5f
                  && child->GetSpec().render_state.dither.present,
                  "注册表：合并结果回填进句柄的规约");
        }

        texture_manager.Release();
    }

    // ── 9. 注册表：父配方缺失 ───────────────────────────────────────────────
    void TestRegistryMissingParent()
    {
        constexpr char kOrphan[] =
            "schema = 1\n"
            "name = \"Reg.Orphan\"\n"
            "parent = \"rcp:@Reg.Nonexistent\"\n";

        hgl::graph::TextureManager texture_manager(nullptr);
        MaterialRecipeRegistry registry;

        Check(registry.RegisterRecipeSource(OS_TEXT("reg-orphan"),
                                            kOrphan,
                                            static_cast<int>(strlen(kOrphan)),
                                            &texture_manager),
              "注册表：父缺失时注册仍先登记");
        Check(registry.FinalizePending() == 1, "注册表：收尾判定父缺失");
        Check(registry.GetCount() == 0 && registry.FindByName("Reg.Orphan") == nullptr,
              "注册表：父缺失的配方被摘掉");

        texture_manager.Release();
    }

    // ── 10. 注册表：父链成环 ────────────────────────────────────────────────
    void TestRegistryParentCycle()
    {
        constexpr char kCycleA[] =
            "schema = 1\n"
            "name = \"Reg.CycleA\"\n"
            "parent = \"rcp:@Reg.CycleB\"\n";

        constexpr char kCycleB[] =
            "schema = 1\n"
            "name = \"Reg.CycleB\"\n"
            "parent = \"rcp:@Reg.CycleA\"\n";

        hgl::graph::TextureManager texture_manager(nullptr);
        MaterialRecipeRegistry registry;

        Check(registry.RegisterRecipeSource(OS_TEXT("reg-cycle-a"),
                                            kCycleA,
                                            static_cast<int>(strlen(kCycleA)),
                                            &texture_manager),
              "注册表：环引用成员先登记");
        Check(registry.RegisterRecipeSource(OS_TEXT("reg-cycle-b"),
                                            kCycleB,
                                            static_cast<int>(strlen(kCycleB)),
                                            &texture_manager),
              "注册表：环引用另一成员先登记");

        Check(registry.FinalizePending() == 2, "注册表：成环的两条都判定失败");
        Check(registry.GetCount() == 0, "注册表：成环配方全部摘掉");

        texture_manager.Release();
    }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestMaterialRecipeInheritance"));

    TestBasicInheritance();
    TestMultiLevelInheritance();
    TestExplicitDefaultOverrides();
    TestPipelineTablePresence();
    TestTextureMerge();
    TestStringArrayReplace();
    TestParentRefParsing();
    TestRegistryChildBeforeParent();
    TestRegistryMissingParent();
    TestRegistryParentCycle();

    if (g_failed > 0)
    {
        GLogError(u8"=== TestMaterialRecipeInheritance: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestMaterialRecipeInheritance: 全部测试通过 ===");
    return 0;
}
