#include <hgl/ecs/core/system/SystemOrderPlanner.h>

namespace hgl::ecs
{
    bool SystemOrderPlanner::ComesBefore(int left, int right) const
    {
        const Node &a = nodes[left];
        const Node &b = nodes[right];
        if (a.phase != b.phase)
            return a.phase < b.phase;
        if (a.group != b.group)
            return a.group < b.group;
        if (a.insertion_order != b.insertion_order)
            return a.insertion_order < b.insertion_order;
        return left < right;
    }

    void SystemOrderPlanner::AddSystem(size_t key, int phase, int group, uint64_t insertion_order)
    {
        const int index = nodes.Add(Node{phase, group, insertion_order, -1, 0, false});
        node_indices[key] = index;
    }

    SystemOrderPlanner::DependencyResult SystemOrderPlanner::AddDependency(size_t dependent, size_t dependency)
    {
        const int *target = node_indices.GetValuePointer(dependent);
        if (!target)
            return DependencyResult::MissingDependent;
        const int *source = node_indices.GetValuePointer(dependency);
        if (!source)
            return DependencyResult::MissingDependency;
        Node &node = nodes[*source];
        node.first_edge = edges.Add(Edge{*target, node.first_edge});
        return DependencyResult::Added;
    }

    bool SystemOrderPlanner::Build(ValueArray<int> &order)
    {
        order.Clear();
        order.Reserve(nodes.GetCount());
        for (Node &node : nodes)
        {
            node.indegree = 0;
            node.emitted = false;
        }
        for (const Edge &edge : edges)
            ++nodes[edge.target].indegree;

        while (order.GetCount() < nodes.GetCount())
        {
            int selected = -1;
            for (int i = 0; i < nodes.GetCount(); ++i)
                if (!nodes[i].emitted && nodes[i].indegree == 0
                    && (selected < 0 || ComesBefore(i, selected)))
                    selected = i;
            if (selected < 0)
                break;

            nodes[selected].emitted = true;
            order.Add(selected);
            for (int edge = nodes[selected].first_edge; edge >= 0; edge = edges[edge].next)
                --nodes[edges[edge].target].indegree;
        }

        if (order.GetCount() == nodes.GetCount())
            return true;

        order.Clear();
        for (Node &node : nodes)
            node.emitted = false;
        while (order.GetCount() < nodes.GetCount())
        {
            int selected = -1;
            for (int i = 0; i < nodes.GetCount(); ++i)
                if (!nodes[i].emitted && (selected < 0 || ComesBefore(i, selected)))
                    selected = i;
            nodes[selected].emitted = true;
            order.Add(selected);
        }
        return false;
    }
}
