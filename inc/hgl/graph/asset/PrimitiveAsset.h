#pragma once

#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/mtl/MaterialRecipe.h>

namespace hgl::graph
{
    /**
     * PrimitiveAsset — immutable asset-level pairing of Geometry + MaterialRecipe.
     *
     * Design intent:
     * - asset-only data
     * - serializable by reference (Geometry asset ref + MaterialRecipe asset ref)
     * - shared by multiple entities / runtime draw objects
     *
     * It intentionally does NOT carry runtime-only state such as:
     * - MaterialProgram / MaterialInstance
     * - DescriptorBindingSet
     * - Pipeline / VIL
     * - SSBO rows / bindless handles
     */
    class PrimitiveAsset
    {
    private:
        Geometry *geometry_ = nullptr;
        const mtl::MaterialRecipe *material_recipe_ = nullptr;

    public:
        PrimitiveAsset() = default;
        PrimitiveAsset(Geometry *geo, const mtl::MaterialRecipe *recipe)
            : geometry_(geo), material_recipe_(recipe) {}

        bool IsValid() const { return geometry_ != nullptr && material_recipe_ != nullptr; }

        Geometry *GetGeometry() const { return geometry_; }
        const mtl::MaterialRecipe *GetMaterialRecipe() const { return material_recipe_; }
    };
}//namespace hgl::graph
