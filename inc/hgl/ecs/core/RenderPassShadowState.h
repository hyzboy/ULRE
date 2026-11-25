#pragma once

#include<glm/glm.hpp>

namespace hgl::ecs
{
    /**
     * 当前渲染 Pass 的阴影上下文（世界私有）。
     *
     * 语义：`Begin()` 声明本 pass 是否产出阴影贴图，并**无条件**清掉原点；原点只能经
     * `SetOrigin()` 在已知参考相机后设置 ⇒ `HasOrigin()` 为假时 `Origin()` 无意义，
     * 消费方（阴影相关系统）必须先用 `HasOrigin()` 判定。
     */
    class RenderPassShadowState
    {
    private:
        bool is_shadow_pass = false;
        bool has_origin = false;
        glm::vec3 origin{0.0f};

    public:
        void Begin(bool shadow_pass)
        {
            is_shadow_pass = shadow_pass;
            has_origin = false;
        }

        void End()
        {
            is_shadow_pass = false;
            has_origin = false;
        }

        void SetOrigin(const glm::vec3 &value)
        {
            origin = value;
            has_origin = true;
        }

        bool IsShadowPass() const { return is_shadow_pass; }
        bool HasOrigin() const { return has_origin; }
        const glm::vec3 &Origin() const { return origin; }
    };
}