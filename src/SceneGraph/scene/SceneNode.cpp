#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKRenderable.h>
namespace hgl
{
    namespace graph
    {
        SceneNode *Duplication(SceneNode *src_node)
        {
            if(!src_node)
                return nullptr;

            SceneNode *node=new SceneNode(*(SceneOrient *)src_node);

            node->SetRenderable(src_node->GetRenderable());

            for(SceneNode *sn:src_node->GetChildNode())
            {
                node->Add(Duplication(sn));
            }

            return node;
        }

        void SceneNode::SetRenderable(Renderable *ri)
        {
            render_obj=ri;

            if(render_obj)
            {
                SetBoundingBox(render_obj->GetBoundingBox());
            }
            else
            {
                BoundingBox.SetZero();

                //WorldBoundingBox=
                    LocalBoundingBox=BoundingBox;
            }
        }

        /**
        * 刷新矩阵变换
        */
        void SceneNode::RefreshMatrix()
        {
            SceneOrient::RefreshMatrix();

//            if (scene_matrix.IsNewestVersion())       //自己不变，不代表下面不变
                //return;

            const Matrix4f &l2w=scene_matrix.GetLocalToWorldMatrix();

            const int count=ChildNode.GetCount();

            SceneNode **sub=ChildNode.GetData();

            for(int i=0;i<count;i++)
            {
                (*sub)->SetParentMatrix(l2w);
                (*sub)->RefreshMatrix();

                sub++;
            }
        }

        /**
        * 刷新绑定盒
        */
        void SceneNode::RefreshBoundingBox()
        {
            int count=ChildNode.GetCount();
            SceneNode **sub=ChildNode.GetData();

            AABB local,world;

            (*sub)->RefreshBoundingBox();
            local=(*sub)->GetLocalBoundingBox();

            ++sub;
            for(int i=1;i<count;i++)
            {
                (*sub)->RefreshBoundingBox();

                local.Enclose((*sub)->GetLocalBoundingBox());

                ++sub;
            }

            LocalBoundingBox=local;
        }

        int SceneNode::GetComponents(ArrayList<Component *> &comp_list,const ComponentManager *mgr)
        {
            if(!mgr)return(-1);
            if(ComponentIsEmpty())return(0);

            int result=0;

            for(Component *c:ComponentList)
            {
                if(c->GetManager()==mgr)
                {
                    comp_list.Add(c);
                    ++result;
                }
            }
            
            return result;
        }

        bool SceneNode::HasComponent(const ComponentManager *mgr)
        {
            if(!mgr)return(false);
            if(ComponentIsEmpty())return(false);

            for(Component *c:ComponentList)
            {
                if(c->GetManager()==mgr)
                    return(true);
            }

            return(false);
        }
    }//namespace graph
}//namespace hgl
