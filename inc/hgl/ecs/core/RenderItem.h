#pragma once

#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/ecs/core/RuntimeTextureBinding.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<cstdint>
#include<memory>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class ShaderMaterialProgram;
        class ProgramInstanceBinding;
        class MaterialInstancePayload;
        class MaterialBindingInstance;
        class ResourceDomain;
        class VertexInputLayout;
        enum class GraphicsPipelinePreset;
    }

    namespace ecs
    {
        class Entity;
    }
}

namespace hgl::ecs
{
    // Forward declarations
    class World;
    class RenderableComponent;

    /**
     * Base RenderItem class - abstract interface for rendering
     * Similar to hgl::graph::DrawNode in the old system
     */
    class RenderItem
    {
    public:
        struct ResolvedMaterialState
        {
            hgl::graph::ProgramInstanceBinding* program_binding = nullptr;
            hgl::graph::ShaderMaterialProgram* program = nullptr;
            hgl::graph::MaterialInstancePayload* payload = nullptr;
            uint64_t binding_id = 0;
            uint64_t payload_id = 0;
            hgl::graph::MaterialBindingInstance* binding_instance = nullptr;
            hgl::graph::ShaderMaterialProgram* material = nullptr;
            hgl::graph::ResourceDomain* domain = nullptr;
            RuntimeTextureBinding runtime_texture_binding{};
            uint32_t domain_id = 0xFFFFFFFFu;
            const hgl::graph::VertexInputLayout* vil = nullptr;
            int mi_id = -1;
            hgl::graph::GraphicsPipelinePreset preset{};

            bool HasBindingInstance() const { return program_binding != nullptr || binding_instance != nullptr; }
            bool HasMaterial() const { return program != nullptr || material != nullptr; }
        };

        uint32_t index = 0;                      // Index in batch
        uint32_t transform_version = 0;          // Transform version for dirty tracking
        uint32_t transform_index = 0;            // Transform index in buffer

        glm::vec3 worldPosition{};               // World space position
        float distanceToCamera = 0.0f;           // Distance to camera for sorting
        bool isVisible = true;                   // Visibility flag

        virtual ~RenderItem() = default;

        // Abstract interface - returns EntityID and entity pointer
        virtual EntityID GetEntityID() const = 0;
        virtual Entity* GetEntity() const = 0;
        virtual std::shared_ptr<TransformComponent> GetTransform() const = 0;
        virtual std::shared_ptr<RenderableComponent> GetRenderable() const = 0;
        virtual glm::mat4 GetWorldMatrix() const = 0;

        // For material batching support
        virtual hgl::graph::Primitive* GetPrimitive() const = 0;
        // Compatibility accessor: prefer GetResolvedMaterialState().binding_instance in runtime paths.
        virtual hgl::graph::MaterialBindingInstance* GetResolvedBindingInstance() const = 0;
        // Compatibility accessor: prefer GetResolvedMaterialState().material in runtime paths.
        virtual hgl::graph::ShaderMaterialProgram* GetShaderMaterialProgram() const = 0;
        // Unified runtime source of truth for material-related state.
        virtual ResolvedMaterialState GetResolvedMaterialState() const;

        // Comparison for sorting
        virtual int Compare(const RenderItem& other) const;
    };

}//namespace hgl::ecs

