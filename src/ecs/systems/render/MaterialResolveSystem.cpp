#include<hgl/ecs/systems/render/MaterialResolveSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<cstdio>

namespace hgl::ecs
{
    namespace
    {
        constexpr const char *kTrackedWireMaterialId = "bounds_wire";

        static bool IsTrackedWireRecord(const hgl::graph::mtl::MaterialAssetRecord *rec)
        {
            return rec
                && !rec->id.empty()
                && rec->id == kTrackedWireMaterialId;
        }
    }

    MaterialResolveSystem::MaterialResolveSystem(const std::string &name)
        : System(name)
    {
        SetSystemType(SystemType::Material);
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

            auto &slot = comp->GetMaterialSlot();
            const bool tracked_wire = IsTrackedWireRecord(slot.record);

            if (!slot.record)
            {
                if (tracked_wire)
                {
                    std::fprintf(stderr,
                        "[WireTrace] MaterialResolveSystem skip: slot.record is null\n");
                }
                continue;
            }

            // Get geometry: prefer unresolved_geometry, fall back to existing Primitive
            graph::Geometry *geom = comp->GetUnresolvedGeometry();
            if (!geom && comp->GetPrimitive())
                geom = comp->GetPrimitive()->GetGeometry();

            if (!geom)
            {
                if (tracked_wire)
                {
                    auto *owner = comp->GetOwner();
                    std::fprintf(stderr,
                        "[WireTrace] MaterialResolveSystem skip: owner='%s' rec.id=%s no geometry (unresolved/primitve both null)\n",
                        owner ? owner->GetName().c_str() : "<null>",
                        slot.record->id.c_str());
                }
                continue;
            }

            const auto &gvf = geom->GetGeometryVertexFormat();

            if (tracked_wire)
            {
                auto *owner = comp->GetOwner();
                std::fprintf(stderr,
                    "[WireTrace] MaterialResolveSystem resolving owner='%s' rec.id=%s gvf_active=%u mi_bytes=%u unresolved_geom=%d has_primitive=%d\n",
                    owner ? owner->GetName().c_str() : "<null>",
                    slot.record->id.c_str(),
                    gvf.GetActiveCount(),
                    slot.GetInstanceDataSize(),
                    comp->GetUnresolvedGeometry() ? 1 : 0,
                    comp->GetPrimitive() ? 1 : 0);
            }

            auto *mi = registry->AcquireMI(*slot.record, gvf,
                                           slot.GetInstanceDataPtr(),
                                           slot.GetInstanceDataSize());
            if (!mi)
            {
                if (tracked_wire)
                {
                    auto *owner = comp->GetOwner();
                    std::fprintf(stderr,
                        "[WireTrace] MaterialResolveSystem AcquireMI failed: owner='%s' rec.id=%s\n",
                        owner ? owner->GetName().c_str() : "<null>",
                        slot.record->id.c_str());
                }
                continue;
            }

            slot.resolved_mi = mi;
            slot.dirty = false;

            if (tracked_wire)
            {
                auto *owner = comp->GetOwner();
                std::fprintf(stderr,
                    "[WireTrace] MaterialResolveSystem AcquireMI ok: owner='%s' rec.id=%s material='%s'\n",
                    owner ? owner->GetName().c_str() : "<null>",
                    slot.record->id.c_str(),
                    (mi->GetMaterial() ? mi->GetMaterial()->GetName().c_str() : "<null>"));
            }

            // If we have unresolved geometry (no Primitive yet), create the full Primitive
            if (comp->GetUnresolvedGeometry())
            {
                auto *prim = prim_mgr->CreatePrimitive(geom, mi);
                if (prim)
                {
                    comp->SetPrimitive(prim);
                    comp->SetUnresolvedGeometry(nullptr);

                    if (tracked_wire)
                    {
                        auto *owner = comp->GetOwner();
                        std::fprintf(stderr,
                            "[WireTrace] MaterialResolveSystem CreatePrimitive ok: owner='%s' rec.id=%s prim=%p\n",
                            owner ? owner->GetName().c_str() : "<null>",
                            slot.record->id.c_str(),
                            static_cast<void *>(prim));
                    }
                }
                else if (tracked_wire)
                {
                    auto *owner = comp->GetOwner();
                    std::fprintf(stderr,
                        "[WireTrace] MaterialResolveSystem CreatePrimitive failed: owner='%s' rec.id=%s\n",
                        owner ? owner->GetName().c_str() : "<null>",
                        slot.record->id.c_str());
                }
            }
        }
    }
}//namespace hgl::ecs
