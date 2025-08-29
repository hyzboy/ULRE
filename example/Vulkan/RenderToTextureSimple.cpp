// RenderToTexture.cpp
// 渲染到纹理示例 - Render To Texture Example
// 
// 该示例演示了如何将场景渲染到纹理中，然后将该纹理应用到另一个对象上
// This example demonstrates how to render a scene to a texture, 
// then apply that texture to another object
//
// 主要步骤 (Main Steps):
// 1. 创建离屏渲染目标 (Create offscreen render target)
// 2. 渲染彩色三角形到纹理 (Render colored triangle to texture) 
// 3. 将渲染的纹理应用到立方体上 (Apply rendered texture to cube)
// 4. 显示最终结果 (Display final result)

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/InlineGeometry.h>
#include"VulkanAppFramework.h"

using namespace hgl;
using namespace hgl::graph;

constexpr uint OFFSCREEN_SIZE   = 512;    // 离屏渲染纹理尺寸 (Offscreen render texture size)
constexpr uint SCREEN_WIDTH     = 1024;
constexpr uint SCREEN_HEIGHT    = (SCREEN_WIDTH/16)*9;

class RenderToTextureApp : public CameraAppFramework
{
    // 离屏渲染结构 (Offscreen rendering structure)
    struct
    {        
        RenderTarget *      render_target       = nullptr;     // 渲染目标 (Render target)
        RenderCmdBuffer *   command_buffer      = nullptr;     // 命令缓冲区 (Command buffer)
        
        MaterialInstance *  material_instance   = nullptr;     // 材质实例 (Material instance)
        Pipeline *          pipeline            = nullptr;     // 渲染管线 (Rendering pipeline)
        Renderable *        renderable          = nullptr;     // 可渲染对象 (Renderable object)

    public:

        bool Submit()
        {            
            if(!render_target->Submit(command_buffer))
                return false;
            if(!render_target->WaitQueue())
                return false;
            return true;
        }
    } offscreen;

    // 主场景渲染结构 (Main scene rendering structure)
    struct
    {
        Sampler *           sampler             = nullptr;     // 纹理采样器 (Texture sampler)
       
        Pipeline *          pipeline            = nullptr;     // 渲染管线 (Rendering pipeline)
        Renderable *        renderable          = nullptr;     // 可渲染对象 (Renderable object)

        SceneNode           scene_root;                         // 场景根节点 (Scene root node)
        RenderList *        render_list         = nullptr;     // 渲染列表 (Render list)
    } cube;

public:

    ~RenderToTextureApp()
    {
        SAFE_CLEAR(cube.render_list);
        SAFE_CLEAR(offscreen.render_target);
    }

    // 初始化UBO (Initialize UBO)
    bool InitUBO(MaterialInstance *material_instance, const VkExtent2D &extent)
    {
        Camera cam;
        cam.width = extent.width;
        cam.height = extent.height;
        cam.RefreshCameraInfo();

        DeviceBuffer *ubo_camera_info = db->CreateUBO(sizeof(CameraInfo), &cam.info);
        if(!ubo_camera_info)
            return false;

        MaterialParameters *mp_global = material_instance->GetMP(DescriptorSetType::Global);
        if(mp_global && mp_global->BindUBO("g_camera", ubo_camera_info))
        {
            mp_global->Update();
        }

        return true;
    }

    // 初始化离屏渲染 (Initialize offscreen rendering)
    bool InitOffscreen()
    {
        // 创建离屏渲染目标 (Create offscreen render target)
        FramebufferInfo fbi(UPF_RGBA8, OFFSCREEN_SIZE, OFFSCREEN_SIZE);
        offscreen.render_target = device->CreateRT(&fbi);
        if(!offscreen.render_target) return false;

        // 创建命令缓冲区 (Create command buffer)
        offscreen.command_buffer = device->CreateRenderCommandBuffer();
        if(!offscreen.command_buffer) return false;
        
        // 创建材质实例 (Create material instance)
        offscreen.material_instance = db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor2D"));
        if(!offscreen.material_instance) return false;

        // 创建渲染管线 (Create rendering pipeline)
        offscreen.pipeline = offscreen.render_target->GetRenderPass()->CreatePipeline(
            offscreen.material_instance, InlinePipeline::Solid2D, Prim::Fan);
        if(!offscreen.pipeline) return false;

        // 初始化UBO (Initialize UBO)
        if(!InitUBO(offscreen.material_instance, offscreen.render_target->GetExtent()))
            return false;

        // 创建彩色三角形 (Create colored triangle)
        {
            using namespace inline_geometry;

            CircleCreateInfo cci;
            
            cci.center = Vector2f(OFFSCREEN_SIZE*0.5f, OFFSCREEN_SIZE*0.5f);
            cci.radius = Vector2f(OFFSCREEN_SIZE*0.3f, OFFSCREEN_SIZE*0.3f);
            cci.field_count = 3;  // 3边形成三角形 (3 sides to form triangle)
            cci.has_color = true;
            cci.center_color = Vector4f(1,0,0,1);  // 红色中心 (Red center)
            cci.border_color = Vector4f(0,0,1,1);  // 蓝色边缘 (Blue edge)

            Primitive *primitive = CreateCircle(db, offscreen.material_instance->GetVIL(), &cci);
            if(!primitive) return false;

            offscreen.renderable = db->CreateRenderable(primitive, offscreen.material_instance, offscreen.pipeline);
            if(!offscreen.renderable) return false;
        }

        // 构建命令缓冲区并提交渲染 (Build command buffer and submit rendering)
        VulkanApplicationFramework::BuildCommandBuffer(offscreen.command_buffer, offscreen.render_target, offscreen.renderable);
        offscreen.Submit();

        return true;
    }

    // 初始化立方体 (Initialize cube)
    bool InitCube()
    {
        cube.render_list = new RenderList(device);

        // 创建材质实例 (Create material instance)
        MaterialInstance *material_instance = db->CreateMaterialInstance(OS_TEXT("res/material/TextureMask3D"));
        if(!material_instance) return false;

        // 创建渲染管线 (Create rendering pipeline)
        cube.pipeline = CreatePipeline(material_instance, InlinePipeline::Solid3D, Prim::Triangles);
        if(!cube.pipeline) return false;
        
        // 创建纹理采样器 (Create texture sampler)
        cube.sampler = db->CreateSampler();
        if(!cube.sampler) return false;
        
        // 绑定离屏渲染的纹理到立方体 (Bind offscreen rendered texture to cube)
        if(!material_instance->BindImageSampler(DescriptorSetType::Value, "tex", 
                                               offscreen.render_target->GetColorTexture(), cube.sampler))
            return false;

        // 创建立方体几何 (Create cube geometry)
        {   
            using namespace inline_geometry;

            CubeCreateInfo cci;
            cci.tex_coord = true;  // 启用纹理坐标 (Enable texture coordinates)

            Primitive *primitive = CreateCube(db, material_instance->GetVIL(), &cci);
            if(!primitive) return false;

            cube.renderable = db->CreateRenderable(primitive, material_instance, cube.pipeline);
            cube.scene_root.CreateSubNode(cube.renderable);
        }

        // 设置相机位置 (Set camera position)
        camera->pos = Vector4f(5,5,5,1.0);
        
        cube.scene_root.RefreshMatrix();
        cube.render_list->Expend(GetCameraInfo(), &cube.scene_root);

        return true;
    }

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH, SCREEN_HEIGHT))
            return false;

        SetClearColor(COLOR::MozillaCharcoal);

        // 初始化离屏渲染 (Initialize offscreen rendering)
        if(!InitOffscreen())
            return false;

        // 初始化立方体 (Initialize cube)
        if(!InitCube())
            return false;

        return true;
    }

    void BuildCommandBuffer(uint32 index)
    {
        cube.scene_root.RefreshMatrix();
        cube.render_list->Expend(GetCameraInfo(), &cube.scene_root);

        VulkanApplicationFramework::BuildCommandBuffer(index, cube.render_list);
    }
};

int main(int, char **)
{
    RenderToTextureApp app;

    if(!app.Init())
        return -1;

    while(app.Run());

    return 0;
}