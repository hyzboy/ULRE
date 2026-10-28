#pragma once

#include<hgl/type/String.h>

namespace hgl::graph
{
    /**
     * 纹理寻址方式。资源句柄（`TextureAsset`）统一用 URI 描述「这张纹理从哪来」，
     * 使得「文件纹理」和「离屏渲染目标附件」在材质/系统侧表现为同一种东西。
     */
    enum class TextureURIKind : uint8
    {
        File = 0,       ///< 磁盘文件（`file:<path>`，无 scheme 时默认按此处理）
        RenderTarget,   ///< 某个 RenderTarget 的附件（`rt:<target>[:<attachment>]`）
    };

    /// `rt:` 缺省附件名：首个颜色附件
    inline constexpr os_char kTextureURIAttachmentColor[]     = OS_TEXT("color0");

    /// `rt:` 深度附件名
    inline constexpr os_char kTextureURIAttachmentDepth[]     = OS_TEXT("depth");

    inline constexpr os_char kTextureURISchemeFile[]          = OS_TEXT("file");
    inline constexpr os_char kTextureURISchemeRenderTarget[]  = OS_TEXT("rt");

    /**
     * 解析后的纹理 URI。
     *
     * 语法：
     * - `file:<path>`（或裸 `<path>`）：磁盘文件。`path` 保留原样，不在这里做规范化，
     *   交由 `MakeLoadedFileKey` 处理，避免出现第二套路径规则。
     * - `rt:<target>[:<attachment>]`：`target` 是 `RenderTargetDesc::name`，
     *   `attachment` 缺省为 `color0`，深度用 `depth`。
     *   分隔符取**最后一个**冒号，且只有其后是合法附件名（`color`/`color<N>`/`depth`）时才
     *   算分隔 —— 因为 RT 名常沿用 `resource_prefix`（如 `RenderToTexture:OffscreenRT`），
     *   名字里带冒号是常态。因此 `rt:A:B:color0` = 名 `A:B` + 附件 `color0`，
     *   而 `rt:A:B` = 名 `A:B` + 缺省 `color0`。
     *
     * 注意 `C:\path\to\texture.Tex2D` 这类盘符不会被误判成 scheme：
     * 只有 `file`/`rt` 两个已知 scheme 才生效，其余形如 `<prefix>:...` 的一律按文件路径处理。
     */
    struct TextureURI
    {
        TextureURIKind  kind    = TextureURIKind::File;
        OSString        raw;            ///< 原始输入
        OSString        path;           ///< File: 文件路径 / RenderTarget: RenderTarget 名
        OSString        attachment;     ///< RenderTarget: 附件名（File 时为空）

        bool IsValid       ()const{return !path.IsEmpty();}
        bool IsFile        ()const{return kind==TextureURIKind::File;}
        bool IsRenderTarget()const{return kind==TextureURIKind::RenderTarget;}

        /// 规范化的 URI 文本（用于日志与去重键）
        OSString ToString()const;

        /// 解析 URI；非法输入返回的 `TextureURI` 满足 `!IsValid()`
        static TextureURI Parse(const OSString &uri);

        /// 无 scheme 前缀时补 `file:`
        static OSString Normalize(const OSString &uri);
    };//struct TextureURI

    /**
     * 名字域转换：`RenderTargetDesc::name` 是 `AnsiString`，URI 用 `OSString`。
     *
     * 两者之间的往返在各处自己写 `reinterpret_cast` 极易出错（`u8char` 在 C++20 下
     * 是 `char8_t`），因此统一收敛到这一对函数。
     */
    AnsiString ToRenderTargetName(const OSString &os_name);
    OSString   FromRenderTargetName(const AnsiString &ansi_name);
}//namespace hgl::graph
