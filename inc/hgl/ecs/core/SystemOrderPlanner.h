#pragma once

#include <hgl/type/UnorderedMap.h>
#include <hgl/type/ValueArray.h>

namespace hgl::ecs
{
    class SystemOrderPlanner
    {
        struct Node
        {
            int phase;
            int group;
            uint64_t insertion_order;
            int first_edge;
            uint32_t indegree;
            bool emitted;
        };

        struct Edge
        {
            int target;
            int next;
        };

        ValueArray<Node> nodes;
        ValueArray<Edge> edges;
        UnorderedMap<size_t, int> node_indices;

        bool ComesBefore(int left, int right) const;

    public:
        enum class DependencyResult
        {
            Added,
            MissingDependent,
            MissingDependency
        };

        void AddSystem(size_t key, int phase, int group, uint64_t insertion_order);
        DependencyResult AddDependency(size_t dependent, size_t dependency);
        bool Build(ValueArray<int> &order);
    };
}
