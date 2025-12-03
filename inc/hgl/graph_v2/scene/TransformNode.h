#pragma once

#include<hgl/graph_v2/interface/ITransformNode.h>
#include<hgl/graph_v2/interface/IComponentContainer.h>
#include<hgl/type/SortedSet.h>
#include<hgl/graph/AABB.h>
#include<hgl/graph/OBB.h>

/**
 * 变换节点实现 (Transform Node Implementation)
 * 
 * 设计说明 (Design Notes):
 * 1. 实现 ITransformNode 和 IComponentContainer 接口
 * 2. 不直接依赖 World，而是依赖 ISceneContext 接口
 * 3. 组件管理通过接口，降低耦合
 * 
 * 与旧系统对比 (Comparison with Legacy):
 * - 旧类名: SceneNode
 * - 新类名: TransformNode (避免命名冲突)
 * - 主要变化: main_world 改为 scene_context，类型为接口
 */

namespace hgl::graph_v2
{
    using TransformNodeList = SortedSet<ITransformNode*>;

    /**
     * 变换节点实现类
     * 
     * 命名说明: 使用 TransformNode 而不是 SceneNode
     * - 避免与旧系统的 SceneNode 冲突
     * - 更准确地反映其职责（坐标变换 + 层次结构）
     * - 便于在迁移期间两套系统共存
     */
    class TransformNode : public ITransformNode, public IComponentContainer
    {
    protected:
        ISceneContext* scene_context = nullptr;             ///< 所属场景上下文（接口）
        ITransformNode* parent_node = nullptr;              ///< 父节点（接口）

        SceneNodeID node_id = -1;                           ///< 节点ID
        IDString node_name;                                 ///< 节点名称

        TransformNodeList child_nodes;                      ///< 子节点列表
        ComponentSet component_set;                         ///< 组件集合（只持有引用）

        AABB local_bounding_box;                            ///< 本地包围盒
        OBB world_bounding_box;                             ///< 世界包围盒

    protected:
        /**
         * 场景上下文变更通知
         * @param new_context 新的场景上下文
         * 
         * 说明: 当节点的场景上下文改变时调用（例如移动到另一个场景）
         */
        virtual void OnSceneContextChanged(ISceneContext* new_context);

    public:
        /**
         * 构造函数
         */
        TransformNode();

        /**
         * 构造函数（指定场景上下文）
         * @param context 场景上下文
         */
        TransformNode(ISceneContext* context);

        /**
         * 析构函数
         * 
         * 说明:
         * - 从父节点和场景中移除自己
         * - 分离所有组件（但不销毁组件）
         * - 销毁所有子节点
         */
        virtual ~TransformNode();

        // 禁用拷贝
        TransformNode(const TransformNode&) = delete;
        TransformNode& operator=(const TransformNode&) = delete;

        // ===== 实现 ITransformNode 接口 =====

        ISceneContext* GetSceneContext() const override;
        ITransformNode* GetParent() const override;
        void SetParent(ITransformNode* parent) override;

        bool AddChild(ITransformNode* child) override;
        bool RemoveChild(ITransformNode* child) override;
        size_t GetChildCount() const override;
        ITransformNode* GetChild(size_t index) const override;

        SceneNodeID GetNodeID() const override;

        void UpdateWorldTransform() override;

        // ===== 实现 IComponentContainer 接口 =====

        bool AttachComponent(component::Component* comp) override;
        void DetachComponent(component::Component* comp) override;
        bool ContainsComponent(component::Component* comp) const override;
        bool HasComponentOfManager(const component::ComponentManager* manager) const override;
        const ComponentSet& GetComponents() const override;
        size_t GetComponentCount() const override;
        void ClearComponents() override;

        // ===== 额外功能 =====

        /**
         * 设置节点名称
         */
        void SetNodeName(const IDString& name) { node_name = name; }
        const IDString& GetNodeName() const { return node_name; }

        /**
         * 设置节点ID
         * 说明: 通常由 SceneContext 在注册时设置
         */
        void SetNodeID(SceneNodeID id) { node_id = id; }

        /**
         * 刷新包围盒
         * 说明: 根据子节点和组件计算包围盒
         */
        virtual void RefreshBoundingVolumes();

        /**
         * 获取包围盒
         */
        const AABB& GetLocalBoundingBox() const { return local_bounding_box; }
        const OBB& GetWorldBoundingBox() const { return world_bounding_box; }

        /**
         * 克隆节点
         * @param new_context 新节点所属的场景上下文
         * @return 克隆的节点
         * 
         * 说明:
         * - 克隆节点本身、子节点和组件引用
         * - 不克隆组件数据（组件由 ComponentManager 管理）
         */
        virtual TransformNode* Clone(ISceneContext* new_context = nullptr) const;

        /**
         * 重置节点
         * 说明: 清空所有子节点和组件，重置变换
         */
        virtual void Reset();
    };

}//namespace hgl::graph_v2
