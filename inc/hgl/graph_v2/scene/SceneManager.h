#pragma once

#include<hgl/graph_v2/scene/SceneContext.h>
#include<hgl/type/Map.h>

/**
 * 场景管理器 (Scene Manager)
 * 
 * 设计目标 (Design Goals):
 * 1. 职责分离 - 从 RenderFramework 中分离场景管理功能
 * 2. 集中管理 - 统一管理所有场景上下文
 * 3. 生命周期管理 - 负责场景的创建和销毁
 * 
 * 与旧系统对比 (Comparison with Legacy):
 * - 旧系统: RenderFramework 直接管理 World 和 SceneRenderer
 * - 新系统: SceneManager 独立管理场景，RenderFramework 只管理渲染资源
 * 
 * 迁移路径 (Migration Path):
 * - 阶段1: 创建 SceneManager，RenderFramework 内部使用
 * - 阶段2: 将场景管理逻辑从 RenderFramework 迁移到 SceneManager
 * - 阶段3: RenderFramework 只保留渲染资源管理
 */

namespace hgl::graph_v2
{
    // 前向声明
    class RenderFrameworkV2;

    /**
     * 场景管理器
     * 
     * 职责 (Responsibilities):
     * - 创建和管理场景上下文
     * - 提供场景查询功能
     * - 管理默认场景
     * 
     * 不负责 (NOT Responsible for):
     * - 渲染资源管理（由 RenderFramework 负责）
     * - 场景渲染逻辑（由 SceneRenderer 负责）
     */
    class SceneManager
    {
    private:
        RenderFrameworkV2* render_framework = nullptr;  ///< 渲染框架引用
        
        Map<IDString, SceneContext*> scene_map;         ///< 场景映射表
        SceneContext* default_scene = nullptr;          ///< 默认场景

    public:
        /**
         * 构造函数
         * @param rf 渲染框架
         */
        SceneManager(RenderFrameworkV2* rf);

        /**
         * 析构函数
         * 说明: 销毁所有场景
         */
        ~SceneManager();

        /**
         * 创建场景
         * @param name 场景名称
         * @return 场景上下文指针
         * 
         * 说明:
         * - 如果同名场景已存在，返回现有场景
         * - 场景由 SceneManager 拥有和管理
         */
        SceneContext* CreateScene(const IDString& name);

        /**
         * 获取场景
         * @param name 场景名称
         * @return 场景上下文指针，未找到返回 nullptr
         */
        SceneContext* GetScene(const IDString& name);

        /**
         * 销毁场景
         * @param name 场景名称
         * @return 销毁成功返回 true
         * 
         * 说明:
         * - 如果是默认场景，会清除默认场景引用
         * - 销毁场景会删除场景内所有节点
         */
        bool DestroyScene(const IDString& name);

        /**
         * 获取默认场景
         */
        SceneContext* GetDefaultScene() { return default_scene; }
        const SceneContext* GetDefaultScene() const { return default_scene; }

        /**
         * 设置默认场景
         * @param scene 场景上下文
         * 
         * 说明: scene 必须是由此 SceneManager 创建的
         */
        void SetDefaultScene(SceneContext* scene);

        /**
         * 设置默认场景（按名称）
         * @param name 场景名称
         * @return 设置成功返回 true
         */
        bool SetDefaultScene(const IDString& name);

        /**
         * 获取场景数量
         */
        size_t GetSceneCount() const { return scene_map.GetCount(); }

        /**
         * 检查是否包含指定场景
         */
        bool ContainsScene(const IDString& name) const;

        /**
         * 清空所有场景
         * 说明: 销毁所有场景，包括默认场景
         */
        void ClearAllScenes();

        /**
         * 获取渲染框架
         */
        RenderFrameworkV2* GetRenderFramework() const { return render_framework; }
    };

}//namespace hgl::graph_v2
