#pragma once

#include<hgl/math/Matrix.h>

namespace hgl
{
    namespace graph
    {
        class SceneNode;

        struct CreateComponentInfo
        {
            graph::SceneNode *owner_node;           ///<所属节点
            graph::Matrix4f mat;                    ///<变换矩阵

        public:

            CreateComponentInfo()
            {
                owner_node=nullptr;
                mat=graph::Identity4f;
            }

            CreateComponentInfo(const CreateComponentInfo &cci)
            {
                owner_node=cci.owner_node;
                mat=cci.mat;
            }

            CreateComponentInfo(graph::SceneNode *pn,const graph::Matrix4f &m)
            {
                owner_node=pn;
                mat=m;
            }

            CreateComponentInfo(graph::SceneNode *pn)
            {
                owner_node=pn;
                mat=graph::Identity4f;
            }

            CreateComponentInfo(const graph::Matrix4f &m)
            {
                owner_node=nullptr;
                mat=m;
            }
        };//struct CreateComponentInfo

    }//namespace graph
}//namespace hgl
