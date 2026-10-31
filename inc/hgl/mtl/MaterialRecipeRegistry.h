#pragma once

/**
 * 材质配方注册表。
 *
 * 表项是**句柄**（`MaterialRecipeAsset`）而不是裸 `MaterialRecipeFileData`：句柄把
 * 「作者写了什么」（规约）、「渲染吃什么」（运行时结果）与「它叫什么、从哪来」
 * （身份）绑在同一个对象上，注册表只维护一张表就能同时回答三个视角。
 *
 * 表拥有句柄，对外一律给裸观察指针（与 `TextureAsset` / `GeometryAsset` 一致）。
 *
 * ── 加载顺序与差量继承 ───────────────────────────────────────────────────
 *
 * `RegisterRecipeSource` 只做「解析 + 建句柄」，随后立刻尝试解析；解析不了（`parent`
 * 指向的配方还没注册）的句柄留在 `Parsed` 态等待。`LoadDirectory` 扫完整个目录后
 * 再补一轮 `ResolvePending`，因此**同一个目录内的父子配方谁先被扫到都无所谓**。
 *
 * 解析永远不动的 `Parsed` 句柄只有两种可能：父配方根本不存在，或父链成环 ——
 * 两者都在收尾时标 `Failed` 并从表里摘掉，与「校验失败就摘掉」的既有语义一致。
 */

#include <hgl/graph/asset/MaterialRecipeAsset.h>
#include <hgl/type/ManagedArray.h>

namespace hgl::graph
{
    class TextureManager;
}

namespace hgl::graph::mtl
{
    class MaterialRecipeRegistry
    {
        ManagedArray<MaterialRecipeAsset> files;

        bool ResolveAndValidate(MaterialRecipeAsset &asset,
                                hgl::graph::TextureManager *texture_manager);

        /// 单份 `Parsed` 句柄就绪后走完「合并父配方 → 重建配方 → 契约校验」
        /// @return `1` = 已就绪，`0` = 父配方还没就绪（保留待下轮），`-1` = 失败
        int ResolveOne(MaterialRecipeAsset &asset,
                       hgl::graph::TextureManager *texture_manager);

        /**
         * 多轮推进：每轮把「父已 `Ready`」的 `Parsed` 句柄解析掉，直到本轮无进展。
         *
         * 为什么用多轮而不是拓扑排序：父配方可能在任何时候被注册（内存注册、
         * 目录里文件名排序在后），而配方数量是个位数到几十，反复扫表比维护
         * 依赖图便宜得多，也不需要在注册时就知道谁依赖谁。
         *
         * @return 本轮新就绪的句柄数
         */
        int ResolvePending(hgl::graph::TextureManager *texture_manager);

        /// 父链上是否存在环（诊断用；只对已解析不动的 `Parsed` 句柄调用）
        bool HasParentCycle(const MaterialRecipeAsset &asset) const;

    public:

        /**
         * 收尾：把仍然悬在 `Parsed` 的句柄判定为失败（父配方不存在 / 父链成环）
         * 并摘掉，与「校验失败就摘掉」的既有语义一致。
         *
         * `LoadDirectory` 在扫完目录后自动调用。连续多次 `LoadFile` 的调用方
         * 应在最后自己调一次 —— 因为子配方可能先于父配方被加载。
         *
         * @return 被判定失败并摘掉的句柄数
         */
        int FinalizePending();
        bool LoadFile(const OSString &path,
                      hgl::graph::TextureManager *texture_manager);
        bool LoadDirectory(const OSString &directory,
                           hgl::graph::TextureManager *texture_manager,
                           int *out_file_count = nullptr,
                           int *out_error_count = nullptr);
        bool RegisterRecipeSource(const os_char *label,
                                  const char *content,
                                  int content_size,
                                  hgl::graph::TextureManager *texture_manager);

        /// 按配方名取运行时结果（既有调用点契约）
        const MaterialRecipe *FindByName(const char *recipe_name) const;

        /// 按配方名取句柄（规约 / 身份 / 就绪度）
        const MaterialRecipeAsset *FindAssetByName(const char *recipe_name) const;

        /// 按来源 `.recipe.toml` 路径取句柄（内存注册的配方没有路径，恒不命中）
        const MaterialRecipeAsset *FindAssetByPath(const OSString &path) const;

        /// 按下标取句柄（表遍历用，越界返回 nullptr）
        const MaterialRecipeAsset *GetAssetAt(int index) const { return files.At(index); }

        int GetCount() const { return files.GetCount(); }
        void Clear() { files.Clear(); }
    };
}
