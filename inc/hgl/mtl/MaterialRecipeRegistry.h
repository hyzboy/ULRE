#pragma once

/**
 * 材质配方注册表。
 *
 * 表项是**句柄**（`MaterialRecipeAsset`）而不是裸 `MaterialRecipeFileData`：句柄把
 * 「作者写了什么」（规约）、「渲染吃什么」（运行时结果）与「它叫什么、从哪来」
 * （身份）绑在同一个对象上，注册表只维护一张表就能同时回答三个视角。
 *
 * 表拥有句柄，对外一律给裸观察指针（与 `TextureAsset` / `GeometryAsset` 一致）。
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

    public:
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
