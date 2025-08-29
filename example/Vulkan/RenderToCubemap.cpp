// RenderToCubemap
// 渲染到立方体贴图范例
// Render To CubeMap Example
// Based on OffscreenRender.cpp but extended to create 6 cube faces

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/InlineGeometry.h>
#include"VulkanAppFramework.h"

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t CUBEMAP_SIZE = 256;      // Size of each cube face
constexpr uint32_t SCREEN_WIDTH = 1024;
constexpr uint32_t SCREEN_HEIGHT = (SCREEN_WIDTH/16)*9;

class TestApp : public CameraAppFramework
{
    // Base render object structure from OffscreenRender.cpp
    struct RenderObject
    {
        Camera cam;
        MaterialInstance* material_instance = nullptr;
        DeviceBuffer* ubo_camera_info = nullptr;
    };

    // Structure for offscreen cube face rendering
    struct : public RenderObject
    {
        RenderTarget* render_target = nullptr;
        RenderCmdBuffer* command_buffer = nullptr;
        Pipeline* pipeline = nullptr;
        Renderable* renderable = nullptr;

        bool Submit()
        {
            if (!render_target->Submit(command_buffer))
                return false;
            if (!render_target->WaitQueue())
                return false;
            return true;
        }
    } cube_faces[6];  // 6 cube faces: +X, -X, +Y, -Y, +Z, -Z

    // Structure for displaying the result
    struct : public RenderObject
    {
        Sampler* sampler = nullptr;
        Pipeline* pipeline = nullptr;
        Renderable* renderable = nullptr;
        SceneNode scene_root;
        RenderList* render_list = nullptr;
    } display;

public:

    ~TestApp()
    {
        for (int i = 0; i < 6; i++)
            SAFE_CLEAR(cube_faces[i].render_target);
        SAFE_CLEAR(display.render_list);
    }

    bool InitUBO(RenderObject* ro, const VkExtent2D& extent)
    {
        ro->cam.width = extent.width;
        ro->cam.height = extent.height;
        ro->cam.RefreshCameraInfo();

        ro->ubo_camera_info = db->CreateUBO(sizeof(CameraInfo), &ro->cam.info);

        if (!ro->ubo_camera_info)
            return false;

        BindCameraUBO(ro->material_instance);
        return true;
    }

    bool InitCubeFaces()
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

        for (int i = 0; i < 6; i++)
        {
            auto& face = cube_faces[i];

            // Create render target
            FramebufferInfo fbi(UPF_RGBA8, CUBEMAP_SIZE, CUBEMAP_SIZE);
            face.render_target = device->CreateRT(&fbi);
            if (!face.render_target) return false;

            // Create command buffer
            face.command_buffer = device->CreateRenderCommandBuffer();
            if (!face.command_buffer) return false;

            // Create material
            face.material_instance = db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor3D"));
            if (!face.material_instance) return false;

            // Setup camera for this face
            face.cam.pos = Vector4f(0, 0, 0, 1);  // Camera at origin
            face.cam.target = Vector4f(face_info[i].target, 1);
            face.cam.up = Vector4f(face_info[i].up, 1);
            face.cam.fov = 90.0f;  // 90 degrees for cube face
            face.cam.zNear = 0.1f;
            face.cam.zFar = 100.0f;

            // Create pipeline
            face.pipeline = face.render_target->GetRenderPass()->CreatePipeline(
                face.material_instance, InlinePipeline::Solid3D, Prim::Triangles);
            if (!face.pipeline) return false;

            // Initialize UBO for this face
            if (!InitUBO(&face, face.render_target->GetExtent()))
                return false;

            // Create scene geometry - a simple colored sphere for each face
            using namespace inline_geometry;
            Primitive* sphere = CreateSphere(db, face.material_instance->GetVIL(), 32);
            if (!sphere) return false;

            face.renderable = db->CreateRenderable(sphere, face.material_instance, face.pipeline);
            if (!face.renderable) return false;

            // Build command buffer for this face (similar to OffscreenRender.cpp)
            VulkanApplicationFramework::BuildCommandBuffer(face.command_buffer, face.render_target, face.renderable);

            // Submit rendering for this face
            face.Submit();
        }

        return true;
    }

    bool InitDisplayObject()
    {
        display.render_list = new RenderList(device);

        display.material_instance = db->CreateMaterialInstance(OS_TEXT("res/material/TextureMask3D"));
        if (!display.material_instance) return false;

        display.pipeline = CreatePipeline(display.material_instance, InlinePipeline::Solid3D, Prim::Triangles);
        if (!display.pipeline) return false;

        display.sampler = db->CreateSampler();
        if (!display.sampler) return false;

        // Use the first cube face texture for display (demo purposes)
        if (!display.material_instance->BindImageSampler(DescriptorSetType::Value, "tex",
                                                         cube_faces[0].render_target->GetColorTexture(),
                                                         display.sampler))
            return false;

        // Create display geometry - a cube to show the texture
        using namespace inline_geometry;
        CubeCreateInfo cci;
        cci.tex_coord = true;

        Primitive* primitive = CreateCube(db, display.material_instance->GetVIL(), &cci);
        if (!primitive) return false;

        display.renderable = db->CreateRenderable(primitive, display.material_instance, display.pipeline);
        if (!display.renderable) return false;

        display.scene_root.CreateSubNode(display.renderable);

        // Position camera for viewing the result
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

        // Initialize cube face rendering
        if (!InitCubeFaces())
            return false;

        // Initialize display object
        if (!InitDisplayObject())
            return false;

        return true;
    }

    void BuildCommandBuffer(uint32_t index)
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