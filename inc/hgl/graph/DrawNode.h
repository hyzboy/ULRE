#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/ArrayList.h>
#include<hgl/type/SortedSet.h>
#include<hgl/graph/NodeTransform.h>   // need full type for combined transform

namespace hgl
{
    namespace graph
    {
        class Mesh;
        class MaterialInstance;
        class MeshComponent;
        class SceneNode;       // forward
        class Component;       // forward

        // 抽象渲染节点：由各组件派生，提供统一的SceneNode/Component/Mesh访问，并提供渲染用的变换
        class DrawNode:public Comparator<DrawNode>
        {
        public:
            uint            index=0;                        ///<在PipelineMaterialBatch中的索引

            uint32          transform_version=0;
            uint32          transform_index=0;

            Vector3f        world_position{};
            float           to_camera_distance=0;

            virtual ~DrawNode()=default;

            virtual SceneNode *        GetSceneNode()          const =0;    ///<所属场景节点（可为空）
            virtual Component *        GetComponent()          const =0;    ///<所属组件（可为空）
            virtual Mesh *             GetMesh()               const =0;    ///<要渲染的Mesh
            virtual MaterialInstance * GetMaterialInstance()   const =0;    ///<使用的材质实例
            virtual NodeTransform *    GetTransform()          const =0;    ///<返回用于渲染的最终变换

            //排序比较，定义在DrawNode.cpp
            const int compare(const DrawNode &)const override;
        };

        // 便捷型：MeshComponent 专用节点（MI 从组件实时取得，支持override）
        class MeshDrawNode final:public DrawNode
        {
            MeshComponent *comp;
        public:
            explicit MeshDrawNode(MeshComponent *c);
            SceneNode *         GetSceneNode()          const override;
            Component *         GetComponent()          const override;
            Mesh *              GetMesh()               const override;
            MaterialInstance *  GetMaterialInstance()   const override;
            NodeTransform *     GetTransform()          const override;
        };

        // StaticMesh 专用：Owner(组件或节点)+MeshNode 变换叠加 + Mesh（MI 从 Mesh 取得）
        class StaticMeshDrawNode final:public DrawNode
        {
            SceneNode *     scene_node;         // 可为空
            Component *     component;          // 可为空
            NodeTransform * owner_transform;    // 组件/节点 变换
            NodeTransform * meshnode_transform; // MeshNode 变换
            Mesh *          mesh;

            // 组合缓存
            mutable uint32          owner_ver_cache=0;
            mutable uint32          meshnode_ver_cache=0;
            mutable NodeTransform   combined_tf;    // 组合后的变换
            void EnsureCombined() const;
        public:
            StaticMeshDrawNode(SceneNode *sn, Component *c, NodeTransform *owner_tf, NodeTransform *meshnode_tf, Mesh *m);
            SceneNode *         GetSceneNode()          const override;
            Component *         GetComponent()          const override;
            Mesh *              GetMesh()               const override;
            MaterialInstance *  GetMaterialInstance()   const override;
            NodeTransform *     GetTransform()          const override;
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
