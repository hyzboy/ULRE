#pragma once

#include<hgl/type/String.h>
#include<hgl/type/EnumUtil.h>

namespace hgl::asset
{
    /**
     * 资产类型。对应 URI 的**第一段**：`geo:builtin:cube` 里的 `geo`。
     *
     * 每新增一种 `AssetObject` 派生类型，就在这里加一个枚举值 + 在
     * `AssetTypeTable` 里加一行 —— 名字 / 解析 / 反查三个方向全部由那张表派生，
     * 不得再写第二份拼写。
     */
    enum class AssetType : uint8
    {
        Unknown = 0,
        Geometry,           ///<`geo`   —— 几何资产（`GeometryAsset`）
        Texture,            ///<`tex`   —— 纹理资产（`TextureAsset`，内部仍由 `TextureURI` 解析）
        MaterialRecipe,     ///<`rcp`   —— 材质配方（`MaterialRecipeAsset`）
        Primitive,          ///<`prim`  —— 图元资产（`PrimitiveAsset`）
        Scene,              ///<`scene` —— 场景资产
        MaterialData,       ///<`mtldata`—— 材质数据块（`MaterialDataBlockAsset`）

        ENUM_CLASS_RANGE(Geometry, MaterialData)
    };

    /**
     * 资产来源。对应 URI 的**第二段**：`geo:builtin:cube` 里的 `builtin`。
     *
     * 语义是「这份资产的数据从哪来」，不是「怎么读」：`File` 只说明存在磁盘文件，
     * 具体是 TOML / FlatBuffers / 二进制由各类型自己的注册器决定。
     */
    enum class AssetSourceKind : uint8
    {
        Unknown = 0,
        Builtin,            ///<`builtin` —— 引擎内建参数化数据，不读盘
        File,               ///<`file`    —— 磁盘文件
        RenderTarget,       ///<`rt`      —— 渲染目标附件（仅纹理）

        ENUM_CLASS_RANGE(Builtin, RenderTarget)
    };

    struct AssetTypeDesc
    {
        AssetType       type;
        const char     *name;       ///<URI 类型段（小写，唯一拼写）
        const char     *label;      ///<人类可读名（日志/诊断用）
    };

    /// 类型拼写表唯一真源：名字 / 解析 / 反查全部从这里派生。
    inline constexpr AssetTypeDesc AssetTypeTable[] =
    {
        { AssetType::Geometry,       "geo",   "geometry"        },
        { AssetType::Texture,        "tex",   "texture"         },
        { AssetType::MaterialRecipe, "rcp",   "material_recipe" },
        { AssetType::Primitive,      "prim",  "primitive"       },
        { AssetType::Scene,          "scene", "scene"           },
        { AssetType::MaterialData,   "mtldata", "material_data"  },
    };

    struct AssetSourceKindDesc
    {
        AssetSourceKind  kind;
        const char      *name;      ///<URI 来源段（小写，唯一拼写）
    };

    /// 来源拼写表唯一真源。
    inline constexpr AssetSourceKindDesc AssetSourceKindTable[] =
    {
        { AssetSourceKind::Builtin,      "builtin" },
        { AssetSourceKind::File,         "file"    },
        { AssetSourceKind::RenderTarget, "rt"      },
    };

    const char     *GetAssetTypeName(AssetType type);
    const char     *GetAssetTypeLabel(AssetType type);
    AssetType       ParseAssetType(const char *name);
    AssetType       ParseAssetType(const AnsiString &name);

    const char     *GetAssetSourceKindName(AssetSourceKind kind);
    AssetSourceKind ParseAssetSourceKind(const char *name);
    AssetSourceKind ParseAssetSourceKind(const AnsiString &name);

    /**
     * 统一资产 URI。
     *
     * ## 语法
     *
     * ```
     * <type> : <source> : <target>      // 完整形式
     * <type> :            <target>      // 省略来源，由消费方按类型推导
     *         : <source> : <target>      // 省略类型，由调用方的 default_type 补齐
     *                     <target>      // 裸定位串，等价于只填 default_type
     * ```
     *
     * 例子：
     * - `geo:builtin:cube`                      —— 内建立方体预设
     * - `geo:file:res/geo/unit_cube.geo.toml`   —— 几何资产文件
     * - `geo:@cube` / `geo:cube`                —— 省略来源（历史写法 `geo:@<name>` 原样兼容）
     * - `tex:file:res/image/brick.tex`          —— 纹理文件
     * - `tex:rt:Offscreen:color0`               —— 渲染目标附件
     * - `rcp:@Lit`                              —— 材质配方（省略来源）
     * - `foo:file:bar.dat`                      —— 未注册类型（`type` 保持 `Unknown`，`type_name` 留原拼写）
     *
     * ## 与 `TextureURI` 的关系
     *
     * `file:<path>` / `rt:<target>[:<attachment>]` 在这里**天然落在「省略类型」这一支**：
     * 首段 `file` / `rt` 不是资产类型，而是来源，于是被识别成「来源段」，
     * 类型留给调用方的 `default_type`（纹理侧传 `AssetType::Texture`）。
     * 因此老的纹理 URI 文本无需改写即可被本解析器读懂；
     * `hgl::graph` 侧另有 `ToAssetURI` / `FromAssetURI` 做 `TextureURI` 往返。
     *
     * ## 分隔符规则
     *
     * 只看**前两个**冒号：类型段与来源段都是不含冒号的短标识符，第三段（`target`）
     * 一律整段保留。这样 `geo:file:C:\a\b.toml` 与 `tex:rt:A:B:color0` 都不需要
     * 额外特判 —— 盘符与 RT 名里的冒号自动归入 `target`。
     */
    struct AssetURI
    {
        AssetType       type   = AssetType::Unknown;
        AssetSourceKind source = AssetSourceKind::Unknown;

        AnsiString      type_name;      ///<类型段原拼写（未注册类型时保留，供降级诊断）
        AnsiString      source_name;    ///<来源段原拼写

        OSString        target;         ///<定位段：预设名 / 文件路径 / 渲染目标名
        OSString        raw;            ///<原始输入

        bool IsValid   ()const{return !target.IsEmpty();}
        bool HasType   ()const{return type  !=AssetType::Unknown;}
        bool HasSource ()const{return source!=AssetSourceKind::Unknown;}
        bool IsBuiltin ()const{return source==AssetSourceKind::Builtin;}
        bool IsFile    ()const{return source==AssetSourceKind::File;}

        /// 规范化文本（用于日志与去重键）。省略的段不会被补上，保持「写什么回什么」。
        OSString ToString()const;

        /**
         * 解析 URI。
         *
         * @param uri          待解析文本
         * @param default_type 输入里没有类型段时补上的类型；`Unknown` 表示不做补齐
         *                     （此时结果的 `type` 保持 `Unknown`，`target` 保留全串）。
         */
        static AssetURI Parse(const OSString &uri,AssetType default_type=AssetType::Unknown);

        /// 按段拼出 URI 文本；`type` / `source` 为 `Unknown` 时对应段被省略。
        static OSString MakeAssetURI(AssetType type,AssetSourceKind source,const OSString &target);

        /// `Parse(uri,default_type).ToString()`
        static OSString Normalize(const OSString &uri,AssetType default_type=AssetType::Unknown);
    };//struct AssetURI

    /// `AnsiString` → `OSString`（URI 段在内部是 `AnsiString`，定位段是 `OSString`）
    AnsiString AssetURISegmentToAnsi(const OSString &segment);
}//namespace hgl::asset
