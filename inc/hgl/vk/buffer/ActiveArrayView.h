#pragma once

#include <hgl/vk/buffer/ArrayView.h>
#include <hgl/type/ActiveIDManager.h>
#include <hgl/log/Log.h>

#include <climits>

namespace hgl::graph
{
/**
 * A fixed-capacity ArrayView whose element indices are allocated and recycled
 * through ActiveIDManager. A live DataID is always a valid ArrayView index.
 */
template<typename T>
class ActiveArrayView
{
public:
    using DataID = uint32_t;
    static constexpr DataID InvalidDataID = ~DataID(0);

private:
    ArrayView<T> array_view;
    ActiveIDManager active_ids;

public:
    ActiveArrayView() = default;
    ~ActiveArrayView() = default;

    ActiveArrayView(const ActiveArrayView &) = delete;
    ActiveArrayView &operator=(const ActiveArrayView &) = delete;

    bool Bind(BufferOwner *buffer, const uint32_t element_count)
    {
        active_ids.Clear(true);
        array_view.Bind(buffer, 0, element_count);
        if (array_view)
            return true;

        array_view.Bind(nullptr);
        GLogError(
            "[ActiveArrayView] Bind failed: buffer=%p element_count=%u",
            buffer,
            element_count);
        return false;
    }

    void Reset()
    {
        active_ids.Clear(true);
        array_view.Bind(nullptr);
    }

    bool AcquireID(DataID &out_id)
    {
        out_id = InvalidDataID;
        if (!array_view)
        {
            GLogError("[ActiveArrayView] AcquireID failed: array view is not bound");
            return false;
        }

        const uint32_t capacity = array_view.GetCount();
        if (capacity == 0 || capacity > uint32_t(INT_MAX))
        {
            GLogError(
                "[ActiveArrayView] AcquireID failed: unsupported capacity=%u",
                capacity);
            return false;
        }

        int raw_id = -1;
        if (active_ids.HasIdleID())
        {
            if (!active_ids.Get(&raw_id))
            {
                GLogError("[ActiveArrayView] AcquireID failed: idle ID acquisition failed");
                return false;
            }
        }
        else
        {
            if (active_ids.GetHistoryMaxId() >= int(capacity))
            {
                GLogError(
                    "[ActiveArrayView] AcquireID failed: capacity exhausted capacity=%u",
                    capacity);
                return false;
            }

            if (active_ids.CreateActive(&raw_id) != 1)
            {
                GLogError("[ActiveArrayView] AcquireID failed: ActiveIDManager allocation failed");
                return false;
            }
        }

        if (raw_id < 0 || uint32_t(raw_id) >= capacity)
        {
            if (raw_id >= 0)
                active_ids.Release(&raw_id);

            GLogError(
                "[ActiveArrayView] AcquireID failed: allocated ID=%d outside capacity=%u",
                raw_id,
                capacity);
            return false;
        }

        out_id = static_cast<DataID>(raw_id);
        return true;
    }

    bool ReleaseID(const DataID id)
    {
        if (id > uint32_t(INT_MAX))
        {
            GLogError(
                "[ActiveArrayView] ReleaseID failed: ID=%u exceeds ActiveIDManager range",
                id);
            return false;
        }

        const int raw_id = static_cast<int>(id);
        if (!active_ids.IsActive(raw_id))
        {
            GLogError(
                "[ActiveArrayView] ReleaseID failed: ID=%u is not active",
                id);
            return false;
        }

        return active_ids.Release(&raw_id) == 1;
    }

    bool IsActiveID(const DataID id) const
    {
        return id <= uint32_t(INT_MAX)
            && active_ids.IsActive(static_cast<int>(id));
    }

    T *GetByID(const DataID id)
    {
        if (!IsActiveID(id))
        {
            GLogError(
                "[ActiveArrayView] GetByID failed: ID=%u is not active",
                id);
            return nullptr;
        }

        return &array_view[id];
    }

    const T *GetByID(const DataID id) const
    {
        if (!IsActiveID(id))
        {
            GLogError(
                "[ActiveArrayView] GetByID failed: ID=%u is not active",
                id);
            return nullptr;
        }

        return &array_view[id];
    }

    bool WriteByID(const DataID id, const T &data)
    {
        T *target = GetByID(id);
        if (!target)
            return false;

        *target = data;
        array_view.MarkDirty();
        return true;
    }

    bool ReadByID(T &out_data, const DataID id) const
    {
        const T *source = GetByID(id);
        if (!source)
            return false;

        out_data = *source;
        return true;
    }

    void Commit()
    {
        array_view.Commit();
    }

    uint32_t GetSSBOId() const
    {
        return array_view.GetSSBOId();
    }

    uint32_t GetCapacity() const
    {
        return array_view.GetCount();
    }

    uint32_t GetActiveCount() const
    {
        return static_cast<uint32_t>(active_ids.GetActiveCount());
    }

private:
    ArrayView<T> &GetArrayView()
    {
        return array_view;
    }

    const ArrayView<T> &GetArrayView() const
    {
        return array_view;
    }
};
} // namespace hgl::graph
