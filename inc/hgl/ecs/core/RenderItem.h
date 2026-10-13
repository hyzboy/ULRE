#pragma once

#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/ecs/support/PositionSourceSpec.h>
#include<hgl/ecs/support/TransformPolicySpec.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/graph/render/RenderItemDescriptor.h>
#include<memory>

namespace hgl
{
    namespace graph
    {
        class ShaderProgram;
        class DescriptorBindingSet;
        class Pipeline;
        class RenderPass;
    }

    namespace ecs
    {
        class Entity;
    }
}

namespace hgl::ecs
{
    /**
     * Base RenderItem class - abstract interface for rendering
     * Similar to hgl::graph::DrawNode in the old system
     */
    class RenderItem
    {
    public:
        uint32_t transform_index = 0;            // Transform index in buffer（PrimitiveBatchPipeline 写、:541 读）

        bool isVisible = true;                   // Visibility flag

        virtual ~RenderItem() = default;

        // Abstract interface - returns EntityID and entity pointer
        virtual EntityID GetEntityID() const = 0;
        virtual Entity* GetEntity() const = 0;
        /// 变换（薄句柄：存储行 + 世界）——T8 起不再返回组件
        virtual TransformAccessor GetTransform() const = 0;
        virtual glm::mat4 GetWorldMatrix() const = 0;

        // For material batching support
        virtual hgl::graph::ShaderProgram* GetShaderProgram() const = 0;
        // 管线按 RenderPass 维度解析：同一世界渲染到多个 RT（如 ShadowMap 的
        // depth-only 离屏 Pass）时，各 RT 使用各自格式匹配的管线。
        virtual hgl::graph::Pipeline* GetPipeline(hgl::graph::RenderPass* render_pass) const = 0;

        // Unified transform ingress for R08 (behavior stays unchanged until strategy rollout)
        virtual TransformPolicySpec GetTransformPolicySpec() const { return TransformPolicySpec{}; }
        virtual PositionSourceSpec GetPositionSourceSpec() const { return PositionSourceSpec::MeshVertex; }

        /// 几何/绘制键（= `MeshDrawParams` 行的 CPU 镜像，24 B POD）。
        /// B3-iii：批排序 / 聚簇 / ICB 一律读这份**行数据**（采集侧从行填入），不再经每实体
        /// `new` 出来的 `GeometryDataBuffer` / `GeometryDrawRange` 副本 —— 后者已删除。
        struct MeshDrawKey
        {
            uint32_t geometry_id  = 0;   ///< MeshDrawParams 池行号（4-ID 的 geometry_id；0 = 无几何）
            uint32_t vertex_base  = 0;
            uint32_t index_base   = 0;
            uint32_t vertex_count = 0;
            uint32_t index_count  = 0;
            uint32_t is_indexed   = 0;

            bool     HasGeometry     () const { return geometry_id != 0; }
            /// 绘制元素数：索引路径取 index_count，否则取 vertex_count（与 GLSL 侧同式）
            uint32_t DrawElementCount() const { return is_indexed ? index_count : vertex_count; }

            bool operator==(const MeshDrawKey &) const = default;   ///< 同键 ⇒ 可同簇/可实例化合并
        };

        // B3-iii：排序键 = **行派生 POD 键**（同键 ⇒ 同簇 ⇒ 实例化合并）。
        // 旧实现是 `RenderItem::Compare`：按 `*GeometryDataBuffer <=>`（geometry_id / vdm / VAB 列表 / ibo）
        // + `*GeometryDrawRange <=>` 逐字段内容比较 —— 那两份每实体 CPU 副本已删除：行就是它们的真相，
        // 且同 asset 必然同键 ⇒ `Compare` 零调用者，随本批删除（排序由 `draw_key` 承担）。
        MeshDrawKey draw_key;   ///< 采集期由 RenderPrimitiveCollectSystem 从行填入（每帧刷新）
    };

}//namespace hgl::ecs
