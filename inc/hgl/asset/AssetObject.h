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

#include <cstdint>

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

        /// 仅编辑器使用（辅助线、调试缓存……）。发行版可不编译该字段，因此
        /// **不进 FlatBuffers 的对外契约**，只在 TOML 的编辑器侧往返。
        PF_EditOnly         = 1u << 4,

        /// 不参与序列化（运行时句柄、派生缓存）。TOML 与 FlatBuffers 都会跳过。
        PF_Transient        = 1u << 5,

        PF_HasRange         = 1u << 6,   ///< `range_min`/`range_max` 有效（软范围，滑块用）
        PF_HasClamp         = 1u << 7,   ///< `range_min`/`range_max` 是硬约束（越界即拒绝）
        PF_HasStep          = 1u << 8,   ///< `step` 有效（拖动步长）
        PF_HasAssetRef      = 1u << 9,   ///< `asset_ref` 有效（该字符串是资产引用）

        PF_Serialized       = 1u << 10,  ///< 参与 TOML / FlatBuffers 往返（`Transient` 的补集）
    };

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

    /// 属性是否参与序列化。
    inline bool IsSerialized(const PropertyMetaInfo &prop)
    {
        return (prop.flags & PF_Serialized) != 0;
    }

    /// 属性是否可编辑（检查器据此决定用可写控件还是只读控件）。
    inline bool IsEditable(const PropertyMetaInfo &prop)
    {
        return (prop.flags & (PF_EditAnywhere | PF_EditDefaultsOnly)) != 0
            && (prop.flags & PF_ReadOnly) == 0;
    }

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
    };
}//namespace hgl::asset
