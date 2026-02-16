// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/WorkManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/filesystem/Filename.h>
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

constexpr const os_char *tex_filename[]=
{
    OS_TEXT("001-online resume.Tex2D"),
    OS_TEXT("002-salary.Tex2D"),
    OS_TEXT("003-application.Tex2D"),
    OS_TEXT("004-job interview.Tex2D")
};

constexpr const size_t TexCount=sizeof(tex_filename)/sizeof(os_char *);

constexpr const float position_data[4]=
{
    0,      //left
    0,      //top
    1.0f/float(TexCount),      //right
    1       //bottom
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

    Texture2DArray *    texture             = nullptr;
    Sampler *           sampler             = nullptr;
    Material *          material            = nullptr;

    Pipeline *          pipeline            = nullptr;
    Primitive *         mesh_rect           = nullptr;

    struct
    {
        Entity *            entity;
        MaterialInstance *  mi;
    }render_obj[TexCount]{};

private:

    bool InitTexture()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        texture = graphics_context->CreateTexture2DArray("freepik icons",
                                                       512,512,            ///<纹理尺寸
                                                       TexCount,           ///<纹理层数
                                                       PF_BC7UN,           ///<纹理格式
                                                       false);             ///<是否自动产生mipmaps

        if(!texture)return(false);

        OSString filename;

        for(uint i=0;i<TexCount;i++)
        {
            filename=filesystem::JoinPathWithFilename(OS_TEXT("res/image/icon/freepik"),tex_filename[i]);

            if(!graphics_context->LoadTexture2DArray(texture,i,filename))
                return(false);
        }

        return(true);
    }

    bool InitMaterial()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        mtl::Material2DCreateConfig cfg(PrimitiveType::SolidRectangles,
                                        CoordinateSystem2D::ZeroToOne,
                                        mtl::WithLocalToWorld::With);

        material=graphics_context->LoadMaterial("Std2D/RectTexture2DArray",&cfg);

        if(!material)
            return(false);

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, InlinePipeline::Solid2D) : nullptr;

        if(!pipeline)
            return(false);

        sampler=graphics_context->CreateSampler();

        if(!material->BindTextureSampler( DescriptorSetType::PerMaterial,
                                        mtl::SamplerName::BaseColor,
                                        texture,
                                        sampler))
            return(false);

        for(uint32_t i=0;i<TexCount;i++)
        {
            render_obj[i].mi=graphics_context->CreateMaterialInstance(material);

            if(!render_obj[i].mi)
                return(false);

            render_obj[i].mi->WriteMIData(i);
        }

        return(true);
    }

    bool InitVBOAndRenderList()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        mesh_rect=graphics_context->CreatePrimitive( "TextureRect",1,render_obj[0].mi,pipeline,
                                    {
                                        {VAN::Position,VF_V4F,position_data},
                                        {VAN::TexCoord,VF_V4F,tex_coord_data}
                                    });

        if(!mesh_rect)
            return(false);

        return(true);
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        math::Vector3f offset(1.0f/float(TexCount),0,0);

        for(uint32_t i=0;i<TexCount;i++)
        {
            offset.x=position_data[2]*float(i);

            render_obj[i].entity = ecs_world->CreateEntity<Entity>("TextureRect");
            auto transform = render_obj[i].entity->AddComponent<TransformComponent>();
            auto primitive = render_obj[i].entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(offset.x, offset.y, offset.z));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            primitive->SetPrimitive(mesh_rect);
            primitive->SetOverrideMaterial(render_obj[i].mi);
            primitive->SetVisible(true);
        }

        return true;
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        if(!InitTexture())
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitVBOAndRenderList())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Draw many rectangle with texture"),256*TexCount,256);
}

