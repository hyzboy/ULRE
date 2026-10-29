#pragma once

#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/asset/PrimitiveAssetSpec.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/ValueArray.h>

namespace hgl
{
    namespace graph
    {
        class GeometryAssetRegistry;
    }

    namespace graph::mtl
    {
        class MaterialRecipeRegistry;
    }
}

namespace hgl::graph
{
    /**
     * 图元资产注册器 —— 「几何 + 材质配方」绑定结果的**唯一真值**。
     *
     * 它刻意不做解析，只做**装配**：几何来自 `GeometryAssetRegistry`（按路径/规约，
     * 该给的 GPU 资源由几何句柄自己负责异步就绪），配方来自 `MaterialRecipeRegistry`
     * （按名）。于是同一份几何或配方可以被任意多个图元资产共享，各自只是一份
     * 「指向谁的指针数组」。
     *
     * 所有权与生命周期：
     * - 本表拥有 `PrimitiveAsset` 及其变体数组；
     * - 表里的几何/配方指针都是**裸观察指针**，指向两个注册器 —— 因此本表必须在
     *   那两个注册器清空之前清空（`GraphicsContext::Shutdown` 里顺序已保证）。
     *
     * 线程约定：只在主线程使用（首次装配可能触发几何创建）。
     */
    class PrimitiveAssetRegistry
    {
    private:

        struct Entry
        {
            PrimitiveAsset                  asset;      ///<对外可见的绑定
            hgl::ValueArray<PrimitiveVariant> variants; ///<变体存储(asset 里存的是本数组的裸指针)
            OSString                        source_path;///<来源 `.primitive.toml`(可为空)
            U8String                        resource_id;
        };

        GeometryAssetRegistry *                 geometry_registry = nullptr;
        hgl::graph::mtl::MaterialRecipeRegistry *recipe_registry = nullptr;

        hgl::ValueArray<Entry *> entries;

    private:

        Entry *FindEntry(const U8String &resource_id)const;

        /// 把规约装配成一条 Entry（几何 + 配方全部解析成功才算数）
        Entry *BuildEntry(const PrimitiveAssetSpec &spec,const U8String &resource_id);

    public:

        PrimitiveAssetRegistry()=default;
        ~PrimitiveAssetRegistry();

        PrimitiveAssetRegistry(const PrimitiveAssetRegistry &)=delete;
        PrimitiveAssetRegistry &operator=(const PrimitiveAssetRegistry &)=delete;

    public:

        void SetSources(GeometryAssetRegistry *geo,
                        hgl::graph::mtl::MaterialRecipeRegistry *recipe)
        {
            geometry_registry=geo;
            recipe_registry=recipe;
        }

        bool IsReady()const{return geometry_registry&&recipe_registry;}

    public:     // 装配入口

        /**
         * @brief 按 `*.primitive.toml` 路径取（无则建）。
         *
         * 规约里的 `geometry` 相对路径按**该 TOML 文件所在目录**解析 —— 与
         * `*.recipe.toml` 里纹理路径的约定一致：资产搬到别处不必改内容。
         */
        PrimitiveAsset *GetOrLoad(const OSString &filename);

        /// 按内存规约建（同名去重）。
        PrimitiveAsset *GetOrCreate(const PrimitiveAssetSpec &spec);

    public:     // 查询

        int GetCount()const{return entries.GetCount();}

        /// 按索引取绑定结果（越界返回 nullptr）
        const PrimitiveAsset *GetAt(const int index)const;

        /// 按资源身份查（`prmfile:<path>` / `prm:@<name>`）
        const PrimitiveAsset *FindByUri(const U8String &resource_id)const;

    public:

        void Clear();
    };//class PrimitiveAssetRegistry
}//namespace hgl::graph
