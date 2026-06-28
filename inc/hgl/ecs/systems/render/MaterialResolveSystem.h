#pragma once

/// MaterialResolveSystem — 延迟 MI 解析 ECS System
///
/// 在 RenderMaterialBind Phase 执行，遍历所有 dirty MaterialResolveRequest 的
/// PrimitiveComponent，根据 record + Geometry GVF 自动解析 MI；
/// 若 PrimitiveComponent 仅持有 unresolved_geometry，则同时创建 Primitive。

#include<hgl/ecs/core/System.h>
#include<hgl/ecs/core/MaterialResolveDiagnostics.h>
#include<hgl/graph/module/MaterialResolveTieredCache.h>

#include <cstdint>
#include <deque>
#include <unordered_map>

namespace hgl::ecs
{
    class ECSContext;

    class MaterialResolveSystem : public System
    {
    private:

        ECSContext *world = nullptr;

        // Phase R2.0: shadow ownership for payload/binding objects used only by tiered-cache probing.
        uint64_t next_shadow_payload_id = 1;
        uint64_t next_shadow_binding_id = 1;
        std::deque<graph::MaterialInstancePayload> shadow_payload_storage;
        std::deque<graph::ProgramInstanceBinding> shadow_binding_storage;
        std::unordered_map<graph::PayloadCacheKey,
                   graph::MaterialInstancePayload *,
                   graph::PayloadCacheKeyHash> shadow_payload_index;
        std::unordered_map<graph::BindingCacheKey,
                   graph::ProgramInstanceBinding *,
                   graph::BindingCacheKeyHash> shadow_binding_index;

        MaterialResolveDiagnostics *GetDiagnostics();

    public:

        MaterialResolveSystem(const std::string &name = "MaterialResolveSystem");
        ~MaterialResolveSystem() override = default;

        void SetWorld(ECSContext *w) { world = w; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
