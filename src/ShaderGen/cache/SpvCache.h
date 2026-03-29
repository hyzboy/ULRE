#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstdint>

namespace hgl::graph::mtl
{
    /// Configuration for the SPV cache directory and versioning.
    struct SpvCacheConfig
    {
        std::string cache_dir;      // Root directory for cached .spv files
        uint32_t    vulkan_version  = 0; // e.g. VK_API_VERSION_1_3 — invalidates cache on upgrade
        uint32_t    compiler_ver    = 0; // ShaderGen internal version — invalidates cache on change
    };

    /// Which shader stage a given SPIR-V binary belongs to.
    enum class SpvStage : uint8_t
    {
        Vertex   = 0,
        Fragment = 1,
    };

    /// Content-addressed SPIR-V cache.
    ///
    /// Cache key  = BLAKE3(glsl_source || vulkan_version || compiler_ver)
    /// Cache file = <cache_dir>/<hex_digest>.spv
    ///
    /// Thread safety: each SpvCache instance must be used from a single thread.
    class SpvCache
    {
    public:
        explicit SpvCache(const SpvCacheConfig& cfg);

        /// Returns false if the cache directory is not accessible.
        bool IsEnabled() const { return enabled_; }

        /// Try to load a cached SPV binary for the given GLSL source.
        /// @param glsl_source Full GLSL text (after all includes / defines have been applied).
        /// @param stage       Vertex or Fragment (baked into the hash to avoid collisions).
        /// @param out_spv     Filled on cache hit.
        /// @return true on cache hit.
        bool TryLoad(const std::string& glsl_source,
                     SpvStage            stage,
                     std::vector<uint32_t>& out_spv) const;

        /// Store a compiled SPV binary under the content-derived cache key.
        /// @param glsl_source The same GLSL source that was compiled.
        /// @param stage       Vertex or Fragment.
        /// @param spv         The compiled SPIR-V words.
        void Store(const std::string& glsl_source,
                   SpvStage            stage,
                   const std::vector<uint32_t>& spv) const;

    private:
        std::string ComputeCachePath(const std::string& glsl_source, SpvStage stage) const;

        SpvCacheConfig cfg_;
        bool           enabled_ = false;
    };

} // namespace hgl::graph::mtl
