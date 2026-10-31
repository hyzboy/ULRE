#pragma once

#include<hgl/graph/asset/TextureURI.h>
#include<hgl/type/ManagedArray.h>
#include<hgl/type/String.h>

/**
 * 纹理数组（`Texture2DArray`）的**层装配规约**。
 *
 * ── 要解决的问题 ───────────────────────────────────────────────────────────
 * 示例里建一张 2D 数组一直是三段硬编码：
 * 1. 先加载第 0 层当「探针」，只为拿 `width/height/format/mip`；
 * 2. `CreateTexture2DArray` 用探针的尺寸建空数组；
 * 3. `for(layer...) LoadTexture2DArray(array, layer, 文件)` 逐层填。
 *
 * 这套流程本身没错，问题是**层清单写在代码里**：文件夹名字表（`PBR_FOLDER_NAME[]`）、
 * 层数（`GRID_SIZE`）、每层的 `baseColor.Tex2D` / `normal.Tex2D` 约定全是 C++ 常量。
 * 换个贴图集就要改代码、重编译；同一份清单在 `CascadeShadowMap` / `PBRSpheres` /
 * `SingleSphereMaterialSwitch` / `AlphaTestShadow` 里各抄一遍。
 *
 * 现在层清单搬进 `*.texarray.toml`，代码只留一个 URI：
 *
 * ```toml
 * schema = 1
 * name   = "csm_pbr_baseColor_array"
 *
 * [[layers]]
 * name = "concrete_plain"
 * uri  = "file:res/image/pbr/Concrete_Plain/baseColor.Tex2D"
 * ```
 *
 * ── 为什么不留 `width` / `height` / `format` ─────────────────────────────────
 * 这三样都能从**第 0 层文件**读出来，写进文件就是第二份真源，改贴图尺寸时必然不同步。
 * 只有 `mipmaps` 留了可选覆盖：数组的 mip 数影响「只写了 mip0 的层」在缩小采样时的
 * 表现，个别用法（alpha test 遮罩）需要钉死成 1 层 mip，不能跟着探针走。
 */

namespace hgl::graph
{
    /// 规约版本；不匹配直接判非法（避免旧代码读新文件时静默按默认值跑）
    inline constexpr int kTextureArraySpecSchema = 1;

    /// 单个数组的层数上限（防御性；真实用途在 1..16）
    inline constexpr int kTextureArrayMaxLayers = 64;

    /// 数组规约文件扩展名
    inline constexpr os_char kTextureArraySpecExtension[] = OS_TEXT(".texarray.toml");

    /// `texarray:@<name>` 的默认目录（相对当前工作目录，与示例中 `mtldata:file:...` 同一套相对约定）
    inline constexpr os_char kTextureArrayDefaultDirectory[] = OS_TEXT("ShaderLibrary/asset/texarray/");

    /// 一层：名字（可选，用于日志）+ 来源纹理 URI
    struct TextureArrayLayerSpec
    {
        AnsiString  name;   ///<层名；文件里没写时由 URI 主干补齐
        OSString    uri;    ///<层来源（`file:<path>` 或裸路径；`rt:` 不支持）
    };

    /// 一个 2D 数组的完整装配规约
    struct TextureArraySpec
    {
        int         schema  = kTextureArraySpecSchema;
        AnsiString  name;                       ///<数组名（决定纹理对象名与日志）
        uint32_t    mipmaps = 0;                ///<0 = 由第 0 层探针决定
        ManagedArray<TextureArrayLayerSpec> layers;
    };

    /// 解析结果码。`Skipped` 只用于空输入（与数据块层保持同一套语义）
    enum class TextureArraySpecParseResult : uint8
    {
        OK = 0,
        Skipped,        ///<空输入，什么都没做
        IoError,        ///<文件读不出来
        InvalidValue,   ///<语法/取值/结构错误
        UnknownKey,     ///<出现了规约里没有的键

        ENUM_CLASS_RANGE(OK, UnknownKey)
    };

    /// 解析失败时的定位信息（写日志与句柄失败原因都用它）
    struct TextureArraySpecParseError
    {
        int         line = 0;       ///<TOML 语法错误的行号（结构错误为 0）
        AnsiString  field;          ///<出错字段路径，如 `layers[2].uri`
        AnsiString  detail;         ///<人类可读原因

        /// 拼成一句可直接展示的文本
        AnsiString Format() const;
    };

    const char *GetTextureArraySpecParseResultName(TextureArraySpecParseResult result);

    /**
     * @brief TOML 文本 → `TextureArraySpec`。
     *
     * 校验项（任一不满足即 `InvalidValue`，不返回半成品）：
     * - `schema` 必须存在且等于 `kTextureArraySpecSchema`；
     * - `name` 非空；
     * - `layers` 至少一层、至多 `kTextureArrayMaxLayers` 层；
     * - 每层 `uri` 非空且是文件来源（`rt:` 不是磁盘纹理，不能当数组层）；
     * - 层名（显式写的，或由 URI 主干补的）互不相同 —— 同名的两层无法在诊断里区分。
     */
    TextureArraySpecParseResult ParseTextureArraySpecText(const char *content,
                                                          int content_size,
                                                          TextureArraySpec &out,
                                                          TextureArraySpecParseError *out_error = nullptr);

    /// 读文件并解析（唯一涉及 IO 的入口）
    TextureArraySpecParseResult ParseTextureArraySpecFile(const OSString &path,
                                                          TextureArraySpec &out,
                                                          TextureArraySpecParseError *out_error = nullptr);

    /// 由路径推出数组名：`.../csm_pbr_baseColor_array.texarray.toml` → `csm_pbr_baseColor_array`
    AnsiString MakeTextureArrayNameFromPath(const OSString &path);

    /**
     * @brief `texarray:` URI → 实际的规约文件路径。
     *
     * - `texarray:file:<path>` → `<path>` 原样返回；
     * - `texarray:@<name>`     → `<kTextureArrayDefaultDirectory><name>.texarray.toml`；
     * - 其它（含非 `texarray:` 的 URI）→ 空。
     *
     * 只做**路径映射**，不检查文件是否存在 —— 存在性由解析入口报 `IoError`。
     */
    OSString ResolveTextureArraySpecPath(const TextureURI &uri);
}//namespace hgl::graph
