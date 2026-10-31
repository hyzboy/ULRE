#pragma once

/**
 * 材质配方的**差量继承**（UE Material Instance 语义）。
 *
 * 一份 `*.recipe.toml` 可以用 `parent` 指向另一份配方，然后**只写与父不同的键**：
 *
 * ```toml
 * schema  = 1
 * name    = "Demo.PBRShiny"          # 子配方自己的身份
 * parent  = "rcp:@Demo.PBRBase"      # 父配方（按名）
 *
 * [render_state.pipeline]
 * cull_mode = "None"                 # 只盖这一个键，其余 10 个字段从父继承
 * ```
 *
 * ── 为什么合并发生在**规约层**而不是运行时配方层 ──────────────────────────
 *
 * `MaterialRecipe`（`inc/hgl/mtl/MaterialRecipe.h`）已经被 `BuildRecipe` 压平过：
 * 管线覆盖变成 `MaterialPipelineField` 位掩码、采样器/宏变成了合并后的最终表，
 * 「哪个键是本文件写的、哪个是从父来的」这个信息在那个形态里已经不存在了。
 * 而 `MaterialRecipeSpec` 恰好用 `Presence<T>` 把这件事记着，所以合并只能在规约层做，
 * 做完再交给 `BuildRecipe` 压平 —— 这样下游（领域契约校验、纹理句柄绑定、归一化）
 * 一行都不用改。
 *
 * ── 逐段语义 ─────────────────────────────────────────────────────────────
 *
 * | 段 | 合并规则 |
 * |----|---------|
 * | `schema` / `name` / `parent` | 恒取**子**（子配方自报版本、身份与它自己的父） |
 * | `domain` | 子写成非 `Custom` 取子，否则取父 |
 * | `fragment.surface_module_name` | 子非空取子，否则取父 |
 * | `fragment.samplers` / `defines` | 子 `present` 即**整表替换**（含 `[]` = 清空），否则取父 |
 * | `transform.*` | 逐字段：子 `present` 取子，否则取父 |
 * | `render_state.alpha_test` / `alpha_cutoff` / `dither` | 逐字段，同上 |
 * | `render_state.pipeline` | 逐字段，`present` 取并集（只要一侧写了这张表，结果就有这张表） |
 * | `textures` | 按 `texture_name` 字典合并：同名**逐字段**覆盖，新名追加 |
 *
 * 纹理表**没有**「删掉父声明的一个槽」的写法。需要清空某个槽就把它改成
 * `asset = ""` + `required = false`（领域契约允许缺省的槽才能这么写），语义是
 * 「这个槽存在但不绑资源」，比留一个不存在的键更好诊断。
 *
 * ── 环检测不在本文件 ─────────────────────────────────────────────────────
 *
 * 本文件只做**两份规约**的合并，不认识注册表，也不知道 `parent` 指向的东西存在与否。
 * 多级解析、环检测和加载顺序都在 `MaterialRecipeRegistry` 里，因为那才是唯一
 * 知道「现在有哪些配方、哪些还没解析完」的层。
 */

#include <hgl/graph/asset/MaterialRecipeSpec.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    /// `parent` 字段解析出来的引用目标形态
    enum class MaterialRecipeParentRefKind : uint8
    {
        None = 0,   ///<`parent` 为空 ⇒ 自足配方
        ByName,     ///<按配方名解析（`rcp:@Name` / `@Name` / `Name`）
        ByPath,     ///<按来源文件解析（`rcp:file:<path>` / 带路径分隔符或 `.toml` 结尾的串）

        ENUM_CLASS_RANGE(None, ByPath)
    };

    const char *GetMaterialRecipeParentRefKindName(
        MaterialRecipeParentRefKind kind) noexcept;

    /// 解析后的父配方引用
    struct MaterialRecipeParentRef
    {
        MaterialRecipeParentRefKind kind = MaterialRecipeParentRefKind::None;
        AnsiString                  name;   ///<`kind == ByName` 时的配方名
        OSString                    path;   ///<`kind == ByPath` 时的来源路径

        bool IsNone() const noexcept { return kind == MaterialRecipeParentRefKind::None; }
    };

    /**
     * 把 `parent` 字段解析成引用目标。
     *
     * @param parent `MaterialRecipeSpec::parent` 的原始文本（可空）
     * @param out    解析结果；失败时被置为 `None`
     * @return `true` = 解析成功（`out.IsNone()` 表示本就没有父）；
     *         `false` = 写法非法（已写出的引用必须能落到名字或路径上）
     */
    bool ParseMaterialRecipeParentRef(const char *parent, MaterialRecipeParentRef &out);

    /**
     * 差量合并：`child` 里 `present == true` 的键覆盖 `parent`，其余从 `parent` 继承。
     *
     * `out` 必须是**空对象**（本函数只往里写，不读旧值）。两个入参都是 `const&`：
     * 唯一的深拷贝发生在 `ManagedArray` 成员上（`ManagedArray` 是 move-only，
     * `Presence<ManagedArray<T>>::operator=` 不可用，因此字符串数组与纹理表
     * 只能逐元素 `Create()` 重建）。
     */
    void MergeMaterialRecipeSpec(const MaterialRecipeSpec &parent,
                                 const MaterialRecipeSpec &child,
                                 MaterialRecipeSpec &out);
}//namespace hgl::graph::mtl
