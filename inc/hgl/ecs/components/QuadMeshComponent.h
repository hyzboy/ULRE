#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/core/ComponentRecords.h>
#include<hgl/type/UnorderedMap.h>
#include<glm/glm.hpp>
#include<vulkan/vulkan.h>

namespace hgl::ecs
{
    class QuadMeshComponent : public Component
    {
    private:

        glm::vec2 size { 1.0f, 1.0f };
        glm::vec2 pivot { 0.5f, 0.5f };
        glm::vec4 uv_rect { 0.0f, 0.0f, 1.0f, 1.0f };
        VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE;
        bool geometry_dirty = true;

    public:

        explicit QuadMeshComponent(const std::string& name = "QuadMesh")
            : Component(name)
        {
        }

        virtual ~QuadMeshComponent() = default;

    public:

        const char* GetSystemGroupName() const override { return "Primitive"; }

        void SetSize(float width, float height)
        {
            const glm::vec2 new_size(width, height);
            if (size != new_size)
            {
                size = new_size;
                geometry_dirty = true;
            }
        }

        glm::vec2 GetSize() const { return size; }

        void SetPivot(float x, float y)
        {
            const glm::vec2 new_pivot(x, y);
            if (pivot != new_pivot)
            {
                pivot = new_pivot;
                geometry_dirty = true;
            }
        }

        glm::vec2 GetPivot() const { return pivot; }

        void SetUVRect(float u0, float v0, float u1, float v1)
        {
            const glm::vec4 new_uv_rect(u0, v0, u1, v1);
            if (uv_rect != new_uv_rect)
            {
                uv_rect = new_uv_rect;
                geometry_dirty = true;
            }
        }

        glm::vec4 GetUVRect() const { return uv_rect; }

        void SetFrontFace(VkFrontFace face)
        {
            if (front_face != face)
            {
                front_face = face;
                geometry_dirty = true;
            }
        }

        VkFrontFace GetFrontFace() const { return front_face; }

        bool IsGeometryDirty() const { return geometry_dirty; }
        void ClearGeometryDirty() { geometry_dirty = false; }
        void MarkGeometryDirty() { geometry_dirty = true; }

    public:

        void OnAttach() override;
        void OnUpdate(float deltaTime) override;
        void OnDetach() override;

        static const char* GetSerializationType();
        static bool SerializeToRecord(const std::shared_ptr<Component>& component,
                                      const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                      ComponentRecord& out_record);
        static void DeserializeFromRecord(const ComponentRecord& record,
                                          Entity* entity,
                                          std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents);
    };
}//namespace hgl::ecs
