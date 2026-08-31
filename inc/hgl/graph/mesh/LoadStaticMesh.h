#pragma once

#include <hgl/type/String.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/mesh/StaticMesh.h>
#include <vector>

namespace hgl::graph
{
    class VulkanDevice;
    class GeometryManager;
    class GeometryVertexFormat;

    bool LoadStaticMeshSceneAsPrimitiveAssets(
        VulkanDevice                     *device,
        GeometryManager                  *geo_mgr,
        const GeometryVertexFormat       &geometry_vertex_format,
        const mtl::MaterialRecipe        *recipe,
        const OSString                   &pack_path,
        const OSString                   &base_dir,
        std::vector<PrimitiveAsset>      &out_assets,
        std::vector<StaticMeshNode>      &out_nodes,
        std::vector<int32_t>             &out_root_nodes);

}//namespace hgl::graph
