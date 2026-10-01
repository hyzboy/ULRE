#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/components/ShadowComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<glm/glm.hpp>
#include<memory>
#include <hgl/type/UnorderedMap.h>
#include<utility>
#include<vector>

namespace hgl::ecs
{

    /**
    * Base renderable component interface
    * Derived classes should implement specific rendering needs
    *
    * A5a：可见性真值收敛到**实体级**（`ECSContext::IsEntityVisible`）——本类不再持
    * 字段（组件级那第二份真值已删）；包围球半径随几何派生状态迁入 `GeometryData`。
    */
    class RenderableComponent : public Component
    {
    public:

        explicit RenderableComponent(const std::string& name = "Renderable")
            : Component(name)
        {
        }

        virtual ~RenderableComponent() = default;

        // ── 阴影关联与缺省约定 ──
        // 每次都按需经 owner 查 ShadowComponent，**不缓存指针**：缓存副本会与实体上
        // 组件的实际挂卸脱节，就变成第二份真值（原组件内的阴影指针缓存已删）。

        ShadowComponent *GetShadowComponent() const
        {
            if (auto *e = GetOwner())
            {
                if (auto shadow = e->GetComponent<ShadowComponent>())
                    return shadow.get();
            }
            return nullptr;
        }

        /// 缺省约定：未挂载 ShadowComponent 默认投射阴影；挂载则遵循组件配置
        bool CanCastShadow() const
        {
            auto *sc = GetShadowComponent();
            return sc ? sc->CanCastShadow() : true;
        }

        /// 缺省约定：未挂载 ShadowComponent 默认不限制投射距离（0.0f）
        float GetShadowMaxDistance() const
        {
            auto *sc = GetShadowComponent();
            return sc ? sc->GetMaxDistance() : 0.0f;
        }

        /// 缺省约定：未挂载 ShadowComponent 默认接收阴影
        bool CanReceiveShadow() const
        {
            auto *sc = GetShadowComponent();
            return sc ? sc->CanReceiveShadow() : true;
        }

        float GetShadowBiasMultiplier() const
        {
            auto *sc = GetShadowComponent();
            return sc ? sc->GetBiasMultiplier() : 1.0f;
        }
    };
}//namespace hgl::ecs
