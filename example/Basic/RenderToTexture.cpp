#include<hgl/WorkManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKRenderTargetSingle.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/math/Math.h>
#include<hgl/color/Color.h>
#include<hgl/graph/camera/ViewModelCameraControl.h>

using namespace hgl;
using namespace hgl::graph;

// Fullscreen (0..1) rect with matching UVs
static constexpr float kQuadPos[4] = {
    0.0f, // left
    0.0f, // top
    1.0f, // right
    1.0f  // bottom
};

static constexpr float kQuadUV[4] = {
    0.0f, 0.0f,
    1.0f, 1.0f
};

class RenderToTextureApp final: public WorkObject
{
private:
    // Offscreen pieces
    IRenderTarget *          offscreen_rt        = nullptr;
    SceneRenderer *          offscreen_renderer  = nullptr;
    Scene *                  offscreen_scene     = nullptr;   // separate offscreen scene
    CameraControl *          offscreen_camera    = nullptr;   // dedicated offscreen camera control

    // Offscreen draw: a sphere
    Material *               off_mtl             = nullptr;
    MaterialInstance *       off_mi              = nullptr;
    Pipeline *               off_pipeline        = nullptr;
    MeshComponent *          off_sphere_comp     = nullptr;

    // Onscreen pieces (draw offscreen color as a textured rect) + a rotating cube in default scene
    Material *               blit_material       = nullptr;
    MaterialInstance *       blit_mi             = nullptr;
    Pipeline *               blit_pipeline       = nullptr;
    MeshComponent *          blit_component      = nullptr;

    Material *               cube_mtl            = nullptr;
    MaterialInstance *       cube_mi             = nullptr;
    Pipeline *               cube_pipeline       = nullptr;
    MeshComponent *          cube_comp           = nullptr;

    float                    cube_theta          = 0.0f;

    bool                     captured_offscreen  = false;  // render offscreen once, then sample

private:
    bool CreateOffscreenRT(uint32_t w, uint32_t h)
    {
        auto rf = GetRenderFramework();
        if (!rf) return false;

        const VulkanDevAttr *dev_attr = rf->GetDevAttr();
        if (!dev_attr) return false;

        const VkFormat color_fmt = dev_attr->surface_format.format;
        const VkFormat depth_fmt = dev_attr->physical_device->GetDepthFormat();

        FramebufferInfo fbi(color_fmt, depth_fmt);
        fbi.SetExtent(w, h);

        offscreen_rt = rf->GetRenderTargetManager()->CreateRT(&fbi);
        if (!offscreen_rt)
            return false;

        // Create a dedicated Scene for offscreen to avoid sampling the same RT in the same scene
        offscreen_scene = new Scene(rf);

        // Create a dedicated SceneRenderer targeting the offscreen RT
        offscreen_renderer = new SceneRenderer(rf, offscreen_rt);
        if (!offscreen_renderer)
            return false;

        // Bind the independent offscreen scene (do not reuse default scene)
        offscreen_renderer->SetScene(offscreen_scene);

        // Create and set a dedicated camera control for the offscreen renderer
        // Use a simple view-model (orbit) camera aimed at the origin
        offscreen_camera = new ViewModelCameraControl();
        if (offscreen_camera)
        {
            offscreen_renderer->SetCameraControl(offscreen_camera);
            offscreen_camera->SetTarget(Vector3f(0,0,0));
            // Optional: tweak initial view a bit for a nicer angle
            // Rotate by some pixels to change yaw/pitch
            static_cast<ViewModelCameraControl*>(offscreen_camera)->Rotate(Vector2f(150.0f, -80.0f));
            // Slight zoom-in
            static_cast<ViewModelCameraControl*>(offscreen_camera)->Zoom(+5.0f);
        }

        // Clear to a recognizable color
        offscreen_renderer->SetClearColor(Color4f(0.1f, 0.2f, 0.6f, 1.0f));

        // Build a simple 3D object in offscreen scene: a colored sphere
        {
            mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                              mtl::WithCamera::With,
                                              mtl::WithLocalToWorld::With,
                                              mtl::WithSky::Without);

            mtl::MaterialCreateInfo *mci = mtl::CreatePureColor3D(GetDevAttr(), &cfg3d);
            if (!mci) return false;

            off_mtl = CreateMaterial("OffscreenPureColor3D", mci);
            if (!off_mtl) return false;

            // Pipeline must be created against the offscreen render pass
            off_pipeline = offscreen_renderer->GetRenderPass()->CreatePipeline(off_mtl, InlinePipeline::Solid3D);
            if (!off_pipeline) return false;

            // Create MI with a color
            Color4f sphere_color = GetColor4f(COLOR::SkyBlue, 1.0f);
            off_mi = CreateMaterialInstance(off_mtl, (VIL*)nullptr, &sphere_color);
            if (!off_mi) return false;

            // Create sphere primitive and mesh
            auto pc = GetPrimitiveCreater(off_mtl);
            if (!pc) return false;

            Primitive *prim = inline_geometry::CreateSphere(pc.get(), 64);
            if (!prim) return false;

            Mesh *mesh = CreateMesh(prim, off_mi, off_pipeline);
            if (!mesh) { delete prim; return false; }

            CreateComponentInfo cci(offscreen_scene->GetRootNode());
            off_sphere_comp = CreateComponent<MeshComponent>(&cci, mesh);
            if (!off_sphere_comp) return false;
        }

        return true;
    }

    bool CreateBlitQuad()
    {
        // Build a 2D textured rect that samples the offscreen color texture
        mtl::Material2DCreateConfig cfg(PrimitiveType::SolidRectangles,
                                        CoordinateSystem2D::ZeroToOne,
                                        mtl::WithLocalToWorld::Without);

        blit_material = LoadMaterial("Std2D/RectTexture2D", &cfg);
        if (!blit_material) return false;

        blit_pipeline = CreatePipeline(blit_material, InlinePipeline::Solid2D);
        if (!blit_pipeline) return false;

        // Use the offscreen color as a texture
        Texture2D *color_tex = offscreen_rt ? offscreen_rt->GetColorTexture(0) : nullptr;
        if (!color_tex) return false;

        Sampler *sampler = CreateSampler();
        if (!sampler) return false;

        if (!blit_material->BindTextureSampler(DescriptorSetType::PerMaterial,
                                               mtl::SamplerName::BaseColor,
                                               color_tex,
                                               sampler))
            return false;

        blit_mi = CreateMaterialInstance(blit_material);
        if (!blit_mi) return false;

        // Create one-vertex rectangle (expanded by GS) with UVs
        Mesh *mesh = CreateMesh("RTT_Blit", 1, blit_mi, blit_pipeline,
                                {
                                    { VAN::Position, VF_V4F, kQuadPos },
                                    { VAN::TexCoord, VF_V4F, kQuadUV }
                                });
        if (!mesh) return false;

        CreateComponentInfo cci(GetSceneRoot());
        blit_component = CreateComponent<MeshComponent>(&cci, mesh);
        if (!blit_component) return false;

        return true;
    }

    bool CreateRotatingCube()
    {
        // Onscreen default scene: create a rotating cube with a solid color
        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::Without);

        mtl::MaterialCreateInfo *mci = mtl::CreatePureColor3D(GetDevAttr(), &cfg3d);
        if (!mci) return false;

        cube_mtl = CreateMaterial("OnscreenPureColor3D", mci);
        if (!cube_mtl) return false;

        cube_pipeline = CreatePipeline(cube_mtl, InlinePipeline::Solid3D);
        if (!cube_pipeline) return false;

        Color4f cube_color = GetColor4f(COLOR::BananaYellow, 1.0f);
        cube_mi = CreateMaterialInstance(cube_mtl, (VIL*)nullptr, &cube_color);
        if (!cube_mi) return false;

        auto pc = GetPrimitiveCreater(cube_mtl);
        if (!pc) return false;

        inline_geometry::CubeCreateInfo cci_cube{}; // defaults OK (with texcoord/normal enabled internally)
        Primitive *prim = inline_geometry::CreateCube(pc.get(), &cci_cube);
        if (!prim) return false;

        Mesh *mesh = CreateMesh(prim, cube_mi, cube_pipeline);
        if (!mesh) { delete prim; return false; }

        CreateComponentInfo cci(GetSceneRoot(), Identity4f);
        cube_comp = CreateComponent<MeshComponent>(&cci, mesh);
        return cube_comp != nullptr;
    }

public:
    using WorkObject::WorkObject;

    ~RenderToTextureApp() override
    {
        delete offscreen_renderer;
        offscreen_renderer = nullptr;
        delete offscreen_scene;
        offscreen_scene = nullptr;
        delete offscreen_camera;
        offscreen_camera = nullptr;
        offscreen_rt = nullptr; // owned by manager
    }

    bool Init() override
    {
        // 1) Create an offscreen RT to render once (with its own scene)
        if (!CreateOffscreenRT(512, 512))
            return false;

        // 2) Create screen-space blit that samples the offscreen color
        if (!CreateBlitQuad())
            return false;

        // 3) Create a rotating cube in the default scene
        if (!CreateRotatingCube())
            return false;

        return true;
    }

    void Render(double delta_time) override
    {
        // First render once into offscreen texture (could be extended to every frame)
        if (offscreen_renderer && !captured_offscreen)
        {
            offscreen_renderer->Tick(delta_time);

            LogInfo("Rendering offscreen RT...\n");

            offscreen_renderer->RenderFrame();

            LogInfo("Offscreen RT rendered.\n");

            offscreen_renderer->Submit();

            LogInfo("Offscreen RT submitted.\n");

            if (offscreen_rt)
            {
                offscreen_rt->WaitQueue();

                LogInfo("Offscreen RT queue idle.\n");
            }

            captured_offscreen = true;
        }

        // Animate cube
        if (cube_comp)
        {
            cube_theta += float(delta_time) * 0.8f; // rad/s
            Matrix4f rot = AxisYRotate(cube_theta) * AxisXRotate(cube_theta * 0.5f);
            cube_comp->SetLocalMatrix(rot);
        }

        // Then present the blit quad to the swapchain (default path)
        WorkObject::Render(delta_time);
    }
};

int os_main(int, os_char **)
{
    return RunFramework<RenderToTextureApp>(OS_TEXT("Render To Texture"), 1280, 720);
}
