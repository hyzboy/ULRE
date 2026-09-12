#pragma once

#include <hgl/vk/buffer/ActiveRowPool.h>

namespace hgl::graph
{

/**
 * ActiveRowView —— 行池上的「行视图」（**统一按字节**）。
 *
 * 行内容一律以 `void* + 字节数` 访问；需要 C++ 结构体时**在使用点指定类型强转**：
 *   T *row = view.GetAs<T>(id);            // 等价 static_cast<T *>(view.GetByID(id))
 *   view.WriteAs<T>(id, value);            // 局部写：不整行清零，与「*row = value」同义
 *   view.ReadAs<T>(id, out);
 * 只有一段字节、没有结构体的行（如纹理引用行）直接用 GetByID / WriteRow / WriteAt。
 *
 * 这样「行池 / 视图 / 租约」只有一套实现，同时覆盖「有结构体」与「只有字节数」两种行。
 *
 * 借用池：不拥有 Buffer、不拥有行号空间。
 */
class ActiveRowView
{
public:
    using RowID = ActiveRowPool::RowID;
    static constexpr RowID InvalidRowID = ActiveRowPool::InvalidRowID;

private:

    ActiveRowPool *pool = nullptr;

public:

    ActiveRowView() = default;
    explicit ActiveRowView(ActiveRowPool *in_pool) { Attach(in_pool); }

    ActiveRowView(ActiveRowView &&other) noexcept
        : pool(other.pool) { other.pool = nullptr; }
    ActiveRowView &operator=(ActiveRowView &&other) noexcept
    {
        if (this != &other)
        {
            pool = other.pool;
            other.pool = nullptr;
        }
        return *this;
    }

    ActiveRowView(const ActiveRowView &) = delete;
    ActiveRowView &operator=(const ActiveRowView &) = delete;

    bool Attach(ActiveRowPool *in_pool)
    {
        if (!in_pool || !in_pool->IsReady())
        {
            Detach();
            GLogError("[ActiveRowView] Attach rejected: pool not ready");
            return false;
        }

        pool = in_pool;
        return true;
    }

    void Detach() { pool = nullptr; }

    bool IsValid() const { return pool != nullptr && pool->IsReady(); }
    operator bool() const { return IsValid(); }

    ActiveRowPool *GetPool() const { return pool; }
    uint32_t GetRowBytes() const { return pool ? pool->GetRowBytes() : 0; }
    uint32_t GetCapacity() const { return pool ? pool->GetRowCapacity() : 0; }
    uint32_t GetActiveCount() const { return pool ? pool->GetActiveCount() : 0; }
    uint32_t GetReservedRowCount() const { return pool ? pool->GetReservedRowCount() : 0; }
    bool IsActiveRow(RowID id) const { return pool && pool->IsActive(id); }

    RowID Acquire() { return pool ? pool->Acquire() : InvalidRowID; }
    bool Release(RowID id) { return pool ? pool->Release(id) : false; }
    bool ReleaseDeferred(RowID id, uint64_t ready_epoch)
    {
        return pool ? pool->ReleaseDeferred(id, ready_epoch) : false;
    }
    bool CommitByID(RowID id) { return pool ? pool->CommitRow(id) : false; }

    // ---- 字节访问 ----

    void *GetByID(RowID id)
    {
        if (!pool || !pool->IsActive(id))
        {
            GLogError("[ActiveRowView] GetByID failed: ID=%u is not active", id);
            return nullptr;
        }

        return pool->RowCPU(id);
    }

    const void *GetByID(RowID id) const
    {
        if (!pool || !pool->IsActive(id))
        {
            GLogError("[ActiveRowView] GetByID failed: ID=%u is not active", id);
            return nullptr;
        }

        return pool->RowCPU(id);
    }

    /** 整行写：先清零整行，再拷贝 bytes（bytes <= 行距）；随后按行提交。 */
    bool WriteRow(RowID id, const void *src, uint32_t bytes)
    {
        void *row = GetByID(id);
        if (!row || !src)
            return false;

        if (bytes > pool->GetRowBytes())
        {
            GLogError("[ActiveRowView] WriteRow rejected oversized write: bytes=%u row_bytes=%u",
                      bytes, pool->GetRowBytes());
            return false;
        }

        memset(row, 0, pool->GetRowBytes());
        if (bytes)
            memcpy(row, src, bytes);

        return CommitByID(id);
    }

    /** 偏移写：不清零，只覆盖 [offset, offset+bytes)；随后按行提交。 */
    bool WriteAt(RowID id, uint32_t offset, const void *src, uint32_t bytes)
    {
        void *row = GetByID(id);
        if (!row || !src)
            return false;

        if (static_cast<uint64_t>(offset) + bytes > pool->GetRowBytes())
        {
            GLogError("[ActiveRowView] WriteAt rejected out-of-range write: offset=%u bytes=%u row_bytes=%u",
                      offset, bytes, pool->GetRowBytes());
            return false;
        }

        memcpy(static_cast<uint8_t *>(row) + offset, src, bytes);
        return CommitByID(id);
    }

    bool ReadByID(RowID id, void *dst, uint32_t bytes) const
    {
        const void *row = GetByID(id);
        if (!row || !dst)
            return false;

        if (bytes > pool->GetRowBytes())
        {
            GLogError("[ActiveRowView] ReadByID rejected oversized read: bytes=%u row_bytes=%u",
                      bytes, pool->GetRowBytes());
            return false;
        }

        memcpy(dst, row, bytes);
        return true;
    }

    // ---- 类型化便利（使用点指定类型） ----

    template<typename T>
    T *GetAs(RowID id) { return static_cast<T *>(GetByID(id)); }

    template<typename T>
    const T *GetAs(RowID id) const { return static_cast<const T *>(GetByID(id)); }

    /** 局部写 sizeof(T) 字节（等价 `*row = value` + 按行提交）。 */
    template<typename T>
    bool WriteAs(RowID id, const T &value)
    {
        if (!pool)
            return false;

        if (sizeof(T) > pool->GetRowBytes())
        {
            GLogError("[ActiveRowView] WriteAs rejected oversized type: sizeof(T)=%zu row_bytes=%u",
                      sizeof(T), pool->GetRowBytes());
            return false;
        }

        return WriteAt(id, 0, &value, uint32_t(sizeof(T)));
    }

    template<typename T>
    bool ReadAs(RowID id, T &out_value) const
    {
        if (!pool)
            return false;

        if (sizeof(T) > pool->GetRowBytes())
        {
            GLogError("[ActiveRowView] ReadAs rejected oversized type: sizeof(T)=%zu row_bytes=%u",
                      sizeof(T), pool->GetRowBytes());
            return false;
        }

        return ReadByID(id, &out_value, uint32_t(sizeof(T)));
    }
};

} // namespace hgl::graph
