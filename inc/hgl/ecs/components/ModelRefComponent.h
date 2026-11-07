#pragma once
// ModelRefComponent.h —— 「这个实体要这个模型」的 ECS 声明（落地计划 A.1）
//
// 与 `GeometryData`（一实体一图元渲染标记）不同，本组件**不是渲染组件**：
// 它是异步模型装配的**请求与进度载体**。挂载它 ⇒ `ModelStreamingSystem`
// 在后续帧里替宿主推进三段渐进实例化（骨架 ⇒ 几何 ⇒ 材质），
// 每段落定触发 `ModelLoadPolicy` 对应回调。示例侧不再手写逐帧轮询。
//
// 所有权口径：
//   · `asset` 句柄归 `ModelAssetRegistry`（进程默认表），组件只借观察指针；
//   · `instance` 由组件自持（实体与资产副本随 `Release()` 归还）；
//   · `policy` 由挂组件的一方持有，可为空（⇒ 只推进、不回调）。
//
// 线程契约：只在主线程读写（与句柄表/流送系统一致）。

#include <hgl/ecs/core/entity/ComponentRow.h>
#include <hgl/ecs/systems/render/streaming/ModelLoadPolicy.h>
#include <hgl/graph/asset/StaticMeshModelInstance.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    class ModelAsset;
}

namespace hgl::ecs
{
    /**
     * @brief 模型引用组件：URI 请求原文 + 句柄观察 + 自持实例 + 三段进度。
     */
    struct ModelRefComponent : public ComponentRow
    {
        // ── 请求 ──────────────────────────────────────────────────────────
        AnsiString uri;                       ///< 请求原文（同时是句柄去重身份）
        graph::ModelAsset *asset = nullptr;   ///< 句柄（registry 拥有，不拥有）
        ModelLoadPolicy *policy  = nullptr;   ///< 拦截/优先级策略（可空，不拥有）

        // ── 装配产物与设定快照 ────────────────────────────────────────────
        graph::StaticMeshModelInstance instance;                  ///< 自持实例
        graph::StaticMeshModelInstance::Options options;          ///< 材质段设定快照
        graph::StaticMeshModelInstance::SkeletonOptions skeleton_options; ///< 骨架段设定快照

        // ── 三段推进标志（系统写；reported 保证每段回调至多一次）──────────
        bool request_started     = false;     ///< 已向句柄表发起请求（防重复投递）
        bool skeleton_done       = false;     ///< `InstantiateSkeleton` 已成功
        bool geometry_done       = false;     ///< `AttachGeometries` 已跑过（不保证挂全）
        bool material_done       = false;     ///< `AttachMaterials` 已跑过
        bool skeleton_reported   = false;
        bool geometry_reported   = false;
        bool material_reported   = false;
        bool ready_reported      = false;     ///< `OnModelReady` 已触发
        bool failed_reported     = false;     ///< `OnModelFailed` 已触发（终态，不重试）

        ModelRefComponent() = default;

        /// 非虚（T2-c：组件行里没有 vptr）；析构由池显式调用 ⇒ 实体先于组件销毁时
        /// `instance.Release()` 内部按 `IsInstantiated()` 早退，不会碰已死世界。
        ~ModelRefComponent() = default;

        /// 装配是否已终态（就绪报过 或 失败报过）
        bool IsSettled() const { return ready_reported || failed_reported; }
    };

    /// T2-c：登记类型号（**CPU-only**：无 GPU 行、不写 `EntityGPU.row[]`）。
    template<> struct ComponentTypeOf<ModelRefComponent> { static constexpr ComponentType value = ComponentType::ModelRef; };
}//namespace hgl::ecs
