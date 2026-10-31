#pragma once

/**
 * 材质配方规约（`*.recipe.toml` 的字段契约真源）。
 *
 * 这里**只**描述「有哪些键、什么类型、缺省值是什么」。跨字段业务规则
 * （`schema` 版本、领域是否已实现、`surface_module_name` 的适用范围、纹理名
 * 合法性/去重、`line_width > 0`……）不属于字段规约，仍在
 * `src/ShaderMaterialData/MaterialRecipeFile.cpp` 里，用读出来的规约再判一次。
 *
 * ── 为什么字段要包 `hgl::serialize::Presence<T>` ───────────────────────────
 *
 * 配方是**局部覆盖**语义：作者只写和领域默认不同的键。`Archive` 的缺键行为是
 * 「保留类内缺省值」，这已经解决了取值问题；但 `false` / `0` / `VK_CULL_MODE_NONE`
 * 都是合法取值，光看值分不清「写成了缺省值」和「根本没写」。`Presence<T>` 把
 * 「这个键出现过」单独记下来，于是：
 *
 *   · 缺键           → `present == false`，不覆盖下游
 *   · 显式写成缺省值 → `present == true`，覆盖成缺省值
 *   · 显式写成别的值 → `present == true`，覆盖成该值
 *
 * 生成代码对 `Presence<T>` 字段的 `ar.Field(...)` 调用自动落到
 * `Archive::Field(id, name, const Presence<T> &)` 特化重载，字段声明处无需额外标注；
 * 写方向（快照 / 编辑器保存）只写 `present == true` 的键，往返一次不会膨胀。
 *
 * ```toml
 * schema = 1
 * name = "Core.PBR.Lit"
 * domain = "StandardOpaque"
 *
 * [fragment]
 * samplers = ["Sampler2D", "Sampler2D"]
 * defines  = ["NORMAL_MAP"]
 *
 * [transform]
 * source = "Vec3NormalTangent"
 *
 * [render_state]
 * alpha_cutoff = 0.25          # 只盖这一个键
 * [render_state.pipeline]
 * cull_mode = "None"           # 只盖这一个键，其余 10 个字段保持契约默认
 *
 * [[textures]]
 * name = "base_color"
 * asset = "res/tex/rock_albedo.tex"
 * ```
 *
 * 生成器：`src/Tools/AssetGen/asset_gen.py`（`[group.MaterialRecipe]`）。
 */

#include <hgl/asset/AssetAnnotation.h>
#include <hgl/mtl/MaterialDomain.h>
#include <hgl/mtl/PipelineConfig.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/serialize/Archive.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    /// 一条纹理解析声明（`[[textures]]` 表）。纯输入，不含任何运行时句柄。
    struct HGL_ASSET_STRUCT RecipeTextureSpec
    {
        HGL_PROPERTY(EditAnywhere, Category = "Texture", Key = "name")
        AnsiString  texture_name;                   ///<TOML/GLSL 纹理名（snake_case，如 "base_color"）

        HGL_PROPERTY(EditAnywhere, Category = "Texture", Key = "asset", AssetRef = "tex")
        AnsiString  resource_id;                    ///<资源标识（路径 / 资产 ID / 逻辑名）

        HGL_PROPERTY(EditAnywhere, Category = "Texture")
        bool        required = true;                ///<true 时缺失应触发显式错误

        HGL_PROPERTY(EditAnywhere, Category = "Texture", Range(0, 31))
        uint32_t    layer = 0;                      ///<Texture2DArray 层号；非数组纹理必须为 0

        HGL_PROPERTY(EditAnywhere, Category = "Texture")
        bool        auto_mipmaps = false;           ///<源无 mip 链时是否自动生成
    };

    /**
     * `[render_state.pipeline]` 的**逐字段**覆盖规约。
     *
     * 11 个字段全部是 `Presence<T>`：只写与契约默认不同的键，未写的键保留默认值。
     * 这是 `MaterialPipelineField` 位掩码的数据来源（`present` ⇒ 掩码对应位置 1）。
     */
    struct HGL_ASSET_STRUCT RecipePipelineSpec
    {
        HGL_PROPERTY(EditAnywhere, Category = "Pipeline")
        hgl::serialize::Presence<VkCullModeFlags> cull_mode;

        HGL_PROPERTY(EditAnywhere, Category = "Pipeline")
        hgl::serialize::Presence<bool> depth_test;

        HGL_PROPERTY(EditAnywhere, Category = "Pipeline")
        hgl::serialize::Presence<bool> depth_write;

        HGL_PROPERTY(EditAnywhere, Category = "Pipeline")
        hgl::serialize::Presence<VkCompareOp> depth_compare_op;

        HGL_PROPERTY(EditAnywhere, Category = "Pipeline")
        hgl::serialize::Presence<bool> alpha_blend;

        HGL_PROPERTY(EditAnywhere, Category = "Pipeline")
        hgl::serialize::Presence<VkBlendFactor> blend_src;

        HGL_PROPERTY(EditAnywhere, Category = "Pipeline")
        hgl::serialize::Presence<VkBlendFactor> blend_dst;

        HGL_PROPERTY(EditAnywhere, Category = "Pipeline")
        hgl::serialize::Presence<bool> dynamic_line_width;

        HGL_PROPERTY(EditAnywhere, Category = "Pipeline")
        hgl::serialize::Presence<bool> overlay;

        HGL_PROPERTY(EditAnywhere, Category = "Pipeline")
        hgl::serialize::Presence<bool> wireframe;

        /// 线宽。物理上必须 > 0：合法性由解析层的跨字段校验负责，这里只管字段。
        HGL_PROPERTY(EditAnywhere, Category = "Pipeline", Clamp(0, 64), Step = 0.25)
        hgl::serialize::Presence<float> line_width;
    };

    /// `[render_state]` 表：三个开关 + 一份可选的逐字段管线覆盖。
    struct HGL_ASSET_STRUCT RecipeRenderStateSpec
    {
        HGL_PROPERTY(EditAnywhere, Category = "RenderState")
        hgl::serialize::Presence<bool> alpha_test;

        HGL_PROPERTY(EditAnywhere, Category = "RenderState", Clamp(0, 1))
        hgl::serialize::Presence<float> alpha_cutoff;

        HGL_PROPERTY(EditAnywhere, Category = "RenderState")
        hgl::serialize::Presence<bool> dither;

        /// 整个 `[render_state.pipeline]` 表。包一层 `Presence` 是为了区分
        /// 「没有这张表」和「有这张表但一个键都没写」（后者是作者的书写错误）。
        HGL_PROPERTY(EditAnywhere, Category = "RenderState")
        hgl::serialize::Presence<RecipePipelineSpec> pipeline;
    };

    /// `[fragment]` 表：表面模块名 + 采样器/宏的**整表替换**。
    struct HGL_ASSET_STRUCT RecipeFragmentSpec
    {
        /// 领域声明了 `no_material_source`（如 Sky）时，配方自行指定的表面函数模块名；
        /// 其余领域留空，由领域契约的 `reference_material_source_module` 给出。
        HGL_PROPERTY(EditAnywhere, Category = "Fragment", Key = "surface_module_name")
        AnsiString  surface_module_name;

        /// 采样器槽位类型名（如 "Sampler2D"）。出现过 ⇒ **整表替换**领域预设，
        /// 空表 = 全部清空（`samplers = []` 与「不写这一行」语义完全不同）。
        HGL_PROPERTY(EditAnywhere, Category = "Fragment")
        hgl::serialize::Presence<hgl::ManagedArray<AnsiString>> samplers;

        /// 编译宏。语义同上：出现过 ⇒ 整表替换。
        HGL_PROPERTY(EditAnywhere, Category = "Fragment")
        hgl::serialize::Presence<hgl::ManagedArray<AnsiString>> defines;
    };

    /// `[transform]` 表：顶点着色器 PCG 节点配置的逐字段覆盖。
    struct HGL_ASSET_STRUCT RecipeTransformSpec
    {
        HGL_PROPERTY(EditAnywhere, Category = "Transform", Key = "source")
        hgl::serialize::Presence<VertexInputMode> input;

        HGL_PROPERTY(EditAnywhere, Category = "Transform", Key = "mapping")
        hgl::serialize::Presence<PositionMappingMode> position_mapping;

        HGL_PROPERTY(EditAnywhere, Category = "Transform")
        hgl::serialize::Presence<OrientationMode> orientation;

        HGL_PROPERTY(EditAnywhere, Category = "Transform")
        hgl::serialize::Presence<ScaleMode> scale;

        HGL_PROPERTY(EditAnywhere, Category = "Transform")
        hgl::serialize::Presence<ProjectionMode> projection;
    };

    /**
     * 一份材质配方文件的完整字段规约。
     *
     * 与运行时的 `MaterialRecipe`（`inc/hgl/mtl/MaterialRecipe.h`）刻意分开：
     * `MaterialRecipe` 带运行时句柄（`TextureAsset*`、SSBO 绑定）与已解析的
     * `VertexShaderNodeConfig`，不是纯数据也算不出稳定哈希；本结构是纯 POD，
     * 只回答「文件里写了什么」。
     */
    struct HGL_ASSET_STRUCT MaterialRecipeSpec
    {
        /// 版本号。当前只接受 1，判断留在解析层（这是跨字段业务规则）。
        HGL_PROPERTY(EditAnywhere, Category = "Identity", Range(1, 1))
        uint32_t    schema = 0;

        HGL_PROPERTY(EditAnywhere, Category = "Identity")
        AnsiString  name;

        HGL_PROPERTY(EditAnywhere, Category = "Identity")
        MaterialDomain domain = MaterialDomain::Custom;

        HGL_PROPERTY(EditAnywhere, Category = "Fragment")
        RecipeFragmentSpec fragment;

        HGL_PROPERTY(EditAnywhere, Category = "Transform")
        RecipeTransformSpec transform;

        HGL_PROPERTY(EditAnywhere, Category = "RenderState")
        RecipeRenderStateSpec render_state;

        /// `[[textures]]` 表数组。缺省/空数组都表示「没有配方级纹理绑定」。
        HGL_PROPERTY(EditAnywhere, Category = "Texture")
        hgl::ManagedArray<RecipeTextureSpec> textures;
    };
}//namespace hgl::graph::mtl
