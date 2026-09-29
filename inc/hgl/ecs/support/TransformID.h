#pragma once

#include<hgl/ecs/support/TransformDataStorage.h>
#include<cstdint>

namespace hgl
{
    namespace ecs
    {
        /**
         * 物体的移动性（静态缓存/每帧 ring 的分流依据）。
         *
         * 位置从 `TransformComponent.h` 移到这里：T8 起变换不再必须由组件承载，
         * 但所有变换 API（accessor / 存储 / 系统）都要用到这个枚举。
         */
        enum class Mobility : uint8_t
        {
            Static,
            Movable
        };

        /**
         * 变换的**世界内**标识。
         *
         * 语义 = 所属世界的 `TransformDataStorage` 行号（沿用原 `HandleID`）。
         *
         * **它不是全局 ID**：离开所属世界（`ECSContext`）单独持有/比较都无意义
         * —— 跨世界使用必须同时携带世界（见 `TransformAccessor`）。
         */
        using TransformID = TransformDataStorage::HandleID;

        inline constexpr TransformID INVALID_TRANSFORM_ID = TransformDataStorage::INVALID_HANDLE;

        inline constexpr bool IsValidTransformID(const TransformID &id)
        {
            return id != INVALID_TRANSFORM_ID;
        }
    }//namespace ecs
}//namespace hgl
