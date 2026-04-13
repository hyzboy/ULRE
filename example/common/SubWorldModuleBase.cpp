#include"SubWorldModuleBase.h"

#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

namespace example::modules
{
    SubWorldModuleBase::MeshResource::~MeshResource()
    {
    }

    bool SubWorldModuleBase::InitMaterialAndPipeline(graph::mtl::Material3DCreateConfig& cfg,
                                                     graph::mtl::MaterialPreset preset,
                                                     GraphicsPipelinePreset inline_pipeline_type)
    {
        if (!render_context || !graphics_context)
            return false;

        auto *material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        material = material_manager->AcquireMaterialInternal(preset, &cfg);
        if (!material)
            return false;

        material_domain = material_manager->GetOrCreateDefaultDomain(material);
        if (!material_domain)
            return false;

        return true;
    }

    bool SubWorldModuleBase::BuildMaterialInstances(const Color4f* colors, size_t count)
    {
        if (!render_context || !graphics_context || !material || !colors || count == 0)
            return false;

        auto *material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        material_slots.clear();
        material_slots.reserve(count);

        if (!material_domain)
            material_domain = material_manager->GetOrCreateDefaultDomain(material);
        if (!material_domain)
            return false;

        for (size_t i = 0; i < count; ++i)
        {
            auto slot = material_manager->AllocMaterialInstanceSlot(
                material_domain,
                &colors[i],
                sizeof(colors[i]));

            if (!slot.domain)
                return false;

            slot.material_template        = material;
            slot.vil                      = material->GetDefaultVIL();
            slot.preset                   = GraphicsPipelinePreset::Solid3D;
            slot.texture_array_slot_flags = material->GetTextureArraySlotFlags();

            if (!slot.IsValid())
                return false;

            material_slots.push_back(slot);
        }

        return true;
    }

    graph::PrimitiveMaterialSlot SubWorldModuleBase::AcquireSlot(const graph::mtl::MaterialAssetRecord &rec,
                                                                 const void *instance_data,
                                                                 uint32_t instance_data_size,
                                                                 graph::MaterialDomainHandle *out_handle)
    {
        if (!graphics_context)
            return {};

        auto *registry = graphics_context->GetMaterialAssetRegistry();
        auto *material_manager = graphics_context->GetMaterialManager();
        if (!registry || !material_manager)
            return {};

        auto handle = registry->Acquire(rec);
        if (!handle.IsValid() || !handle.material)
            return {};

        const VIL *resolved_vil = registry->ResolveVIL(handle.material, rec);
        if (!resolved_vil)
            return {};

        if (out_handle)
            *out_handle = handle;

        PrimitiveMaterialSlot slot = material_manager->AllocMaterialInstanceSlot(
            handle.domain,
            instance_data,
            instance_data_size);

        if (!slot.domain)
            return {};

        slot.material_template        = handle.material;
        slot.vil                      = resolved_vil;
        slot.preset                   = rec.pipeline;
        slot.texture_array_slot_flags = handle.material->GetTextureArraySlotFlags();

        return slot;
    }

    SubWorldModuleBase::MeshResource*
    SubWorldModuleBase::CreatePrimitiveMesh(Geometry* geometry, const PrimitiveMaterialSlot &slot)
    {
        if (!render_context || !graphics_context || !geometry || !slot.IsValid())
            return nullptr;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!geometry_manager || !primitive_manager)
            return nullptr;

        geometry_manager->Add(geometry);

        Primitive* primitive = primitive_manager->CreatePrimitive(geometry, slot);
        if (!primitive)
            return nullptr;

        auto res = std::make_unique<MeshResource>();
        res->geometry = geometry;
        res->primitive = primitive;

        MeshResource* ptr = res.get();
        mesh_resources.push_back(std::move(res));
        return ptr;
    }

    bool SubWorldModuleBase::Mount(graph::RenderContext* in_render_context,
                                   ECSContext* in_root_context,
                                   const std::string& anchor_name,
                                   SubWorldMode mode)
    {
        render_context = in_render_context;
        graphics_context = render_context ? render_context->GetGraphicsContext() : nullptr;
        root_context = in_root_context;

        if (!render_context || !graphics_context || !root_context)
            return false;

        if (!OnInitializeSharedResources())
            return false;

        anchor_entity = root_context->CreateEntity<Entity>(anchor_name);
        if (!anchor_entity)
            return false;

        subworld_component = anchor_entity->AddComponent<SubWorldComponent>(mode);
        if (!subworld_component)
            return false;

        ECSContext* sub_context = subworld_component->GetSubContext();
        if (!sub_context)
            return false;

        if (!OnInstallLocalSystems(sub_context))
            return false;

        return OnBuildLocalScene(sub_context);
    }
}
