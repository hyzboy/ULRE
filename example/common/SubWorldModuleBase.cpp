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
        delete primitive;
        delete geometry;
    }

    bool SubWorldModuleBase::InitMaterialAndPipeline(graph::mtl::Material3DCreateConfig& cfg,
                                                     graph::mtl::MaterialPreset preset,
                                                     InlinePipeline inline_pipeline_type)
    {
        if (!render_context || !graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        material = material_manager->CreateMaterial(preset, &cfg);
        if (!material)
            return false;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderFormat() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, inline_pipeline_type) : nullptr;
        return pipeline != nullptr;
    }

    bool SubWorldModuleBase::BuildMaterialInstances(const Color4f* colors, size_t count)
    {
        if (!render_context || !graphics_context || !material || !colors || count == 0)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        material_instances.clear();
        material_instances.reserve(count);

        for (size_t i = 0; i < count; ++i)
        {
            auto* mi = material_manager->CreateMaterialInstance(material, (VIL*)nullptr, &colors[i]);
            if (!mi)
                return false;

            material_instances.push_back(mi);
        }

        return true;
    }

    SubWorldModuleBase::MeshResource*
    SubWorldModuleBase::CreatePrimitiveMesh(Geometry* geometry, MaterialInstance* mi)
    {
        if (!render_context || !graphics_context || !geometry || !mi || !pipeline)
            return nullptr;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!geometry_manager || !primitive_manager)
            return nullptr;

        geometry_manager->Add(geometry);

        Primitive* primitive = primitive_manager->CreatePrimitive(geometry, mi, pipeline);
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
