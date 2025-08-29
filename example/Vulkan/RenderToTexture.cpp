// RenderToTexture.cpp
// 渲染到纹理示例 - Render To Texture Example
// 
// 该示例演示了如何将场景渲染到纹理中，然后将该纹理应用到另一个对象上
// This example demonstrates how to render a scene to a texture, 
// then apply that texture to another object
//
// 主要步骤 (Main Steps):
// 1. 创建离屏渲染目标 (Create offscreen render target)
// 2. 渲染场景到纹理 (Render scene to texture) 
// 3. 将渲染的纹理应用到立方体上 (Apply rendered texture to cube)
// 4. 显示最终结果 (Display final result)

#include"VulkanAppFramework.h"
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t TEXTURE_SIZE = 512;    // 渲染目标纹理尺寸 (Render target texture size)
constexpr uint32_t SCREEN_WIDTH = 1024;
constexpr uint32_t SCREEN_HEIGHT = (SCREEN_WIDTH/16)*9;

class RenderToTextureApp : public CameraAppFramework
{
private:
    // 离屏渲染结构 (Offscreen rendering structure)
    struct OffscreenRender
    {
        RenderTarget *      render_target       = nullptr;     // 渲染目标 (Render target)
        RenderCmdBuffer *   command_buffer      = nullptr;     // 命令缓冲区 (Command buffer)
        
        MaterialInstance *  material_instance   = nullptr;     // 材质实例 (Material instance)
        Pipeline *          pipeline            = nullptr;     // 渲染管线 (Rendering pipeline)
        Renderable *        rotating_triangle   = nullptr;     // 旋转三角形 (Rotating triangle)
        
        Camera              camera;                             // 离屏相机 (Offscreen camera)
        DeviceBuffer *      ubo_camera_info     = nullptr;     // 相机UBO (Camera UBO)
        
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
    struct MainScene
    {
        MaterialInstance *  material_instance   = nullptr;     // 材质实例 (Material instance)  
        Pipeline *          pipeline            = nullptr;     // 渲染管线 (Rendering pipeline)
        Renderable *        textured_cube       = nullptr;     // 带纹理的立方体 (Textured cube)
        Sampler *           sampler             = nullptr;     // 纹理采样器 (Texture sampler)
        
        SceneNode           scene_root;                         // 场景根节点 (Scene root node)
        RenderList *        render_list         = nullptr;     // 渲染列表 (Render list)
    } main_scene;

public:
    ~RenderToTextureApp()
    {
        SAFE_CLEAR(main_scene.render_list);
        SAFE_CLEAR(offscreen.render_target);
    }

    // 初始化离屏渲染 (Initialize offscreen rendering)
    bool InitOffscreenRender()
    {
        LOG_INFO(OS_TEXT("初始化离屏渲染... (Initializing offscreen render...)"));
        
        // 创建渲染目标 (Create render target)
        FramebufferInfo fbi(UPF_RGBA8, TEXTURE_SIZE, TEXTURE_SIZE);
        offscreen.render_target = device->CreateRT(&fbi);
        if(!offscreen.render_target)
        {
            LOG_ERROR(OS_TEXT("创建渲染目标失败 (Failed to create render target)"));
            return false;
        }
        
        // 创建命令缓冲区 (Create command buffer)
        offscreen.command_buffer = device->CreateRenderCommandBuffer();
        if(!offscreen.command_buffer)
        {
            LOG_ERROR(OS_TEXT("创建命令缓冲区失败 (Failed to create command buffer)"));
            return false;
        }
        
        // 设置离屏相机 (Setup offscreen camera)
        offscreen.camera.width = TEXTURE_SIZE;
        offscreen.camera.height = TEXTURE_SIZE;
        offscreen.camera.fov = 60.0f;
        offscreen.camera.znear = 0.1f;
        offscreen.camera.zfar = 100.0f;
        offscreen.camera.RefreshCameraInfo();
        
        // 创建相机UBO (Create camera UBO)
        offscreen.ubo_camera_info = db->CreateUBO(sizeof(CameraInfo), &offscreen.camera.info);
        if(!offscreen.ubo_camera_info)
        {
            LOG_ERROR(OS_TEXT("创建相机UBO失败 (Failed to create camera UBO)"));
            return false;
        }
        
        // 创建材质 (Create material)
        offscreen.material_instance = db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor2D"));
        if(!offscreen.material_instance)
        {
            LOG_ERROR(OS_TEXT("创建材质实例失败 (Failed to create material instance)"));
            return false;
        }
        
        // 绑定相机UBO (Bind camera UBO)
        MaterialParameters *mp_global = offscreen.material_instance->GetMP(DescriptorSetType::Global);
        if(mp_global && mp_global->BindUBO("g_camera", offscreen.ubo_camera_info))
        {
            mp_global->Update();
        }
        
        // 创建渲染管线 (Create rendering pipeline)
        offscreen.pipeline = offscreen.render_target->GetRenderPass()->CreatePipeline(
            offscreen.material_instance, 
            InlinePipeline::Solid2D, 
            Prim::Triangles
        );
        if(!offscreen.pipeline)
        {
            LOG_ERROR(OS_TEXT("创建渲染管线失败 (Failed to create rendering pipeline)"));
            return false;
        }
        
        // 创建旋转三角形 (Create rotating triangle)
        if(!CreateRotatingTriangle())
            return false;
            
        LOG_INFO(OS_TEXT("离屏渲染初始化完成 (Offscreen render initialization completed)"));
        return true;
    }
    
    // 创建旋转三角形 (Create rotating triangle)
    bool CreateRotatingTriangle()
    {
        using namespace inline_geometry;
        
        // 使用内联几何创建简单的彩色形状 (Use inline geometry to create simple colored shape)
        CircleCreateInfo cci;
        
        cci.center = Vector2f(TEXTURE_SIZE * 0.5f, TEXTURE_SIZE * 0.5f);
        cci.radius = Vector2f(TEXTURE_SIZE * 0.3f, TEXTURE_SIZE * 0.3f);
        cci.field_count = 3;  // 3边形成三角形 (3 sides to form triangle)
        cci.has_color = true;
        cci.center_color = Vector4f(1.0f, 0.0f, 0.0f, 1.0f);  // 红色中心 (Red center)
        cci.border_color = Vector4f(0.0f, 0.0f, 1.0f, 1.0f);  // 蓝色边缘 (Blue edge)
        
        Primitive *primitive = CreateCircle(db, offscreen.material_instance->GetVIL(), &cci);
        if(!primitive)
        {
            LOG_ERROR(OS_TEXT("创建三角形原语失败 (Failed to create triangle primitive)"));
            return false;
        }
        
        offscreen.rotating_triangle = db->CreateRenderable(primitive, offscreen.material_instance, offscreen.pipeline);
        if(!offscreen.rotating_triangle)
        {
            LOG_ERROR(OS_TEXT("创建三角形可渲染对象失败 (Failed to create triangle renderable)"));
            return false;
        }
        
        return true;
    }
    
    // 初始化主场景 (Initialize main scene)
    bool InitMainScene()
    {
        LOG_INFO(OS_TEXT("初始化主场景... (Initializing main scene...)"));
        
        // 创建渲染列表 (Create render list)
        main_scene.render_list = new RenderList(device);
        
        // 创建材质实例 (Create material instance)
        main_scene.material_instance = db->CreateMaterialInstance(OS_TEXT("res/material/TextureMask3D"));
        if(!main_scene.material_instance)
        {
            LOG_ERROR(OS_TEXT("创建主场景材质实例失败 (Failed to create main scene material instance)"));
            return false;
        }
        
        // 创建渲染管线 (Create rendering pipeline)
        main_scene.pipeline = CreatePipeline(main_scene.material_instance, InlinePipeline::Solid3D, Prim::Triangles);
        if(!main_scene.pipeline)
        {
            LOG_ERROR(OS_TEXT("创建主场景渲染管线失败 (Failed to create main scene rendering pipeline)"));
            return false;
        }
        
        // 创建纹理采样器 (Create texture sampler)
        main_scene.sampler = db->CreateSampler();
        if(!main_scene.sampler)
        {
            LOG_ERROR(OS_TEXT("创建纹理采样器失败 (Failed to create texture sampler)"));
            return false;
        }
        
        // 绑定离屏渲染的纹理到立方体 (Bind offscreen rendered texture to cube)
        if(!main_scene.material_instance->BindImageSampler(
            DescriptorSetType::Value, 
            "tex", 
            offscreen.render_target->GetColorTexture(), 
            main_scene.sampler))
        {
            LOG_ERROR(OS_TEXT("绑定纹理失败 (Failed to bind texture)"));
            return false;
        }
        
        // 创建立方体 (Create cube)
        {
            using namespace inline_geometry;
            
            CubeCreateInfo cci;
            cci.tex_coord = true;  // 启用纹理坐标 (Enable texture coordinates)
            
            Primitive *primitive = CreateCube(db, main_scene.material_instance->GetVIL(), &cci);
            if(!primitive)
            {
                LOG_ERROR(OS_TEXT("创建立方体原语失败 (Failed to create cube primitive)"));
                return false;
            }
            
            main_scene.textured_cube = db->CreateRenderable(primitive, main_scene.material_instance, main_scene.pipeline);
            if(!main_scene.textured_cube)
            {
                LOG_ERROR(OS_TEXT("创建立方体可渲染对象失败 (Failed to create cube renderable)"));
                return false;
            }
            
            // 添加到场景 (Add to scene)
            main_scene.scene_root.CreateSubNode(main_scene.textured_cube);
        }
        
        // 设置相机位置 (Set camera position)
        camera->pos = Vector4f(3, 3, 3, 1.0f);
        camera->lookat = Vector4f(0, 0, 0, 1.0f);
        
        LOG_INFO(OS_TEXT("主场景初始化完成 (Main scene initialization completed)"));
        return true;
    }
    
    // 渲染离屏内容 (Render offscreen content)  
    void RenderOffscreenContent()
    {
        // 构建离屏渲染命令缓冲区 (Build offscreen render command buffer)
        VulkanApplicationFramework::BuildCommandBuffer(
            offscreen.command_buffer, 
            offscreen.render_target, 
            offscreen.rotating_triangle
        );
        
        // 提交离屏渲染 (Submit offscreen rendering)
        offscreen.Submit();
    }

public:
    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH, SCREEN_HEIGHT))
            return false;
            
        SetClearColor(COLOR::DarkSlateGray);
        
        LOG_INFO(OS_TEXT("开始初始化渲染到纹理示例... (Starting Render To Texture example initialization...)"));
        
        // 初始化离屏渲染 (Initialize offscreen rendering)
        if(!InitOffscreenRender())
        {
            LOG_ERROR(OS_TEXT("离屏渲染初始化失败 (Offscreen render initialization failed)"));
            return false;
        }
        
        // 渲染离屏内容到纹理 (Render offscreen content to texture)
        RenderOffscreenContent();
        
        // 初始化主场景 (Initialize main scene)  
        if(!InitMainScene())
        {
            LOG_ERROR(OS_TEXT("主场景初始化失败 (Main scene initialization failed)"));
            return false;
        }
        
        LOG_INFO(OS_TEXT("渲染到纹理示例初始化完成 (Render To Texture example initialization completed)"));
        return true;
    }
    
    void BuildCommandBuffer(uint32 index)
    {
        // 更新主场景 (Update main scene)
        main_scene.scene_root.RefreshMatrix();
        main_scene.render_list->Expend(GetCameraInfo(), &main_scene.scene_root);
        
        // 构建主场景命令缓冲区 (Build main scene command buffer)
        VulkanApplicationFramework::BuildCommandBuffer(index, main_scene.render_list);
    }
};

int main(int, char **)
{
    LOG_INFO(OS_TEXT("启动渲染到纹理示例 (Starting Render To Texture example)"));
    
    RenderToTextureApp app;
    
    if(!app.Init())
    {
        LOG_ERROR(OS_TEXT("应用程序初始化失败 (Application initialization failed)"));
        return -1;
    }
    
    LOG_INFO(OS_TEXT("进入主循环 (Entering main loop)"));
    while(app.Run());
    
    LOG_INFO(OS_TEXT("渲染到纹理示例结束 (Render To Texture example ended)"));
    return 0;
}