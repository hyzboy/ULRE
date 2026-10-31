#pragma once

#include<hgl/object/TickObject.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/color/Color4f.h>
#include<hgl/vk/VKRenderTarget.h>
#include <memory>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class Camera;
        class Geometry;
        class GeometryAsset;
        class GeometryAssetRegistry;
    class GeometryVertexFormat;
        struct ViewportInfo;
        struct CameraInfo;
        struct GlobalSSBOBinding;
        namespace mtl { struct MaterialRecipe; }
    }

    /**
    * 工作对象</p>
    *
    * 逻辑更新写在 Tick（每帧、渲染前）；渲染帧内的录制钩子是 OnRenderPass。
    * 两者时序与合同见 OnRenderPass 的注释——不要在 OnRenderPass 里改场景状态。
    */
    class WorkObject:public TickObject
    {
    protected:

        OBJECT_LOGGER

    private:

        ecs::ECSContext *world=nullptr;

    protected:

        // 以下数据在 ECS 模式下来自 ECSContext/GraphicsContext

    public:

        /// ECS 世界（唯一权威）。图形资源一律经它派生，见下方便捷方法。
        ecs::ECSContext *           GetECSContext       ()const{return world;}

        graph::RenderContext *      GetRenderContext    ()const
        {
            return world ? world->GetRenderContext() : nullptr;
        }

        graph::GraphicsContext *    GetGraphicsContext  ()const
        {
            return world ? world->GetGraphicsContext() : nullptr;
        }

        /// Find a loaded material recipe, loading the default recipe directory on first miss.
        const graph::mtl::MaterialRecipe *GetRecipe(const char *name);

        /**
         * @brief 按 `AssetURI` 取几何句柄（无则建）。
         *
         * `geo:builtin:cube` / `builtin:cube` / `geo:file:res/geo/x.geo.toml` / 裸路径，
         * 语义见 `GeometryAssetRegistry::GetOrLoadURI`。
         *
         * `format_override` 为空时用预设自带的顶点格式（`GetStandardGeometryVertexFormat`）；
         * 需要 Position/TexCoord/Normal 之外的语义（顶点色、gizmo 专用布局等）时传入。
         *
         * 返回的句柄由 `GraphicsContext` 的几何资产注册器持有，**不要**
         * `GeometryManager::Add()`（那是另一条所有权线，会双销毁）。
         */
        const graph::GeometryAsset *GetGeometryAsset(const char *uri,
                                                    const graph::GeometryVertexFormat *format_override=nullptr);

        /**
         * @brief `GetGeometryAsset()` 的裸几何便捷入口。
         *
         * 生命周期跟随注册器：**不要**自己 delete，也不要交给 `GeometryManager`。
         * `format_override` 语义同 `GetGeometryAsset()`。
         * 返回的几何可能是异步流送的载体，`file:` 来源需要渲染门禁判断是否就绪。
         */
        graph::Geometry *GetGeometry(const char *uri,
                                      const graph::GeometryVertexFormat *format_override=nullptr);

        /**
         * @brief 按 `mtldata` URI + 行号取材质数据行的全局绑定。
         *
         * 这是**手工建实体**示例的入口：它们不走 `RenderableBuilder`，但要给
         * `MaterialRecipe::material_ssbo_binding` 和 `MaterialData::SetDataResource()`
         * 各填一个 `GlobalSSBOBinding` —— 一个 URI + 一个行号就够，数据本身在文件里。
         *
         * @code
         * GlobalSSBOBinding binding{};
         * if (!GetMaterialDataBinding("mtldata:@sphere", 0, binding)) return false;
         * recipe.material_ssbo_binding = binding;
         * ...
         * material_data_comp->SetDataResource(binding);
         * @endcode
         *
         * 数据块首次使用时整块上传（同一块重复调用只上传一次）。URI 写法见
         * `MaterialDataBlockRegistry::GetOrLoadURI`。失败时 `out` 保持不动并返回 false。
         */
        bool GetMaterialDataBinding(const char *uri, uint32_t row_index,
                                    graph::GlobalSSBOBinding &out);

        bool LoadRecipeFile(const OSString &path);
        bool LoadRecipeDirectory(const OSString &path);

        graph::VulkanDevice *       GetDevice           ()const
        {
            auto *gc = GetGraphicsContext();
            return gc ? gc->GetDevice() : nullptr;
        }

        /// 几何资产注册器（设备级；`GetGeometry` / `GetGeometryAsset` 的真身）。
        /// 需要挂共享顶点池创建器时用 `graph::SharedCreaterScope`。
        graph::GeometryAssetRegistry * GetGeometryAssetRegistry()const;
        template<typename T> T *    GetManager          ()const
        {
            auto *gc = GetGraphicsContext();
            return gc ? gc->GetManager<T>() : nullptr;
        }

        const VkExtent2D *          GetExtent           ();
        const graph::ViewportInfo * GetViewportInfo     ()const;
        const math::Vector2i *      GetMouseCoord       ()const;

        /// 设置清屏色。清屏色唯一权威在渲染目标上（RenderTargetDesc::clear_color），
        /// 本方法转发写入当前世界绑定的 RT——与离屏 RT 的 desc 声明同一存储。
        void SetClearColor(const Color4f &color);

    public:

        virtual const bool IsDestroy  ()const{return false;}   ///< 退出机制未实现，恒 false（见 WorkManager::Run）

    protected:

        // 保护的默认构造函数，用于子类或框架初始化
        WorkObject() = default;

    public:

        virtual ~WorkObject()=default;

        /// 注入 ECS 世界（由 RunFramework / 外部事件循环驱动方在 Init() 之前调用）
        void SetECSContext(ecs::ECSContext *ctx);

        virtual bool Init()=0;

        virtual void Tick(double);

        /// 渲染帧内录制钩子：动态渲染 pass 已开（BeginRenderPass 之后）、
        /// ECS 系统绘制之前、命令缓冲录制中执行。
        ///
        /// 合同：current_render_cmd 有效，只准录制绘制命令（vkCmdDraw 等）；
        /// **不要在此修改场景状态**（transform/材质等）——那属于 Tick
        ///（TransformSystem 在本回调之后才提交变换，Tick 里改与本回调里改
        /// 效果同帧等价；放 Tick 语义正确且不占用录制时间）。
        /// 范本：example/ApiValidation/SimpleMeshTriangle.cpp
        virtual void OnRenderPass(double delta_time) {}

    public:

        // Use RenderContext/GraphicsContext directly for resource creation.
    };//class WorkObject
}//namespace hgl
