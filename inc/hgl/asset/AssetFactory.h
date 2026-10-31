#pragma once

/**
 * 资产对象工厂 —— 「URI 进、AssetObject 出」的唯一入口。
 *
 * ── 要解决的问题 ───────────────────────────────────────────────────────────
 * 每种资产（几何 / 纹理 / 图元 / 材质配方 / 场景……）都有自己的注册表与加载器，
 * 调用方却只拿到一串 URI。没有工厂时，调用方必须自己 `if` 一遍类型再挑注册表，
 * 既漏不掉也加不了；每加一种资产就要改所有调用点。
 *
 * 这里把「类型 → 加载器」做成**运行期自注册表**：每种资产的实现文件在自己
 * 的 .cpp 里 `AssetFactory::Instance().Register(AssetType::Geometry, &LoadGeometry)`
 * 即可，工厂与调用方一行都不用改。
 *
 * ── 未注册类型怎么办 ───────────────────────────────────────────────────────
 * **降级而不是失败**。这是刻意的：
 *   · 离线工具（打包、依赖扫描、文档导出）只需要知道「这个场景引用了什么」，
 *     不需要真的把每个资产读进内存；能拿到 URI 就够了；
 *   · 老版本引擎/工具遇到新类型资产时，不应该整体崩掉；
 *   · 与 `AssetURI` 已注册类型段「保留原拼写」的语义自然衔接。
 * 降级产物是 `GenericAssetObject`：只记 URI，不读任何内容，
 * `GetClassMetaInfo()` 返回 nullptr 就是这个信号。
 *
 * ── 线程约定 ───────────────────────────────────────────────────────────────
 * 注册表**不是**线程安全的。注册只在启动期（静态初始化或首次使用前）发生，
 * 之后只读。加载器本身可能触及 GPU 资源，同样应在主线程调用。
 */

#include <hgl/asset/AssetURI.h>
#include <hgl/asset/AssetObject.h>
#include <hgl/type/Smart.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::asset
{
    /// 一次加载的结果码。`Fallback` 不是错误，是**设计内**的降级。
    enum class AssetLoadStatus : uint8
    {
        Ok = 0,        ///< 由已注册的加载器成功创建
        Fallback,      ///< 类型未注册（或调用方禁止加载），降级为 `GenericAssetObject`
        ParseFailed,   ///< 加载器接手但读取/校验失败

        ENUM_CLASS_RANGE(Ok, ParseFailed)
    };

    using AssetObjectPtr = SharedPtr<AssetObject>;

    /// 加载器实现。返回 false 表示「我认这个类型但这次没读成」，`error` 里写原因。
    /// 返回 true 时必须已经把 `out` 填成有效对象。
    using AssetLoaderFn = bool (*)(const AssetURI &uri, AssetObjectPtr &out, AnsiString &error);

    /// 加载选项。
    struct AssetLoadOptions
    {
        /// `AssetURI` 省略类型段时补的类型（例如纹理调用方传 `AssetType::Texture`，
        /// 这样历史的 `file:rock.tex` / `rt:Offscreen:color0` 无需改写）。
        AssetType default_type = AssetType::Unknown;

        /// 类型未注册时是否降级为 `GenericAssetObject`。
        /// 置 false 则返回空指针 + `AssetLoadStatus::Fallback`，供「必须真读到」的场合用。
        bool allow_fallback = true;
    };

    /**
     * @brief 通用占位资产对象 —— 未注册类型的降级产物。
     *
     * 只保留定位信息，不读文件、不建 GPU 资源。它的存在让「只关心引用了什么」
     * 的工具（离线打包、依赖图、资源报告、跨版本迁移）不必认识每一种资产。
     *
     * `GetClassMetaInfo()` 恒为 nullptr —— 这是「没有具体规约」的显式信号，
     * 检查器遇到它应退化成「只显示 URI」。
     */
    class GenericAssetObject : public AssetObject
    {
    protected:

        AssetURI    uri;
        AnsiString  asset_name;

    public:

        explicit GenericAssetObject(const AssetURI &u);

        const ClassMetaInfo *GetClassMetaInfo() const override { return nullptr; }
        const AnsiString    &GetAssetName()     const override { return asset_name; }

        const AssetURI      &GetURI()           const { return uri; }

        /// 恒为 true：提示调用方「这只是一个占位」。
        bool IsFallback() const { return true; }
    };

    /**
     * @brief 资产对象工厂。
     *
     * ```cpp
     * // 某资产的实现文件里自注册（启动期执行一次）
     * AssetFactory::Instance().Register(AssetType::Geometry, &LoadGeometryAsset);
     *
     * // 调用方只认 URI
     * AssetLoadOptions opt;
     * opt.default_type = AssetType::Texture;
     * auto obj = AssetFactory::Instance().Create(u8"tex:file:rock.tex", opt);
     * ```
     */
    class AssetFactory
    {
    protected:

        /// 按 `AssetType` 的整数值直接索引；`RANGE_SIZE` 个槽位全部预置为空。
        AssetLoaderFn loaders[static_cast<size_t>(AssetType::RANGE_SIZE)];

    public:

        static AssetFactory &Instance();

        /// 登记加载器。`type` 非法或 `fn` 为空时返回 false。
        bool Register(AssetType type, AssetLoaderFn fn);

        /// 注销加载器（主要给测试用）。
        bool Unregister(AssetType type);

        bool IsRegistered(AssetType type) const;

        /// 已注册的加载器个数。
        uint32_t GetRegisteredCount() const;

        /// 清空全部登记（主要给测试用）。
        void Clear();

        /// 单个 URI。失败返回空指针，`out_status` 给出原因。
        AssetObjectPtr Create(const AssetURI &uri,
                              const AssetLoadOptions &options = AssetLoadOptions(),
                              AssetLoadStatus *out_status = nullptr);

        /// 单个 URI（文本形式，就地解析）。
        AssetObjectPtr Create(const OSString &uri_text,
                              const AssetLoadOptions &options = AssetLoadOptions(),
                              AssetLoadStatus *out_status = nullptr);

        /**
         * @brief 一批 URI 一次读完。
         *
         * 输入顺序即输出顺序；单个失败不会中断其它项（失败位留空指针）。
         * `out_objects` 由调用方提供、长度至少 `count`；可传 nullptr 表示只要求返回成功数。
         * 每次都**整体覆写**（不清空、不追加），因此调用方无需预置内容。
         * 返回成功（`Ok` + `Fallback`）的个数。
         */
        uint32_t Create(const OSString *uri_texts, uint32_t count,
                        AssetObjectPtr *out_objects,
                        const AssetLoadOptions &options = AssetLoadOptions());
    };
}//namespace hgl::asset