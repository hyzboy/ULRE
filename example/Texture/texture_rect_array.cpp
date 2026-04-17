// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/ShaderMaterialProgramManager.h>
#include<hgl/graph/module/SamplerManager.h>

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

constexpr const float rect_right=1.0f/float(TexCount);

constexpr const float position_data[12]=
{
    0,0,
    0,1,
    rect_right,0,
    rect_right,0,
    0,1,
    rect_right,1
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

    Texture2DArray *    texture             = nullptr;
    Sampler *           sampler             = nullptr;
    ShaderMaterialProgram *          material            = nullptr;

    Primitive *         mesh_rect           = nullptr;

    struct
    {
        Entity *            entity;
        MaterialInstance *  mi;
    }render_obj[TexCount]{};

private:

    bool InitTexture()
    {

        auto* tex_manager = GetTextureManager();
        if (!tex_manager)
            return false;

        texture = tex_manager->CreateTexture2DArray("freepik icons",
                                512,512,            ///<纹理尺寸
                                TexCount,           ///<纹理层数
                                PF_BC7UN,           ///<纹理格式
                                false);             ///<是否自动产生mipmaps

        if(!texture)return(false);

        OSString filename;

        for(uint i=0;i<TexCount;i++)
        {
            filename=filesystem::JoinPathWithFilename(OS_TEXT("res/image/icon/freepik"),tex_filename[i]);

            if(!tex_manager->LoadTexture2DArray(texture,i,filename))
                return(false);
        }

        return(true);
    }

    bool InitMaterial()
    {

        auto* sampler_manager = GetSamplerManager();
        if (!sampler_manager)
            return false;

        static const mtl::MaterialRecipe kTexArrayCfg {
            .id              = "texture_rect_array",
            .preset          = mtl::MaterialPreset::PureTexture2D,
            .dim             = mtl::MaterialRecipe::Dim::D2,
            .coord_2d  = CoordinateSystem2D::ZeroToOne,
            .pipeline  = GraphicsPipelinePreset::Solid2D,
            .textures  = {
                {mtl::SamplerSlot::BaseColor, mtl::TextureSourceMode::Array, ""},
            },
        };
        render_obj[0].mi = AcquireMI(kTexArrayCfg);
        if(!render_obj[0].mi)
            return(false);

        material = render_obj[0].mi->GetMaterial();

        sampler=sampler_manager->CreateSampler();

        if(!material->BindTextureSampler( mtl::SamplerSlot::BaseColor,
                          texture,
                          sampler))
            return(false);

        for(uint32_t i=0;i<TexCount;i++)
        {
            if(i > 0)
                render_obj[i].mi = AcquireMI(kTexArrayCfg);

            if(!render_obj[i].mi)
                return(false);

            // Array 模式：使用 SetTextureArrayLayer 设置纹理层索引
            render_obj[i].mi->SetTextureArrayLayer(mtl::SamplerSlot::BaseColor, i);
        }

        return(true);
    }

    bool InitVBOAndRenderList()
    {

        auto* device = GetDevice();
        auto* buffer_manager = GetBufferManager();
        auto* geometry_manager = GetGeometryManager();
        auto* primitive_manager = GetPrimitiveManager();
        if (!device || !buffer_manager || !geometry_manager || !primitive_manager)
            return false;

        GeometryCreater pc(device, GeometryVertexFormat::FromVIL(render_obj[0].mi->GetVIL()), buffer_manager);
        pc.Init("TextureRect", 6);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        mesh_rect = primitive_manager->CreatePrimitive(geometry, render_obj[0].mi);

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
            offset.x=rect_right*2*float(i);

            render_obj[i].entity = ecs_world->CreateEntity<Entity>("TextureRect");
            auto transform = render_obj[i].entity->AddComponent<TransformComponent>(Mobility::Static);
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
    TestApp() = default;
    explicit TestApp(std::shared_ptr<ecs::ECSContext> ctx) : WorkObject(std::move(ctx)) {}
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

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Draw many rectangle with texture"),argc,argv,256*TexCount,256);
}


