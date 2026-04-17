#pragma once

/// MaterialResolveSystem — 延迟 MI 解析 ECS System
///
/// 在 RenderMaterialBind Phase 执行，遍历所有 dirty MaterialSlot 的
/// PrimitiveComponent，根据 record + Geometry GVF 自动解析 MI；
/// 若 PrimitiveComponent 仅持有 unresolved_geometry，则同时创建 Primitive。

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class ECSContext;

    class MaterialResolveSystem : public System
    {
    private:

        ECSContext *world = nullptr;

    public:

        MaterialResolveSystem(const std::string &name = "MaterialResolveSystem");
        ~MaterialResolveSystem() override = default;

        void SetWorld(ECSContext *w) { world = w; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
