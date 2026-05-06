#include <hgl/shadergen/internal/CompositorSourceCache.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>

namespace hgl::graph::internal {

CompositorSourceCache::CompositorSourceCache(std::string shader_library_path)
    : shader_library_path_(std::move(shader_library_path))
{}

bool CompositorSourceCache::ReadFileCached(
    const std::string &rel_path,
    std::string &out_source,
    std::string &out_error) const
{
    const std::string full_path = shader_library_path_ + "/" + rel_path;
    {
        std::lock_guard<std::mutex> lock(file_cache_mutex_);
        const auto it = file_cache_.find(full_path);
        if (it != file_cache_.end())
        {
            out_source = it->second;
            return true;
        }
    }

    std::string source;
    if (!ReadTextFile(full_path, source, out_error))
        return false;

    std::lock_guard<std::mutex> lock(file_cache_mutex_);
    file_cache_.emplace(full_path, source);
    out_source = std::move(source);
    return true;
}

} // namespace hgl::graph::internal
