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

// Helper class to encapsulate the offscreen rendering setup/lifecycle
class OffscreenScene
{
public:
    IRenderTarget *   rt          = nullptr;
    Scene *           scene       = nullptr;
    SceneRenderer *   renderer    = nullptr;

    // content
    Material *        mtl         = nullptr;
    MaterialInstance* mi          = nullptr;
    Pipeline *        pipeline    = nullptr;
    MeshComponent *   sphere_comp = nullptr;

public:
    ~OffscreenScene()
    {
        SAFE_CLEAR(renderer);
        SAFE_CLEAR(scene);
        rt = nullptr; // managed by manager
    }

    bool Init(WorkObject *owner, uint32_t w, uint32_t h)
    {
        if(!owner) return false;
        auto rf = owner->GetRenderFramework();
        if (!rf) return false;

        const VulkanDevAttr *dev_attr = rf->GetDevAttr();
        if (!dev_attr) return false;

        const VkFormat color_fmt = dev_attr->surface_format.format;
        const VkFormat depth_fmt = dev_attr->physical_device->GetDepthFormat();

        FramebufferInfo fbi(color_fmt, depth_fmt);
        fbi.SetExtent(w, h);

        rt = rf->GetRenderTargetManager()->CreateRT(&fbi);
        if (!rt) return false;

        scene = new Scene(rf);
        renderer = new SceneRenderer(rf, rt);
        if(!renderer) return false;
        renderer->SetScene(scene);

        // setup camera for offscreen
        auto *vmcc = new ViewModelCameraControl();
        renderer->SetCameraControl(vmcc);
        vmcc->SetTarget(Vector3f(0,0,0));
        static_cast<ViewModelCameraControl*>(vmcc)->Rotate(Vector2f(150.0f, -80.0f));
        static_cast<ViewModelCameraControl*>(vmcc)->Zoom(+5.0f);

        renderer->SetClearColor(Color4f(0.1f, 0.2f, 0.6f, 1.0f));
        return true;
    }

    bool BuildSphere(WorkObject *owner)
    {
        if(!owner || !renderer) return false;

        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::Without);

        mtl::MaterialCreateInfo *mci = mtl::CreateGizmo3D(owner->GetDevAttr(), &cfg3d);
        if (!mci) return false;

        mtl = owner->CreateMaterial("OffscreenPureColor3D", mci);
        if (!mtl) return false;

        pipeline = renderer->CreatePipeline(mtl, InlinePipeline::Solid3D);
        if (!pipeline) return false;

        Color4f sphere_color = GetColor4f(COLOR::SkyBlue, 1.0f);
        mi = owner->CreateMaterialInstance(mtl, (VIL*)nullptr, &sphere_color);
        if (!mi) return false;

        auto pc = owner->GetPrimitiveCreater(mtl);
        if (!pc) return false;

        Primitive *prim = inline_geometry::CreateSphere(pc.get(), 64);
        if (!prim) return false;

        SubMesh *mesh = owner->CreateSubMesh(prim, mi, pipeline);
        if (!mesh) { delete prim; return false; }

        CreateComponentInfo cci(renderer->GetSceneRoot());
        sphere_comp = owner->CreateComponent<MeshComponent>(&cci, mesh);
        return sphere_comp != nullptr;
    }

    bool RenderOnce()
    {
        if(!renderer) return false;
        return renderer->RenderSubmitAndWait();
    }
};

class RenderToTextureApp final: public WorkObject
{
private:
    // Offscreen encapsulated
    OffscreenScene *         offscreen          = nullptr;

    // Onscreen pieces
    Material *               cube_mtl            = nullptr;
    MaterialInstance *       cube_mi             = nullptr;
    Pipeline *               cube_pipeline       = nullptr;
    Sampler *                cube_sampler        = nullptr;
    MeshComponent *          cube_comp           = nullptr;

    float                    cube_theta          = 0.0f;

    bool                     captured_offscreen  = false;  // render offscreen once, then sample

private:
    void SetupMainCamera()
    {
        CameraControl *cc = GetCameraControl();
        if(cc)
        {
            cc->SetPosition(Vector3f(3,3,3));
            cc->SetTarget(Vector3f(0,0,0));
        }
    }

    bool CreateOffscreenRT(uint32_t w, uint32_t h)
    {
        offscreen = new OffscreenScene();
        if(!offscreen) return false;

        if(!offscreen->Init(this, w, h))
            return false;

        if(!offscreen->BuildSphere(this))
            return false;

        return true;
    }

    bool CreateRotatingCube()
    {
        // Onscreen default scene: create a rotating cube with a solid color
        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::With);

        mtl::MaterialCreateInfo *mci = mtl::CreateTextureBlinnPhong(GetDevAttr(), &cfg3d);
        if (!mci) return false;

        cube_mtl = CreateMaterial("OnscreenCube", mci);
        if (!cube_mtl) return false;

        cube_sampler=CreateSampler();

        // sample from offscreen color 0
        cube_mtl->BindTextureSampler(   DescriptorSetType::PerMaterial,
                                        mtl::SamplerName::BaseColor,
                                        offscreen && offscreen->rt ? offscreen->rt->GetColorTexture(0) : nullptr,
                                        cube_sampler);

        cube_pipeline = CreatePipeline(cube_mtl, InlinePipeline::Solid3D);
        if (!cube_pipeline) return false;

        cube_mi = CreateMaterialInstance(cube_mtl);
        if (!cube_mi) return false;

        auto pc = GetPrimitiveCreater(cube_mtl);
        if (!pc) return false;

        inline_geometry::CubeCreateInfo cci_cube{}; // defaults OK

        cci_cube.tex_coord=true;
        cci_cube.normal=true;

        Primitive *prim = inline_geometry::CreateCube(pc.get(), &cci_cube);
        if (!prim) return false;

        SubMesh *mesh = CreateSubMesh(prim, cube_mi, cube_pipeline);
        if (!mesh) { delete prim; return false; }

        CreateComponentInfo cci(GetSceneRoot(), Identity4f);
        cube_comp = CreateComponent<MeshComponent>(&cci, mesh);
        return cube_comp != nullptr;
    }

public:
    using WorkObject::WorkObject;

    ~RenderToTextureApp() override
    {
        SAFE_CLEAR(offscreen);
    }

    bool Init() override
    {
        // 1) Create an offscreen RT to render once (with its own scene)
        if (!CreateOffscreenRT(512, 512))
            return false;

        // 2) Create a rotating cube in the default scene
        if (!CreateRotatingCube())
            return false;

        // 3) Setup main scene camera to ensure cube is visible
        SetupMainCamera();

        return true;
    }

    void Render(double delta_time) override
    {
        // First render once into offscreen texture (could be extended to every frame)
        if (offscreen && !captured_offscreen)
        {
            LogInfo("Rendering offscreen RT...\n");

            offscreen->RenderOnce();

            LogInfo("Offscreen RT done.\n");

            captured_offscreen = true;
        }

        // Animate cube
        if (cube_comp)
        {
            cube_theta += float(delta_time) * 0.8f;
            cube_theta = fmodf(cube_theta, 2.0f*HGL_PI);
            Matrix4f rot = AxisZRotate(cube_theta) * AxisXRotate(cube_theta * 0.5f);
            cube_comp->SetLocalMatrix(rot);
        }

        // Present default scene
        WorkObject::Render(delta_time);
    }
};

int os_main(int, os_char **)
{
    return RunFramework<RenderToTextureApp>(OS_TEXT("Render To Texture"), 1280, 720);
}
