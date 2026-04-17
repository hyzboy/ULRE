#include<hgl/ecs/systems/render/MaterialResolveSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/mesh/Primitive.h>

namespace hgl::ecs
{
    MaterialResolveSystem::MaterialResolveSystem(const std::string &name)
        : System(name)
    {
        SetSystemType(SystemType::ShaderMaterialProgram);
        SetExecutionOrder(ExecutionPhase::RenderMaterialBind);
        SetRenderElementType("Primitive");
    }

    void MaterialResolveSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        auto *gc = world->GetGraphicsContext();
        if (!gc)
            return;

        auto *registry = gc->GetMaterialAssetRegistry();
        auto *prim_mgr = gc->GetPrimitiveManager();
        if (!registry || !prim_mgr)
            return;

        std::vector<std::shared_ptr<PrimitiveComponent>> primitives;
        world->GetComponents<PrimitiveComponent>(primitives);

        for (auto &comp : primitives)
        {
            if (!comp || !comp->NeedsMaterialResolve())
                continue;

            auto &slot = comp->GetMaterialResolveRequest();
            if (!slot.record)
                continue;

            // Get geometry: prefer unresolved_geometry, fall back to existing Primitive
            graph::Geometry *geom = comp->GetUnresolvedGeometry();
            if (!geom && comp->GetPrimitive())
                geom = comp->GetPrimitive()->GetGeometry();

            if (!geom)
                continue;

            const auto &gvf = geom->GetGeometryVertexFormat();

            auto *mi = registry->AcquireMI(*slot.record, gvf,
                                           slot.GetInstanceDataPtr(),
                                           slot.GetInstanceDataSize());
            if (!mi)
                continue;

            slot.resolved_mi = mi;
            slot.dirty = false;

            // If we have unresolved geometry (no Primitive yet), create the full Primitive
            if (comp->GetUnresolvedGeometry())
            {
                auto *prim = prim_mgr->CreatePrimitive(geom, mi);
                if (prim)
                {
                    comp->SetPrimitive(prim);
                    comp->SetUnresolvedGeometry(nullptr);
                }
            }
        }
    }
}//namespace hgl::ecs
