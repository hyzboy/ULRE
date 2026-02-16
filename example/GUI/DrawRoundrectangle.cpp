// 画一个圆角矩形，它是UI的基本绘图元件
// 通过控制尺寸、每个角的半径，可绘制出正圆、矩形、圆角矩形

#include<hgl/WorkManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/math/Math.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;

constexpr float position_data[4]=
{
    0,     //left
    0,     //top
    1,     //right
    1      //bottom
};

constexpr float tex_coord_data[4]=
{
    0,0,
    1,1
};

struct RoundedRectConfig
{

};

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_world = nullptr;
    hgl::ecs::Entity *rect_entity = nullptr;

    Texture2D *         texture             =nullptr;
    Sampler *           sampler             =nullptr;
    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Pipeline *          pipeline            =nullptr;

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
        auto* texture_manager = graphics_context->GetTextureManager();
        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!material_manager || !texture_manager || !sampler_manager)
            return false;

        mtl::Material2DCreateConfig cfg(PrimitiveType::SolidRectangles,
                                        CoordinateSystem2D::ZeroToOne,
                                        mtl::WithLocalToWorld::Without);

        material=material_manager->LoadMaterial("Std2D/RectTexture2D",&cfg);

        if(!material)
            return(false);

        //        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, InlinePipeline::Solid2D) : nullptr;

        if(!pipeline)
            return(false);

        texture=texture_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);

        if(!texture)return(false);

        sampler=sampler_manager->CreateSampler();

        if(!material->BindImageSampler( DescriptorSetType::PerMaterial,     ///<描述符合集
           mtl::SamplerName::BaseColor,        ///<采样器名称
           texture,                            ///<纹理
           sampler))                           ///<采样器
            return(false);

        material_instance=material_manager->CreateMaterialInstance(material);

        return(true);
    }

    bool InitVBO()
    {
        if(!ecs_world)
        {
            ecs_world = GetECSContext();
            if(!ecs_world)
                return false;
        }

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

        GeometryCreater pc(device, material_instance->GetVIL(), buffer_manager);
        pc.Init("TextureRect", 1);
        if (!pc.WriteVAB(VAN::Position, VF_V4F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V4F, tex_coord_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        Primitive *primitive = primitive_manager->CreatePrimitive(geometry, material_instance, pipeline);

        if(!primitive)
            return false;

        rect_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("Rect2D");
        if(!rect_entity)
            return false;

        auto transform = rect_entity->AddComponent<hgl::ecs::TransformComponent>();
        transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto prim_comp = rect_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        if(!prim_comp)
            return false;

        prim_comp->SetPrimitive(primitive);
        prim_comp->SetVisible(true);

        return true;
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Draw a rectangle with texture"),256,256);
}

