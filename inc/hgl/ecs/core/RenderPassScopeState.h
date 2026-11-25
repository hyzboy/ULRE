#pragma once

#include<glm/glm.hpp>

namespace hgl::ecs
{
    /**
     * 当前渲染 Pass 的作用域状态（世界私有）。
     *
     * 这三件事共享**同一条生命周期**：都由 `RenderTo` 在进入 pass 时设定、退出时
     * 经 state_guard 恢复 ⇒ 合成一个对象，避免多处各自 Reset 时漏掉一个。
     *
     * 阴影上下文语义：`BeginShadow()` 声明本 pass 是否产出阴影贴图并**无条件**清掉原点；
     * 原点只能经 `SetShadowOrigin()` 在已知参考相机后设置 ⇒ `HasShadowOrigin()` 为假时
     * `ShadowOrigin()` 无意义，消费方必须先用 `HasShadowOrigin()` 判定。
     */
    class RenderPassScopeState
    {
    private:
        uint32_t active_camera_slot = 0;
        int mobility_filter = -1;       ///< -1 = 全部，0 = 仅静态，1 = 仅动态

        bool is_shadow_pass = false;
        bool has_shadow_origin = false;
        glm::vec3 shadow_origin{0.0f};

    public:
        void SetCameraSlot(uint32_t slot) { active_camera_slot = slot; }
        uint32_t GetCameraSlot() const { return active_camera_slot; }

        void SetMobilityFilter(int filter) { mobility_filter = filter; }
        int GetMobilityFilter() const { return mobility_filter; }

        /// 进入 pass：声明阴影身份并清掉可能的旧原点
        void BeginShadow(bool shadow_pass)
        {
            is_shadow_pass = shadow_pass;
            has_shadow_origin = false;
        }

        /// 退出 pass：阴影上下文回到"非阴影、无原点"
        void ClearShadow()
        {
            is_shadow_pass = false;
            has_shadow_origin = false;
        }

        void SetShadowOrigin(const glm::vec3 &value)
        {
            shadow_origin = value;
            has_shadow_origin = true;
        }

        bool IsShadowPass() const { return is_shadow_pass; }
        bool HasShadowOrigin() const { return has_shadow_origin; }
        const glm::vec3 &ShadowOrigin() const { return shadow_origin; }
    };
}