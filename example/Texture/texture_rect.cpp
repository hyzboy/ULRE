// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/framework/WorkManager.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/SamplerManager.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateRectTexture2DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V2F, 2, sizeof(float) * 2);
        gvf.Add(VertexSemantic::TexCoord, VF_V2F, 2, sizeof(float) * 2);
        return gvf;
    }
}

constexpr float position_data[12]=
{
    0,0,
    0,1,
    1,0,
    1,0,
    0,1,
    1,1
};

constexpr float tex_coord_data[12]=
{
    0,0,
    0,1,
    1,0,
    1,0,
    0,1,
    1,1
};

class TestApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;
    Entity *            rect_entity         = nullptr;

    Texture2D *         texture             = nullptr;
    Sampler *           sampler             = nullptr;
    Material *          material            = nullptr;
    MaterialInstance *  material_instance   = nullptr;
    Pipeline *          pipeline            = nullptr;
    Primitive *         prim_rect           = nullptr;
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;

private:

    bool InitMaterial()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* sampler_manager = graphics_context->GetSamplerManager();
        auto* tex_manager = graphics_context->GetTextureManager();
        auto* device = graphics_context->GetDevice();
        if (!material_manager || !sampler_manager || !tex_manager || !device)
            return false;

        mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,
                                        CoordinateSystem2D::ZeroToOne,
                                        mtl::WithLocalToWorld::Without);

        material=material_manager->CreateMaterial(mtl::MaterialPreset::RectTexture2D,&cfg);

        if(!material)
            return(false);

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, InlinePipeline::Solid2D) : nullptr;

        if(!pipeline)
            return(false);

        if (!bindless_texture_manager)
        {
            bindless_texture_manager = std::make_unique<BindlessTextureManager>();
            if (!bindless_texture_manager->Init(VkDevice(*device)))
                return false;

            render_context->SetBindlessTextureManager(bindless_texture_manager.get());
            material_manager->SetBindlessLayout(bindless_texture_manager->GetLayout());
        }

        texture=tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);

        if(!texture)return(false);

        sampler=sampler_manager->CreateSampler();

        material_instance=material_manager->CreateMaterialInstance(material);

        return(true);
    }

    bool InitVBO()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
        auto* buffer_manager = graphics_context->GetBufferManager();
        auto* geometry_manager = graphics_context->GetGeometryManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!device || !buffer_manager || !geometry_manager || !primitive_manager)
            return false;

        GeometryCreater pc(device, CreateRectTexture2DGeometryVertexFormat(), buffer_manager);
        pc.Init("TextureRect", 6);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        prim_rect = primitive_manager->CreatePrimitive(geometry, material_instance, pipeline);

        if(!prim_rect)
            return(false);

        return(true);
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        auto rdbs = ecs_world->GetSystem<RenderDescriptorBindingSystem>();
        if (!rdbs)
            return false;

        auto* render_context = GetRenderContext();
        auto* bindless_mgr = render_context ? render_context->GetBindlessTextureManager() : nullptr;
        if (!bindless_mgr)
            return false;

        if (rdbs->RegisterTexture2DResource("", texture, sampler, bindless_mgr) == 0)
            return false;
        if (!rdbs->RegisterMaterialTextureSampler(material, mtl::SamplerName::BaseColor, texture, sampler))
            return false;

        rect_entity = ecs_world->CreateEntity<Entity>("TextureRect");
        auto rect_transform = rect_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto rect_primitive = rect_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        rect_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        rect_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        rect_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        rect_transform->SetMovable(false);

        rect_primitive->SetPrimitive(prim_rect);
        rect_primitive->SetVisible(true);

        return true;
    }

public:
    bool Init() override
    {
        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Draw a rectangle with texture"),argc,argv,256,256);
}
