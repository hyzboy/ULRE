﻿// 5.SceneTree
//      用于测试树形排列的场景中，每一级节点对变换矩阵的处理是否正确

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/SceneDB.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/RenderList.h>
#include<hgl/Time.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

class TestApp:public CameraAppFramework
{
    Color4f color;

private:

    double      start_time;

    SceneNode   render_root;
    RenderList  render_list;

    vulkan::Material *          material            =nullptr;
    vulkan::MaterialInstance *  material_instance   =nullptr;

    vulkan::Buffer *            ubo_color           =nullptr;

    vulkan::Renderable *        renderable_object   =nullptr;

    vulkan::Pipeline *          pipeline_line       =nullptr;

public:

    TestApp()
    {
        start_time=GetDoubleTime();
    }

    ~TestApp()=default;

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("res/shader/OnlyPosition3D.vert"),
                                               OS_TEXT("res/shader/FlatColor.frag"));
        if(!material)
            return(false);

        material_instance=material->CreateInstance();

        db->Add(material);
        db->Add(material_instance);
        return(true);
    }

    void CreateRenderObject()
    {
        struct CubeCreateInfo cci;

        renderable_object=CreateRenderableCube(db,material,&cci);
    }

    bool InitUBO()
    {
        if(!InitCameraUBO(material_instance,"world"))
            return(false);
            
        color.Set(1,1,0,1);
        ubo_color=device->CreateUBO(sizeof(Color4f),&color);

        material_instance->BindUBO("color_material",ubo_color);

        material_instance->Update();
        return(true);
    }

    bool InitPipeline()
    {
        AutoDelete<vulkan::PipelineCreater> 
        pipeline_creater=new vulkan::PipelineCreater(device,material,sc_render_target);
        pipeline_creater->SetDepthTest(true);
        pipeline_creater->SetDepthWrite(true);
        pipeline_creater->CloseCullFace();
        pipeline_creater->SetPolygonMode(VK_POLYGON_MODE_LINE);
        pipeline_creater->Set(PRIM_TRIANGLES);

        pipeline_line=pipeline_creater->Create();
        if(!pipeline_line)
            return(false);

        db->Add(pipeline_line);
        return(true);
    }

    bool InitScene()
    {
        SceneNode *cur_node;

        uint count;
        float size;

        RenderableInstance *ri=db->CreateRenderableInstance(pipeline_line,material_instance,renderable_object);

        for(uint i=0;i<360;i++)
        {
            size=(i+1)/100.0f;

            cur_node=render_root.CreateSubNode( rotate(i/5.0f,camera.up_vector)*
                                                translate(i/4.0f,0,0)*
                                                scale(size));

            count=(rand()%16)+1;

            for(uint n=0;n<count;n++)
                cur_node->Add(ri,translate(0,0,size*n*1.01));
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
