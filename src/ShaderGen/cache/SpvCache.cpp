#include "SpvCache.h"

#include <hgl/util/hash/Hash.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <iostream>

namespace hgl::graph::mtl
{

SpvCache::SpvCache(const SpvCacheConfig& cfg)
    : cfg_(cfg)
{
    if (cfg_.cache_dir.empty())
        return;

    std::error_code ec;
    std::filesystem::create_directories(cfg_.cache_dir, ec);
    if (ec)
    {
        std::cerr << "[SpvCache] Failed to create cache directory '"
                  << cfg_.cache_dir << "': " << ec.message() << "\n";
        return;
    }
    enabled_ = true;
}

// ---------------------------------------------------------------------------
//  Internal helpers
// ---------------------------------------------------------------------------

std::string SpvCache::ComputeCachePath(const std::string& glsl_source, SpvStage stage) const
{
    // Build the data to hash: GLSL source + version tags + stage byte.
    // We concatenate into a single buffer to produce one hash call.
    const uint32_t vk_ver   = cfg_.vulkan_version;
    const uint32_t comp_ver = cfg_.compiler_ver;
    const uint8_t  stage_u8 = static_cast<uint8_t>(stage);

    std::string buf;
    buf.reserve(glsl_source.size() + sizeof(vk_ver) + sizeof(comp_ver) + 1);
    buf.append(glsl_source);
    buf.append(reinterpret_cast<const char*>(&vk_ver),   sizeof(vk_ver));
    buf.append(reinterpret_cast<const char*>(&comp_ver), sizeof(comp_ver));
    buf.push_back(static_cast<char>(stage_u8));

    // BLAKE3 digest = 32 bytes
    uint8_t digest[32]{};
    util::hash::Hash(util::hash::Algorithm::BLAKE3,
                     buf.data(), buf.size(),
                     digest);

    // Encode as lowercase hex
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (uint8_t b : digest)
        hex << std::setw(2) << static_cast<unsigned>(b);

    return (std::filesystem::path(cfg_.cache_dir) / (hex.str() + ".spv")).string();
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

bool SpvCache::TryLoad(const std::string& glsl_source,
                       SpvStage            stage,
                       std::vector<uint32_t>& out_spv) const
{
    if (!enabled_) return false;

    const std::string path = ComputeCachePath(glsl_source, stage);

    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) return false;

    const std::streamsize file_size = ifs.tellg();
    if (file_size <= 0 || file_size % sizeof(uint32_t) != 0)
        return false;

    ifs.seekg(0);
    out_spv.resize(static_cast<size_t>(file_size) / sizeof(uint32_t));
    ifs.read(reinterpret_cast<char*>(out_spv.data()), file_size);
    return ifs.good();
}

void SpvCache::Store(const std::string& glsl_source,
                     SpvStage            stage,
                     const std::vector<uint32_t>& spv) const
{
    if (!enabled_ || spv.empty()) return;

    const std::string path = ComputeCachePath(glsl_source, stage);

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs)
    {
        std::cerr << "[SpvCache] Failed to write cache file: " << path << "\n";
        return;
    }
    ofs.write(reinterpret_cast<const char*>(spv.data()),
              static_cast<std::streamsize>(spv.size() * sizeof(uint32_t)));
}

} // namespace hgl::graph::mtl
