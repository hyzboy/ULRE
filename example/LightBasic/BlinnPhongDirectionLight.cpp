// BlinnPhong direction light

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/Ray.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/mtl/BlinnPhong.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/PrimitiveCreater.h>

using namespace hgl;
using namespace hgl::graph;

static float lumiance_data[2]={1,1};

static Color4f white_color(1,1,1,1);

static mtl::blinnphong::SunLight sun_light=
{
    Vector4f(1,1,1,0),      //direction
    Vector4f(1,0.95,0.9,1)  //color
};

constexpr const COLOR AxisColor[4]=
{
    //COLOR::Red,     //X轴颜色
    //COLOR::Green,   //Y轴颜色
    //COLOR::Blue,    //Z轴颜色
    COLOR::White,    //全局颜色
    COLOR::GhostWhite,
    COLOR::BlanchedAlmond,
    COLOR::AntiqueWhite
};

constexpr const os_char *tex_filename[]=
{
    OS_TEXT("eucalyptus-cross-versailles.Tex2D"),
    OS_TEXT("tints-ashton-green-stretcher.Tex2D"),
    OS_TEXT("worn-clay-cobble-in-desert-stretcher.Tex2D"),
    OS_TEXT("plain-grey-sheer.Tex2D"),
};

constexpr const size_t TexCount=sizeof(tex_filename)/sizeof(os_char *);

class TestApp:public SceneAppFramework
{
private:    //plane grid

    Material *          mtl_vertex_lum      =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;
    Pipeline *          p_line              =nullptr;
    Primitive *         prim_plane_grid     =nullptr;

private:

    DeviceBuffer *      ubo_sun             =nullptr;

    
    Texture2DArray *    texture             =nullptr;
    Sampler *           sampler             =nullptr;

private:    //sphere

    Material *          mtl_blinnphong      =nullptr;
    PrimitiveCreater *  pc_blinnphong       =nullptr;
    VertexDataManager * vdm_blinnphong      =nullptr;

    MaterialInstance *  mi_blinnphong[4]{};
    Pipeline *          p_blinnphong        =nullptr;

    Primitive *         prim_sphere         =nullptr;
    Primitive *         prim_cone           =nullptr;
    Primitive *         prim_cylinder       =nullptr;

private:
    
    bool InitTexture()
    {
        texture=db->CreateTexture2DArray(   "SeamlessBackground",
                                            256,256,            ///<纹理尺寸
                                            TexCount,           ///<纹理层数
                                            PF_BC1_RGBUN,       ///<纹理格式
                                            false);             ///<是否自动产生mipmaps

        if(!texture)return(false);

        OSString filename;

        for(uint i=0;i<TexCount;i++)
        {
            filename=filesystem::MergeFilename(OS_TEXT("res/image/seamless"),tex_filename[i]);

            if(!db->LoadTexture2DToArray(texture,i,filename))
                return(false);
        }

        return(true);
    }

    bool InitVertexLumMP()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"VertexLuminance3D",PrimitiveType::Lines);

        cfg.local_to_world=true;

        mtl_vertex_lum=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
        if(!mtl_vertex_lum)return(false);

        mi_plane_grid=db->CreateMaterialInstance(mtl_vertex_lum,nullptr,&white_color);
        if(!mi_plane_grid)return(false);

        p_line=CreatePipeline(mtl_vertex_lum,InlinePipeline::Solid3D,PrimitiveType::Lines);

        if(!p_line)
            return(false);

        return(true);
    }

    bool CreateBlinnPhongUBO()
    {
        ubo_sun=db->CreateUBO("sun",sizeof(sun_light),&sun_light);
        if(!ubo_sun)return(false);

        return(true);
    }

    bool InitBlinnPhongSunLightMP()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"BlinnPhong3D",PrimitiveType::Triangles);

        cfg.local_to_world=true;
        cfg.material_instance=true;

        mtl_blinnphong=db->LoadMaterial("Std3D/BlinnPhong/SunLightPureColorTexture",&cfg);
        if(!mtl_blinnphong)return(false);

        mtl_blinnphong->BindUBO(DescriptorSetType::Global,"sun",ubo_sun);
        mtl_blinnphong->Update();
        
        sampler=db->CreateSampler();

        if(!mtl_blinnphong->BindImageSampler(   DescriptorSetType::PerMaterial,     ///<描述符合集
                                                mtl::SamplerName::BaseColor,        ///<采样器名称
                                                texture,                            ///<纹理
                                                sampler))                           ///<采样器
            return(false);

        struct MIData
        {
            Color4f color;
            uint32 tex_id[4];
        }mi_data;

        constexpr const uint MIDataStride=sizeof(MIData);

        for(uint i=0;i<4;i++)
        {
            mi_data.color=GetColor4f(AxisColor[i],4);
            mi_data.tex_id[0]=i;

            mi_blinnphong[i]=db->CreateMaterialInstance(mtl_blinnphong,     //材质
                                                        nullptr,            //顶点输入配置,这里使用nullptr，即代表会使用默认配置，那么结果将等同于上面的mtl_blinnphong->GetDefaultVIL()
                                                        &mi_data);          //材质实例参数
            if(!mi_blinnphong[i])return(false);
        }

        p_blinnphong=CreatePipeline(mtl_blinnphong,InlinePipeline::Solid3D,PrimitiveType::Triangles);

        if(!p_blinnphong)
            return(false);

        return(true);
    }

    bool InitVDMAndPC()
    {
        vdm_blinnphong=new VertexDataManager(device,mtl_blinnphong->GetDefaultVIL());
        if(!vdm_blinnphong->Init(   1024*1024,          //VAB最大容量
                                    1024*1024,          //索引最大容量
                                    IndexType::U16))    //索引类型
        {
            delete vdm_blinnphong;
            vdm_blinnphong=nullptr;

            return(false);
        }

        pc_blinnphong=new PrimitiveCreater(vdm_blinnphong);
        //pc_blinnphong=new PrimitiveCreater(device,mtl_blinnphong->GetDefaultVIL());

        return(true);
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        //Plane Grid
        //{
        //    struct PlaneGridCreateInfo pgci;

        //    pgci.grid_size.Set(32,32);
        //    pgci.sub_count.Set(8,8);

        //    pgci.lum=0.5;
        //    pgci.sub_lum=0.75;

        //    prim_plane_grid=CreatePlaneGrid(pc_blinnphong,mtl_vertex_lum->GetDefaultVIL(),&pgci);
        //}

        //Sphere
        prim_sphere=CreateSphere(pc_blinnphong,16);

        //Cone
        {
            struct ConeCreateInfo cci;

            cci.radius      =1;         //圆锥半径
            cci.halfExtend  =1;         //圆锤一半高度
            cci.numberSlices=16;        //圆锥底部分割数
            cci.numberStacks=8;         //圆锥高度分割数

            prim_cone=CreateCone(pc_blinnphong,&cci);
        }

        //Cyliner
        {
            struct CylinderCreateInfo cci;

            cci.halfExtend  =1.25;      //圆柱一半高度
            cci.numberSlices=16;        //圆柱底部分割数
            cci.radius      =1.25f;     //圆柱半径

            prim_cylinder=CreateCylinder(pc_blinnphong,&cci);
        }

        return(true);
    }    
    
    Renderable *Add(Primitive *r,MaterialInstance *mi,Pipeline *p,const Matrix4f &mat=Identity4f)
    {
        if(!r)
            return(nullptr);
        if(!mi)
            return(nullptr);
        if(!p)
            return(nullptr);

        Renderable *ri=db->CreateRenderable(r,mi,p);

        if(!ri)
        {
            LOG_ERROR("Create Renderable failed! Primitive: "+r->GetName());
            return(nullptr);
        }

        render_root.CreateSubNode(mat,ri);

        return ri;
    }

    bool InitScene()
    {
        //Add(prim_plane_grid,mi_plane_grid,p_line,Identity4f);

        Add(prim_sphere,    mi_blinnphong[0],p_blinnphong,translate(Vector3f(0,0,2)));

        Add(prim_cone,      mi_blinnphong[1],p_blinnphong);
        Add(prim_cylinder,  mi_blinnphong[2],p_blinnphong,translate(Vector3f(0,0,-5)));

        camera->pos=Vector3f(32,32,32);
        camera_control->SetTarget(Vector3f(0,0,0));
        camera_control->Refresh();

        render_root.RefreshMatrix();
        render_list->Expend(&render_root);

        return(true);
    }

public:

    bool Init(uint w,uint h)
    {        
        if(!SceneAppFramework::Init(w,h))
            return(false);

        if(!InitTexture())
            return(false);

        //if(!InitVertexLumMP())
        //    return(false);
        
        if(!CreateBlinnPhongUBO())
            return(false);

        if(!InitBlinnPhongSunLightMP())
            return(false);

        if(!InitVDMAndPC())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }

    ~TestApp()
    {
        SAFE_CLEAR(prim_cone)
        SAFE_CLEAR(prim_cylinder)
        SAFE_CLEAR(prim_sphere)
        SAFE_CLEAR(prim_plane_grid)

        SAFE_CLEAR(pc_blinnphong)
        SAFE_CLEAR(vdm_blinnphong)
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1280,720);
}
