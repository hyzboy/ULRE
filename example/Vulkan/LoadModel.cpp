// 5.LoadModel
//  加载纯色无贴图模型

#include"ViewModelFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/RenderableInstance.h>

#include"AssimpLoaderMesh.h"
#include<hgl/graph/VertexBuffer.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

vulkan::Renderable *CreateMeshRenderable(SceneDB *db,vulkan::Material *mtl,const MeshData *mesh)
{
    const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

    uint draw_count=0;

    if(mesh->indices16.GetCount()>0)
        draw_count=mesh->indices16.GetCount();
    else
        draw_count=mesh->indices32.GetCount();

    vulkan::Renderable *render_obj=nullptr;
    {
        const int vertex_binding=vsm->GetStageInputBinding("Vertex");

        if(vertex_binding==-1)
            return(nullptr);

        vulkan::VertexBuffer *vbo=db->CreateVBO(FMT_RGB32F,mesh->position.GetCount(),mesh->position.GetData());

        render_obj=mtl->CreateRenderable();
        render_obj->Set(vertex_binding,vbo);
        render_obj->SetBoundingBox(mesh->bounding_box);
    }

    const int normal_binding=vsm->GetStageInputBinding("Normal");

    if(normal_binding!=-1)
    {
        vulkan::VertexBuffer *vbo=db->CreateVBO(FMT_RGB32F,mesh->normal.GetCount(),mesh->normal.GetData());

        render_obj->Set(normal_binding,vbo);
    }

    const int tagent_binding=vsm->GetStageInputBinding("Tangent");

    if(tagent_binding!=-1)
    {
        vulkan::VertexBuffer *vbo=db->CreateVBO(FMT_RGB32F,mesh->tangent.GetCount(),mesh->tangent.GetData());

        render_obj->Set(tagent_binding,vbo);
    }

    const int bitagent_binding=vsm->GetStageInputBinding("Bitangent");

    if(bitagent_binding!=-1)
    {
        vulkan::VertexBuffer *vbo=db->CreateVBO(FMT_RGB32F,mesh->bitangent.GetCount(),mesh->bitangent.GetData());

        render_obj->Set(bitagent_binding,vbo);
    }

    if(mesh->indices16.GetCount()>0)
        render_obj->Set(db->CreateIBO16(mesh->indices16.GetCount(),mesh->indices16.GetData()));
    else
        render_obj->Set(db->CreateIBO32(mesh->indices32.GetCount(),mesh->indices32.GetData()));

    db->Add(render_obj);
    return(render_obj);
}

class TestApp:public ViewModelFramework
{
private:

    vulkan::Material *          material            =nullptr;
    vulkan::DescriptorSets *    descriptor_sets     =nullptr;

    vulkan::Pipeline *          pipeline_line       =nullptr;

private:

    ModelData *model_data;

    vulkan::Renderable **mesh_renderable;
    RenderableInstance **mesh_renderable_instance;

public:

    ~TestApp()
    {
        delete[] mesh_renderable_instance;
        delete[] mesh_renderable;
    }

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("OnlyPosition3D.vert.spv"),
                                               OS_TEXT("FlatColor.frag.spv"));
        if(!material)
            return(false);

        descriptor_sets=material->CreateDescriptorSets();

        db->Add(material);
        db->Add(descriptor_sets);
        return(true);
    }

    void CreateRenderObject()
    {
        const uint count=model_data->mesh_data.GetCount();
        MeshData **md=model_data->mesh_data.GetData();

        mesh_renderable         =new vulkan::Renderable *[count];
        mesh_renderable_instance=new RenderableInstance *[count];

        for(uint i=0;i<count;i++)
        {
            mesh_renderable[i]=CreateMeshRenderable(db,material,*md);
            mesh_renderable_instance[i]=db->CreateRenderableInstance(pipeline_line,descriptor_sets,mesh_renderable[i]);
            ++md;
        }
    }

    bool InitUBO()
    {
        if(!InitCameraUBO(descriptor_sets,material->GetUBO("world")))
            return(false);

        descriptor_sets->Update();
        return(true);
    }

    bool InitPipeline()
    {
        constexpr os_char PIPELINE_FILENAME[]=OS_TEXT("2DSolid.pipeline");

        {
            vulkan::PipelineCreater *pipeline_creater=new vulkan::PipelineCreater(device,material,device->GetRenderPass(),device->GetExtent());
            pipeline_creater->SetDepthTest(false);
            pipeline_creater->SetDepthWrite(false);
            pipeline_creater->SetPolygonMode(VK_POLYGON_MODE_LINE);
            pipeline_creater->CloseCullFace();
            pipeline_creater->Set(PRIM_TRIANGLES);

            pipeline_line=pipeline_creater->Create();
            if(!pipeline_line)
                return(false);

            db->Add(pipeline_line);

            delete pipeline_creater;
        }

        return pipeline_line;
    }

    void CreateSceneNode(SceneNode *scene_node,ModelSceneNode *model_node)
    {
        scene_node->SetLocalMatrix(model_node->local_matrix);

        {
            const uint count=model_node->mesh_index.GetCount();
            const uint32 *mesh_index=model_node->mesh_index.GetData();

            for(uint i=0;i<count;i++)
            {
                scene_node->Add(mesh_renderable_instance[*mesh_index]);

                ++mesh_index;
            }
        }

        {
            const uint count=model_node->children_node.GetCount();
            ModelSceneNode **sub_model_node=model_node->children_node.GetData();

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
        
        CreateSceneNode(&render_root,model_data->root_node);

        return(true);
    }
};//class TestApp:public ViewModelFramework

#ifdef _WIN32
int wmain(int argc,wchar_t **argv)
#else
int main(int argc,char **argv)
#endif//
{
    TestApp app;

    ModelData *model_data=AssimpLoadModel(argv[1]);

    if(!model_data)
        return -1;

    if(app.Init(model_data))
        while(app.Run());

    if(model_data)
        delete model_data;

    return 0;
}
