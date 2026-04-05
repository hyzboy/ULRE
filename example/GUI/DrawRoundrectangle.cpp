// 画一个圆角矩形，它是UI的基本绘图元件
// 通过控制尺寸、每个角的半径，可绘制出正圆、矩形、圆角矩形

#include<hgl/WorkManager.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
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

    Texture2D *         texture             =nullptr;
    Sampler *           sampler             =nullptr;
    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;

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

        static const mtl::MaterialAssetRecord kRoundRectCfg {
            .id       = "roundrect_texture",
            .preset   = mtl::MaterialPreset::PureTexture2D,
            .dim      = mtl::MaterialAssetRecord::Dim::D2,
            .l2w      = false,
            .coord_2d = CoordinateSystem2D::ZeroToOne,
            .pipeline = GraphicsPipelinePreset::Solid2D,
        };

        MaterialAssetRegistry registry(material_manager, nullptr, nullptr);
        auto handle = registry.Acquire(kRoundRectCfg);
        if(!handle.IsValid())
            return(false);

        material = handle.material;

        texture=texture_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);

        if(!texture)return(false);

        sampler=sampler_manager->CreateSampler();

        if(!material->BindImageSampler( DescriptorSetType::Material,     ///<描述符合集
           mtl::SamplerName::ToDescriptorName(SamplerName::SamplerSlot::BaseColor),        ///<采样器名称
           texture,                            ///<纹理
           sampler))                           ///<采样器
            return(false);

        material_instance = registry.CreateMI(handle, kRoundRectCfg);

        return(material_instance!=nullptr);
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
        pc.Init("TextureRect", 6);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        Primitive *primitive = primitive_manager->CreatePrimitive(geometry, material_instance);

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

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Draw a rectangle with texture"),argc,argv,256,256);
}

