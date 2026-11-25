#pragma once

#include<hgl/mtl/MaterialRecipeRegistry.h>
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::graph
{
    class TextureManager;

    /**
     * 按需材质配方解析器（GraphicsContext 私有）。
     *
     * 职责 = 把**任意形态的请求串**（短名 / 逻辑路径 / `rcp:` URI / 物理路径）解析成
     * 注册表里的配方，必要时从包裹索引定位取流、注册，并按需补加载父链。
     *
     * 两条不变量（原样保留）：
     * - **失败也记忆**：确认取不到的请求记成空串，不再重试取流（否则每帧刷屏）；
     *   注册器内容变化（显式装载 / 无设备→有设备 / 清空）必须 `ClearCache()` 整表作废。
     * - **父链按需**：子配方先于父被加载时会停在等待态；这里递归补载父配方（按名或按路径），
     *   父注册触发的 `ResolvePending` 会把子推进到 Ready。深度上限 8 防成环。
     *
     * 所有权：注册表归 GraphicsContext；纹理管理器**不持有**，只经注入的 getter 现取
     * （它可能在世界初始化时才被创建）。
     */
    class MaterialRecipeResolver
    {
    public:
        /// 延迟取纹理管理器（注册配方时建纹理句柄用）。owner 由绑定方提供，本类不解读。
        using TextureManagerGetter = TextureManager *(*)(void *owner);

    private:
        static constexpr int kMaxParentDepth = 8;

        mtl::MaterialRecipeRegistry *registry = nullptr;
        TextureManagerGetter texture_manager_getter = nullptr;
        void *owner = nullptr;

        /// 请求串 → 注册表内配方名（空串 = 已确认取不到）
        UnorderedMap<AnsiString, AnsiString> lookup_cache;

        const mtl::MaterialRecipe *Resolve(const char *request, int depth);

        /// 把任意请求形态定位成「可交给 `OpenAssetInputStream` 的 URI 文本」
        static bool LocateURI(const char *request, OSString &out_uri);

        /// 取流 → `MaterialRecipeRegistry::LoadFromStream` 的最短路径
        bool LoadFromURI(const OSString &uri_text, AnsiString &out_recipe_name);

        TextureManager *CurrentTextureManager() const
        {
            return (texture_manager_getter && owner) ? texture_manager_getter(owner) : nullptr;
        }

    public:
        /// 绑定注册表与纹理管理器取用方式（注册表须比本解析器活得久）
        void Bind(mtl::MaterialRecipeRegistry *recipe_registry,
                  TextureManagerGetter getter, void *owner_ptr)
        {
            registry = recipe_registry;
            texture_manager_getter = getter;
            owner = owner_ptr;
            lookup_cache.Clear();
        }

        /// 注册器内容变化时整表作废（失败标记与新注册名都要重新裁决）
        void ClearCache() { lookup_cache.Clear(); }

        /// 公开入口：解析（找不到/解析失败 ⇒ nullptr）
        const mtl::MaterialRecipe *Resolve(const char *request) { return Resolve(request, 0); }
    };
}