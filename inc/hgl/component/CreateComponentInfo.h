#pragma once

#include<hgl/math/Matrix.h>

namespace hgl::graph
{
    class SceneNode;

    struct CreateComponentInfo
    {
        graph::SceneNode *owner_node;           ///<所属节点
        Matrix4f mat;                    ///<变换矩阵

    public:

        CreateComponentInfo()
        {
            owner_node=nullptr;
            mat=math::Identity4f;
        }

        CreateComponentInfo(const CreateComponentInfo &cci)
        {
            owner_node=cci.owner_node;
            mat=cci.mat;
        }

        CreateComponentInfo(graph::SceneNode *pn,const Matrix4f &m)
        {
            owner_node=pn;
            mat=m;
        }

        CreateComponentInfo(graph::SceneNode *pn)
        {
            owner_node=pn;
            mat=math::Identity4f;
        }

        CreateComponentInfo(const Matrix4f &m)
        {
            owner_node=nullptr;
            mat=m;
        }
    };//struct CreateComponentInfo
}//namespace hgl::graph
