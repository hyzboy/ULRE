// 5.LoadModel
//  加载纯色无贴图模型

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/SceneDB.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/RenderList.h>

#include"AssimpLoaderMesh.h"
#include<hgl/graph/VertexBuffer.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

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

    return(render_obj);
}

class TestApp:public VulkanApplicationFramework
{
private:

    uint swap_chain_count=0;

    SceneDB *   db                  =nullptr;
    SceneNode   render_root;
    RenderList  render_list;

    Camera      camera;

    vulkan::Material *          material            =nullptr;
    vulkan::DescriptorSets *    descriptor_sets     =nullptr;

    vulkan::Buffer *            ubo_world_matrix    =nullptr;

    vulkan::Pipeline *          pipeline_line       =nullptr;
    vulkan::CommandBuffer **    cmd_buf             =nullptr;

private:

    ModelData *model_data;

    vulkan::Renderable **mesh_renderable;

public:

    ~TestApp()
    {
        SAFE_CLEAR(db);

        SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);
    }

private:

    void InitCamera()
    {
        math::vec center_point=model_data->bounding_box.CenterPoint();
        math::vec min_point=model_data->bounding_box.minPoint;
        math::vec max_point=model_data->bounding_box.maxPoint;

        camera.type=CameraType::Perspective;
        camera.center=center_point.xyz();
        camera.eye=max_point.xyz();
        //camera.center.Set(0,0,0);
        //camera.eye.Set(10,10,5);
        camera.up_vector.Set(0,0,1);
        camera.forward_vector.Set(0,1,0);
        camera.znear=4;
        camera.zfar=1000;
        camera.fov=90;
        camera.width=SCREEN_WIDTH;
        camera.height=SCREEN_HEIGHT;

        camera.Refresh();      //更新矩阵计算
    }

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

        mesh_renderable=new vulkan::Renderable *[count];

        for(uint i=0;i<count;i++)
        {
            mesh_renderable[i]=CreateMeshRenderable(db,material,*md);
            ++md;
        }
    }

    bool InitUBO()
    {
        const VkExtent2D extent=device->GetExtent();

        ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&camera.matrix);

        if(!ubo_world_matrix)
            return(false);

        if(!descriptor_sets->BindUBO(material->GetUBO("world"),*ubo_world_matrix))
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

    void CreateRenderableInstance(SceneNode *scene_node,ModelSceneNode *model_node)
    {
        scene_node->SetLocalMatrix(model_node->local_matrix);

        {
            const uint count=model_node->mesh_index.GetCount();
            const uint32 *mesh_index=model_node->mesh_index.GetData();

            for(uint i=0;i<count;i++)
            {
                scene_node->Add(db->CreateRenderableInstance(pipeline_line,descriptor_sets,mesh_renderable[*mesh_index]));

                ++mesh_index;
            }
        }

        {
            const uint count=model_node->children_node.GetCount();
            ModelSceneNode **sub_model_node=model_node->children_node.GetData();

            for(uint i=0;i<count;i++)
            {
                CreateRenderableInstance(scene_node->CreateSubNode(),*sub_model_node);

                ++sub_model_node;
            }
        }
    }

    bool InitScene()
    {
        CreateRenderableInstance(&render_root,model_data->root_node);

        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);

        return(true);
    }

    bool InitCommandBuffer()
    {
        cmd_buf=hgl_zero_new<vulkan::CommandBuffer *>(swap_chain_count);

        for(uint i=0;i<swap_chain_count;i++)
        {
            cmd_buf[i]=device->CreateCommandBuffer();

            if(!cmd_buf[i])
                return(false);

            cmd_buf[i]->Begin();
            cmd_buf[i]->BeginRenderPass(device->GetRenderPass(),device->GetFramebuffer(i));
            render_list.Render(cmd_buf[i]);
            cmd_buf[i]->EndRenderPass();
            cmd_buf[i]->End();
        }

        return(true);
    }

public:

    bool Init(ModelData *md)
    {
        if(!md)
            return(false);

        model_data=md;

        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        swap_chain_count=device->GetSwapChainImageCount();

        db=new SceneDB(device);

        if(!InitMaterial())
            return(false);

        CreateRenderObject();

        InitCamera();

        if(!InitUBO())
            return(false);

        if(!InitPipeline())
            return(false);

        if(!InitScene())
            return(false);

        if(!InitCommandBuffer())
            return(false);

        return(true);
    }

    void Draw() override
    {
        const uint32_t frame_index=device->GetCurrentFrameIndices();

        const vulkan::CommandBuffer *cb=cmd_buf[frame_index];

        Submit(*cb);
    }
};//class TestApp:public VulkanApplicationFramework

#ifdef _WIN32
int wmain(int argc,wchar_t **argv)
#else
int main(int argc,char **argv)
#endif//
{
    TestApp app;

    ModelData *model_data=AssimpLoadModel(argv[1]);

    if(app.Init(model_data))
        while(app.Run());

    if(model_data)
        delete model_data;

    return 0;
}
