// fourth_triangle
// 该范例主要演示使用材质实例传递颜色参数绘制三角形

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/VKRenderablePrimitiveCreater.h>
#include<hgl/graph/mtl/2d/Material2DCreateConfig.h>
#include<hgl/graph/RenderList.h>

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

constexpr float color_data[6][4]=
{
    {1,0,0,1},
    {1,1,0,1},
    {0,1,0,1},
    {0,1,1,1},
    {0,0,1,1},
    {1,0,1,1},
};

class TestApp:public VulkanApplicationFramework
{
private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    MaterialInstance *  material_instance[6]{};
    Renderable *        render_obj[6]       {};

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        {
            mtl::Material2DCreateConfig cfg(device->GetDeviceAttribute(),"PureColor2D");

            cfg.coordinate_system=CoordinateSystem2D::NDC;
            cfg.local_to_world=true;

            AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreatePureColor2D(&cfg);

/*            for(uint i=0;i<6;i++)
            {
                material_instance[i]=db->CreateMaterialInstance(mci);

                if(!material_instance[i])
                    return(false);

                material_instance[i]->SetFloat4(0,color_data[i]);
            }*/
        }

//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
//        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);     //等同上一行，为Framework重载，默认使用swapchain的render target
        
        return pipeline;
    }

    bool InitVBO()
    {
        //RenderablePrimitiveCreater rpc(db,VERTEX_COUNT);

        //if(!rpc.SetVBO(VAN::Position,   VF_V2F, position_data))return(false);
        //
        //render_obj=rpc.Create(material_instance,pipeline);

        //if(!render_obj)
        //    return(false);
        //
        //for(uint i=0;i<6;i++)
        //{
        //    render_root.CreateSubNode(rotate(deg2rad(60*i),Vector3f(0,0,1)),render_obj);
        //}

        //render_root.RefreshMatrix();

        //render_list->Expend(&render_root);

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

        if(!InitVBO())
            return(false);

        BuildCommandBuffer(render_list);

        return(true);
    }

    void Resize(int w,int h)override
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
