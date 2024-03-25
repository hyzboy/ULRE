// 该范例主要演示使用一个材质下的不同材质实例传递颜色参数绘制三角形，并依赖RenderList中的自动合并功能，让同一材质下所有不同材质实例的对象一次渲染完成。

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/VKRenderablePrimitiveCreater.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/RenderList.h>
#include<hgl/color/Color.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1024;
constexpr uint32_t SCREEN_HEIGHT=1024;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0,  0.0,
    -0.1,  0.9,
     0.1,  0.9
};

constexpr uint DRAW_OBJECT_COUNT=12;

#define USE_MATERIAL_FILE   true        //是否使用材质文件

class TestApp:public VulkanApplicationFramework
{
private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    Material *          material            =nullptr;

    struct
    {
        MaterialInstance *  mi;
        Renderable *        r;
    }render_obj[DRAW_OBJECT_COUNT]{};

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        {
            mtl::Material2DCreateConfig cfg(device->GetDeviceAttribute(),"PureColor2D",Prim::Triangles);

            cfg.coordinate_system=CoordinateSystem2D::NDC;
            cfg.local_to_world=true;

        #ifndef USE_MATERIAL_FILE
            AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreatePureColor2D(&cfg);                       //走程序内置材质创建函数
            material=db->CreateMaterial(mci);
        #else
            material=db->LoadMaterial("Std2D/PureColor2D",&cfg);                                            //走材质文件加载
        #endif//USE_MATERIAL_FILE

            if(!material)
                return(false);

            for(uint i=0;i<DRAW_OBJECT_COUNT;i++)
            {
                render_obj[i].mi=db->CreateMaterialInstance(material);

                if(!render_obj[i].mi)
                    return(false);

                Color4f color=GetColor4f((COLOR)(i+int(COLOR::Blue)),1.0);

                render_obj[i].mi->WriteMIData(color);       //设置MaterialInstance的数据
            }
        }

        pipeline=CreatePipeline(material,InlinePipeline::Solid2D,Prim::Triangles);
        
        return pipeline;
    }

    bool InitVBOAndRenderList()
    {
        RenderablePrimitiveCreater rpc(db,"Triangle",VERTEX_COUNT);

        if(!rpc.SetVBO(VAN::Position,   VF_V2F, position_data))return(false);
        
        for(uint i=0;i<DRAW_OBJECT_COUNT;i++)
        {
            render_obj[i].r=rpc.Create(render_obj[i].mi,pipeline);

            if(!render_obj[i].r)
                return(false);

            render_root.CreateSubNode(rotate(deg2rad(double(360/DRAW_OBJECT_COUNT*i)),Vector3f(0,0,1)),render_obj[i].r);
        }

        render_root.RefreshMatrix();

        render_list->Expend(&render_root);

        return(true);
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_list);
    }

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        render_list=new RenderList(device);

        if(!InitMaterial())
            return(false);

        if(!InitVBOAndRenderList())
            return(false);

        BuildCommandBuffer(render_list);

        return(true);
    }

    void Resize(uint w,uint h)override
    {
        VulkanApplicationFramework::Resize(w,h);

        BuildCommandBuffer(render_list);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
