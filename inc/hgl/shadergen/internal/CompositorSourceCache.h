#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace hgl::graph::internal {

class CompositorSourceCache
{
public:

    explicit CompositorSourceCache(std::string shader_library_path);

    const std::string &GetShaderLibraryPath() const
    {
        return shader_library_path_;
    }

    bool ReadFileCached(const std::string &rel_path, std::string &out_source, std::string &out_error) const;

private:

    std::string shader_library_path_;

    mutable std::mutex file_cache_mutex_;
    mutable std::unordered_map<std::string, std::string> file_cache_;
};

} // namespace hgl::graph::internal