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
        TextureArray,   ///< 由层装配规约构建的 2D 数组（`texarray:@<name>` / `texarray:file:<path>`）
        CubeFile,       ///< 立方体贴图文件（`cubefile:<path>`，供 `sky_cube` 这类 TextureCube 槽位）
    };

    /// `rt:` 缺省附件名：首个颜色附件
    inline constexpr os_char kTextureURIAttachmentColor[]     = OS_TEXT("color0");

    /// `rt:` 深度附件名
    inline constexpr os_char kTextureURIAttachmentDepth[]     = OS_TEXT("depth");

    inline constexpr os_char kTextureURISchemeFile[]          = OS_TEXT("file");
    inline constexpr os_char kTextureURISchemeRenderTarget[]  = OS_TEXT("rt");
    inline constexpr os_char kTextureURISchemeTextureArray[]  = OS_TEXT("texarray");
    inline constexpr os_char kTextureURISchemeCubeFile[]      = OS_TEXT("cubefile");

    /**
     * `texarray:` 的两种写法（`path` 里保留原始标记，便于回写与去重）：
     * - `texarray:@<name>`：按**已注册**的数组名取（对应 `<name>.texarray.toml`）；
     * - `texarray:file:<path>`：直接给规约文件路径。
     *
     * `@` 前缀与数据块那一层（`mtldata:@<name>`）保持一致，避免「名字」和「路径」
     * 在同一个字段里混着猜。
     */
    inline constexpr os_char kTextureURIArrayNamePrefix          = OS_TEXT('@');
    inline constexpr os_char kTextureURIArrayFilePrefix[]        = OS_TEXT("file:");

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
     * - `texarray:@<name>` / `texarray:file:<path>`：2D 数组，来源是层装配规约文件。
     *   这种纹理**不是一张文件贴图**，而是一组层拼出来的对象，因此单独一个 kind。
     * - `cubefile:<path>`：立方体贴图文件（`.CubeTexture`）。同样是「一张文件」，
     *   但对象类型是 `TextureCube`，与 2D 走的是不同的加载器，因此单独一个 kind。
     *   注意它**不能**简写成裸路径 —— 裸路径一律按 2D 处理。
     *
     * 注意 `C:\path\to\texture.Tex2D` 这类盘符不会被误判成 scheme：
     * 只有 `file`/`rt`/`texarray`/`cubefile` 几个已知 scheme 才生效，
     * 其余形如 `<prefix>:...` 的一律按文件路径处理。
     */
    struct TextureURI
    {
        TextureURIKind  kind    = TextureURIKind::File;
        OSString        raw;            ///< 原始输入
        OSString        path;           ///< File/CubeFile: 文件路径 / RenderTarget: RenderTarget 名
        OSString        attachment;     ///< RenderTarget: 附件名（File 时为空）

        bool IsValid       ()const{return !path.IsEmpty();}
        bool IsFile        ()const{return kind==TextureURIKind::File;}
        bool IsCubeFile    ()const{return kind==TextureURIKind::CubeFile;}
        bool IsRenderTarget()const{return kind==TextureURIKind::RenderTarget;}
        bool IsTextureArray()const{return kind==TextureURIKind::TextureArray;}

        /// 是否是「磁盘上的单张文件」来源（2D 或立方体贴图）
        bool IsDiskFile    ()const{return kind==TextureURIKind::File||kind==TextureURIKind::CubeFile;}

        /// `texarray:@<name>` 形态（按名字取已注册的数组）
        bool IsArrayByName()const;

        /// `texarray:@<name>` → `<name>`；其它形态返回空
        OSString GetArrayName()const;

        /// `texarray:file:<path>` → `<path>`；按名字形态返回空
        OSString GetArraySpecPath()const;

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
