#pragma once

#include<hgl/graph/NodeTransform.h>
#include<hgl/graph_v2/interface/ISceneContext.h>

/**
 * 变换节点接口 (Transform Node Interface)
 * 
 * 设计目标 (Design Goals):
 * 1. 通过接口解耦 - 节点不直接依赖具体的 World 类
 * 2. 简化依赖 - 只依赖 ISceneContext 接口
 * 3. 保持现有功能 - 继承自 NodeTransform，保留坐标变换能力
 * 
 * 与旧系统对比 (Comparison with Legacy):
 * - 旧系统: SceneNode 直接依赖 World* main_world
 * - 新系统: TransformNode 依赖 ISceneContext* scene_context
 */

namespace hgl::graph_v2
{
    class IComponentContainer;  // 前向声明

    /**
     * 变换节点接口
     * 
     * 职责 (Responsibilities):
     * - 提供场景层次结构 (父子节点关系)
     * - 提供坐标变换 (Transform)
     * - 访问所属场景上下文
     * 
     * 不负责 (NOT Responsible for):
     * - 组件管理 (由 IComponentContainer 负责)
     * - 渲染逻辑 (由 RenderContext 显式传递处理)
     * - 直接访问 RenderFramework
     */
    class ITransformNode : public hgl::graph::NodeTransform
    {
    public:
        virtual ~ITransformNode() = default;

        /**
         * 获取节点所属的场景上下文
         * @return 场景上下文接口指针，如果节点未加入场景则返回 nullptr
         * 
         * 说明: 与旧系统 GetWorld() 不同，这里返回的是接口而不是具体类
         */
        virtual ISceneContext* GetSceneContext() const = 0;

        /**
         * 获取父节点
         * @return 父节点指针，如果是根节点则返回 nullptr
         */
        virtual ITransformNode* GetParent() const = 0;

        /**
         * 设置父节点
         * @param parent 新的父节点，nullptr 表示设为根节点
         * 
         * 说明: 会自动处理场景上下文的变更
         */
        virtual void SetParent(ITransformNode* parent) = 0;

        /**
         * 添加子节点
         * @param child 子节点
         * @return 添加成功返回 true
         * 
         * 说明: 会自动调用 child->SetParent(this)
         */
        virtual bool AddChild(ITransformNode* child) = 0;

        /**
         * 移除子节点
         * @param child 子节点
         * @return 移除成功返回 true
         */
        virtual bool RemoveChild(ITransformNode* child) = 0;

        /**
         * 获取子节点数量
         */
        virtual size_t GetChildCount() const = 0;

        /**
         * 获取指定索引的子节点
         * @param index 索引
         * @return 子节点指针，索引越界返回 nullptr
         */
        virtual ITransformNode* GetChild(size_t index) const = 0;

        /**
         * 获取节点ID
         * @return 节点ID，-1 表示未设置
         */
        virtual SceneNodeID GetNodeID() const = 0;

        /**
         * 更新世界变换矩阵
         * 递归更新自己和所有子节点的世界变换
         * 
         * 说明: 重写基类方法，同时更新组件的变换
         */
        virtual void UpdateWorldTransform() override = 0;
    };

}//namespace hgl::graph_v2
