#pragma once

#include <cstdint>

namespace hgl
{
    namespace ecs
    {
        /**
         * RenderPassSlot —— **渲染 pass 槽位（跨模块单源）**。
         *
         * 为什么需要它：本引擎一帧内会渲染多个 pass（主帧 + 本帧的阴影视图），而下列三处
         * 都必须**按 pass 分份**存数据，遍数必须一致：
         *   · cull 竞技场的扇区（`GPUCullSystem::ArenaSectorBase`）
         *   · 批行 / 每批计数的块（`GPUCullSystem::BatchBlockBase`）
         *   · 实体 4-ID 表（`EntityRenderIDTable`，行内容按 pass 解析：阴影 pass 用阴影程序/材质变体）
         * 各写一遍"2"就是漂移面 ⇒ 这里单源定义，其余位置以**静态断言**守同值
         * （GLSL 侧走 push/#define，由门 `B.batch-info-struct-parity` 做数值对拍）。
         *
         * `I` 为 pass 号、`Count` 为遍数；越界的 pass 由各调用点 fail-fast（响亮），这里不下钳位。
         */
        struct RenderPassSlot
        {
            static constexpr uint32_t Main   = 0;   ///< 主帧 pass（可见画面）
            static constexpr uint32_t Shadow = 1;   ///< 阴影 pass（本帧的阴影视图）
            static constexpr uint32_t Count  = 2;
        };
    }//namespace ecs
}//namespace hgl