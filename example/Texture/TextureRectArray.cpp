// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/math/Vector.h>

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
    GeometryVertexFormat CreateRectTexture2DArrayGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V2F},
            {VertexSemantic::TexCoord, VF_V2F},
        };
        return gvf;
    }
}

/// 四格：层清单在 `ShaderLibrary/asset/texarray/freepik_icons.texarray.toml` 里
constexpr const size_t TexCount=4;

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

class TextureRectArrayApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;

    /// 每格一个配方：四格是同一张数组纹理的不同 layer，图层号是配方的固有属性。
    graph::mtl::MaterialRecipe rect_recipe[TexCount];
    PrimitiveAsset      rect_asset[TexCount];

    struct
    {
        Entity *entity;
    }render_obj[TexCount]{};

private:

    bool InitMaterial()
    {
        const mtl::MaterialRecipe *loaded_recipe =
            GetRecipe("TextureRectArray.UnlitTexture");
        if (!loaded_recipe)
            return false;

        for(uint32_t i=0;i<TexCount;i++)
        {
            rect_recipe[i] = *loaded_recipe;

            // 纹理由配方文件声明（`texarray:@freepik_icons`）；四格之间唯一的差别是图层号，
            // 而图层号属于「同一张纹理的不同用法」，配方里留 0，由示例按格子改写。
            // 采样器由 Bindless 预设统一管理，材质侧不再持有 Sampler。
            for(graph::mtl::RecipeTextureBinding &binding:rect_recipe[i].textures)
                if(binding.texture_name=="base_color")
                    binding.array_layer=i;
        }

        return(true);
    }

    bool InitVBOAndRenderList()
    {
        auto* device = GetDevice();
        auto* buffer_manager = GetManager<BufferManager>();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryCreater pc(device, CreateRectTexture2DArrayGeometryVertexFormat(), buffer_manager);
        pc.Init("TextureRect", 6);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        // 四个配方共享同一份几何体：差别只在图层号（配方固有属性）。
        for(uint32_t i=0;i<TexCount;i++)
            rect_asset[i] = PrimitiveAsset(geometry, &rect_recipe[i], PrimitiveType::Triangles);

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
            auto transform = ecs_world->GetTransform(ecs_world->CreateTransform(render_obj[i].entity->GetEntityID(), Mobility::Static));
            auto primitive = render_obj[i].entity->GetContext()->GetOrCreateGeometryData(render_obj[i].entity);
            // 材质数据层组件（配方/绑定/行号的承载者）——渲染侧需要它存在
            render_obj[i].entity->GetContext()->GetOrCreateMaterialData(render_obj[i].entity);

            transform.SetLocalPosition(glm::vec3(offset.x, offset.y, offset.z));
            transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform.SetMobility(Mobility::Static);

            primitive->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive->GetOwnerID())->SetPrimitiveAsset(&rect_asset[i]);
            // 纹理与图层已由配方声明（rect_recipe[i].AddTexture），实体端不再重复绑定
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        }

        return true;
    }

public:
    TextureRectArrayApp() = default;
    bool Init() override
    {
        if(!InitMaterial())
            return(false);

        if(!InitVBOAndRenderList())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TextureRectArrayApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TextureRectArrayApp>(OS_TEXT("Draw many rectangle with texture"),argc,argv,256*TexCount,256);
}
