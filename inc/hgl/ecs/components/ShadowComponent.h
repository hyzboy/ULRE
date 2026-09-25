#pragma once

#include<hgl/ecs/core/Component.h>

namespace hgl::ecs
{
    /**
     * ShadowComponent - 实体阴影属性控制组件
     * 挂载在拥有 RenderableComponent 的 Entity 上。
     * 若实体未挂载本组件，管线一律采用全局默认规则（投射且接收）。
     */
    class ShadowComponent : public Component
    {
    private:

        bool  cast_shadow        = true;   ///< 是否投射阴影（默认开启）
        float max_cast_distance  = 0.0f;   ///< 最大投射距离（米；<= 0.0f 代表不限/使用场景全局级联）
        bool  receive_shadow     = true;   ///< 是否接收阴影
        float bias_multiplier    = 1.0f;   ///< 局部 Bias 调节倍率（防止特异薄片物体阴影悬浮/acne）

    public:

        explicit ShadowComponent(const std::string &name = "Shadow");
        ~ShadowComponent() override = default;

        void OnAttach() override;
        void OnDetach() override;

        // ── 投射控制 (Caster) ──
        bool CanCastShadow() const { return cast_shadow; }
        void SetCastShadow(bool enable) { cast_shadow = enable; }

        float GetMaxDistance() const { return max_cast_distance; }
        void SetMaxDistance(float dist) { max_cast_distance = dist; }

        // ── 接收控制 (Receiver) ──
        bool CanReceiveShadow() const { return receive_shadow; }
        void SetReceiveShadow(bool enable) { receive_shadow = enable; }

        // ── 局部偏差 ──
        float GetBiasMultiplier() const { return bias_multiplier; }
        void SetBiasMultiplier(float multiplier) { bias_multiplier = multiplier; }
    };
}//namespace hgl::ecs
