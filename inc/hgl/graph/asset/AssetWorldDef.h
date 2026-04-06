#pragma once

#include <hgl/graph/mesh/StaticMesh.h>
#include <memory>
#include <string>

namespace hgl::graph
{
    /**
     * AssetWorldDef — one registered static-mesh asset definition.
     *
     * Owns the StaticMesh (primitive geometry + material bindings).
     * Instances in the ECS world refer to this by ID; the definition
     * itself lives at Application / AssetWorldRegistry level, completely
     * outside the ECS.
     */
    struct AssetWorldDef
    {
        using ID = uint32_t;
        static constexpr ID INVALID_ID = 0;

        ID              id   = INVALID_ID;
        std::string     name;
        std::shared_ptr<StaticMesh> mesh;

        AssetWorldDef() = default;

        AssetWorldDef(ID id_, std::string name_, std::shared_ptr<StaticMesh> mesh_)
            : id(id_), name(std::move(name_)), mesh(std::move(mesh_))
        {
        }

        bool IsValid() const { return id != INVALID_ID && mesh != nullptr; }
    };

}//namespace hgl::graph
