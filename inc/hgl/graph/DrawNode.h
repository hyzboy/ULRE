#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/ArrayList.h>
#include<hgl/type/SortedSet.h>
#include<hgl/graph/NodeTransform.h>   // need full type for combined transform
#include<hgl/component/Component.h>
#include<compare>

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

        // 渲染节点：封装PrimitiveComponent，提供统一的渲染接口
        class DrawNode
        {
        private:
            PrimitiveComponent *comp;
            Primitive *primitive;

        public:

            uint            index=0;                        ///<在PipelineMaterialBatch中的索引

            uint32          transform_version=0;
            uint32          transform_index=0;

            Vector3f  world_position{};               ///<世界坐标位置
            float           to_camera_distance=0;

            explicit DrawNode(PrimitiveComponent *);
            ~DrawNode()=default;

            SceneNode *         GetSceneNode()          const;    ///<所属场景节点（可为空）
            PrimitiveComponent *GetPrimitiveComponent() const { return comp; }    ///<所属组件（可为空）
            Primitive *         GetPrimitive()          const { return primitive; }    ///<要渲染的Primitive
            MaterialInstance *  GetMaterialInstance()   const;    ///<使用的材质实例
            NodeTransform *     GetTransform()          const;    ///<返回用于渲染的最终变换

            //排序比较，定义在DrawNode.cpp
            std::strong_ordering operator<=>(const DrawNode &other) const;
        };

        using DrawNodeList=ArrayList<DrawNode *>;

        using DrawNodePointerList=ArrayList<DrawNode *>; // 指针版本的列表，兼容旧代码

        using MaterialInstanceSets=SortedSet<MaterialInstance *>;       ///<材质实例集合
    }//namespace graph
}//namespace hgl
