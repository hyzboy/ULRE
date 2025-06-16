#include<hgl/graph/SceneNode.h>
#include<hgl/component/SceneComponent.h>
#include<hgl/graph/Mesh.h>

namespace hgl::graph
{
    SceneNode *Duplication(SceneNode *src_node)
    {
        if(!src_node)
            return nullptr;

        SceneNode *node=new SceneNode(*(SceneOrient *)src_node);

        for(SceneNode *sn:src_node->GetChildNode())
        {
            node->Add(Duplication(sn));
        }

        for(Component *c:src_node->GetComponents())
        {
            node->AttachComponent(c->Duplication());
        }

        return node;
    }

    //void SceneNode::SetRenderable(Mesh *ri)
    //{
    //    render_obj=ri;

    //    if(render_obj)
    //    {
    //        SetBoundingBox(render_obj->GetBoundingBox());
    //    }
    //    else
    //    {
    //        bounding_box.SetZero();

    //        //WorldBoundingBox=
    //            local_bounding_box=bounding_box;
    //    }
    //}

    /**
    * 刷新矩阵变换
    */
    void SceneNode::RefreshMatrix()
    {
        SceneOrient::RefreshMatrix();

//            if (scene_matrix.IsNewestVersion())       //自己不变，不代表下面不变
            //return;

        const Matrix4f &l2w=scene_matrix.GetLocalToWorldMatrix();

        for(SceneNode *sub:child_nodes)
        {
            sub->SetParentMatrix(l2w);
            sub->RefreshMatrix();
        }

        for(Component *com:component_set)
        {
            SceneComponent *sc=dynamic_cast<SceneComponent *>(com);

            if(!sc)
                continue;

            sc->SetParentMatrix(l2w);
            sc->RefreshMatrix();
        }
    }

    /**
    * 刷新绑定盒
    */
    void SceneNode::RefreshBoundingBox()
    {
        int count=child_nodes.GetCount();
        SceneNode **sub=child_nodes.GetData();

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

        local_bounding_box=local;
    }

    int SceneNode::GetComponents(ComponentList &comp_list,const ComponentManager *mgr)
    {
        if(!mgr)return(-1);
        if(ComponentIsEmpty())return(0);

        int result=0;

        for(Component *c:component_set)
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

        for(Component *c:component_set)
        {
            if(c->GetManager()==mgr)
                return(true);
        }

        return(false);
    }

    SceneNode::~SceneNode()
    {
        for(Component *c:component_set)
        {
            c->OnDetach(this);
        }
    }
}//namespace hgl::graph
