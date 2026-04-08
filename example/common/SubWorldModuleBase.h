#pragma once

#include"ISubWorldModule.h"

#include<hgl/ecs/core/Entity.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialAssetRecord.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/PrimitiveMaterialSlot.h>
#include<hgl/color/Color4f.h>
#include<hgl/vk/VK.h>

#include<memory>
#include<vector>

namespace hgl::graph
{
    class RenderContext;
    class GraphicsContext;
    class Geometry;
    class Primitive;
    class MaterialTemplate;
    class MaterialResourceDomain;
}

namespace example::modules
{
    class SubWorldModuleBase : public ISubWorldModule
    {
    protected:
        struct MeshResource
        {
            hgl::graph::Geometry* geometry = nullptr;
            hgl::graph::Primitive* primitive = nullptr;

            ~MeshResource();
        };

        hgl::graph::RenderContext* render_context = nullptr;
        hgl::graph::GraphicsContext* graphics_context = nullptr;
        hgl::ecs::ECSContext* root_context = nullptr;
        hgl::ecs::Entity* anchor_entity = nullptr;
        std::shared_ptr<hgl::ecs::SubWorldComponent> subworld_component;

        hgl::graph::MaterialTemplate* material = nullptr;
        hgl::graph::MaterialResourceDomain* material_domain = nullptr;

        std::vector<hgl::graph::PrimitiveMaterialSlot> material_slots;
        std::vector<std::unique_ptr<MeshResource>> mesh_resources;

    protected:
        virtual bool OnInitializeSharedResources() = 0;
        virtual bool OnInstallLocalSystems(hgl::ecs::ECSContext* sub_context) = 0;
        virtual bool OnBuildLocalScene(hgl::ecs::ECSContext* sub_context) = 0;

        bool InitMaterialAndPipeline(hgl::graph::mtl::Material3DCreateConfig& cfg,
                         hgl::graph::mtl::MaterialPreset preset,
                         hgl::graph::GraphicsPipelinePreset inline_pipeline_type);

        bool BuildMaterialInstances(const hgl::Color4f* colors, size_t count);

        hgl::graph::PrimitiveMaterialSlot AcquireSlot(const hgl::graph::mtl::MaterialAssetRecord &rec,
                       const void *instance_data = nullptr,
                       uint32_t instance_data_size = 0,
                       hgl::graph::MaterialDomainHandle *out_handle = nullptr);

        MeshResource* CreatePrimitiveMesh(hgl::graph::Geometry* geometry,
                          const hgl::graph::PrimitiveMaterialSlot &slot);

    public:
        bool Mount(hgl::graph::RenderContext* in_render_context,
                   hgl::ecs::ECSContext* in_root_context,
                   const std::string& anchor_name,
                   hgl::ecs::SubWorldMode mode) override;
    };
}
