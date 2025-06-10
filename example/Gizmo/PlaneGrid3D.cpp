// PlaneGrid3D

#include<hgl/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/FirstPersonCameraControl.h>
#include<hgl/color/Color.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    Material *          material            =nullptr;
    Pipeline *          pipeline            =nullptr;

    Primitive *         prim_plane_grid     =nullptr;
    MaterialInstance *  material_instance[3]{};

private:

    bool InitMDP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        cfg.local_to_world=true;
        cfg.position_format=VAT_VEC2;

        material=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
        if(!material)return(false);

        VILConfig vil_config;

        vil_config.Add(VAN::Luminance,VF_V1UN8);

        Color4f GridColor;
        COLOR ce=COLOR::BlenderAxisRed;

        for(uint i=0;i<3;i++)
        {
            GridColor=GetColor4f(ce,1.0);

            material_instance[i]=db->CreateMaterialInstance(material,&vil_config,&GridColor);

            ce=COLOR((int)ce+1);
        }

        pipeline=CreatePipeline(material_instance[0],InlinePipeline::Solid3D,PrimitiveType::Lines);

        return pipeline;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        struct PlaneGridCreateInfo pgci;

        pgci.grid_size.Set(32,32);
        pgci.sub_count.Set(8,8);

        pgci.lum=180;
        pgci.sub_lum=255;

        PrimitiveCreater pc(GetDevice(),material_instance[0]->GetVIL());

        prim_plane_grid=CreatePlaneGrid2D(&pc,&pgci);

        return prim_plane_grid;
    }

    void Add(SceneNode *parent_node,MaterialInstance *mi,const Matrix4f &mat)
    {
        Mesh *ri=db->CreateMesh(prim_plane_grid,mi,pipeline);

        if(!ri)
            return;

        parent_node->Add(new SceneNode(mat,ri));
    }

    bool InitScene()
    {
        SceneNode *scene_root=GetSceneRoot();       //取得缺省场景根节点

        Add(scene_root,material_instance[0],Matrix4f(1.0f));
        Add(scene_root,material_instance[1],rotate(HGL_RAD_90,0,1,0));
        Add(scene_root,material_instance[2],rotate(HGL_RAD_90,1,0,0));

        Camera *cur_camera=GetCamera();             //取得缺省相机

        cur_camera->pos=Vector3f(32,32,32);

        CameraControl *camera_control=GetCameraControl();

        if(camera_control
           &&camera_control->GetControlName()==FirstPersonCameraControl::StaticControlName())
        {
            FirstPersonCameraControl *fp_cam_ctl=(FirstPersonCameraControl *)camera_control;

            fp_cam_ctl->SetTarget(Vector3f(0,0,0));
        }

//        camera_control->SetReserveDirection(true,true);        //反转x,y

        return(true);
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(prim_plane_grid);
    }

    bool Init() override
    {
        if(!InitMDP())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }
};//class TestApp:public CameraAppFramework

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("PlaneGrid3D"),1280,720);
}
