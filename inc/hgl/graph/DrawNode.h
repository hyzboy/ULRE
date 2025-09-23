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
        class MeshNode;            // forward
        class MaterialInstance;
        class MeshComponent;
        class StaticMeshComponent; // forward
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

        // StaticMesh 专用：Component(NodeTransform) 变换叠加 MeshNode 变换 + Mesh（MI 从 Mesh 取得）
        class StaticMeshDrawNode final:public DrawNode
        {
            StaticMeshComponent *    component;              // StaticMesh 组件
            MeshNode *              mesh_node;              // MeshNode 对象（保存以便需要时访问更多数据）
            NodeTransform *         meshnode_transform;     // MeshNode 变换（=mesh_node）
            Mesh *                  mesh;

            // 组合缓存
            mutable uint32          scenenode_ver_cache=0;  // 实际缓存的是 component 的版本
            mutable uint32          meshnode_ver_cache=0;
            mutable NodeTransform   combined_tf;            // 组合后的变换
            void EnsureCombined() const;
        public:
            StaticMeshDrawNode(StaticMeshComponent *c, MeshNode *meshnode, Mesh *m);
            SceneNode *         GetSceneNode()          const override;
            Component *         GetComponent()          const override;
            Mesh *              GetMesh()               const override;
            MaterialInstance *  GetMaterialInstance()   const override;
            NodeTransform *     GetTransform()          const override;

            // 可选访问器
            MeshNode *          GetMeshNode()           const { return mesh_node; }
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
