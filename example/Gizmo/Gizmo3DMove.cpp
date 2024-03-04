// Gizmo 3D Move

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>

using namespace hgl;
using namespace hgl::graph;

static Color4f white_color(1,1,1,1);
static Color4f yellow_color(1,1,0,1);

class TestApp:public SceneAppFramework
{
    Color4f color;

    DeviceBuffer *ubo_color=nullptr;

private:

    Material *          mtl_vtx_lum         =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;
    Pipeline *          pipeline_vtx_lum    =nullptr;
    Primitive *         prim_plane_grid       =nullptr;

    Material *          mtl_vtx_color       =nullptr;
    MaterialInstance *  mi_line             =nullptr;
    Pipeline *          pipeline_vtx_color  =nullptr;
    Primitive *         ro_line             =nullptr;

private:

    bool InitMaterialAndPipeline()
    {
        {
            mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"VertexLuminance3D",Prim::Lines);

            cfg.local_to_world=true;

            mtl_vtx_lum=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
            if(!mtl_vtx_lum)return(false);

            mi_plane_grid=db->CreateMaterialInstance(mtl_vtx_lum,nullptr,&white_color);
            if(!mi_plane_grid)return(false);

            pipeline_vtx_lum=CreatePipeline(mtl_vtx_lum,InlinePipeline::Solid3D,Prim::Lines);

            if(!pipeline_vtx_lum)
                return(false);
        }

        {
            mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"VertexColor3D",Prim::Lines);

            cfg.local_to_world=true;

            mtl_vtx_color=db->LoadMaterial("Std3D/VertexColor3D",&cfg);
            if(!mtl_vtx_color)return(false);

            mi_line=db->CreateMaterialInstance(mtl_vtx_color);
            if(!mi_line)return(false);

            {
                const PipelineData *ipd=GetPipelineData(InlinePipeline::Solid3D);

                if(!ipd)
                    return(false);

                PipelineData *pd=new PipelineData(ipd);

                pd->SetLineWidth(2);

                pipeline_vtx_color=CreatePipeline(mtl_vtx_color,pd,Prim::Lines);

                delete pd;

                if(!pipeline_vtx_color)
                    return(false);
            }
        }

        return(true);
    }
    
    Renderable *Add(Primitive *r,MaterialInstance *mi,Pipeline *p)
    {
        Renderable *ri=db->CreateRenderable(r,mi,p);

        if(!ri)
        {
            LOG_ERROR(OS_TEXT("Create Renderable failed."));
            return(nullptr);
        }

        render_root.CreateSubNode(ri);

        return ri;
    }

    /**
     * 写入一个坐标轴的线条数据.
     * 
     * \param pos       要写入数据的指针
     * \param max_line  主线条方向
     * \param oa1       其它轴1方向
     * \param oa2       其它轴2方向
     */
    void WriteAxisPosition(Vector3f *pos,const Vector3f &max_line,const Vector3f &oa1,const Vector3f &oa2)
    {
        constexpr const float AXIS_LENGTH    =4;
        constexpr const float AXIS_MIN_STEP  =1;
        constexpr const float AXIS_ARROW_SIZE=0.25;

        const Vector3f end_pos  =max_line*AXIS_LENGTH;                          ///<最终点位置
        const Vector3f cross_pos=max_line*AXIS_MIN_STEP;                        ///<坐标轴尾部交叉线位置
        const Vector3f arrow_pos=max_line*(AXIS_LENGTH-AXIS_MIN_STEP);          ///<箭头末端在主线上的位置

        //主线
        pos[0]=Vector3f(0,            0, 0);
        pos[1]=end_pos;

        //四根箭头线
        pos[2]=end_pos;
        pos[3]=arrow_pos-oa1*AXIS_ARROW_SIZE;

        pos[4]=end_pos;
        pos[5]=arrow_pos+oa1*AXIS_ARROW_SIZE;

        pos[6]=end_pos;
        pos[7]=arrow_pos-oa2*AXIS_ARROW_SIZE;

        pos[8]=end_pos;
        pos[9]=arrow_pos+oa2*AXIS_ARROW_SIZE;

        //侧边连接其它轴线
        pos[10]=cross_pos;
        pos[11]=cross_pos+oa1*AXIS_MIN_STEP;
        pos[12]=cross_pos;
        pos[13]=cross_pos+oa2*AXIS_MIN_STEP;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        {
            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(32,32);

            pgci.sub_count.Set(8,8);

            pgci.lum=0.5;
            pgci.sub_lum=0.75;

            prim_plane_grid=CreatePlaneGrid(db,mtl_vtx_lum->GetDefaultVIL(),&pgci);
        }

        {
            constexpr const uint AXIS_MAX_LINES     =7;
            constexpr const uint AXIS_MAX_VERTICES  =AXIS_MAX_LINES*2*3;

            ro_line=db->CreatePrimitive("Line",AXIS_MAX_VERTICES);
            if(!ro_line)return(false);

            Vector3f position_data[3][AXIS_MAX_LINES*2];

            WriteAxisPosition(position_data[0],Vector3f(1,0,0),Vector3f(0,1,0),Vector3f(0,0,1));
            WriteAxisPosition(position_data[1],Vector3f(0,1,0),Vector3f(1,0,0),Vector3f(0,0,1));
            WriteAxisPosition(position_data[2],Vector3f(0,0,1),Vector3f(1,0,0),Vector3f(0,1,0));

            Color4f color_data[3][AXIS_MAX_LINES*2];

            for(Color4f &c:color_data[0])c=Color4f(1,0,0,1);
            for(Color4f &c:color_data[1])c=Color4f(0,1,0,1);
            for(Color4f &c:color_data[2])c=Color4f(0,0,1,1);
            
            if(!ro_line->Set(VAN::Position, db->CreateVBO(VF_V3F,AXIS_MAX_VERTICES,position_data)))return(false);
            if(!ro_line->Set(VAN::Color,    db->CreateVBO(VF_V4F,AXIS_MAX_VERTICES,color_data   )))return(false);
        }

        return(true);
    }

    bool InitScene()
    {
        Add(prim_plane_grid,mi_plane_grid,pipeline_vtx_lum);
        Add(ro_line,mi_line,pipeline_vtx_color);

        camera->pos=Vector3f(32,32,32);
        camera_control->SetTarget(Vector3f(0,0,0));
        camera_control->Refresh();

        render_root.RefreshMatrix();
        render_list->Expend(&render_root);

        return(true);
    }

public:

    bool Init(uint w,uint h) override
    {
        if(!SceneAppFramework::Init(w,h))
            return(false);

        if(!InitMaterialAndPipeline())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1280,720);
}
