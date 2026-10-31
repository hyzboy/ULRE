#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/type/String.h>
#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/SSBOTypes.h>

namespace hgl::graph
{
    // GlobalSSBOType 枚举定义于 SSBOTypes.h（供仅含 SSBOTypes.h 的 mtl 头直接使用）。

    constexpr bool IsGlobalSSBOType(const GlobalSSBOType type) noexcept
    {
        return type >= GlobalSSBOType::BEGIN_RANGE && type <= GlobalSSBOType::END_RANGE;
    }

    /// 未覆盖类型的名字哨兵（`GetGlobalSSBOTypeName` 的 default 出口）。
    /// 单独命名是为了让下面的 constexpr 覆盖性断言能**按指针**判定「这个类型没被覆盖」——
    /// switch + default 漏 case 编译期不报，只会在这里静默显形。
    inline constexpr const char *kUnknownGlobalSSBOTypeName = "UnknownGlobalSSBO";

    inline constexpr const char *GetGlobalSSBOTypeName(const GlobalSSBOType type) noexcept
    {
        switch (type)
        {
        case GlobalSSBOType::MeshDrawParams:        return "MeshDrawParams";
        case GlobalSSBOType::PBRSurface:           return "PBRSurface";
        case GlobalSSBOType::EmissiveSurface:      return "EmissiveSurface";
        case GlobalSSBOType::TransmissionSurface:  return "TransmissionSurface";
        case GlobalSSBOType::GeometryAABB:         return "GeometryAABB";
        case GlobalSSBOType::MaterialData:         return "MaterialData";
        default:                                   return kUnknownGlobalSSBOTypeName;
        }
    }

    /// 类型名表必须覆盖**全部**枚举项（遍历全枚举，不手列清单 ⇒ 新增类型时自动被要求补 case）。
    constexpr bool GlobalSSBOTypeNameTableComplete() noexcept
    {
        for (uint32_t i = 0; i < GlobalSSBOTypeCount; ++i)
        {
            if (GetGlobalSSBOTypeName(static_cast<GlobalSSBOType>(i)) == kUnknownGlobalSSBOTypeName)
                return false;
        }

        return true;
    }
    static_assert(GlobalSSBOTypeNameTableComplete(),
        "GetGlobalSSBOTypeName 必须覆盖 GlobalSSBOType 的全部枚举项（漏 case 会静默返回 Unknown）");

    /**
     * 名字 → 类型（大小写不敏感）。
     *
     * 名字↔值的唯一真源仍是上面的 `GetGlobalSSBOTypeName`：本函数**遍历全枚举**
     * 反查，不另立一张字面量表，新增类型时自动可用。找不到返回 `false` 且
     * `out` 不变。
     */
    inline bool ParseGlobalSSBOType(const char *text, GlobalSSBOType &out) noexcept
    {
        if (!text || !text[0])
            return false;

        for (uint32_t i = 0; i < GlobalSSBOTypeCount; ++i)
        {
            const auto type = static_cast<GlobalSSBOType>(i);

            if (hgl::stricmp(text, GetGlobalSSBOTypeName(type)) == 0)
            {
                out = type;
                return true;
            }
        }

        return false;
    }

    inline constexpr uint32_t GetGlobalSSBOTypeStructVersion(const GlobalSSBOType type) noexcept
    {
        switch (type)
        {
        case GlobalSSBOType::PBRSurface:
        case GlobalSSBOType::EmissiveSurface:
        case GlobalSSBOType::TransmissionSurface:
            return 1;
        default:
            break;
        }

        return 0;
    }

    inline constexpr uint32_t GetGlobalSSBOTypeStructStride(const GlobalSSBOType type) noexcept
    {
        switch (type)
        {
        case GlobalSSBOType::PBRSurface:            return sizeof(float) * 8;
        case GlobalSSBOType::EmissiveSurface:       return sizeof(float) * 4;
        case GlobalSSBOType::TransmissionSurface:   return sizeof(uint32_t) * 4;
        default:
            break;
        }

        return 0;
    }

    /// 材质字段行类型（version != 0）必须给出非零 stride —— 同样遍历全枚举自动判定。
    /// stride 表是**材质字段行**专用的：MeshDrawParams / GeometryAABB 等非材质类型本就不在此表内，
    /// 返回 0 表「无需校验」。
    /// 故判据取「version != 0 ⇒ stride != 0」，而不是「所有类型都必须有 stride」。
    constexpr bool GlobalSSBOTypeStrideTableComplete() noexcept
    {
        for (uint32_t i = 0; i < GlobalSSBOTypeCount; ++i)
        {
            const auto type = static_cast<GlobalSSBOType>(i);

            if (GetGlobalSSBOTypeStructVersion(type) != 0
             && GetGlobalSSBOTypeStructStride(type) == 0)
                return false;
        }

        return true;
    }
    static_assert(GlobalSSBOTypeStrideTableComplete(),
        "材质字段行类型（version != 0）必须补 GetGlobalSSBOTypeStructStride 的 case");

    /**
     * Identifies one live global-data row in a shared global SSBO.
     * The type selects the physical backing buffer; data_index selects its row.
     */
    struct GlobalSSBOBinding
    {
        GlobalSSBOType ssbo_type = GlobalSSBOType::PBRSurface;
        uint32_t ssbo_id = 0;
        uint32_t data_index = uint32_t(-1);

        constexpr bool IsValid() const noexcept
        {
            return IsGlobalSSBOType(ssbo_type)
                && ssbo_id != 0
                && data_index != uint32_t(-1);
        }

        /**
         * @brief 同一数据块内的相邻行：块内第 n 行 = 基行 + n。
         *
         * 一个块文件里的多行常常对应"同一种材质、不同颜色"的一批实体，
         * 它们只差一个行号，靠这个函数从一次 `GetMaterialDataBinding()` 展开即可，
         * 不必逐行再查一次 URI。越界由调用方负责（与 `GetRowBinding` 同一契约）。
         */
        constexpr GlobalSSBOBinding RowOffset(const uint32_t delta) const noexcept
        {
            GlobalSSBOBinding result = *this;
            result.data_index += delta;
            return result;
        }

        /// 有效性判定与 `IsValid()` 同一语义；显式 bool 只为让 `if (!binding)` 这类
        /// 判空写法（迁移前 `if (!accessor)`）不用逐处改成 `!x.IsValid()`。
        explicit constexpr operator bool() const noexcept
        {
            return IsValid();
        }
    };

    struct GlobalSSBOConfig
    {
        GlobalSSBOType type;
        const char *name;
        uint32_t row_bytes;
        uint32_t default_capacity;
        uint32_t reserve_rows;
    };
}
