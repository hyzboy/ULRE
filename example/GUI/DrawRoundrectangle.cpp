// 画一个圆角矩形，它是UI的基本绘图元件
// 通过控制尺寸、每个角的半径，可绘制出正圆、矩形、圆角矩形

#include<hgl/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/math/Math.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    GeometryVertexFormat CreateRectTexture2DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V2F},
            {VertexSemantic::TexCoord, VF_V2F},
        };
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
    Geometry *          geometry            =nullptr;
    graph::mtl::MaterialRecipe rect_recipe{};
    PrimitiveAsset             rect_asset{};
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;

private:

    bool CreateRenderObject()
    {
        if(!ecs_world)
        {
            ecs_world = GetECSContext();
            if(!ecs_world)
                return false;
        }

        auto* device = GetDevice();
        auto* buffer_manager = GetManager<BufferManager>();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryCreater pc(device, CreateRectTexture2DGeometryVertexFormat(), buffer_manager);
        pc.Init("TextureRect", 6);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data))
            return false;

        geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        return true;
    }

    bool InitMaterial()
    {
        if(!geometry)
            return false;

        auto* texture_manager = GetManager<TextureManager>();
        auto* sampler_manager = GetManager<SamplerManager>();
        if (!texture_manager || !sampler_manager)
            return false;

        texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"), true);
        if(!texture)
            return false;

        sampler = sampler_manager->CreateSampler();
        if(!sampler)
            return false;

        rect_recipe.recipe_name = "DrawRoundrectangle.RectTexture2D";
        rect_recipe.mtl_def_id = "RectTexture2D";
        rect_recipe.domain = "DrawRoundrectangle";
        rect_asset = PrimitiveAsset(geometry, &rect_recipe, PrimitiveType::Triangles);

        rect_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("Rect2D");
        if(!rect_entity)
            return false;

        auto transform = rect_entity->AddComponent<hgl::ecs::TransformComponent>();
        transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto prim_comp = rect_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        if(!prim_comp)
            return false;

        prim_comp->SetPrimitiveAsset(&rect_asset);
        prim_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor, texture, sampler);
        prim_comp->SetVisible(true);

        return true;
    }

public:
    bool Init() override
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        if(!CreateRenderObject())
            return(false);

        if(!InitMaterial())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Draw a rectangle with texture"),argc,argv,256,256);
}
