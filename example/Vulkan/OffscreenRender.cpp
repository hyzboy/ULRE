#include<hgl/graph/vulkan/VKRenderTarget.h>
#include"VulkanAppFramework.h"

using namespace hgl;
using namespace hgl::graph;

constexpr uint OFFSCREEN_SIZE   =512;
constexpr uint SCREEN_WIDTH     =1024;
constexpr uint SCREEN_HEIGHT    =(SCREEN_WIDTH/16)*9;

constexpr uint32_t TRIANGLE_VERTEX_COUNT=3;

constexpr float triangle_vertex_data[TRIANGLE_VERTEX_COUNT][2]=
{
    {OFFSCREEN_SIZE*0.5,   OFFSCREEN_SIZE*0.25},
    {OFFSCREEN_SIZE*0.75,  OFFSCREEN_SIZE*0.75},
    {OFFSCREEN_SIZE*0.25,  OFFSCREEN_SIZE*0.75}
};

constexpr float triangle_color_data[TRIANGLE_VERTEX_COUNT][3]=
{   {1,0,0},
    {0,1,0},
    {0,0,1}
};

constexpr uint32_t RECT_VERTEX_COUNT=4;

constexpr float rect_vertex_data[RECT_VERTEX_COUNT][2]=
{
    {0,             0},
    {SCREEN_WIDTH,  0},
    {0,             SCREEN_HEIGHT},
    {SCREEN_WIDTH,  SCREEN_HEIGHT}
};

constexpr float rect_texcoord_data[RECT_VERTEX_COUNT][2]=
{
    {0,0},
    {1,0},
    {0,1},
    {1,1}
};

constexpr uint32_t RECT_INDEX_COUNT=6;

constexpr uint16 rect_index_data[RECT_INDEX_COUNT]=
{
    0,1,3,
    0,3,2
};

class TestApp:public VulkanApplicationFramework
{
    struct RenderObject
    {
        Camera cam;

        vulkan::MaterialInstance *  material_instance   =nullptr;
        vulkan::Buffer *            ubo_world_matrix    =nullptr;
    };

    struct:public RenderObject
    {        
        vulkan::RenderTarget *      render_taget        =nullptr;
        
        vulkan::Pipeline *          pipeline            =nullptr;
        vulkan::RenderableInstance *renderable_instance =nullptr;
    }os;
    
    struct:public RenderObject
    {
        Camera cam;

        vulkan::Sampler *           sampler             =nullptr;        
       
        vulkan::Pipeline *          pipeline            =nullptr;
        vulkan::RenderableInstance *renderable_instance =nullptr;
    }rect;

public:

    ~TestApp()
    {
        delete os.render_taget;
    }

    bool InitUBO(RenderObject *ro,const VkExtent2D &extent)
    {
        ro->cam.width=extent.width;
        ro->cam.height=extent.height;

        ro->cam.Refresh();

        ro->ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&ro->cam.matrix);

        if(!ro->ubo_world_matrix)
            return(false);
            
        ro->material_instance->BindUBO("world",ro->ubo_world_matrix);
        ro->material_instance->Update();
        return(true);
    }

    bool InitOffscreen()
    {
        os.render_taget=device->CreateRenderTarget(OFFSCREEN_SIZE,OFFSCREEN_SIZE,UFMT_RGBA8,FMT_D16UN);
        if(!os.render_taget)return(false);
        
        os.material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor2D"));
        if(!os.material_instance)return(false);

        os.pipeline=db->CreatePipeline(os.material_instance,os.render_taget,vulkan::InlinePipeline::Solid2D);
        if(!os.pipeline)return(false);

        if(!InitUBO(&os,os.render_taget->GetExtent()))
            return(false);

        {
            vulkan::Renderable *render_obj=db->CreateRenderable(TRIANGLE_VERTEX_COUNT);
            if(!render_obj)return(false);        

            if(!render_obj->Set(VAN::Position,  db->CreateVAB(VAF_VEC2,TRIANGLE_VERTEX_COUNT,triangle_vertex_data)))return(false);
            if(!render_obj->Set(VAN::Color,     db->CreateVAB(VAF_VEC3,TRIANGLE_VERTEX_COUNT,triangle_color_data)))return(false);

            os.renderable_instance=db->CreateRenderableInstance(render_obj,os.material_instance,os.pipeline);

            if(!os.renderable_instance)return(false);
        }

        BuildCommandBuffer(os.render_taget,os.renderable_instance);

        os.render_taget->Submit(nullptr);
        os.render_taget->WaitQueue();

        return(true);
    }

    bool InitRectangle()
    {
        rect.material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/Texture2D"));        
        if(!rect.material_instance)return(false);

        rect.pipeline=CreatePipeline(rect.material_instance,vulkan::InlinePipeline::Solid2D);
        if(!rect.pipeline)return(false);

        rect.sampler=db->CreateSampler();
        rect.material_instance->BindSampler("tex",os.render_taget->GetColorTexture(),rect.sampler);

        if(!InitUBO(&rect,sc_render_target->GetExtent()))
            return(false);

        {
            vulkan::Renderable *render_obj=db->CreateRenderable(RECT_VERTEX_COUNT);
            if(!render_obj)return(false);

            if(!render_obj->Set(VAN::Position,db->CreateVAB(VAF_VEC2,RECT_VERTEX_COUNT,rect_vertex_data)))return(false);
            if(!render_obj->Set(VAN::TexCoord,db->CreateVAB(VAF_VEC2,RECT_VERTEX_COUNT,rect_texcoord_data)))return(false);
            if(!render_obj->Set(db->CreateIBO16(RECT_INDEX_COUNT,rect_index_data)))return(false);

            rect.renderable_instance=db->CreateRenderableInstance(render_obj,rect.material_instance,rect.pipeline);
        }
    
        BuildCommandBuffer(rect.renderable_instance);

        return(true);
    }

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitOffscreen())
            return(false);

        if(!InitRectangle())
            return(false);

        return(true);
    }    

    void Resize(int w,int h)override
    {
        rect.cam.width=w;
        rect.cam.height=h;

        rect.cam.Refresh();

        rect.ubo_world_matrix->Write(&rect.cam.matrix);
        
        BuildCommandBuffer(rect.renderable_instance);
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
