// 画一个圆角矩形，它是UI的基本绘图元件
// 通过控制尺寸、每个角的半径，可绘制出正圆、矩形、圆角矩形

#include<hgl/WorkManager.h>
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

    Geometry *          rect_geometry       =nullptr;

    inline static const mtl::MaterialAssetRecord kRoundRectCfg {
        .id       = "roundrect_texture",
        .preset   = mtl::MaterialPreset::PureTexture2D,
        .dim      = mtl::MaterialAssetRecord::Dim::D2,
        .l2w      = false,
        .coord_2d = CoordinateSystem2D::ZeroToOne,
        .pipeline = GraphicsPipelinePreset::Solid2D,
        .textures = {
            {mtl::SamplerSlot::BaseColor, mtl::TextureSourceMode::None, "res/image/lena.Tex2D"},
        },
    };

private:

    bool InitVBO()
    {
        if(!ecs_world)
        {
            ecs_world = GetECSContext();
            if(!ecs_world)
                return false;
        }

        auto* device = GetDevice();
        auto* buffer_manager = GetBufferManager();
        auto* geometry_manager = GetGeometryManager();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryVertexFormat gvf;
        gvf.Set(VAN::Position, VF_V2F);
        gvf.Set(VAN::TexCoord, VF_V2F);

        GeometryCreater pc(device, gvf, buffer_manager);
        pc.Init("TextureRect", 6);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data))
            return false;

        rect_geometry = pc.Create();
        if (!rect_geometry)
            return false;
        geometry_manager->Add(rect_geometry);

        rect_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("Rect2D");
        if(!rect_entity)
            return false;

        auto transform = rect_entity->AddComponent<hgl::ecs::TransformComponent>();
        transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto prim_comp = rect_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        if(!prim_comp)
            return false;

        prim_comp->SetUnresolvedGeometry(rect_geometry);
        prim_comp->SetMaterialRecord(&kRoundRectCfg);
        prim_comp->SetVisible(true);

        return true;
    }

public:
    bool Init() override
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        if(!InitVBO())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Draw a rectangle with texture"),argc,argv,256,256);
}


