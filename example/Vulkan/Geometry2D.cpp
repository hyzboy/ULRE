// Geometry2D
// 该范例有两个作用：
//  一、测试绘制2D几何体
//  二、试验动态合并材质渲染机制
//  三、试验Database/SceneGraph

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

static Vector4f color(1,1,0,1);

class TestApp:public VulkanApplicationFramework
{
    Camera cam;

private:

    SceneNode           render_root;
    RenderList          render_list;

    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;

    Renderable *        ro_rectangle        =nullptr;
    Renderable *        ro_circle           =nullptr;
    Renderable *        ro_round_rectangle  =nullptr;

    GPUBuffer *         ubo_world_matrix    =nullptr;
    GPUBuffer *         ubo_color_material  =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material=db->CreateMaterial(OS_TEXT("res/material/PureColor2D"));

        if(!material)return(false);

        material_instance=db->CreateMaterialInstance(material);

        if(!material_instance)return(false);

        pipeline=CreatePipeline(material,InlinePipeline::Solid2D,Prim::Fan);

        return pipeline;
    }

    void CreateRenderObject()
    {
        {
            struct RectangleCreateInfo rci;

            rci.scope.Set(10,10,20,20);

            ro_rectangle=CreateRenderableRectangle(db,material,&rci);
        }

        {
            struct RoundRectangleCreateInfo rrci;

            rrci.scope.Set(SCREEN_WIDTH-30,10,20,20);
            rrci.radius=5;
            rrci.round_per=5;

            ro_round_rectangle=CreateRenderableRoundRectangle(db,material,&rrci);
        }

        {
            struct CircleCreateInfo cci;

            cci.center.x=SCREEN_WIDTH/2;
            cci.center.y=SCREEN_HEIGHT/2;

            cci.radius.x=SCREEN_WIDTH*0.35;
            cci.radius.y=SCREEN_HEIGHT*0.35;

            cci.field_count=8;

            ro_circle=CreateRenderableCircle(db,material,&cci);
        }
    }

    GPUBuffer *CreateUBO(const AnsiString &name,const VkDeviceSize size,void *data)
    {
        GPUBuffer *ubo=db->CreateUBO(size,data);

        if(!ubo)
            return(nullptr);

        if(!material_instance->BindUBO(name,ubo))
        {
            std::cerr<<"Bind UBO<"<<name.c_str()<<"> to material failed!"<<std::endl;
            SAFE_CLEAR(ubo);
            return(nullptr);
        }

        return ubo;
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.width=extent.width;
        cam.height=extent.height;

        cam.Refresh();
        
        ubo_world_matrix    =CreateUBO("world",         sizeof(WorldMatrix),&cam.matrix);
        ubo_color_material  =CreateUBO("color_material",sizeof(Vector4f),&color);

        material_instance->Update();
        return(true);
    }

    bool InitScene()
    {
        render_root.Add(db->CreateRenderableInstance(ro_rectangle,      material_instance,pipeline));
        render_root.Add(db->CreateRenderableInstance(ro_round_rectangle,material_instance,pipeline));
        render_root.Add(db->CreateRenderableInstance(ro_circle,         material_instance,pipeline));

        render_root.ExpendToList(&render_list);
        BuildCommandBuffer(&render_list);
        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitMaterial())
            return(false);

        CreateRenderObject();

        if(!InitUBO())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }

    void Resize(int w,int h) override
    {
        cam.width=w;
        cam.height=h;

        cam.Refresh();

        ubo_world_matrix->Write(&cam.matrix);

        BuildCommandBuffer(&render_list);
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
