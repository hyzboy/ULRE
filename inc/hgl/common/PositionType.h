#pragma once

namespace hgl::graph
{
    /// Describes the dimensionality / source of the Position vertex attribute.
    enum class PositionType : uint8_t
    {
        None = 0,   ///< No position input (e.g. purely procedural / PCG)
        Vec2,       ///< 2-component position (x, y); preprocessor pads to vec3(x,y,0)
        Vec3,       ///< 3-component position (x, y, z); passed through as-is
        PCG,        ///< Procedurally-generated position
    };
}//namespace hgl::graph
