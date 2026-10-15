#pragma once

#include<hgl/ecs/core/EntityHandle.h>
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
        // A5（2026-10-14）：原有一个"L2W 行号"字段（由批处理管线里的一个步骤写入；该步骤本身未留下别的职责，
        // 也在同批删除 —— 行号现在只活在变换侧算式与 GPU 剔除写的 per-batch 表里）。
        // 它的读取点（CPU 批行写入）与写入端的消费者随批 D / T4c4 删除后**只写不读** ⇒ 字段连同赋值
        // 整删（实测把赋值改成常量，示例帧摘要逐位不变）。行号现在只活在变换侧算式与 GPU 剔除的
        // `l2w_index` 表里（`ShaderLibrary/ecs/entity_gpu_cull.glsl:390`）。

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

        // A5（2026-10-14）：原先这里还有两个带默认实现的虚接口（"位置来源"与"朝向策略"）。
        // **全仓零 override** ⇒ 唯一消费者（批处理管线里的行号赋值）那两组
        // switch 的九个分支全不可达 ⇒ 与两个只被本文件 include 的头**一并整删**（零兼容口径）。
        // 将来真出现生产者（如 2D 分派 / 地形）应以**每实例事实**表达，不要恢复虚接口。
        // 记录见 `doc/future/stage_B_debt_list.md` §2.5。

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
