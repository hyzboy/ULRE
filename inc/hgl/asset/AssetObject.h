#pragma once

/**
 * 资产对象元数据（反射描述）与基类。
 *
 * 本头只放**描述结构**，不含任何解析逻辑：谁来填这些表？—— `AssetGen` 生成器。
 * 运行时读取真源仍是 C++ 结构本身；这里的表是给**通用工具**用的：
 *
 *   · 编辑器检查器（Inspector）：按 `category` 分组、按 `display_name` 显示、
 *     用 `Range/Step` 造滑块、用 `AssetRef` 弹资产选择器；
 *   · 文档导出：`tooltip` + `cpp_type` + 缺省值 ⇒ 自动生成属性手册；
 *   · 通用批量工具：按 `offset/size` 做逐属性对拍、diff、迁移。
 *
 * 生成物在 `inc/hgl/asset/generated/<Group>Meta.gen.h`，由
 * `src/Tools/AssetGen/asset_gen.py` 产出，**不得手改**。
 *
 * ── 为什么偏移量在表里而不是靠虚函数 ───────────────────────────────────────
 * 资产规约是**值语义 POD**（可批量 memcpy、可按值传参），给它塞虚表会让
 * `sizeof` 翻倍、破坏 trivially-copyable 性质。因此 `AssetObject` 是**可选**的
 * 多态外壳，绝大多数规约只吃生成出来的自由函数 + 元数据表，不继承任何东西。
 */

#include <hgl/type/String.h>
#include <hgl/type/EnumUtil.h>
#include <hgl/type/ValueArray.h>
#include <hgl/serialize/EnumMeta.h>

#include <cstdint>

namespace hgl::serialize
{
    class Archive;
}//namespace hgl::serialize

namespace hgl::asset
{
    /// 属性在 TOML / 检查器里的值类别。
    enum class PropertyKind : uint8_t
    {
        Bool = 0,
        Int8, Int16, Int32, Int64,
        UInt8, UInt16, UInt32, UInt64,
        Float, Double,
        String,
        Vector2f, Vector3f, Vector4f,
        Enum,        ///< 有底层整数类型的 `enum class`；`enum_name` 给出完全限定名
        Struct,      ///< 另一个 `HGL_ASSET_STRUCT`（TOML 里是子表）
        ValueArray,  ///< `hgl::ValueArray<T>`（值语义动态数组）
        PtrArray,    ///< `hgl::ManagedArray<T>`（指针语义动态数组，TOML 里是表数组）

        /// 「没有类别」。只用于 `PropertyMetaInfo::elem_kind`（非容器字段的占位），
        /// 刻意放在 `ENUM_CLASS_RANGE` 之外 —— 它不是一种可赋给 `kind` 的取值，
        /// `RangeCheck<PropertyKind>(None)` 应当为 false。
        None,

        ENUM_CLASS_RANGE(Bool, PtrArray)
    };

    /// 属性的编辑器 / 序列化行为位。可组合。
    enum PropertyFlags : uint32_t
    {
        PF_None             = 0,

        PF_EditAnywhere     = 1u << 0,   ///< 任何实例都可编辑
        PF_EditDefaultsOnly = 1u << 1,   ///< 只能在原型/资产上编辑
        PF_VisibleAnywhere  = 1u << 2,   ///< 只读展示
        PF_ReadOnly         = 1u << 3,   ///< 编辑器只读

        /// 仅编辑器使用（辅助线、调试选项……）。字段仍存在于 C++ 结构中；
        /// 它进入作者侧 TOML，但不进入 FlatBuffers 发行版契约。
        PF_EditOnly         = 1u << 4,

        /// 不参与序列化（运行时句柄、派生缓存）。TOML 与 FlatBuffers 都会跳过。
        PF_Transient        = 1u << 5,

        PF_HasRange         = 1u << 6,   ///< `range_min`/`range_max` 有效（软范围，滑块用）
        PF_HasClamp         = 1u << 7,   ///< `range_min`/`range_max` 是硬约束（越界即拒绝）
        PF_HasStep          = 1u << 8,   ///< `step` 有效（拖动步长）
        PF_HasAssetRef      = 1u << 9,   ///< `asset_ref` 有效（该字符串是资产引用）

        PF_Serialized       = 1u << 10,  ///< 至少参与一种持久化格式（`Transient` 的补集）
        PF_SerializedToml   = 1u << 11,  ///< 参与作者侧 TOML 往返
        PF_SerializedFlatBuffers = 1u << 12, ///< 进入发行版 FlatBuffers 快照

        /**
         * 字段是 `hgl::serialize::Presence<T>`（三态：缺席 / 显式写缺省值 / 显式写非缺省值）。
         *
         * 树形后端（TOML）天然能表达三态 —— 键不出现就是缺席；但 FlatBuffers 的
         * 标量字段「不写」与「写 0」在字节上是同一件事，因此二进制后端必须知道
         * 哪些标量字段属于 `Presence<T>`，才能对它们**总是**真写入 `voffset`。
         * 非标量字段（表 / 向量）的缺席由「树里没有这个节点」自然表达，无需此位。
         */
        PF_Presence         = 1u << 13,
    };

    /// `kind == Enum` 时由生成器提供的名字表：名字 → 数值（TOML 文本后端与诊断用）。
    using EnumParseFn  = bool (*)(const char *name, int64_t &out);

    /// `kind == Enum` 时由生成器提供的名字表：数值 → 名字（写出与诊断用）。
    using EnumToNameFn = const char *(*)(int64_t value);

    /// 单个属性的描述。
    struct PropertyMetaInfo
    {
        const char *name;            ///< C++ 字段名（诊断用，永远有值）
        const char *toml_key;        ///< TOML 键名（缺省等于 `name`，可用 `Key=` 覆盖）
        const char *display_name;    ///< 检查器显示名（缺省等于 `name`）
        const char *category;        ///< 检查器分组（缺省 "Default"）
        const char *tooltip;         ///< 悬浮说明（缺省取 `///<` 尾注释，否则空串）
        const char *cpp_type;        ///< C++ 声明类型原文（文档导引用）

        const char *enum_name;       ///< `kind==Enum` 时的完全限定枚举名，否则 nullptr
        const char *asset_ref;       ///< `PF_HasAssetRef` 时的资产类别（geo/recipe/tex…）

        PropertyKind kind;
        uint32_t     flags;

        uint32_t     offset;         ///< 相对结构起始的字节偏移
        uint32_t     size;           ///< 字段字节数

        /// 数组字段的定长长度；`0` 表示动态数组（ValueArray / ManagedArray）。
        uint32_t     fixed_length;

        double       range_min;
        double       range_max;
        double       step;

        /// `kind` 为 `Struct`/`ValueArray`/`PtrArray` 且元素是资产结构时，指向该
        /// 结构的元数据（检查器展开子项、严格键校验递归都靠它）；否则 nullptr。
        const struct ClassMetaInfo *struct_meta;

        /// 容器（`ValueArray`/`PtrArray`）的元素类别；非容器字段为 `None`。
        /// FlatBuffers 后端据此决定向量元素是标量、字符串还是子表。
        PropertyKind elem_kind = PropertyKind::None;

        /// `kind == Enum` 时的名字表；其它类别为 nullptr。
        EnumParseFn  enum_parse   = nullptr;

        /// `kind == Enum` 时的名字表；其它类别为 nullptr。
        EnumToNameFn enum_to_name = nullptr;
    };

    /// 单个资产结构的描述。
    struct ClassMetaInfo
    {
        const char *name;            ///< C++ 结构/类名
        const char *asset_kind;      ///< 资产类别（由 `asset_gen.toml` 配置，如 "geo"）
        const char *doc;             ///< 结构级说明（来自 `///` 文档注释或配置）

        const PropertyMetaInfo *properties;
        uint32_t                property_count;

        uint32_t                struct_size;
        uint32_t                struct_align;
    };

    // ── 通用属性值（GUI / 文档 / diff 的公共载体）────────────────────────────
    //
    // 不追求覆盖所有类型：只覆盖检查器控件真正需要的那几种。容器与嵌套结构在检查器里
    // 是「展开后的子项列表」，走 `ClassMetaInfo` 递归，而不是塞进一个膨胀的 variant。

    struct PropertyValue
    {
        enum class Kind : uint8_t
        {
            None = 0,
            Bool,
            Integer,
            Float,
            String,
            Vector2,
            Vector3,
            Vector4,
        };

        Kind     kind = Kind::None;
        bool     b    = false;
        int64_t  i    = 0;
        double   f    = 0.0;
        double   v[4] = {0.0, 0.0, 0.0, 0.0};
        AnsiString s;

        bool IsNumeric() const
        {
            return kind == Kind::Integer || kind == Kind::Float;
        }
    };

    /// 属性是否参与至少一种持久化格式；按具体格式判断时使用下方专用查询。
    inline bool IsSerialized(const PropertyMetaInfo &prop)
    {
        return (prop.flags & PF_Serialized) != 0;
    }

    inline bool IsSerializedToml(const PropertyMetaInfo &prop)
    {
        return (prop.flags & PF_SerializedToml) != 0;
    }

    inline bool IsSerializedFlatBuffers(const PropertyMetaInfo &prop)
    {
        return (prop.flags & PF_SerializedFlatBuffers) != 0;
    }

    /// 属性是否声明为 `hgl::serialize::Presence<T>`（三态字段）。
    inline bool IsPresenceProperty(const PropertyMetaInfo &prop)
    {
        return (prop.flags & PF_Presence) != 0;
    }

    /// 属性是否是数组容器（`ValueArray` / `PtrArray`）。
    inline bool IsArrayProperty(const PropertyMetaInfo &prop)
    {
        return prop.kind == PropertyKind::ValueArray
            || prop.kind == PropertyKind::PtrArray;
    }

    /// 属性在文本后端（TOML/JSON）里的键名。
    inline const char *TomlKeyOf(const PropertyMetaInfo &prop)
    {
        return prop.toml_key ? prop.toml_key : prop.name;
    }

    /// 属性是否可编辑（检查器据此决定用可写控件还是只读控件）。
    inline bool IsEditable(const PropertyMetaInfo &prop)
    {
        return (prop.flags & (PF_EditAnywhere | PF_EditDefaultsOnly)) != 0
            && (prop.flags & PF_ReadOnly) == 0;
    }

    /**
     * 资产句柄的**装配阶段** —— 所有 `AssetObject` 派生类共用的唯一状态机。
     *
     * 为什么上移到这里：`MaterialRecipeAssetState` 与 `MaterialDataBlockAssetState`
     * 曾经各自定义了一份 `{Parsed, Ready, Failed}`，字面相同、语义相同、连诊断文案
     * 都一模一样。每加一种资产就要再抄一遍，等于把「同一条就绪度契约」复制 N 份。
     *
     * ```
     * Unloaded ──开始装载──> Loading ──数据到手──> Parsed ──校验/上船通过──> Ready
     *                            └──────失败──────> Failed <──校验/上船失败────┘
     * ```
     *
     * 三态是**最小公共集**，不是每种资产都会走满：纯文本解析的配方资产从 `Parsed`
     * 起步（没有可挂起的装载阶段）；需要读盘/上传的资产则从 `Unloaded`/`Loading` 起步。
     *
     * ## 与 `hgl::PayloadState` 正交，不要合并
     *
     * `AssetState` 回答「**这个句柄现在能不能用**」，`hgl::PayloadState`
     * （`<hgl/object/MetaObject.h>`）回答「**这份载荷还在不在内存里**」。两者是不同
     * 的轴，不是同一个状态机的两套措辞：
     *
     * | | `AssetState` | `PayloadState` |
     * |---|---|---|
     * | 归属 | `AssetObject`（`SharedPtr` 持有，无 `Init/Release`） | `MetaObject`（`RefObject`，有引用计数） |
     * | 生命周期 | 一次装配，不可回退 | `MetaOnly ⇄ Resident` 可反复往返 |
     * | 典型消费者 | `MaterialRecipeAsset`、`MaterialDataBlockAsset`（纯文本解析，无 GPU 载荷） | `GeometryAsset`、`TextureAsset`（有可卸载的 GPU 载荷） |
     *
     * 把 `AssetObject` 改成继承 `MetaObject` 会让「一份解析好的 TOML」凭空背上
     * 对象 ID、`Init/Release` 与引用计数，并把「装卸载可反复」的语义塞进一个
     * 本来只需要「就绪/失败」二值判断的句柄。**两者保持独立是有意的。**
     */
    enum class AssetState : uint8_t
    {
        Unloaded = 0,   ///<尚未开始装载
        Loading,        ///<装载/解析在途
        Parsed,         ///<数据已解析，领域契约与外部依赖**尚未**校验
        Ready,          ///<校验通过，可参与渲染
        Failed,         ///<装载或校验失败，诊断文案见具体类

        ENUM_CLASS_RANGE(Unloaded, Failed)
    };

    /// 状态名的唯一真源是文末的 `HGL_ENUM_BEGIN(hgl::asset::AssetState)` 登记块
    /// （见 `hgl::serialize::EnumNameOf` / `EnumParse`）。

    /**
     * 资产对象基类 —— **可选**的多态外壳。
     *
     * 只在「需要把不同类型资产放进同一个容器/句柄表」的场合继承它（例如场景加载器
     * 持有 `AssetObject*` 列表）。纯数据规约（`BuiltinSphereParams` 之类）**不要**继承：
     * 它们要保持 trivially-copyable，直接吃生成的 `ReadXxx/WriteXxx` 即可。
     */
    class AssetObject
    {
    protected:
        AssetObject() = default;

    public:
        virtual ~AssetObject() = default;

        /// 该实例的运行时类型描述（生成物里的静态表）。
        virtual const ClassMetaInfo *GetClassMetaInfo() const = 0;

        /// 资产名（可空；由派生类决定语义，例如 `*.geo.toml` 的 `name`）。
        virtual const AnsiString &GetAssetName() const = 0;

        // ── 就绪度统一契约 ────────────────────────────────────────────────────
        //
        // 唯一的状态源是 `GetAssetState()`。`IsReady()` / `IsFailed()` 只有一个
        // 实现，派生类不要各自再写一遍 —— 那正是本状态机上移要消灭的重复。
        // 不需要异步装载的派生类什么都不用覆盖，默认停在 `Unloaded`。

        /// 当前装配阶段（未覆盖 ⇒ `Unloaded`）。
        virtual AssetState GetAssetState() const { return AssetState::Unloaded; }

        /// 可参与渲染。
        virtual bool IsReady() const { return GetAssetState() == AssetState::Ready; }

        /// 装载或校验失败。
        bool IsFailed() const { return GetAssetState() == AssetState::Failed; }

        // ── 序列化统一虚接口 ──────────────────────────────────────────────────
        //
        // **只此一个**。不要在这里加 `SaveToToml` / `LoadFromFlat` 之类格式专用
        // 的兄弟接口：物理格式（TOML / FlatBuffers）由
        // `<hgl/serialize/ArchiveFile.h>` 按文件名后缀在基类之外分派，派生类只需
        // 关心「有哪些字段」。字段遍历本身也不该手写 —— 由 `HGL_ASSET_STRUCT` /
        // `HGL_PROPERTY` 注解经 AssetGen 生成，派生类里的实现应当只有一行：
        //
        // ```cpp
        // bool XxxAsset::Serialize(hgl::serialize::Archive &ar)
        // {
        //     hgl::serialize::StructSerializer<XxxSpec>::Do(ar, spec);
        //     return true;
        // }
        // ```
        //
        // 默认返回 `false` = 「本类型尚未接入统一序列化」，`AssetObject` 的可选性
        // 因此得以保持：不需要存档的派生类什么都不用写。

        /// 在一次 `Archive` 游标上遍历本类型的全部可序列化字段（读写同源）。
        virtual bool Serialize(hgl::serialize::Archive &/*ar*/) { return false; }
    };
}//namespace hgl::asset

// ── `AssetState` 登记 ───────────────────────────────────────────────────────
//
// 登记块必须写在全局作用域（见 `<hgl/serialize/EnumMeta.h>` 的硬性约定），
// 因此放在命名空间之外。此后 `EnumNameOf` / `EnumParse` / Archive 文本后端
// 都能直接处理这个枚举，不需要任何手写名字表。

HGL_ENUM_BEGIN(hgl::asset::AssetState)
    HGL_ENUM_ITEM(Unloaded, "尚未开始装载")
    HGL_ENUM_ITEM(Loading, "装载在途")
    HGL_ENUM_ITEM(Parsed, "数据已解析，尚未校验")
    HGL_ENUM_ITEM(Ready, "校验通过，可参与渲染")
    HGL_ENUM_ITEM(Failed, "装载或校验失败")
HGL_ENUM_END(hgl::asset::AssetState)
