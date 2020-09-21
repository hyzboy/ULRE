// 2.texture_rect
// 该示例是1.indices_rect的进化，演示在矩形上贴上贴图

#include"VulkanAppFramework.h"
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTextureFromFile(Device *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

constexpr uint32_t VERTEX_COUNT=4;

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {0,             0},
    {SCREEN_WIDTH,  0},
    {0,             SCREEN_HEIGHT},
    {SCREEN_WIDTH,  SCREEN_HEIGHT}
};

constexpr float tex_coord_data[VERTEX_COUNT][2]=
{
    {0,0},
    {1,0},
    {0,1},
    {1,1}
};

constexpr uint32_t INDEX_COUNT=6;

constexpr uint16 index_data[INDEX_COUNT]=
{
    0,1,3,
    0,3,2
};

class TestApp:public VulkanApplicationFramework
{
    Camera cam;

private:

    vulkan::Texture2D *         texture             =nullptr;
    vulkan::Sampler *           sampler             =nullptr;
    vulkan::MaterialInstance *  material_instance   =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_world_matrix    =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

    vulkan::VAB *               vertex_buffer       =nullptr;
    vulkan::VAB *               tex_coord_buffer    =nullptr;
    vulkan::IndexBuffer *       index_buffer        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(index_buffer);
        SAFE_CLEAR(tex_coord_buffer);
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR(texture);
    }

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/Texture2D"));        
        if(!material_instance)return(false);

        pipeline=CreatePipeline(material_instance,OS_TEXT("res/pipeline/solid2d"));
        if(!pipeline)return(false);

        texture=vulkan::CreateTextureFromFile(device,OS_TEXT("res/image/lena.Tex2D"));
        if(!texture)return(false);

        sampler=db->CreateSampler();

        material_instance->BindSampler("tex",texture,sampler);

        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.vp_width=cam.width=extent.width;
        cam.vp_height=cam.height=extent.height;        

        cam.Refresh();

        ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&cam.matrix);

        if(!ubo_world_matrix)
            return(false);

        if(!material_instance->BindUBO("world",ubo_world_matrix))return(false);
        material_instance->Update();
        return(true);
    }

    bool InitVBO()
    {
        render_obj=db->CreateRenderable(material_instance,VERTEX_COUNT);
        if(!render_obj)return(false);

        vertex_buffer   =device->CreateVAB(VAF_VEC2,VERTEX_COUNT,vertex_data);
        tex_coord_buffer=device->CreateVAB(VAF_VEC2,VERTEX_COUNT,tex_coord_data);
        index_buffer    =device->CreateIBO16(INDEX_COUNT,index_data);

        if(!render_obj->Set(VAN::Position,vertex_buffer))return(false);
        if(!render_obj->Set(VAN::TexCoord,tex_coord_buffer))return(false);
        if(!render_obj->Set(index_buffer))return(false);

        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);
            
        if(!InitMaterial())
            return(false);

        if(!InitUBO())
            return(false);

        if(!InitVBO())
            return(false);
            
        BuildCommandBuffer(pipeline,material_instance,render_obj);

        return(true);
    }

    void Resize(int w,int h)override
    {
        cam.width=w;
        cam.height=h;

        cam.Refresh();

        ubo_world_matrix->Write(&cam.matrix);

        BuildCommandBuffer(pipeline,material_instance,render_obj);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
#ifdef _DEBUG
    if(!vulkan::CheckStrideBytesByFormat())
        return 0xff;
#endif//

    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
