#pragma once

#include<hgl/graph_v2/interface/ISceneContext.h>
#include<hgl/graph_v2/interface/ITransformNode.h>
#include<hgl/type/Map.h>

/**
 * 场景上下文实现 (Scene Context Implementation)
 * 
 * 设计说明 (Design Notes):
 * 1. 纯场景容器 - 只管理场景节点，不涉及渲染
 * 2. 明确所有权 - 拥有根节点和节点注册表
 * 3. 从 World 中分离出来 - World 将持有 SceneContext
 * 
 * 迁移路径 (Migration Path):
 * - 阶段1: 创建 SceneContext，World 内部使用
 * - 阶段2: 将 World 的节点管理功能迁移到 SceneContext
 * - 阶段3: SceneNode 改为依赖 ISceneContext 而不是 World
 */

namespace hgl::graph_v2
{
    /**
     * 场景上下文实现类
     * 
     * 实现细节 (Implementation Details):
     * - 使用 Map 存储节点ID到节点的映射，便于快速查找
     * - 拥有根节点的所有权，负责创建和销毁
     * - 维护场景内所有节点的注册表
     */
    class SceneContext : public ISceneContext
    {
    private:
        IDString context_name;                              ///< 场景名称
        ITransformNode* root_node = nullptr;                ///< 根节点（拥有所有权）
        Map<SceneNodeID, ITransformNode*> node_registry;    ///< 节点注册表（ID -> Node）

    public:
        /**
         * 构造函数
         * @param name 场景名称
         */
        SceneContext(const IDString& name);

        /**
         * 析构函数
         * 说明: 会销毁根节点和整个场景树
         */
        virtual ~SceneContext();

        // 实现 ISceneContext 接口
        const IDString& GetName() const override;
        ITransformNode* GetRootNode() override;
        const ITransformNode* GetRootNode() const override;

        bool RegisterNode(SceneNodeID id, ITransformNode* node) override;
        bool UnregisterNode(SceneNodeID id) override;
        ITransformNode* FindNode(SceneNodeID id) override;
        const ITransformNode* FindNode(SceneNodeID id) const override;
        size_t GetNodeCount() const override;

        /**
         * 创建并设置根节点
         * @return 根节点指针
         * 
         * 说明: 如果根节点已存在，返回现有根节点
         */
        ITransformNode* CreateRootNode();

        /**
         * 清空场景
         * 说明: 销毁所有节点，重置场景
         */
        void Clear();

        /**
         * 遍历所有节点（深度优先）
         * @param func 遍历函数，返回 false 停止遍历
         */
        template<typename Func>
        void TraverseNodes(Func func)
        {
            // TODO: 实现节点遍历
            // 从根节点开始，递归遍历所有子节点
        }
    };

}//namespace hgl::graph_v2
