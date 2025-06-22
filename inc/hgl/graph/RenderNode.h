#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/SortedSet.h>

namespace hgl
{
    namespace graph
    {
        class Mesh;
        class MaterialInstance;
        class MeshComponent;

        struct RenderNode:public Comparator<RenderNode>
        {
            uint            index;                          ///<在MaterialRenderList中的索引

            MeshComponent * sm_component;                   ///<静态网格组件

            uint32          l2w_version;
            uint32          l2w_index;

            Vector3f        world_position;
            float           to_camera_distance;

            //包围盒
            //屏幕空间Tile分布

        public:

            //该函数位于MaterialRenderList.cpp
            const int compare(const RenderNode &)const override;

        public:

            Mesh *GetMesh()const;
            MaterialInstance *GetMaterialInstance()const;
        };

        using RenderNodeList=ArrayList<RenderNode>;
        using RenderNodePointerList=ArrayList<RenderNode *>;

        using MaterialInstanceSets=SortedSet<MaterialInstance *>;       ///<材质实例集合
    }//namespace graph

    template<> inline const int ItemComparator<graph::RenderNode>::compare(const graph::RenderNode &a,const graph::RenderNode &b)
    {
        return a.compare(b);
    }
}//namespace hgl
