#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKRenderable.h>
namespace hgl
{
    namespace graph
    {
        SceneNode::SceneNode(SceneNode *node):SceneOrient(*node)
        {
            if(!node)
                return;

            BoundingBox=node->BoundingBox;
            LocalBoundingBox=node->LocalBoundingBox;

            render_obj=node->render_obj;

            for(SceneNode *sn:node->SubNode)
            {
                SceneNode *new_sn=new SceneNode(sn);

                SubNode.Add(new_sn);
            }
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
        * 刷新变换
        * @param parent_transform 上级节点变换
        */
        bool SceneNode::RefreshTransform(const Transform &parent_transform)
        {
            if(!parent_transform.IsLastVersion())
                return(false);

            if(!parent_transform.IsIdentity())
                SceneOrient::RefreshTransform(parent_transform);
            else
                SetWorldTransform(LocalTransform);

            const int count=SubNode.GetCount();

            SceneNode **sub=SubNode.GetData();

            for(int i=0;i<count;i++)
            {
                if(!(*sub)->RefreshTransform(WorldTransform))
                    return(false);

                sub++;
            }

            return(true);
        }

        /**
        * 刷新绑定盒
        */
        void SceneNode::RefreshBoundingBox()
        {
            int count=SubNode.GetCount();
            SceneNode **sub=SubNode.GetData();

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

        ///**
        //* 从当前节点展开输出到一个渲染列表
        //* @param rl 渲染列表
        //* @param func 过滤函数
        //* @param func_data 过滤函数用辅助数据
        //* @return 成功与否
        //*/
        //bool SceneNode::ExpendToList(RenderList *rl,FilterSceneNodeFunc func,void *func_data)
        //{
        //    if(!rl)return(false);

        //    if(func)
        //        if(!func(this,func_data))
        //            return(false);

        //    {
        //        int count=renderable_instances.GetCount();

        //        if(count>0)
        //            rl->Add(this);
        //    }

        //    {
        //        int count=SubNode.GetCount();
        //        SceneNode **sub=SubNode.GetData();

        //        for(int i=0;i<count;i++)
        //        {
        //            (*sub)->ExpendToList(rl,func,func_data);                //展开子节点

        //            ++sub;
        //        }
        //    }

        //    return(true);
        //}

        ///**
        //* 从当前节点展开输出到一个渲染列表
        //* @param rl 渲染列表
        //* @param cam 摄像机
        //* @param comp_func 渲染列表远近比较函数
        //*/
        //bool SceneNode::ExpendToList(RenderList *rl,Camera *cam,RenderListCompFunc comp_func)
        //{
        //    if(!rl||!cam)return(false);

        //    if(!ExpendToList(rl))
        //        return(false);

        //    if(comp_func)
        //    {
        //    }

        //    return(true);
        //}
    }//namespace graph
}//namespace hgl
