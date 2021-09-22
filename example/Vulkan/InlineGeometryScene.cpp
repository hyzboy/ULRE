// 7.InlineGeometryScene
//  全内置几何体场景

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

struct PhongLight
{
    Vector4f color;
    Vector4f position;
};

struct PhongMaterial
{
    Vector4f BaseColor;
    Vector4f specular;
    float ambient;
};

constexpr size_t v3flen=sizeof(PhongLight);

class TestApp:public CameraAppFramework
{
    PhongLight light;
    PhongMaterial phong;

private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;
    
    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;

    Material *          axis_material       =nullptr;
    MaterialInstance *  axis_mi             =nullptr;

    PipelineData *      pipeline_data       =nullptr;
    Pipeline *          axis_pipeline       =nullptr;
    Pipeline *          pipeline_solid      =nullptr;

    GPUBuffer *         ubo_light           =nullptr;
    GPUBuffer *         ubo_phong           =nullptr;

    struct
    {
        Sampler *       sampler             =nullptr;
        Texture2D *     color               =nullptr;
        Texture2D *     normal              =nullptr;
    }texture;

    Renderable          *ro_axis,
                        *ro_cube,
                        *ro_sphere,
                        *ro_torus,
                        *ro_cylinder,
                        *ro_cone;

private:

    bool InitMaterial()
    {
        light.color.Set(1,1,1,1);
        light.position.Set(1000,1000,1000,1.0);

        phong.BaseColor.Set(1,1,1,1);
        phong.ambient=0.5;
        phong.specular.Set(0.3,0.3,0.3,32);

        {
            axis_material=db->CreateMaterial(OS_TEXT("res/material/VertexColor3D"));
            if(!axis_material)return(false);

            axis_mi=db->CreateMaterialInstance(axis_material);
            if(!axis_mi)return(false);

            axis_pipeline=CreatePipeline(axis_material,InlinePipeline::Solid3D,Prim::Lines);
            if(!axis_pipeline)return(false);
        }
        
        {
            texture.color   =db->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"),true);
            texture.normal  =db->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"),true);

            VkSamplerCreateInfo sampler_create_info=
            {
                VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                nullptr,
                0,
                VK_FILTER_LINEAR,
                VK_FILTER_LINEAR,
                VK_SAMPLER_MIPMAP_MODE_LINEAR,
                VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VK_SAMPLER_ADDRESS_MODE_REPEAT,
                0.0f,
                VK_TRUE,
                device->GetPhysicalDevice()->GetMaxSamplerAnisotropy(),
                false,
                VK_COMPARE_OP_NEVER,
                0.0f,
                static_cast<float>(texture.color->GetMipLevel()),
                VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                false
            };

            texture.sampler =db->CreateSampler(&sampler_create_info);
        }

        {
            material=db->CreateMaterial(OS_TEXT("res/material/TextureNormal"));
            if(!material)return(false);

            material_instance=db->CreateMaterialInstance(material);
            if(!material_instance)return(false);

            {            
                MaterialParameters *mp_texture=material_instance->GetMP(DescriptorSetType::Value);
        
                if(!mp_texture)
                    return(false);

                mp_texture->BindSampler("TexColor"    ,texture.color,    texture.sampler);
                mp_texture->BindSampler("TexNormal"   ,texture.normal,   texture.sampler);
            }
        }

        pipeline_data=GetPipelineData(InlinePipeline::Solid3D);
        if(!pipeline_data)return(false);

        pipeline_solid=CreatePipeline(material,pipeline_data,Prim::Triangles);
        if(!pipeline_solid)return(false);

        return(true);
    }

    void CreateRenderObject()
    {
        {
            struct AxisCreateInfo aci;

            aci.size=200;

            ro_axis=CreateRenderableAxis(db,axis_material,&aci);
        }
        
        {
            struct CubeCreateInfo cci;
            cci.has_color=true;
            cci.color.Set(1,1,1,1);
            ro_cube=CreateRenderableCube(db,material,&cci);
        }
        
        {
            ro_sphere=CreateRenderableSphere(db,material,64);
        }

        {
            TorusCreateInfo tci;

            tci.innerRadius=50;
            tci.outerRadius=70;

            tci.numberSlices=128;
            tci.numberStacks=64;

            tci.uv_scale.x=4;
            tci.uv_scale.y=1;

            ro_torus=CreateRenderableTorus(db,material,&tci);
        }

        {
            CylinderCreateInfo cci;

            cci.halfExtend=10;
            cci.radius=10;
            cci.numberSlices=32;

            ro_cylinder=CreateRenderableCylinder(db,material,&cci);
        }

        {
            ConeCreateInfo cci;

            cci.halfExtend=10;
            cci.radius=10;
            cci.numberSlices=128;
            cci.numberStacks=32;

            ro_cone=CreateRenderableCone(db,material,&cci);
        }
    }

    bool InitUBO()
    {
        ubo_light=db->CreateUBO(sizeof(PhongLight),&light);
        ubo_phong=db->CreateUBO(sizeof(PhongMaterial),&phong);
        
        {
            MaterialParameters *mp_value=material_instance->GetMP(DescriptorSetType::Value);
        
            if(!mp_value)
                return(false);

            mp_value->BindUBO("light",ubo_light);
            mp_value->BindUBO("phong",ubo_phong);

            mp_value->Update();
        }
        
        {
            MaterialParameters *mp_global=material_instance->GetMP(DescriptorSetType::Global);
        
            if(!mp_global)
                return(false);

            if(!mp_global->BindUBO("g_camera",GetCameraInfoBuffer()))return(false);

            mp_global->Update();
        }
        
        {
            MaterialParameters *mp_global=axis_mi->GetMP(DescriptorSetType::Global);
        
            if(!mp_global)
                return(false);

            if(!mp_global->BindUBO("g_camera",GetCameraInfoBuffer()))return(false);

            mp_global->Update();
        }

        return(true);
    }
    
    void Add(Renderable *r,Pipeline *pl)
    {
        auto ri=db->CreateRenderableInstance(r,material_instance,pl);

        render_root.CreateSubNode(ri);
    }

    void Add(Renderable *r,Pipeline *pl,const Matrix4f &mat)
    {
        auto ri=db->CreateRenderableInstance(r,material_instance,pl);

        render_root.CreateSubNode(mat,ri);
    }

    bool InitScene()
    {
        render_root.CreateSubNode(db->CreateRenderableInstance(ro_axis,axis_mi,axis_pipeline));

        Add(ro_torus    ,pipeline_solid);
        Add(ro_cube     ,pipeline_solid,translate(-10,  0, 5)*scale(10,10,10));
        Add(ro_sphere   ,pipeline_solid,translate( 10,  0, 5)*scale(10,10,10));
        Add(ro_cylinder ,pipeline_solid,translate(  0, 16, 0));
        Add(ro_cone     ,pipeline_solid,translate(  0,-16, 0));

        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);

        return(true);
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_list);
    }

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);
            
        render_list=new RenderList(device);
            
        if(!InitMaterial())
            return(false);

        if(!InitUBO())
            return(false);

        CreateRenderObject();

        if(!InitScene())
            return(false);

        return(true);
    }
    
    void BuildCommandBuffer(uint32_t index) override
    {
        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);
        
        VulkanApplicationFramework::BuildCommandBuffer(index,render_list);
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
