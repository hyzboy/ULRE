#pragma once

#include<hgl/math/Matrix.h>

namespace hgl
{
    namespace graph
    {
        class SceneNode;

        struct CreateComponentInfo
        {
            graph::SceneNode *parent_node;          ///<父节点
            graph::Matrix4f mat;                    ///<矩阵

        public:

            CreateComponentInfo()
            {
                parent_node=nullptr;
                mat=graph::Identity4f;
            }

            CreateComponentInfo(const CreateComponentInfo &cci)
            {
                parent_node=cci.parent_node;
                mat=cci.mat;
            }

            CreateComponentInfo(graph::SceneNode *pn,const graph::Matrix4f &m)
            {
                parent_node=pn;
                mat=m;
            }

            CreateComponentInfo(graph::SceneNode *pn)
            {
                parent_node=pn;
                mat=graph::Identity4f;
            }

            CreateComponentInfo(const graph::Matrix4f &m)
            {
                parent_node=nullptr;
                mat=m;
            }
        };//struct CreateComponentInfo

    }//namespace graph
}//namespace hgl
