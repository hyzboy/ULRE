// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/WorkManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/module/TextureManager.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

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

private:

    bool InitMaterial()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        mtl::Material2DCreateConfig cfg(PrimitiveType::SolidRectangles,
                                        CoordinateSystem2D::ZeroToOne,
                                        mtl::WithLocalToWorld::Without);

        material=render_context->LoadMaterial("Std2D/RectTexture2D",&cfg);

        if(!material)
            return(false);

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, InlinePipeline::Solid2D) : nullptr;

        if(!pipeline)
            return(false);

        TextureManager *tex_manager = render_context->GetTextureManager();

        texture=tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);

        if(!texture)return(false);

        sampler=render_context->CreateSampler();

        if(!material->BindTextureSampler( DescriptorSetType::PerMaterial,
                                        mtl::SamplerName::BaseColor,
                                        texture,
                                        sampler))
            return(false);

        material_instance=render_context->CreateMaterialInstance(material);

        return(true);
    }

    bool InitVBO()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        prim_rect=render_context->CreatePrimitive("TextureRect",1,material_instance,pipeline,
                                    {
                                        {VAN::Position,VF_V4F,position_data},
                                        {VAN::TexCoord,VF_V4F,tex_coord_data}
                                    });

        if(!prim_rect)
            return(false);

        return(true);
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        rect_entity = ecs_world->CreateEntity<Entity>("TextureRect");
        auto rect_transform = rect_entity->AddComponent<TransformComponent>();
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

    using WorkObject::WorkObject;

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

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Draw a rectangle with texture"),256,256);
}

