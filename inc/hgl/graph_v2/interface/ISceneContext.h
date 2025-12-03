#pragma once

#include<hgl/type/DataType.h>
#include<hgl/type/String.h>

/**
 * 重构后的场景系统 - 核心接口定义
 * Refactored Scene System - Core Interface Definitions
 * 
 * 设计目标 (Design Goals):
 * 1. 解除循环依赖 - SceneContext 作为纯场景容器，不依赖渲染系统
 * 2. 单一职责 - 只负责场景节点的管理，不涉及渲染资源
 * 3. 明确所有权 - SceneContext 拥有场景节点的生命周期
 * 
 * 与旧系统对比 (Comparison with Legacy):
 * - 旧系统: World 混合了场景管理和渲染资源
 * - 新系统: SceneContext 只管理场景，RenderWorld 管理渲染资源
 */

namespace hgl::graph_v2
{
    // 前向声明
    class ITransformNode;
    
    using SceneNodeID = int64;

    /**
     * 场景上下文接口 (Scene Context Interface)
     * 
     * 职责 (Responsibilities):
     * - 管理场景名称
     * - 管理场景根节点
     * - 提供节点注册和查询功能
     * 
     * 不负责 (NOT Responsible for):
     * - 渲染资源管理 (Render resource management)
     * - 事件分发 (Event dispatching)
     * - UBO/DescriptorBinding 等渲染相关内容
     */
    class ISceneContext
    {
    public:
        virtual ~ISceneContext() = default;

        /**
         * 获取场景名称
         * @return 场景名称
         */
        virtual const IDString& GetName() const = 0;

        /**
         * 获取场景根节点
         * @return 根节点指针，如果场景为空则返回 nullptr
         */
        virtual ITransformNode* GetRootNode() = 0;
        virtual const ITransformNode* GetRootNode() const = 0;

        /**
         * 注册节点到场景
         * 用于在场景中快速查找节点
         * @param id 节点ID
         * @param node 节点指针
         * @return 注册成功返回 true
         */
        virtual bool RegisterNode(SceneNodeID id, ITransformNode* node) = 0;

        /**
         * 从场景中注销节点
         * @param id 节点ID
         * @return 注销成功返回 true
         */
        virtual bool UnregisterNode(SceneNodeID id) = 0;

        /**
         * 根据ID查找节点
         * @param id 节点ID
         * @return 找到的节点指针，未找到返回 nullptr
         */
        virtual ITransformNode* FindNode(SceneNodeID id) = 0;
        virtual const ITransformNode* FindNode(SceneNodeID id) const = 0;

        /**
         * 获取场景中所有节点的数量
         * @return 节点数量
         */
        virtual size_t GetNodeCount() const = 0;
    };

}//namespace hgl::graph_v2
