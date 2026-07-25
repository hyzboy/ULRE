// 画一个带纹理的四边形 (ECS)
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
    GeometryVertexFormat CreatePureTexture2DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V2F, 2, sizeof(float) * 2);
        gvf.Add(VertexSemantic::TexCoord, VF_V2F, 2, sizeof(float) * 2);
        return gvf;
    }
}

constexpr uint32_t VERTEX_COUNT=4;

constexpr float position_data[VERTEX_COUNT][2]=
{
    {-1, -1},
    { 1, -1},
    { 1,  1},
    {-1,  1},
};

constexpr float tex_coord_data[VERTEX_COUNT][2]=
{
    {0,0},
    {1,0},
    {1,1},
    {0,1}
};

class TestApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;
    Entity *            quad_entity         = nullptr;

    Texture2D *         texture             = nullptr;
    Sampler *           sampler             = nullptr;
    Material *          material            = nullptr;
    MaterialInstance *  material_instance   = nullptr;
    Primitive *         prim_quad           = nullptr;
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
        if (!material_manager || !sampler_manager || !tex_manager)
            return false;

        mtl::Material2DCreateConfig cfg(PrimitiveType::Fan,
                                        CoordinateSystem2D::NDC,
                                        mtl::WithLocalToWorld::Without);

        material=material_manager->AcquireMaterialProgram(mtl::MaterialPreset::PureTexture2D,&cfg);

        if(!material)
            return(false);

        auto* device = graphics_context->GetDevice();
        if (!device)
            return false;

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

        GeometryCreater pc(device, CreatePureTexture2DGeometryVertexFormat(), buffer_manager);
        pc.Init("TextureQuad", VERTEX_COUNT);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        prim_quad = primitive_manager->CreatePrimitive(geometry, material_instance, nullptr);

        if(!prim_quad)
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

        quad_entity = ecs_world->CreateEntity<Entity>("TextureQuad");
        auto quad_transform = quad_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto quad_primitive = quad_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        quad_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        quad_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        quad_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        quad_transform->SetMovable(false);

        quad_primitive->SetPrimitive(prim_quad);
        quad_primitive->RequestPipeline(InlinePipeline::Solid2D);
        quad_primitive->SetVisible(true);

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
    return RunFramework<TestApp>(OS_TEXT("Draw a quad with texture"),argc,argv,256,256);
}

