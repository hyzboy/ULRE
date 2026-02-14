#pragma once

#include<string>
#include<vector>
#include<cstdint>
#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class Material;
        class MaterialInstance;
    }
}

namespace hgl::ecs
{
    class StaticMesh
    {
    public:
        struct PrimitiveInfo
        {
            graph::Primitive* primitive = nullptr;
            graph::Material* material = nullptr;
            graph::MaterialInstance* material_instance = nullptr;
        };

        struct Node
        {
            std::string name;
            int parent = -1;
            std::vector<uint32_t> children;
            std::vector<uint32_t> primitive_indices;
            glm::vec3 translation{0.0f, 0.0f, 0.0f};
            glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
            glm::vec3 scale{1.0f, 1.0f, 1.0f};
        };

    private:
        std::vector<PrimitiveInfo> primitives;
        std::vector<graph::Material*> materials;
        std::vector<graph::MaterialInstance*> material_instances;
        std::vector<Node> nodes;

    public:
        StaticMesh() = default;
        ~StaticMesh() = default;

    public:
        uint32_t AddPrimitive(graph::Primitive* primitive,
                              graph::MaterialInstance* mi = nullptr,
                              graph::Material* material = nullptr);

        uint32_t AddNode(const std::string& name, int parent = -1);
        void AddPrimitiveToNode(uint32_t node_index, uint32_t primitive_index);
        void SetNodeTRS(uint32_t node_index,
                        const glm::vec3& translation,
                        const glm::quat& rotation,
                        const glm::vec3& scale);

        void Clear();

        const std::vector<Node>& GetNodes() const { return nodes; }
        const std::vector<PrimitiveInfo>& GetPrimitives() const { return primitives; }
        const std::vector<graph::Material*>& GetMaterials() const { return materials; }
        const std::vector<graph::MaterialInstance*>& GetMaterialInstances() const { return material_instances; }
    };
}
