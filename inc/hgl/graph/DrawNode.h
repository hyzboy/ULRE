#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/ArrayList.h>
#include<hgl/type/SortedSet.h>
#include<hgl/graph/NodeTransform.h>   // need full type for combined transform
#include<hgl/component/Component.h>

namespace hgl
{
    namespace component
    {
        class Component;
        class PrimitiveComponent;
        class StaticMeshComponent; // forward
    }//namespace component

    using namespace component;

    namespace graph
    {
        class Primitive;
        class MeshNode;            // forward
        class MaterialInstance;
        class SceneNode;       // forward

        // 抽象渲染节点：由各组件派生，提供统一的SceneNode/Component/Mesh访问，并提供渲染用的变换
        class DrawNode:public Comparator<DrawNode>
        {
        public:

            uint            index=0;                        ///<在PipelineMaterialBatch中的索引

            uint32          transform_version=0;
            uint32          transform_index=0;

            Vector3f  world_position{};               ///<世界坐标位置
            float           to_camera_distance=0;

            virtual ~DrawNode()=default;

            virtual SceneNode *         GetSceneNode()          const =0;    ///<所属场景节点（可为空）
            virtual PrimitiveComponent *GetPrimitiveComponent() const =0;    ///<所属组件（可为空）
            virtual Primitive *         GetPrimitive()          const =0;    ///<要渲染的Primitive
            virtual MaterialInstance *  GetMaterialInstance()   const =0;    ///<使用的材质实例
            virtual NodeTransform *     GetTransform()          const =0;    ///<返回用于渲染的最终变换

            //排序比较，定义在DrawNode.cpp
            const int compare(const DrawNode &)const override;
        };

        // 便捷型：PrimitiveComponent 专用节点（MI 从组件实时取得，支持override）
        class DrawNodePrimitive:public DrawNode
        {
            PrimitiveComponent *comp;
            Primitive *primitive;

        public:

            explicit DrawNodePrimitive(PrimitiveComponent *);
            SceneNode *         GetSceneNode()          const override;
            PrimitiveComponent *GetPrimitiveComponent() const override;
            Primitive *         GetPrimitive()          const override{return primitive;}
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
