// RenderToCubemap
// 渲染到立方体贴图范例
// Render To CubeMap Example

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/Camera.h>
#include<hgl/math/Math.h>
#include"VulkanAppFramework.h"

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t CUBEMAP_SIZE = 256;      // Size of each cube face
constexpr uint32_t SCREEN_WIDTH = 1024;
constexpr uint32_t SCREEN_HEIGHT = (SCREEN_WIDTH/16)*9;

class TestApp : public CameraAppFramework
{
    // Structure for each cube face render target
    struct CubeFaceRenderTarget
    {
        Camera camera;                      // Camera for this face
        RenderTarget* render_target = nullptr;      // Render target for this face
        RenderCmdBuffer* command_buffer = nullptr;  // Command buffer for this face
        MaterialInstance* material_instance = nullptr;
        DeviceBuffer* ubo_camera_info = nullptr;
        
        // Scene objects for this face
        Pipeline* pipeline = nullptr;
        Renderable* sphere_renderable = nullptr;
        Renderable* cube_renderable = nullptr;
        SceneNode scene_root;
        RenderList* render_list = nullptr;
        
        void InitCamera(const Vector3f& position, const Vector3f& target, const Vector3f& up)
        {
            camera.width = CUBEMAP_SIZE;
            camera.height = CUBEMAP_SIZE;
            camera.fov = 90.0f;  // 90 degrees for cube face
            camera.zNear = 0.1f;
            camera.zFar = 100.0f;
            
            camera.pos = Vector4f(position, 1.0f);
            camera.target = Vector4f(target, 1.0f);
            camera.up = Vector4f(up, 1.0f);
            
            camera.RefreshCameraInfo();
        }
        
        bool Submit()
        {
            if (!render_target->Submit(command_buffer))
                return false;
            if (!render_target->WaitQueue())
                return false;
            return true;
        }
    };
    
    // 6 cube faces: +X, -X, +Y, -Y, +Z, -Z
    CubeFaceRenderTarget cube_faces[6];
    
    // Display objects
    struct DisplayObject
    {
        MaterialInstance* material_instance = nullptr;
        Sampler* sampler = nullptr;
        Pipeline* pipeline = nullptr;
        Renderable* renderable = nullptr;
        SceneNode scene_root;
        RenderList* render_list = nullptr;
    } display;
    
    // Generated cube map texture (we'll try to create this from the 6 faces)
    TextureCube* generated_cubemap = nullptr;

public:
    
    ~TestApp()
    {
        for (int i = 0; i < 6; i++)
        {
            SAFE_CLEAR(cube_faces[i].render_list);
            SAFE_CLEAR(cube_faces[i].render_target);
        }
        SAFE_CLEAR(display.render_list);
        SAFE_CLEAR(generated_cubemap);
    }
    
    bool InitCubeFaceRenderTargets()
    {
        // Define the 6 camera orientations for cube faces
        struct CubeFaceInfo
        {
            Vector3f target;
            Vector3f up;
        };
        
        CubeFaceInfo face_info[6] = {
            { Vector3f(1, 0, 0), Vector3f(0, -1, 0) },   // +X
            { Vector3f(-1, 0, 0), Vector3f(0, -1, 0) },  // -X
            { Vector3f(0, 1, 0), Vector3f(0, 0, 1) },    // +Y
            { Vector3f(0, -1, 0), Vector3f(0, 0, -1) },  // -Y
            { Vector3f(0, 0, 1), Vector3f(0, -1, 0) },   // +Z
            { Vector3f(0, 0, -1), Vector3f(0, -1, 0) }   // -Z
        };
        
        Vector3f center_position(0, 0, 0);  // Camera at origin
        
        for (int i = 0; i < 6; i++)
        {
            CubeFaceRenderTarget& face = cube_faces[i];
            
            // Create render target
            FramebufferInfo fbi(UPF_RGBA8, CUBEMAP_SIZE, CUBEMAP_SIZE);
            face.render_target = device->CreateRT(&fbi);
            if (!face.render_target) return false;
            
            // Create command buffer
            face.command_buffer = device->CreateRenderCommandBuffer();
            if (!face.command_buffer) return false;
            
            // Initialize camera for this face
            face.InitCamera(center_position, face_info[i].target, face_info[i].up);
            
            // Create material and UBO
            face.material_instance = db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor3D"));
            if (!face.material_instance) return false;
            
            face.ubo_camera_info = db->CreateUBO(sizeof(CameraInfo), &face.camera.info);
            if (!face.ubo_camera_info) return false;
            
            BindCameraUBO(face.material_instance);
            
            // Create pipeline
            face.pipeline = face.render_target->GetRenderPass()->CreatePipeline(
                face.material_instance, InlinePipeline::Solid3D, Prim::Triangles);
            if (!face.pipeline) return false;
            
            // Create render list
            face.render_list = new RenderList(device);
            
            // Create scene objects for this face
            if (!CreateSceneObjects(face)) return false;
            
            // Build command buffer for this face
            BuildCubeFaceCommandBuffer(face);
        }
        
        return true;
    }
    
    bool CreateSceneObjects(CubeFaceRenderTarget& face)
    {
        using namespace inline_geometry;
        
        // Create a colorful sphere
        Primitive* sphere = CreateSphere(db, face.material_instance->GetVIL(), 32);
        if (!sphere) return false;
        
        face.sphere_renderable = db->CreateRenderable(sphere, face.material_instance, face.pipeline);
        if (!face.sphere_renderable) return false;
        
        // Add sphere to scene (offset to make it visible)
        SceneNode* sphere_node = face.scene_root.CreateSubNode(face.sphere_renderable);
        sphere_node->SetLocalMatrix(translate(Vector3f(2, 0, 0)) * scale(0.5f, 0.5f, 0.5f));
        
        // Create a cube
        CubeCreateInfo cci;
        Primitive* cube = CreateCube(db, face.material_instance->GetVIL(), &cci);
        if (!cube) return false;
        
        face.cube_renderable = db->CreateRenderable(cube, face.material_instance, face.pipeline);
        if (!face.cube_renderable) return false;
        
        // Add cube to scene (offset in different direction)
        SceneNode* cube_node = face.scene_root.CreateSubNode(face.cube_renderable);
        cube_node->SetLocalMatrix(translate(Vector3f(-2, 0, 0)) * scale(0.8f, 0.8f, 0.8f));
        
        // Refresh scene matrix and populate render list
        face.scene_root.RefreshMatrix();
        face.render_list->Expend(face.camera.info, &face.scene_root);
        
        return true;
    }
    
    void BuildCubeFaceCommandBuffer(CubeFaceRenderTarget& face)
    {
        // Build the render list and use the framework method
        // This is similar to how the cube part of OffscreenRender works
        face.scene_root.RefreshMatrix();
        face.render_list->Expend(face.camera.info, &face.scene_root);
        
        // Build the command buffer manually for our custom render target
        face.command_buffer->Begin();
        face.command_buffer->BindFramebuffer(face.render_target->GetFramebuffer());
        face.command_buffer->SetClearColor(0, clear_color);
        face.command_buffer->BeginRenderPass();
        face.render_list->Render(face.command_buffer);
        face.command_buffer->EndRenderPass();
        face.command_buffer->End();
    }
    
    bool InitDisplayObject()
    {
        display.render_list = new RenderList(device);
        
        // For now, just use one of the face textures to display
        // In a full implementation, we would create a cube map from all 6 faces
        display.material_instance = db->CreateMaterialInstance(OS_TEXT("res/material/TextureMask3D"));
        if (!display.material_instance) return false;
        
        display.pipeline = CreatePipeline(display.material_instance, InlinePipeline::Solid3D, Prim::Triangles);
        if (!display.pipeline) return false;
        
        display.sampler = db->CreateSampler();
        if (!display.sampler) return false;
        
        // Use the first face texture for display (as a demo)
        if (!display.material_instance->BindImageSampler(DescriptorSetType::Value, "tex", 
                                                         cube_faces[0].render_target->GetColorTexture(), 
                                                         display.sampler))
            return false;
        
        // Create a sphere to display the result
        using namespace inline_geometry;
        Primitive* sphere = CreateSphere(db, display.material_instance->GetVIL(), 64);
        if (!sphere) return false;
        
        display.renderable = db->CreateRenderable(sphere, display.material_instance, display.pipeline);
        if (!display.renderable) return false;
        
        display.scene_root.CreateSubNode(display.renderable);
        
        // Position camera for viewing
        camera->pos = Vector4f(5, 5, 5, 1.0);
        
        display.scene_root.RefreshMatrix();
        display.render_list->Expend(GetCameraInfo(), &display.scene_root);
        
        return true;
    }
    
    bool Init()
    {
        if (!CameraAppFramework::Init(SCREEN_WIDTH, SCREEN_HEIGHT))
            return false;
            
        SetClearColor(COLOR::MozillaCharcoal);
        
        // Initialize cube face render targets
        if (!InitCubeFaceRenderTargets())
            return false;
            
        // Render to all cube faces
        RenderAllCubeFaces();
        
        // Initialize display object
        if (!InitDisplayObject())
            return false;
            
        return true;
    }
    
    void RenderAllCubeFaces()
    {
        // Render scene to each cube face
        for (int i = 0; i < 6; i++)
        {
            cube_faces[i].Submit();
        }
    }
    
    void BuildCommandBuffer(uint32 index)
    {
        display.scene_root.RefreshMatrix();
        display.render_list->Expend(GetCameraInfo(), &display.scene_root);
        
        VulkanApplicationFramework::BuildCommandBuffer(index, display.render_list);
    }
}; // class TestApp

int main(int, char**)
{
    TestApp app;
    
    if (!app.Init())
        return -1;
        
    while (app.Run());
    
    return 0;
}