#include<hgl/graph/SceneNode.h>
#include<hgl/component/SceneComponent.h>
#include<hgl/graph/mesh/SubMesh.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/SceneRenderer.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/log/Log.h>

DEFINE_LOGGER_MODULE(SceneNode)

namespace hgl::graph
{
    RenderContext *SceneNode::GetRenderContext()const
    {
        if(!main_scene)
            return(nullptr);

        io::EventDispatcher *ep=main_scene->GetEventDispatcher()->GetParent();

        if(!ep)
        {
            //有可能的，没有加入任保何SceneRenderer的场景
            MLogWarning(SceneNode,"SceneNode::GetRenderContext(): Scene has no parent EventDispatcher!");
            return(nullptr);
        }

        SceneEventDispatcher *sep=dynamic_cast<SceneEventDispatcher *>(ep);

        if(!sep)
        {
            //这明显不对好不好
            MLogFatal(SceneNode,"SceneNode::GetRenderContext(): Scene's parent EventDispatcher is not a SceneEventDispatcher!");
            return(nullptr);
        }

        return sep->GetRenderContext();
    }

    RenderFramework *SceneNode::GetRenderFramework()const
    {
        return main_scene?main_scene->GetRenderFramework():nullptr;
    }

    //void SceneNode::SetRenderable(SubMesh *ri)
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

    SceneNode *SceneNode::CreateNode(Scene *scene)const
    {
        if(scene)
        {
            return scene->CreateNode<SceneNode>();
        }
        else if(this&&this->GetScene())
        {
            return this->GetScene()->CreateNode<SceneNode>();
        }

        return(nullptr);
    }

    SceneNode *SceneNode::Clone(Scene *scene) const                                                               ///<复制一个场景节点
    {
        if(!this && !scene)
            return nullptr;

        if(!scene)
        {
            scene=this->GetScene();

            if(!scene)
            {
                MLogError(SceneNode,"SceneNode::Clone(): No scene specified and this node has no scene!");
                return(nullptr);
            }
        }

        SceneNode *node = this->CreateNode(scene);

        if(!node)
        {
            MLogError(SceneNode,"SceneNode::Clone(): CreateNode() failed!");

            return(nullptr);
        }

        node->SetTransformState(GetTransformState());  //复制本地矩阵

        CloneChildren(node);    //复制子节点
        CloneComponents(node);    //复制组件

        return node;
    }

    void SceneNode::Reset()
    {
        SetParent(nullptr); //清除父节点

        local_bounding_box.Clear();
        world_bounding_box.Clear();

        child_nodes.Clear();
        component_set.Clear();

        NodeTransform::Reset();
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
                main_scene->GetEventDispatcher()->RemoveChildDispatcher(self_ep);    //从旧的场景中移除事件分发器
            }

            if(new_scene)
            {
                new_scene->GetEventDispatcher()->AddChildDispatcher(self_ep);        //添加到新的场景中
            }
        }

        main_scene=new_scene;
    }
}//namespace hgl::graph
