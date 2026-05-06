#pragma once

namespace hgl::graph::mtl {
class MaterialCreateInfo;
}

namespace hgl::graph::mtl::internal {

bool ApplyCompositorLayoutDefines(MaterialCreateInfo &mci);

} // namespace hgl::graph::mtl::internal