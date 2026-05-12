#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/core/ComponentRecords.h>
#include<hgl/math/VectorTypes.h>
#include<hgl/type/UnorderedMap.h>
#include<glm/glm.hpp>

namespace hgl::ecs
{
    enum class BillboardScaleMode : uint8_t
    {
        WorldSize = 0,
        FixedPixelSize = 1,
    };

    class BillboardScaleComponent : public Component
    {
    private:

        BillboardScaleMode mode = BillboardScaleMode::FixedPixelSize;
        hgl::math::Vector2u pixel_size { 256, 256 };
        glm::vec2 world_size { 1.0f, 1.0f };
        float reference_world_diameter = 1.0f;
        float min_uniform_scale = 0.01f;
        bool enabled = true;
        bool scale_dirty = true;

    public:

        explicit BillboardScaleComponent(const std::string& name = "BillboardScale")
            : Component(name)
        {
        }

        virtual ~BillboardScaleComponent() = default;

    public:

        void SetFixedPixelSize(bool fixed)
        {
            const BillboardScaleMode new_mode = fixed ? BillboardScaleMode::FixedPixelSize
                                                      : BillboardScaleMode::WorldSize;
            if (mode != new_mode)
            {
                mode = new_mode;
                scale_dirty = true;
            }
        }

        bool IsFixedPixelSize() const { return mode == BillboardScaleMode::FixedPixelSize; }

        void SetScaleMode(BillboardScaleMode new_mode)
        {
            if (mode != new_mode)
            {
                mode = new_mode;
                scale_dirty = true;
            }
        }

        BillboardScaleMode GetScaleMode() const { return mode; }

        void SetPixelSize(uint32_t width, uint32_t height)
        {
            if (pixel_size.x != width || pixel_size.y != height)
            {
                pixel_size = hgl::math::Vector2u(width, height);
                scale_dirty = true;
            }
        }

        void SetPixelSize(const hgl::math::Vector2u& size)
        {
            SetPixelSize(size.x, size.y);
        }

        const hgl::math::Vector2u& GetPixelSize() const { return pixel_size; }

        void SetWorldSize(float width, float height)
        {
            const glm::vec2 new_size(width, height);
            if (world_size != new_size)
            {
                world_size = new_size;
                scale_dirty = true;
            }
        }

        void SetWorldSize(const glm::vec2& size)
        {
            SetWorldSize(size.x, size.y);
        }

        const glm::vec2& GetWorldSize() const { return world_size; }

        void SetReferenceWorldDiameter(float diameter)
        {
            if (diameter > 1e-6f && reference_world_diameter != diameter)
            {
                reference_world_diameter = diameter;
                scale_dirty = true;
            }
        }

        float GetReferenceWorldDiameter() const { return reference_world_diameter; }

        void SetMinUniformScale(float min_scale)
        {
            if (min_scale > 0.0f && min_uniform_scale != min_scale)
            {
                min_uniform_scale = min_scale;
                scale_dirty = true;
            }
        }

        float GetMinUniformScale() const { return min_uniform_scale; }

        void SetEnabled(bool e) { enabled = e; }
        bool IsEnabled() const { return enabled; }

        bool IsScaleDirty() const { return scale_dirty; }
        void ClearScaleDirty() { scale_dirty = false; }
        void MarkScaleDirty() { scale_dirty = true; }

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
