// 5.LoadModel
//  加载纯色无贴图模型

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/VertexBufferData.h>

#include<hgl/graph/data/SceneNodeData.h>

namespace hgl
{
    namespace graph
    {
        ModelData *LoadModelFromFile(const OSString &filename);
    }
}

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

vulkan::Renderable *CreateMeshRenderable(SceneDB *db,vulkan::Material *mtl,const MeshData *mesh)
{
    const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

    uint draw_count=mesh->indices_count;

    vulkan::Renderable *render_obj=nullptr;
    {
        const int vertex_binding=vsm->GetStageInputBinding("Vertex");

        if(vertex_binding==-1)
            return(nullptr);

        vulkan::VAB *vbo=db->CreateVAB(FMT_RGB32F,mesh->vertex_count,mesh->position);

        render_obj=mtl->CreateRenderable();
        render_obj->Set(vertex_binding,vbo);
        render_obj->SetBoundingBox(mesh->bounding_box);
    }

    const int normal_binding=vsm->GetStageInputBinding("Normal");

    if(normal_binding!=-1)
    {
        vulkan::VAB *vbo=db->CreateVAB(FMT_RGB32F,mesh->vertex_count,mesh->normal);

        render_obj->Set(normal_binding,vbo);
    }

    const int tagent_binding=vsm->GetStageInputBinding("Tangent");

    if(tagent_binding!=-1)
    {
        vulkan::VAB *vbo=db->CreateVAB(FMT_RGB32F,mesh->vertex_count,mesh->tangent);

        render_obj->Set(tagent_binding,vbo);
    }

    const int bitagent_binding=vsm->GetStageInputBinding("Bitangent");

    if(bitagent_binding!=-1)
    {
        vulkan::VAB *vbo=db->CreateVAB(FMT_RGB32F,mesh->vertex_count,mesh->bitangent);

        render_obj->Set(bitagent_binding,vbo);
    }

    if(mesh->vertex_count<=0xFFFF)
        render_obj->Set(db->CreateIBO16(mesh->indices_count,mesh->indices16));
    else
        render_obj->Set(db->CreateIBO32(mesh->indices_count,mesh->indices32));

    db->Add(render_obj);
    return(render_obj);
}

class TestApp:public CameraAppFramework
{
    struct
    {
        Vector4f color;
        float metallic;
        float roughness;
    }pbr_material;

    Vector3f sun_direction;

    vulkan::Buffer *            ubo_pbr_material    =nullptr;
    vulkan::Buffer *            ubo_sun             =nullptr;

private:

    struct MP
    {
        vulkan::Material *          material            =nullptr;
        vulkan::MaterialInstance *  material_instance   =nullptr;
        vulkan::Pipeline *          pipeline            =nullptr;
    }mp_line,mp_solid;

private:

    SceneNode   render_root;
    RenderList  render_list;

    ModelData *model_data;

    vulkan::Renderable **mesh_renderable;
    RenderableInstance **mesh_renderable_instance;

    vulkan::Renderable *axis_renderable;
    RenderableInstance *axis_renderable_instance;

    vulkan::Renderable *bbox_renderable;
    RenderableInstance *bbox_renderable_instance;

private:

    bool InitPipeline(MP *mp,VkPrimitiveTopology primitive)
    {
        AutoDelete<vulkan::PipelineCreater> 
        pipeline_creater=new vulkan::PipelineCreater(device,mp->material,sc_render_target);
        pipeline_creater->SetDepthTest(true);
        pipeline_creater->SetDepthWrite(true);
        pipeline_creater->SetPolygonMode(VK_POLYGON_MODE_FILL);
        pipeline_creater->CloseCullFace();
        pipeline_creater->Set(primitive);

        mp->pipeline=pipeline_creater->Create();

        if(!mp->pipeline)
            return(false);

        db->Add(mp->pipeline);
        return(true);
    }

    bool InitMaterial(MP *mp,const OSString &vs_file,const OSString &fs_file)
    {
        mp->material=shader_manage->CreateMaterial(vs_file,fs_file);
        if(!mp->material)
            return(false);

        mp->material_instance=mp->material->CreateInstance();

        db->Add(mp->material);
        db->Add(mp->material_instance);
        return(true);
    }

    bool InitMP(MP *mp,VkPrimitiveTopology primitive,const OSString &vs_file,const OSString &fs_file)
    {
        if(!InitMaterial(mp,vs_file,fs_file))
            return(false);

        if(!InitPipeline(mp,primitive))
            return(false);

        return(true);
    }

    RenderableInstance *CreateRenderableInstance(const MP &mp,vulkan::Renderable *r)
    {
        return db->CreateRenderableInstance(mp.pipeline,mp.material_instance,r);
    }

    void CreateRenderObject()
    {
        const uint count=model_data->mesh_list.GetCount();
        MeshData **md=model_data->mesh_list.GetData();

        mesh_renderable         =new vulkan::Renderable *[count];
        mesh_renderable_instance=new RenderableInstance *[count];

        for(uint i=0;i<count;i++)
        {
            mesh_renderable[i]=CreateMeshRenderable(db,mp_solid.material,*md);
            mesh_renderable_instance[i]=CreateRenderableInstance(mp_solid,mesh_renderable[i]);
            ++md;
        }

        {
            AxisCreateInfo aci;//(model_data->bounding_box);

            aci.size.Set(1000,1000,1000);

            axis_renderable=CreateRenderableAxis(db,mp_line.material,&aci);
            axis_renderable_instance=CreateRenderableInstance(mp_line,axis_renderable);
        }

        {
            CubeCreateInfo cci(model_data->bounding_box);

            cci.has_color=true;
            cci.color.Set(1,1,1,1);

            bbox_renderable=CreateRenderableBoundingBox(db,mp_line.material,&cci);
            bbox_renderable_instance=CreateRenderableInstance(mp_line,bbox_renderable);
        }
    }

    bool InitUBO()
    {
        LCG lcg;

        pbr_material.color=Vector4f(1,1,1,1.0);
        pbr_material.metallic=0.5;
        pbr_material.roughness=0.5;

        ubo_pbr_material=device->CreateUBO(sizeof(pbr_material),&pbr_material);

        sun_direction=Vector3f::RandomDir(lcg);
        ubo_sun=device->CreateUBO(sizeof(sun_direction),&sun_direction);

        mp_line.material_instance->BindUBO("world",GetCameraMatrixBuffer());
        mp_line.material_instance->Update();

        mp_solid.material_instance->BindUBO("world",GetCameraMatrixBuffer());
        mp_solid.material_instance->BindUBO("fs_world",GetCameraMatrixBuffer());

        mp_solid.material_instance->BindUBO("pbr_material",ubo_pbr_material);
        mp_solid.material_instance->BindUBO("sun",ubo_sun);
        mp_solid.material_instance->Update();

        db->Add(ubo_pbr_material);
        db->Add(ubo_sun);
        return(true);
    }

    void CreateSceneNode(SceneNode *scene_node,SceneNodeData *node_data)
    {
        scene_node->SetLocalMatrix(node_data->local_matrix);

        {
            const uint count=node_data->mesh_count;
            const uint32 *mesh_index=node_data->mesh_index;

            for(uint i=0;i<count;i++)
            {
                scene_node->Add(mesh_renderable_instance[*mesh_index]);

                ++mesh_index;
            }
        }

        {
            const uint count=node_data->sub_nodes.GetCount();
            SceneNodeData **sub_model_node=node_data->sub_nodes.GetData();

            for(uint i=0;i<count;i++)
            {
                CreateSceneNode(scene_node->CreateSubNode(),*sub_model_node);

                ++sub_model_node;
            }
        }
    }

public:

    bool Init(ModelData *md)
    {
        if(!md)
            return(false);

        model_data=md;

        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))//,model_data->bounding_box))
            return(false);
        
        if(!InitMP(&mp_solid, PRIM_TRIANGLES,   OS_TEXT("res/shader/pbr_Light.vert"),
                                                OS_TEXT("res/shader/pbr_DirectionLight.frag")))
            return(false);

        if(!InitMP(&mp_line,  PRIM_LINES,       OS_TEXT("res/shader/PositionColor3D.vert"),
                                                OS_TEXT("res/shader/VertexColor.frag")))
            return(false);

        if(!InitUBO())
            return(false);

        CreateRenderObject();

        render_root.Add(axis_renderable_instance);
        render_root.Add(bbox_renderable_instance);
        
        CreateSceneNode(render_root.CreateSubNode(),model_data->root_node);

        render_root.RefreshMatrix();
        render_list.Clear();
        render_root.ExpendToList(&render_list);

        return(true);
    }
    
    void BuildCommandBuffer(uint32 index)
    {
        VulkanApplicationFramework::BuildCommandBuffer(index,&render_list);
    }
};//class TestApp:public ViewModelFramework

int os_main(const int argc,const os_char **argv)
{
    TestApp app;

    ModelData *model_data=LoadModelFromFile(argv[1]);

    if(!model_data)
        return -1;

    if(app.Init(model_data))
        while(app.Run());

    if(model_data)
        delete model_data;

    return 0;
}
