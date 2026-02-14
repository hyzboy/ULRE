#include<hgl/ecs/components/StaticMesh.h>
#include<algorithm>

namespace hgl::ecs
{
    namespace
    {
        template<typename T>
        void AddUnique(std::vector<T*>& list, T* value)
        {
            if (!value)
                return;

            if (std::find(list.begin(), list.end(), value) == list.end())
                list.push_back(value);
        }
    }

    uint32_t StaticMesh::AddPrimitive(graph::Primitive* primitive,
                                      graph::MaterialInstance* mi,
                                      graph::Material* material)
    {
        PrimitiveInfo info;
        info.primitive = primitive;
        info.material_instance = mi;
        info.material = material;

        const uint32_t index = static_cast<uint32_t>(primitives.size());
        primitives.push_back(info);

        AddUnique(material_instances, mi);
        AddUnique(materials, material);

        return index;
    }

    uint32_t StaticMesh::AddNode(const std::string& name, int parent)
    {
        Node node;
        node.name = name;
        node.parent = parent;

        const uint32_t index = static_cast<uint32_t>(nodes.size());
        nodes.push_back(node);

        if (parent >= 0 && parent < static_cast<int>(nodes.size()))
            nodes[static_cast<size_t>(parent)].children.push_back(index);

        return index;
    }

    void StaticMesh::AddPrimitiveToNode(uint32_t node_index, uint32_t primitive_index)
    {
        if (node_index >= nodes.size())
            return;

        if (primitive_index >= primitives.size())
            return;

        nodes[node_index].primitive_indices.push_back(primitive_index);
    }

    void StaticMesh::SetNodeTRS(uint32_t node_index,
                                const glm::vec3& translation,
                                const glm::quat& rotation,
                                const glm::vec3& scale)
    {
        if (node_index >= nodes.size())
            return;

        Node& node = nodes[node_index];
        node.translation = translation;
        node.rotation = rotation;
        node.scale = scale;
    }

    void StaticMesh::Clear()
    {
        primitives.clear();
        materials.clear();
        material_instances.clear();
        nodes.clear();
    }
}

