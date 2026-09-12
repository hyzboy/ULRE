#pragma once

#include <hgl/vk/buffer/ActiveRowView.h>

namespace hgl::graph
{

/**
 * ActiveRowLease —— 行租约（通用 RAII 访问器，**统一按字节**）。
 *
 * 构造 = 关联池 + 申请一行；析构 = 自动归还行号。不依赖任何 registry。
 * 行内容按字节访问；需要结构体时在使用点指定类型：
 *   acc.Write(row)            // 类型由实参推导
 *   acc.GetAs<PBRSurfaceRow>()  // 需要指针时显式指定
 *   acc.WriteRow(bytes, n) / acc.WriteAt(off, bytes, n)   // 纯字节行
 */
class ActiveRowLease
{
public:
    using RowID = ActiveRowPool::RowID;
    static constexpr RowID InvalidRowID = ActiveRowPool::InvalidRowID;

protected:

    ActiveRowView view;
    RowID row_id = InvalidRowID;
    void *data   = nullptr;
    uint32_t row_bytes = 0;

    void MoveFrom(ActiveRowLease &other) noexcept
    {
        view      = std::move(other.view);
        row_id    = other.row_id;
        data      = other.data;
        row_bytes = other.row_bytes;

        other.row_id    = InvalidRowID;
        other.data      = nullptr;
        other.row_bytes = 0;
    }

public:

    ActiveRowLease() = default;
    explicit ActiveRowLease(ActiveRowPool *pool) { Acquire(pool); }
    ~ActiveRowLease() { Release(); }

    ActiveRowLease(const ActiveRowLease &) = delete;
    ActiveRowLease &operator=(const ActiveRowLease &) = delete;

    ActiveRowLease(ActiveRowLease &&other) noexcept { MoveFrom(other); }
    ActiveRowLease &operator=(ActiveRowLease &&other) noexcept
    {
        if (this != &other)
        {
            Release();
            MoveFrom(other);
        }
        return *this;
    }

    bool Acquire(ActiveRowPool *pool)
    {
        Release();

        if (!view.Attach(pool))
            return false;

        const RowID id = view.Acquire();
        if (id == InvalidRowID)
        {
            view.Detach();
            return false;
        }

        void *row = view.GetByID(id);
        if (!row)
        {
            view.Release(id);
            view.Detach();
            return false;
        }

        row_id    = id;
        data      = row;
        row_bytes = view.GetRowBytes();
        return true;
    }

    void Release()
    {
        if (row_id != InvalidRowID)
            view.Release(row_id);

        row_id    = InvalidRowID;
        data      = nullptr;
        row_bytes = 0;
        view.Detach();
    }

    /** 延迟归还（retire 语义）：交给池的延迟回收队列。 */
    bool ReleaseDeferred(uint64_t ready_epoch)
    {
        if (row_id == InvalidRowID)
            return false;

        const bool ok = view.ReleaseDeferred(row_id, ready_epoch);
        if (ok)
        {
            row_id    = InvalidRowID;
            data      = nullptr;
            row_bytes = 0;
            view.Detach();
        }
        return ok;
    }

    bool IsValid() const { return data != nullptr && row_id != InvalidRowID; }
    operator bool() const { return IsValid(); }

    RowID GetRowID() const { return row_id; }
    ActiveRowPool *GetPool() const { return view.GetPool(); }
    ActiveRowView &GetView() { return view; }
    const ActiveRowView &GetView() const { return view; }
    uint32_t GetRowBytes() const { return row_bytes; }

    // ---- 字节访问 ----

    void       *Get()       { return data; }
    const void *Get() const { return data; }

    bool WriteRow(const void *src, uint32_t bytes)
    {
        return IsValid() ? view.WriteRow(row_id, src, bytes) : false;
    }

    bool WriteAt(uint32_t offset, const void *src, uint32_t bytes)
    {
        return IsValid() ? view.WriteAt(row_id, offset, src, bytes) : false;
    }

    bool ReadBytes(void *dst, uint32_t bytes) const
    {
        return IsValid() ? view.ReadByID(row_id, dst, bytes) : false;
    }

    // ---- 类型化便利（使用点指定类型） ----

    template<typename T>
    T *GetAs() { return static_cast<T *>(data); }

    template<typename T>
    const T *GetAs() const { return static_cast<const T *>(data); }

    /** 写 sizeof(T) 字节（等价 `*row = value` + 按行提交）。 */
    template<typename T>
    bool Write(const T &value)
    {
        if (!IsValid())
        {
            GLogError("[ActiveRowLease] Write failed: invalid lease");
            return false;
        }

        return view.WriteAs(row_id, value);
    }

    template<typename T>
    bool Read(T &out_value) const
    {
        if (!IsValid())
        {
            GLogError("[ActiveRowLease] Read failed: invalid lease");
            return false;
        }

        return view.ReadAs(row_id, out_value);
    }

    bool Commit()
    {
        return IsValid() ? view.CommitByID(row_id) : false;
    }
};

} // namespace hgl::graph
