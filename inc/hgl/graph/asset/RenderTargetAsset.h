#pragma once

/**
 * RenderTargetAsset —— 可命名的离屏渲染目标规约
 *
 * 为什么存在：
 * 离屏渲染的目标（尺寸、颜色/深度格式、清屏色）原先只能由代码用
 * `RenderTargetDesc::OffscreenColorDepth(w,h,name)` 现拼，并且「谁消费这个 RT」
 * 这件事完全靠调用方手动 `GetColorTexture()` 再包装成纹理句柄来维持。
 * 结果是：RT 与材质的关联是**命令式的、一次性的**，RT 重建（Resize）后关联失效，
 * 也无法从外部配置调整。
 *
 * 本文件把 RT 提成一种**有名字的资产**：
 * - 名字即寻址：纹理侧用 `rt:<name>[:<attachment>]` 引用（见 TextureURI.h）
 * - 尺寸有两种口径：绝对像素，或窗口千分比（跟随窗口缩放）
 * - 附件按名字寻址：`color0`/`color1`/…/`depth`
 * - 可以从 TOML 读入（见 `LoadRenderTargetAssetSpecFromTOML`），便于外部调参
 *
 * 分工：
 * - 本文件只描述「这个 RT 长什么样」，不持有任何 GPU 对象
 * - `RenderTargetManager` 负责按 spec 创建/重建 RT，并把附件绑定到 `rt:` 纹理句柄
 */

#include<hgl/graph/render/RenderTargetDesc.h>
#include<hgl/type/String.h>

namespace hgl::graph
{
    class IRenderTarget;
    class Texture2D;

    /**
     * RT 尺寸口径。
     */
    enum class RenderTargetSizeMode : uint8
    {
        Absolute = 0,       ///< `width`/`height` 是像素
        WindowRelative,     ///< `width`/`height` 是窗口尺寸的千分比（1000 = 100%）
    };

    struct RenderTargetAssetSpec
    {
        /// 资产名（同时是 `rt:` 的 target 名，必须唯一且非空）
        AnsiString name;

        RenderTargetSizeMode size_mode = RenderTargetSizeMode::Absolute;

        uint32_t width  = 0;
        uint32_t height = 0;

        /// 格式/深度/清屏/flags 模板。`kind` 固定为 Offscreen，`width`/`height`/`name`
        /// 在 `ToDesc()` 里被本 spec 覆写，因此模板里**不要**填这三项。
        RenderTargetDesc tpl;

    public:

        bool IsValid()const;

        uint32_t ResolveWidth (uint32_t window_width)const;
        uint32_t ResolveHeight(uint32_t window_height)const;

        /// 用给定窗口尺寸展开成可直接交给 `RenderTargetManager::Create` 的 desc
        bool ToDesc(uint32_t window_width,uint32_t window_height,RenderTargetDesc &out)const;

        /// 附件数量（颜色数 + 深度数），用于诊断
        uint32_t GetAttachmentCount()const;

    public:     // 便捷构造（与 RenderTargetDesc 的三个工厂一一对应）

        static RenderTargetAssetSpec ColorDepth(uint32_t w,uint32_t h,const AnsiString &name,VkFormat depth_fmt=PF_UNDEFINED);
        static RenderTargetAssetSpec ColorOnly (uint32_t w,uint32_t h,const AnsiString &name);
        static RenderTargetAssetSpec DepthOnly (uint32_t w,uint32_t h,const AnsiString &name,VkFormat depth_fmt=PF_UNDEFINED);
    };//struct RenderTargetAssetSpec

// 附件寻址

    /// `index` → `color<index>`（`index==0` 时就是 `color0`）
    OSString MakeColorAttachmentName(uint32_t index);

    /// `color<N>` / `color` → N。返回 false 表示不是颜色附件名。
    bool ParseColorAttachmentName(const OSString &name,uint32_t &out_index);

    /**
     * 把附件名解析成实际纹理。
     *
     * - `color0`（缺省）/`color`/`color<N>` → 第 N 个颜色附件
     * - `depth` → 深度附件
     *
     * 失败返回 nullptr，`error` 非空时写入原因（UTF-8，可直接用于 `%s`）。
     */
    Texture2D *ResolveRenderTargetAttachment(IRenderTarget *rt,const OSString &attachment,const char **error=nullptr);

// 格式名 ↔ VkFormat（TOML 友好）

    /// `VK_FORMAT_R8G8B8A8_UNORM` → `"rgba8"`；未知格式返回 `nullptr`
    const char *GetRenderTargetFormatName(VkFormat fmt);

    /// `"rgba8"` → `VK_FORMAT_R8G8B8A8_UNORM`；未知文本返回 false
    bool ParseRenderTargetFormat(const char *text,VkFormat &out_fmt);

// TOML

    /**
     * 从 TOML 文件读入一个 RT 资产规约。
     *
     * 结构：
     * ```toml
     * name         = "OffscreenColor"
     * size_mode    = "absolute"        # 或 "window_relative"（千分比）
     * width        = 512
     * height       = 512
     * has_color    = true
     * color_formats = ["rgba8"]        # 省略 ⇒ 设备默认 surface format
     * has_depth    = true
     * depth_format = "d32"             # 省略 ⇒ 设备默认深度格式
     * resizable    = true
     * follow_window = false
     * slot_count   = 1
     * clear_color  = [0.1, 0.2, 0.3, 1.0]
     * ```
     */
    bool LoadRenderTargetAssetSpecFromTOML(const OSString &filename,RenderTargetAssetSpec &out);
}//namespace hgl::graph
