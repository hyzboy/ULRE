#pragma once

#include"ISubWorldModule.h"

#include<hgl/ecs/core/Entity.h>
#include<hgl/mtl/Material3DCreateConfig.h>
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
    class Material;
    class GraphicsPipeline;
    class MaterialInstance;
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

        hgl::graph::Material* material = nullptr;
        hgl::graph::GraphicsPipeline* pipeline = nullptr;
        std::vector<hgl::graph::MaterialInstance*> material_instances;
        std::vector<std::unique_ptr<MeshResource>> mesh_resources;

    protected:
        virtual bool OnInitializeSharedResources() = 0;
        virtual bool OnInstallLocalSystems(hgl::ecs::ECSContext* sub_context) = 0;
        virtual bool OnBuildLocalScene(hgl::ecs::ECSContext* sub_context) = 0;

        bool InitMaterialAndPipeline(hgl::graph::mtl::Material3DCreateConfig& cfg,
                         hgl::graph::mtl::MaterialPreset preset,
                         hgl::graph::InlinePipeline inline_pipeline_type);

        bool BuildMaterialInstances(const hgl::Color4f* colors, size_t count);

        MeshResource* CreatePrimitiveMesh(hgl::graph::Geometry* geometry,
                                          hgl::graph::MaterialInstance* mi);

    public:
        bool Mount(hgl::graph::RenderContext* in_render_context,
                   hgl::ecs::ECSContext* in_root_context,
                   const std::string& anchor_name,
                   hgl::ecs::SubWorldMode mode) override;
    };
}
