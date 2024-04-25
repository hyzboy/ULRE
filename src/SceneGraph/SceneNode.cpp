#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKRenderable.h>
namespace hgl
{
    namespace graph
    {
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
        * 刷新矩阵
        * @param root_matrix 根矩阵
        */
        void SceneNode::RefreshMatrix(const Matrix4f *root_matrix)
        {
            if(root_matrix)
                RefreshLocalToWorldMatrix(root_matrix);
            else
                SetLocalToWorldMatrix(LocalMatrix);

            const int count=SubNode.GetCount();

            SceneNode **sub=SubNode.GetData();

            for(int i=0;i<count;i++)
            {
                (*sub)->RefreshMatrix(&LocalToWorldMatrix);

                sub++;
            }
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
