#pragma once
// ModelCollisionProxyComponent.h —— 「模型的 CPU 侧碰撞/拾取代理」的 ECS 声明（落地计划 T4.3）
//
// 设计约束（**方向性硬规则**，不是风格建议）：
//   · 本组件是 GPU 模型模板的**物理/拾取侧镜像**，只描述**粗包围体**（盒 / 胶囊 / 凸包），
//     不描述渲染零件，也不引用任何 GPU 资源；
//   · 依赖方向单向：物理 / 拾取 / 导航**只允许**查询本组件，
//     **禁止**反向读取 `ModelPartRow` / 模板池 / 任何 BDA 地址去推物理体；
//   · 因此 GPU 侧可以任意改展开策略（换 LOD、换阴影代理、改部件树），
//     物理结果**逐字节不变** —— 这是把「渲染自由度」与「物理确定性」解耦的唯一手段。
//
// 为什么必须解耦：模板展开是**可变**的（LOD 换级、阴影代理、逐部件剔除、实例掩码），
// 若物理去解析渲染零件，一次 LOD 换级就会改碰撞体形状，出现「走近楼房突然能穿墙」这类
// 只在大场景出现的抖动弹跳。
//
// 与渲染侧的连接方式：`GPUModelRefComponent::template_id` 只是**渲染引用**；
// 本组件的 `template_id` 是**同一模型的物理条目 id**，两者由上层（装配/流送）配对写入，
// 但互不读取 —— 断链也不影响任何一侧退化为「无碰撞 / 无渲染」。
//
// 线程契约：只在主线程读写（与其它组件一致）。

#include <hgl/ecs/core/entity/ComponentRow.h>
#include <hgl/math/VectorTypes.h>
#include <hgl/math/VectorOperations.h>
#include <hgl/type/ValueArray.h>

namespace hgl::ecs
{
    using math::Vector3f;   ///< 行内用短名（`math::Vector3f` == `glm::vec3`，与 CameraComponent 同口径）
    /// 碰撞代形体种类（粗包围体；细节由物理后端自行细分）
    enum class ModelCollisionShape : uint8_t
    {
        None = 0,   ///< 无代理（等同未挂载 ⇒ 上层回退到实体自身包围盒）
        Box,        ///< 有向盒（`half_extents` + `local_rotation`）
        Capsule,    ///< 胶囊（`radius` + `half_height`，轴 = 局部 Z，引擎 Z-up）
        ConvexHull, ///< 凸包（顶点表见 `hulls`；`hull_offsets` 给每包区间）
    };

    /**
     * @brief 单个体（粗包围体）描述 —— 与渲染零件**无**任何映射关系。
     *
     * `local_*` 全在**模型局部空间**（模板根的 TRS 之前），实例化时由物理侧
     * 用实体 L2W 变换；这样同一模型的多个实例共享一份代理数据。
     */
    struct ModelCollisionBody
    {
        ModelCollisionShape shape = ModelCollisionShape::None;

        math::Vector3f local_center = Vector3f(0.0f, 0.0f, 0.0f);       ///< Box/Capsule 中心
        math::Vector3f half_extents = Vector3f(0.0f, 0.0f, 0.0f);       ///< Box 半长
        math::Vector3f local_rotation = Vector3f(0.0f, 0.0f, 0.0f);     ///< Box 局部欧拉（弧度，XYZ 序）

        float radius      = 0.0f;   ///< Capsule 半径 / ConvexHull 包围半径
        float half_height = 0.0f;   ///< Capsule 半高（不含两端半球）

        uint32_t hull_vertex_offset = 0; ///< ConvexHull 顶点在 `hulls` 中的起始下标
        uint32_t hull_vertex_count  = 0; ///< ConvexHull 顶点数

        uint32_t collision_flags = 0;    ///< 后端自定义位（如「只用于射线」「不参与刚体」）
    };

    /**
     * @brief 模型碰撞代理组件（CPU-only：无 GPU 行，`gpu_row_bytes = 0`）。
     *
     * 一实体 = 一组粗包围体（多数模型只有 1 个 Box）。`bodies` 用 `ValueArray`
     * 是因为体数随模型复杂度变化；**不**持有渲染部件列表。
     */
    struct ModelCollisionProxyComponent : public ComponentRow
    {
        /// 渲染模板 id 的物理侧对应物（0 = 未配对；两侧都容忍 0）
        uint32_t template_id = 0;

        /// 粗包围体列表（顺序 = 装配序；物理后端可任意重排自己的宽相结构）
        ValueArray<ModelCollisionBody> bodies;

        /// ConvexHull 顶点池（每体用 `hull_vertex_offset/count` 取区间；空 = 无凸包）
        ValueArray<math::Vector3f> hulls;

        ModelCollisionProxyComponent() = default;

        /// 非虚（T2-c：组件行里没有 vptr）；析构由池显式调用
        ~ModelCollisionProxyComponent() = default;

        // ── 装配 ──────────────────────────────────────────────────────────

        /// 清空全部体与凸包顶点（`template_id` 保留 —— 重装配同一模型时用）
        void ClearBodies()
        {
            bodies.Clear();
            hulls.Clear();
        }

        /// 追加一个 Box（返回体下标）
        uint32_t AddBox(const Vector3f &local_center,
                        const Vector3f &half_extents,
                        const Vector3f &local_rotation = Vector3f(0.0f, 0.0f, 0.0f))
        {
            ModelCollisionBody body;
            body.shape          = ModelCollisionShape::Box;
            body.local_center   = local_center;
            body.half_extents   = half_extents;
            body.local_rotation = local_rotation;

            return static_cast<uint32_t>(bodies.Add(body));
        }

        /// 追加一个 Capsule（轴 = 局部 Z，符合引擎 Z-up）
        uint32_t AddCapsule(const Vector3f &local_center, float radius, float half_height)
        {
            ModelCollisionBody body;
            body.shape       = ModelCollisionShape::Capsule;
            body.local_center = local_center;
            body.radius      = radius;
            body.half_height = half_height;

            return static_cast<uint32_t>(bodies.Add(body));
        }

        /**
         * 追加一个凸包（`count` 个局部空间顶点）。
         *
         * 顶点会被**复制**进本组件的 `hulls` 池 ⇒ 调用方的临时缓冲可以立刻释放。
         * `count == 0` ⇒ 不追加，返回 `kInvalidBodyIndex`。
         */
        uint32_t AddConvexHull(const math::Vector3f *vertices, uint32_t count, const Vector3f &local_center)
        {
            if (!vertices || count == 0u)
                return kInvalidBodyIndex;

            ModelCollisionBody body;
            body.shape               = ModelCollisionShape::ConvexHull;
            body.local_center        = local_center;
            body.hull_vertex_offset  = static_cast<uint32_t>(hulls.GetCount());

            for (uint32_t i = 0; i < count; ++i)
                hulls.Add(vertices[i]);

            body.hull_vertex_count = count;

            float max_dist = 0.0f;

            for (uint32_t i = 0; i < count; ++i)
            {
                const float dist = math::Length(vertices[i], local_center);

                if (dist > max_dist)
                    max_dist = dist;
            }

            body.radius = max_dist;   ///< 粗略包围半径（宽相可用）

            return static_cast<uint32_t>(bodies.Add(body));
        }

        // ── 查询 ──────────────────────────────────────────────────────────

        uint32_t GetBodyCount() const { return static_cast<uint32_t>(bodies.GetCount()); }

        const ModelCollisionBody *GetBody(uint32_t index) const
        {
            return (index < GetBodyCount()) ? &bodies.GetData()[index] : nullptr;
        }

        /// 取某凸包的顶点区间（越界 / 非凸包 / 空 ⇒ nullptr）
        const math::Vector3f *GetHullVertices(uint32_t index) const
        {
            const ModelCollisionBody *body = GetBody(index);

            if (!body || body->shape != ModelCollisionShape::ConvexHull || body->hull_vertex_count == 0u)
                return nullptr;

            if (static_cast<uint64_t>(body->hull_vertex_offset) + body->hull_vertex_count >
                static_cast<uint64_t>(hulls.GetCount()))
                return nullptr;

            return hulls.GetData() + body->hull_vertex_offset;
        }

        // ── 使用约定 ──────────────────────────────────────────────────────

        /// 「未挂载 / 空体表」统一判据：物理与拾取侧必须先问这一句再取值
        bool HasCollision() const { return bodies.GetCount() > 0; }

        /// 非法体下标（`AddConvexHull` 失败返回）
        static constexpr uint32_t kInvalidBodyIndex = 0xFFFFFFFFu;
    };

    /// 获取实体的碰撞代理（未挂载 ⇒ nullptr）
    ModelCollisionProxyComponent *GetModelCollisionProxy(Entity *owner);
    const ModelCollisionProxyComponent *GetModelCollisionProxy(const Entity *owner);

    /// T2-c：登记类型号（**CPU-only**：无 GPU 行、不写 `EntityGPU.row[]`）。
    template<> struct ComponentTypeOf<ModelCollisionProxyComponent> { static constexpr ComponentType value = ComponentType::ModelCollisionProxy; };
}//namespace hgl::ecs
