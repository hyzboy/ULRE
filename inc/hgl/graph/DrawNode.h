#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/ArrayList.h>
#include<hgl/type/SortedSet.h>

namespace hgl
{
    namespace graph
    {
        class Mesh;
        class MaterialInstance;
        class MeshComponent;
        class NodeTransform;   // forward

        // 抽象渲染节点：由各组件派生，提供统一的Owner/Mesh/MI访问
        class DrawNode:public Comparator<DrawNode>
        {
        public:
            uint            index=0;                        ///<在PipelineMaterialBatch中的索引

            uint32          transform_version=0;
            uint32          transform_index=0;

            Vector3f        world_position{};
            float           to_camera_distance=0;

            virtual ~DrawNode()=default;

            virtual NodeTransform *     GetOwner()              const =0;    ///<变换拥有者（组件/节点）
            virtual Mesh *              GetMesh()               const =0;    ///<要渲染的Mesh
            virtual MaterialInstance *  GetMaterialInstance()   const =0;    ///<使用的材质实例
            virtual NodeTransform *     GetTransform()          const =0;    ///<返回用于渲染的最终变换

            //排序比较，定义在DrawNode.cpp
            const int compare(const DrawNode &)const override;
        };

        // 便捷型：MeshComponent 专用节点（MI 从组件实时取得，支持override）
        class MeshComponentDrawNode final:public DrawNode
        {
            MeshComponent *comp;
        public:
            explicit MeshComponentDrawNode(MeshComponent *c);
            NodeTransform *     GetOwner()            const override;
            Mesh *              GetMesh()             const override;
            MaterialInstance *  GetMaterialInstance() const override;
            NodeTransform *     GetTransform()        const override;
        };

        // 通用型：Owner+Mesh 组合（MI 从 Mesh 取得）
        class OwnerMeshDrawNode final:public DrawNode
        {
            NodeTransform *owner;
            Mesh *mesh;
        public:
            OwnerMeshDrawNode(NodeTransform *o,Mesh *m);
            NodeTransform *     GetOwner()            const override;
            Mesh *              GetMesh()             const override;
            MaterialInstance *  GetMaterialInstance() const override;
            NodeTransform *     GetTransform()        const override;
        };

        using DrawNodeList=ArrayList<DrawNode *>;
        using DrawNodePointerList=ArrayList<DrawNode *>; // 指针版本的列表，兼容旧代码

        using MaterialInstanceSets=SortedSet<MaterialInstance *>;       ///<材质实例集合
    }//namespace graph

    template<> inline const int ItemComparator<graph::DrawNode>::compare(const graph::DrawNode &a,const graph::DrawNode &b)
    {
        return a.compare(b);
    }

    // 指针版本的比较器，便于对 DrawNode* 进行排序
    template<> inline const int ItemComparator<graph::DrawNode *>::compare(graph::DrawNode * const &a,graph::DrawNode * const &b)
    {
        return a->compare(*b);
    }
}//namespace hgl
