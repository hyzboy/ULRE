// 5.LoadModel
//  加载纯色无贴图模型

#include"ViewModelFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/VertexBuffer.h>

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

        vulkan::VertexBuffer *vbo=db->CreateVBO(FMT_RGB32F,mesh->vertex_count,mesh->position);

        render_obj=mtl->CreateRenderable();
        render_obj->Set(vertex_binding,vbo);
        render_obj->SetBoundingBox(mesh->bounding_box);
    }

    const int normal_binding=vsm->GetStageInputBinding("Normal");

    if(normal_binding!=-1)
    {
        vulkan::VertexBuffer *vbo=db->CreateVBO(FMT_RGB32F,mesh->vertex_count,mesh->normal);

        render_obj->Set(normal_binding,vbo);
    }

    const int tagent_binding=vsm->GetStageInputBinding("Tangent");

    if(tagent_binding!=-1)
    {
        vulkan::VertexBuffer *vbo=db->CreateVBO(FMT_RGB32F,mesh->vertex_count,mesh->tangent);

        render_obj->Set(tagent_binding,vbo);
    }

    const int bitagent_binding=vsm->GetStageInputBinding("Bitangent");

    if(bitagent_binding!=-1)
    {
        vulkan::VertexBuffer *vbo=db->CreateVBO(FMT_RGB32F,mesh->vertex_count,mesh->bitangent);

        render_obj->Set(bitagent_binding,vbo);
    }

    if(mesh->vertex_count<=0xFFFF)
        render_obj->Set(db->CreateIBO16(mesh->indices_count,mesh->indices16));
    else
        render_obj->Set(db->CreateIBO32(mesh->indices_count,mesh->indices32));

    db->Add(render_obj);
    return(render_obj);
}

class TestApp:public ViewModelFramework
{
    struct
    {
        Vector4f color;
        Vector4f abiment;
    }color_material;

    Vector3f sun_direction;

private:

    vulkan::Material *          material            =nullptr;
    vulkan::MaterialInstance *  material_instance   =nullptr;

    vulkan::Buffer *            ubo_color           =nullptr;
    vulkan::Buffer *            ubo_sun             =nullptr;

    vulkan::Pipeline *          pipeline_solid      =nullptr;
    vulkan::Pipeline *          pipeline_lines      =nullptr;

private:

    ModelData *model_data;

    vulkan::Renderable **mesh_renderable;
    RenderableInstance **mesh_renderable_instance;

    vulkan::Renderable *axis_renderable;
    RenderableInstance *axis_renderable_instance;

    vulkan::Renderable *bbox_renderable;
    RenderableInstance *bbox_renderable_instance;

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("res/shader/LightPosition3D.vert"),
                                               OS_TEXT("res/shader/VertexColor.frag"));
        if(!material)
            return(false);

        material_instance=material->CreateInstance();

        db->Add(material);
        db->Add(material_instance);
        return(true);
    }

    void CreateRenderObject()
    {
        const uint count=model_data->mesh_list.GetCount();
        MeshData **md=model_data->mesh_list.GetData();

        mesh_renderable         =new vulkan::Renderable *[count];
        mesh_renderable_instance=new RenderableInstance *[count];

        for(uint i=0;i<count;i++)
        {
            mesh_renderable[i]=CreateMeshRenderable(db,material,*md);
            mesh_renderable_instance[i]=db->CreateRenderableInstance(pipeline_solid,material_instance,mesh_renderable[i]);
            ++md;
        }

        {
            AxisCreateInfo aci(model_data->bounding_box);

            axis_renderable=CreateRenderableAxis(db,material,&aci);
            axis_renderable_instance=db->CreateRenderableInstance(pipeline_lines,material_instance,axis_renderable);
        }

        {
            CubeCreateInfo cci(model_data->bounding_box);

            bbox_renderable=CreateRenderableBoundingBox(db,material,&cci);
            bbox_renderable_instance=db->CreateRenderableInstance(pipeline_lines,material_instance,bbox_renderable);
        }
    }

    bool InitUBO()
    {
        LCG lcg;

        color_material.color=Vector4f(1,1,1,1.0);
        color_material.abiment.Set(0.25,0.25,0.25,1.0);
        ubo_color=device->CreateUBO(sizeof(color_material),&color_material);

        sun_direction=Vector3f::RandomDir(lcg);
        ubo_sun=device->CreateUBO(sizeof(sun_direction),&sun_direction);

        material_instance->BindUBO("world",GetCameraMatrixBuffer());
        material_instance->BindUBO("color_material",ubo_color);
        material_instance->BindUBO("sun",ubo_sun);
        material_instance->Update();

        db->Add(ubo_color);
        db->Add(ubo_sun);
        return(true);
    }

    bool InitPipeline()
    {
        AutoDelete<vulkan::PipelineCreater> 
        pipeline_creater=new vulkan::PipelineCreater(device,material,sc_render_target);
        pipeline_creater->SetDepthTest(false);
        pipeline_creater->SetDepthWrite(false);
        pipeline_creater->SetPolygonMode(VK_POLYGON_MODE_FILL);
        pipeline_creater->CloseCullFace();
        pipeline_creater->Set(PRIM_TRIANGLES);

        pipeline_solid=pipeline_creater->Create();

        if(!pipeline_solid)
            return(false);

        db->Add(pipeline_solid);

        pipeline_creater->Set(PRIM_LINES);

        pipeline_lines=pipeline_creater->Create();

        if(!pipeline_lines)
            return(false);

        db->Add(pipeline_lines);
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

        if(!ViewModelFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT,model_data->bounding_box))
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitUBO())
            return(false);

        if(!InitPipeline())
            return(false);

        CreateRenderObject();

        //render_root.Add(axis_renderable_instance);
        //render_root.Add(bbox_renderable_instance);

        CreateSceneNode(render_root.CreateSubNode(),model_data->root_node);

        return(true);
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
