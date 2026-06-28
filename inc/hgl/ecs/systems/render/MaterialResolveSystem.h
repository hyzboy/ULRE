#pragma once

/// MaterialResolveSystem — 延迟 MI 解析 ECS System
///
/// 在 RenderMaterialBind Phase 执行，遍历所有 dirty MaterialResolveRequest 的
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
        bool decoupled_cache_enabled = false;   // Phase R1.2 placeholder, default off.
        uint64_t last_cache_stats_log_ms = 0;
        uint64_t cache_stats_log_interval_ms = 1000;

    public:

        MaterialResolveSystem(const std::string &name = "MaterialResolveSystem");
        ~MaterialResolveSystem() override = default;

        void SetWorld(ECSContext *w) { world = w; }
        void SetDecoupledCacheEnabled(bool enabled) { decoupled_cache_enabled = enabled; }
        bool IsDecoupledCacheEnabled() const { return decoupled_cache_enabled; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
