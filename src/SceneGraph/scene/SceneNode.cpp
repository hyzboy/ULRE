#include<hgl/graph/SceneNode.h>
#include<hgl/component/SceneComponent.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/RenderFramework.h>

namespace hgl::graph
{
    RenderFramework *SceneNode::GetRenderFramework()const
    {
        return main_scene?main_scene->GetRenderFramework():nullptr;
    }

    //void SceneNode::SetRenderable(Mesh *ri)
    //{
    //    render_obj=ri;
    //
    //    if(render_obj)
    //    {
    //        SetBoundingBox(render_obj->GetBoundingBox());
    //    }
    //    else
    //    {
    //        bounding_box.SetZero();
    //
    //        //WorldBoundingBox=
    //            local_bounding_box=bounding_box;
    //    }
    //}

    /**
    * 刷新矩阵变换
    */
    void SceneNode::UpdateWorldTransform()
    {
        NodeTransform::UpdateWorldTransform();

//            if (transform_state.IsNewestVersion())       //自己不变，不代表下面不变
            //return;

        const Matrix4f &l2w=transform_state.GetLocalToWorldMatrix();

        for(SceneNode *sub:child_nodes)
        {
            sub->SetParentMatrix(l2w);
            sub->UpdateWorldTransform();
        }

        for(Component *com:component_set)
        {
            SceneComponent *sc=dynamic_cast<SceneComponent *>(com);

            if(!sc)
                continue;

            sc->SetParentMatrix(l2w);
            sc->UpdateWorldTransform();
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
        SetParent(nullptr);

        for(Component *c:component_set)
        {
            c->OnDetach(this);
        }
    }

    void SceneNode::OnChangeScene(Scene *new_scene)
    {
        if(main_scene==new_scene)
            return;

        auto self_ep=GetEventDispatcher();

        if(self_ep)
        {
            if(main_scene)
            {
                main_scene->GetEventDispatcher().RemoveChildDispatcher(self_ep);    //从旧的场景中移除事件分发器
            }

            if(new_scene)
            {
                new_scene->GetEventDispatcher().AddChildDispatcher(self_ep);        //添加到新的场景中
            }
        }

        main_scene=new_scene;
    }
}//namespace hgl::graph
