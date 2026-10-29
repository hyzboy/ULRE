// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
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

class TextureRectArrayApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;

    Texture2DArray *    texture             = nullptr;
    /// 材质授权统一走句柄：上面的数组纹理有一个 TextureAsset 句柄（Resident，只因文件同步装配而成）。
    TextureAsset *      texture_asset       = nullptr;
    /// 每格一个配方：四格是同一张数组纹理的不同 layer，图层号是配方的固有属性。
    graph::mtl::MaterialRecipe rect_recipe[TexCount];
    PrimitiveAsset      rect_asset[TexCount];

    struct
    {
        Entity *entity;
    }render_obj[TexCount]{};

private:

    bool InitTexture()
    {
        auto* tex_manager = GetManager<TextureManager>();
        if (!tex_manager)
            return false;

        texture = tex_manager->CreateTexture2DArray("freepik icons",
                                512,512,            ///<纹理尺寸
                                TexCount,           ///<纹理层数
                                PF_BC7UN,           ///<纹理格式
                                1);             ///<mip级别数：1 = 只用 0 级（资产自带完整链）

        if(!texture)return(false);

        OSString filename;

        for(uint i=0;i<TexCount;i++)
        {
            filename=filesystem::JoinPathWithFilename(OS_TEXT("res/image/icon/freepik"),tex_filename[i]);

            if(!tex_manager->LoadTexture2DArray(texture,i,filename))
                return(false);
        }

        texture_asset=tex_manager->WrapTextureAsset(texture);

        return(texture_asset!=nullptr);
    }

    bool InitMaterial()
    {
        const mtl::MaterialRecipe *loaded_recipe =
            GetRecipe("TextureRectArray.UnlitTexture");
        if (!loaded_recipe)
            return false;

        for(uint32_t i=0;i<TexCount;i++)
        {
            rect_recipe[i] = *loaded_recipe;

            // 纹理 + 图层都是材质原型固有属性 ⇒ 写进配方；采样器由 Bindless 预设统一管理。
            if(!rect_recipe[i].AddTexture("base_color",texture_asset,true,i))
                return(false);
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
};//class TextureRectArrayApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TextureRectArrayApp>(OS_TEXT("Draw many rectangle with texture"),argc,argv,256*TexCount,256);
}
