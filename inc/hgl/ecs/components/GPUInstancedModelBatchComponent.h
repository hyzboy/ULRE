#pragma once
// GPUInstancedModelBatchComponent.h —— 单实体承载海量同模板实例（落地计划 T2.2）
//
// 场景:植被林、石块堆这类「一个逻辑实体 = 数千份相同模型」的批次。传统做法要
// new 数千个实体/组件;本组件让一个实体持一份紧凑的实例矩阵数组(CPU 权威),
// 上传后由 GPU 展开成逐部件实例绘制(T3 接收集/上传路径)。
//
// 数据面二选一:
//   · CPU 权威:`cpu_matrices`(ValueArray 连续排布,脏时整段重传);
//   · GPU 权威:`device_matrices` 外部设备缓冲(观察者持有,组件只借指针;
//     非空时优先于 cpu_matrices 使用)。
// 两者皆空 ⇒ 批次不参与展开。
//
// 线程契约:只在主线程读写;帧内上传由收集系统消费(T3 接线)。

#include <hgl/ecs/core/entity/ComponentRow.h>
#include <hgl/math/Matrix.h>
#include <hgl/type/ValueArray.h>

namespace hgl
{
    namespace graph { class DeviceBuffer; }
}

namespace hgl::ecs
{
    /**
     * @brief 海量实例化批次组件(CPU-only:无 GPU 行,gpu_row_bytes = 0)。
     */
    struct GPUInstancedModelBatchComponent : public ComponentRow
    {
        // ── 核心 ──────────────────────────────────────────────────────────
        uint32_t template_id = 0;   ///< 批次统一引用的模板池行 ID(0 = 无效)

        // ── CPU 权威实例矩阵(连续紧凑)───────────────────────────────────
        hgl::ValueArray<math::Matrix4f> cpu_matrices;
        bool cpu_dirty = true;      ///< 上传节流:置位才重传 GPU(T3 消费)

        // ── GPU 权威外部设备缓冲(可空;非空时优先)───────────────────────
        graph::DeviceBuffer *device_matrices = nullptr;
        uint32_t device_instance_count = 0;   ///< 外部缓冲内的实例数

        // ── 预留插槽(空壳,字段即 ABI,后续阶段接消费者)───────────────────
        uint32_t custom_data_index = 0;   ///< 批次自定义数据行(逐实例颜色/材质微调,T4 接)

        GPUInstancedModelBatchComponent() = default;

        /// 非虚(T2-c:组件行里没有 vptr);析构由池显式调用。
        /// 注意:`device_matrices` 是借用指针,组件不释放;`cpu_matrices`
        /// 随 ValueArray 自身析构回收。
        ~GPUInstancedModelBatchComponent() = default;

        /// 当前生效的实例数(GPU 权威优先)。
        uint32_t GetInstanceCount() const
        {
            return device_matrices ? device_instance_count
                                   : static_cast<uint32_t>(cpu_matrices.GetCount());
        }

        // ── CPU 侧批量操作 ────────────────────────────────────────────────
        void SetMatrixAt(uint32_t index, const math::Matrix4f &m)
        {
            if (index < static_cast<uint32_t>(cpu_matrices.GetCount()))
            {
                cpu_matrices.GetData()[index] = m;
                cpu_dirty = true;
            }
        }

        void AddMatrix(const math::Matrix4f &m)
        {
            cpu_matrices.Add(m);
            cpu_dirty = true;
        }

        void ClearMatrices()
        {
            cpu_matrices.Clear();
            cpu_dirty = true;
        }
    };

    /// T2-c:登记类型号(**CPU-only**:无 GPU 行、不写 `EntityGPU.row[]`)。
    template<> struct ComponentTypeOf<GPUInstancedModelBatchComponent> { static constexpr ComponentType value = ComponentType::GPUInstancedModelBatch; };
}//namespace hgl::ecs
