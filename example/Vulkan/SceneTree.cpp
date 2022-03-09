// SceneTree
//      用于测试树形排列的场景中，每一级节点对变换矩阵的处理是否正确,以及Instance绘制

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/RenderList.h>
#include<hgl/Time.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

class TestApp:public CameraAppFramework
{
    struct
    {
        Color4f color;
        Color4f abiment;
    }color_material;

    Vector3f sun_direction;

private:

    double      start_time;

    SceneNode   render_root;
    RenderList  render_list;

    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;

    GPUBuffer *         ubo_color           =nullptr;
    GPUBuffer *         ubo_sun             =nullptr;

    Renderable *        renderable_object   =nullptr;

    Pipeline *          pipeline            =nullptr;

public:

    TestApp()
    {
        start_time=GetDoubleTime();
    }

    ~TestApp()=default;

private:

    bool InitMaterial()
    {
        material=db->CreateMaterial(OS_TEXT("res/shader/VertexLight.vert"),
                                               OS_TEXT("res/shader/VertexColor.frag"));
        if(!material)
            return(false);

        material_instance=db->CreateMaterialInstance(material);

        return(true);
    }

    void CreateRenderObject()
    {
        renderable_object=CreateRenderableSphere(db,material_instance->GetVAB(),40);
    }

    bool InitUBO()
    {
        color_material.color.Set(1,1,1,1);
        color_material.abiment.Set(0.25,0.25,0.25,1.0);

        ubo_color=device->CreateUBO(sizeof(color_material),&color_material);
        if(!ubo_color)return(false);

        sun_direction=normalized(Vector3f(rand(),rand(),rand()));

        ubo_sun=device->CreateUBO(sizeof(sun_direction),&sun_direction);
        if(!ubo_sun)return(false);

        material_instance->BindUBO("world",GetCameraMatrixBuffer());
        material_instance->BindUBO("color_material",ubo_color);
        material_instance->BindUBO("sun",ubo_sun);

        material_instance->Update();

        db->Add(ubo_color);
        db->Add(ubo_sun);
        return(true);
    }

    bool InitPipeline()
    {
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid3D,Prim::Triangles);
        
        return pipeline;
    }

    bool InitScene()
    {
        SceneNode *cur_node;

        uint count;
        float size;

        RenderableInstance *ri=db->CreateRenderableInstance(pipeline,material_instance,renderable_object);

        for(uint i=0;i<360;i++)
        {
            size=(i+1)/100.0f;

            cur_node=render_root.CreateSubNode( rotate(i/5.0f,camera->world_up)*
                                                translate(i/4.0f,0,0)*
                                                scale(size));

            count=(rand()%16)+1;

            for(uint n=0;n<count;n++)
                cur_node->CreateSubNode(translate(0,0,size*n*1.01),ri);
        }

        render_root.RefreshMatrix();
        render_list.Clear();
        render_root.ExpendToList(&render_list);
        return(true);
    }

public:

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitMaterial())
            return(false);

        CreateRenderObject();

        if(!InitUBO())
            return(false);

        if(!InitPipeline())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }

    void Draw() override
    {
        CameraAppFramework::Draw();

        Matrix4f rot=rotate(GetDoubleTime()-start_time,camera.up_vector);

        render_root.RefreshMatrix(&rot);
        render_list.Clear();
        render_root.ExpendToList(&render_list);
    }
    
    void BuildCommandBuffer(uint32 index)
    {
        VulkanApplicationFramework::BuildCommandBuffer(index,&render_list);
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
