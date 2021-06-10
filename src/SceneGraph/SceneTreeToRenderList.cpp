#include<hgl/graph/SceneTreeToRenderList.h>
#include<hgl/graph/VKRenderableInstance.h>

namespace hgl
{
    namespace graph
    {    
        SceneTreeToRenderList::SceneTreeToRenderList(GPUDevice *d)
        {
            device=d;
            hgl_zero(camera_info);

            render_node_list=nullptr;
            render_list=nullptr;
        }

        SceneTreeToRenderList::~SceneTreeToRenderList()
        {
            SAFE_CLEAR(render_node_list);
        }
        
        /**
        * 理论上讲，我们需要按以下顺序排序
        *
        * for(material)
        *   for(pipeline)
        *       for(material_instance)
        *           for(vbo)
        *               for(distance)
        */

        int SceneTreeToRenderList::Comp(RenderNode *obj_one,RenderNode *obj_two)
        {
            int off;

            //比较材质
            MaterialInstance *mi1=obj_one->ri->GetMaterialInstance();
            MaterialInstance *mi2=obj_two->ri->GetMaterialInstance();
                       
            off=mi1->GetMaterial()-mi2->GetMaterial();

            if(off)
                return off;
            
            //比较管线
            Pipeline *p1=obj_one->ri->GetPipeline();
            Pipeline *p2=obj_two->ri->GetPipeline();

            off=p1-p2;

            if(off)
                return off;

            //比较材质实例
            off=mi1-mi2;

            if(off)
                return off;
            
            //比较vbo+ebo
            off=obj_one->ri->GetBufferHash()
               -obj_two->ri->GetBufferHash();

            if(off)
                return off;

            //比较距离
            return( obj_one->distance_to_camera_square-
                    obj_two->distance_to_camera_square);
        }

        //bool SceneTreeToRenderList::InFrustum(const SceneNode *,void *)
        //{
        //    return(true);
        //}

        bool SceneTreeToRenderList::Begin()
        {
            if(!render_node_list)
                render_node_list=new RenderNodeList;

            render_node_list->ClearData();

            pipeline_sets.ClearData();
            material_sets.ClearData();
            mat_inst_sets.ClearData();

            return(true);
        }

        bool SceneTreeToRenderList::End()
        {

        }

        bool SceneTreeToRenderList::Expend(SceneNode *sn)
        {
            if(!sn)return(false);

            if(sn->render_obj)
            {
                RenderNode *rn=new RenderNode;

                rn->WorldCenter=sn->GetWorldCenter();

                rn->distance_to_camera_square=length_squared(rn->WorldCenter,camera_info.pos);
//                rn->distance_to_camera=sqrtf(rn->distance_to_camera_square);

                rn->ri=sn->render_obj;

                render_node_list->Add(rn);
            }

            for(SceneNode *sub:sn->SubNode)
                Expend(sub);

            return(true);
        }

        bool SceneTreeToRenderList::Expend(RenderList *rl,const CameraInfo &ci,SceneNode *sn)
        {
            if(!device)return(false);
            if(!rl||sn)return(false);

            camera_info=ci;

            //Frustum f;

            render_list=rl;

            Begin();
            Expend(sn);
            End();

            return(true);
        }
    }//namespace graph
}//namespace hgl
