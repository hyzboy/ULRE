#pragma once

#include <hgl/type/UnorderedMap.h>
#include <hgl/type/ValueArray.h>

namespace hgl::ecs
{
    class SystemScheduleState
    {
    public:
        using DependencyMap = UnorderedMap<size_t, ValueArray<size_t>>;

    private:
        DependencyMap tick_dependencies;
        DependencyMap render_dependencies;
        bool tick_dirty = false;
        bool render_dirty = false;
        uint64_t next_insertion_order = 1;

        DependencyMap &MutableDependencies(bool render)
        {
            return render ? render_dependencies : tick_dependencies;
        }

    public:
        const DependencyMap &Dependencies(bool render) const
        {
            return render ? render_dependencies : tick_dependencies;
        }

        bool &Dirty(bool render) { return render ? render_dirty : tick_dirty; }
        uint64_t NextInsertionOrder() { return next_insertion_order++; }

        void RemoveDependencies(bool render, size_t key)
        {
            MutableDependencies(render).DeleteByKey(key);
        }

        void AddDependency(bool render, size_t dependent, size_t dependency)
        {
            if (dependent == dependency)
                return;
            auto &list = MutableDependencies(render)[dependent];
            if (!list.Contains(dependency))
            {
                list.Add(dependency);
                Dirty(render) = true;
            }
        }

        void ClearDependencies()
        {
            tick_dependencies.Clear();
            render_dependencies.Clear();
        }
    };
}
