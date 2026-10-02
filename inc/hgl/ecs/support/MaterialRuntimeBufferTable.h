#pragma once

#include<hgl/CoreType.h>
#include<hgl/log/Log.h>
#include<hgl/type/UnorderedMap.h>
#include<cstdint>

namespace hgl::graph
{
    class BufferOwner;
    class DeviceBuffer;
    class IndirectMeshTaskBuffer;
}//namespace hgl::graph

namespace hgl::ecs
{
    /**
     * 设备缓冲**句柄表**（B5-a —— P-5「每实例指针字段归位：行内只留资源池行号 + 偏移」的落地物）。
     *
     * 为什么存在：`MaterialRuntimeSlot` 是每实例状态（将来要成为定长行 arena 的一行），
     * 行内**不放指针**（T8 教训：行内放重型/指针类型会让行宽失控、且 GPU/序列化无法消费）。
     * 于是缓冲区一律以**句柄（uint32 行号）**入行，指针本体存在这张表里。
     *
     * 语义：
     *  · 句柄 0 = 「无缓冲」（与全仓「行 0 哨兵」惯例一致，`Register(nullptr)` 即返回 0）；
     *  · **同一指针重复登记 ⇒ 同一句柄**（interned），并**带引用计数**——示例里多个实体
     *    共享同一块缓冲（如 `ComputeFrustumCull` 的多实例）⇒ 必须按引用持有，
     *    否则先销毁的 slot 会把别人还在用的句柄回收掉；
     *  · `Release` 按引用递减，归零才真正移除；
     *  · 归属：**世界级**（`ECSContext` 持有，与 MaterialRuntimeTable 同域）。
     */
    class MaterialRuntimeBufferTable
    {
    public:

        static constexpr uint32_t kInvalidBufferHandle = 0;

        /// 登记（同一指针幂等、引用 +1）；`nullptr` ⇒ 返回 0（不占句柄）
        uint32_t Register(graph::BufferOwner *buffer);

        /// 解析（句柄 0 / 不存在 ⇒ nullptr）；T 必须是登记时的实际类型
        template<typename T = graph::BufferOwner>
        T *Resolve(const uint32_t handle) const
        {
            if (handle == kInvalidBufferHandle)
                return nullptr;

            const auto *it = buffer_by_handle.GetValuePointer(handle);

            return it ? static_cast<T *>(*it) : nullptr;
        }

        /// 引用 -1（句柄 0 直接忽略；归零则移除并回收句柄号）
        void Release(const uint32_t handle);

        /// 观测（测试/诊断）
        int GetLiveHandleCount() const { return buffer_by_handle.GetCount(); }
        uint32_t GetRegisterCount() const { return register_count; }
        uint32_t GetReleaseCount() const { return release_count; }

    private:

        hgl::UnorderedMap<graph::BufferOwner *, uint32_t> handle_by_buffer;
        hgl::UnorderedMap<uint32_t, graph::BufferOwner *> buffer_by_handle;
        hgl::UnorderedMap<uint32_t, uint32_t> ref_count_by_handle;
        uint32_t next_handle = 1;           ///< 0 保留给「无缓冲」
        uint32_t register_count = 0;        ///< 登记次数（幂等性可据此验）
        uint32_t release_count = 0;
    };

    inline uint32_t MaterialRuntimeBufferTable::Register(graph::BufferOwner *buffer)
    {
        if (!buffer)
            return kInvalidBufferHandle;

        ++register_count;

        if (const auto *exist = handle_by_buffer.GetValuePointer(buffer))
        {
            const uint32_t handle = *exist;

            if (uint32_t *rc = ref_count_by_handle.GetValuePointer(handle))
                ++(*rc);

            return handle;
        }

        const uint32_t handle = next_handle++;

        if (!handle_by_buffer.Add(buffer, handle)
         || !buffer_by_handle.Add(handle, buffer)
         || !ref_count_by_handle.Add(handle, 1u))
        {
            GLogError("[MaterialRuntimeBufferTable] 句柄表登记失败（内存不足？）handle=%u", handle);
            handle_by_buffer.DeleteByKey(buffer);
            buffer_by_handle.DeleteByKey(handle);
            ref_count_by_handle.DeleteByKey(handle);
            return kInvalidBufferHandle;
        }

        return handle;
    }

    inline void MaterialRuntimeBufferTable::Release(const uint32_t handle)
    {
        if (handle == kInvalidBufferHandle)
            return;

        uint32_t *rc = ref_count_by_handle.GetValuePointer(handle);

        if (!rc)
            return;                     // 已回收/无效句柄：静默忽略（不报错，避免关闭期刷屏）

        ++release_count;

        if (--(*rc) > 0)
            return;

        if (const auto *owner = buffer_by_handle.GetValuePointer(handle))
            handle_by_buffer.DeleteByKey(*owner);

        buffer_by_handle.DeleteByKey(handle);
        ref_count_by_handle.DeleteByKey(handle);
    }
}//namespace hgl::ecs
