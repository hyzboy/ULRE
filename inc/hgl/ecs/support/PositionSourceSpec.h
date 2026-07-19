#pragma once

#include <cstdint>

namespace hgl::ecs
{
    // Defines how local/object-space positions are produced before transform policy is applied.
    enum class PositionSourceSpec : uint8_t
    {
        MeshVertex = 0,        // Read position directly from mesh vertex attributes
        Quad2DGenerated,       // Generate unit quad positions from 2D source
        TerrainHeightmapGrid,  // Generate 3D local positions from heightmap + regular grid
        ProceduralGenerated,   // Generate positions procedurally in shader/runtime path
    };
}
