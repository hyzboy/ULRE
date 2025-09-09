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

        struct DrawNode:public Comparator<DrawNode>
        {
            uint            index;                          ///<在PipelineMaterialBatch中的索引

            MeshComponent * mesh_component;                 ///<静态网格组件

            uint32          l2w_version;
            uint32          l2w_index;

            Vector3f        world_position;
            float           to_camera_distance;

            //包围盒
            //屏幕空间Tile分布

        public:

            //该函数位于PipelineMaterialBatch.cpp
            const int compare(const DrawNode &)const override;

        public:

            Mesh *GetMesh()const;
            MaterialInstance *GetMaterialInstance()const;
        };

        using DrawNodeList=ArrayList<DrawNode>;
        using DrawNodePointerList=ArrayList<DrawNode *>;

        using MaterialInstanceSets=SortedSet<MaterialInstance *>;       ///<材质实例集合
    }//namespace graph

    template<> inline const int ItemComparator<graph::DrawNode>::compare(const graph::DrawNode &a,const graph::DrawNode &b)
    {
        return a.compare(b);
    }
}//namespace hgl
