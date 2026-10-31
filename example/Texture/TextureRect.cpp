// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/mtl/MaterialResolver.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>

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

class TextureRectApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;
    Entity *            rect_entity         = nullptr;

    graph::mtl::MaterialRecipe rect_recipe{};
    PrimitiveAsset      rect_asset{};

private:

    bool InitVBO()
    {
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

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);
        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("TextureRect.UnlitTexture");
        if (!loaded_recipe)
            return false;
        rect_recipe = *loaded_recipe;

        // 纹理由配方文件的 `[[textures]]` 声明（`file:res/image/lena.Tex2D`）；
        // 采样器由 Bindless 预设统一管理，材质侧不再持有。
        rect_asset = PrimitiveAsset(geometry, &rect_recipe, PrimitiveType::Triangles);

        return(true);
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        rect_entity = ecs_world->CreateEntity<Entity>("TextureRect");
        auto rect_transform = ecs_world->GetTransform(ecs_world->CreateTransform(rect_entity->GetEntityID(), Mobility::Static));
        auto rect_primitive = rect_entity->GetContext()->GetOrCreateGeometryData(rect_entity);
        // 材质数据层组件（配方/绑定/行号的承载者）——渲染侧需要它存在
        rect_entity->GetContext()->GetOrCreateMaterialData(rect_entity);

        rect_transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        rect_transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        rect_transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        rect_transform.SetMobility(Mobility::Static);

        rect_primitive->GetOwner()->GetContext()->GetOrCreateGeometryData(rect_primitive->GetOwnerID())->SetPrimitiveAsset(&rect_asset);
        // 纹理已由配方文件声明（[[textures]] base_color），实体端不再重复绑定
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        return true;
    }

public:
    bool Init() override
    {
        if(!InitVBO())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TextureRectApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TextureRectApp>(OS_TEXT("Draw a rectangle with texture"),argc,argv,256,256);
}
