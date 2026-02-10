#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/variant.hpp>

namespace hgl::ecs
{
    struct TransformRecord
    {
        std::array<float, 3> position{};
        std::array<float, 4> rotation{};
        std::array<float, 3> scale{};
        bool movable = true;
        int32_t parentIndex = -1;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(position), CEREAL_NVP(rotation), CEREAL_NVP(scale),
               CEREAL_NVP(movable), CEREAL_NVP(parentIndex));
        }
    };

    struct BoundingBoxRecord
    {
        std::array<float, 3> min{};
        std::array<float, 3> max{};
        bool hasWorld = false;
        std::array<float, 3> worldMin{};
        std::array<float, 3> worldMax{};

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(min), CEREAL_NVP(max), CEREAL_NVP(hasWorld),
               CEREAL_NVP(worldMin), CEREAL_NVP(worldMax));
        }
    };

    struct RenderableRecord
    {
        bool visible = true;
        float boundingRadius = 1.0f;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(visible), CEREAL_NVP(boundingRadius));
        }
    };

    struct PrimitiveRecord
    {
        RenderableRecord renderable;
        bool hasPrimitive = false;
        bool hasOverrideMaterial = false;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(renderable), CEREAL_NVP(hasPrimitive), CEREAL_NVP(hasOverrideMaterial));
        }
    };

    struct CameraRecord
    {
        std::array<float, 3> position{};
        std::array<float, 3> target{};
        std::array<float, 3> worldUp{};

        float fov = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;

        float yaw = 0.0f;
        float pitch = 0.0f;
        float roll = 0.0f;

        std::array<float, 3> forward{};
        std::array<float, 3> right{};
        std::array<float, 3> up{};

        int controlMode = 0;

        float distance = 0.0f;
        float minDistance = 0.0f;
        float maxDistance = 0.0f;

        float rotationSensitivity = 0.0f;
        float zoomSensitivity = 0.0f;
        float moveSpeed = 0.0f;

        std::array<float, 2> inputInvert{};

        bool isMainCamera = false;
        bool matrixDirty = false;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(position), CEREAL_NVP(target), CEREAL_NVP(worldUp),
               CEREAL_NVP(fov), CEREAL_NVP(nearPlane), CEREAL_NVP(farPlane),
               CEREAL_NVP(yaw), CEREAL_NVP(pitch), CEREAL_NVP(roll),
               CEREAL_NVP(forward), CEREAL_NVP(right), CEREAL_NVP(up),
               CEREAL_NVP(controlMode), CEREAL_NVP(distance),
               CEREAL_NVP(minDistance), CEREAL_NVP(maxDistance),
               CEREAL_NVP(rotationSensitivity), CEREAL_NVP(zoomSensitivity),
               CEREAL_NVP(moveSpeed), CEREAL_NVP(inputInvert),
               CEREAL_NVP(isMainCamera), CEREAL_NVP(matrixDirty));
        }
    };

    using ComponentPayload = std::variant<TransformRecord, BoundingBoxRecord, RenderableRecord, PrimitiveRecord, CameraRecord>;

    struct ComponentRecord
    {
        std::string type;
        ComponentPayload payload;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(type), CEREAL_NVP(payload));
        }
    };

    struct EntityRecord
    {
        std::string name;
        std::vector<ComponentRecord> components;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(name), CEREAL_NVP(components));
        }
    };

    struct WorldRecord
    {
        std::vector<EntityRecord> entities;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(entities));
        }
    };
}
