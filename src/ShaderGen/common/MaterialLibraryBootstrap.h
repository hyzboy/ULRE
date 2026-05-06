#pragma once

namespace hgl::graph::mtl::bootstrap
{

// Ensure one-time startup initialization required by MaterialLibrary dispatch.
// Safe to call repeatedly from any entry path.
void EnsureMaterialLibraryBootstrap();

} // namespace hgl::graph::mtl::bootstrap
