#include <hgl/shadergen/SPVCache.h>
#include <fstream>

namespace hgl::graph
{
    void SPVCache::Store(const SPVCacheKey &key, CompiledSPV spv)
    {
        cache_[key] = std::move(spv);
    }

    const CompiledSPV *SPVCache::Lookup(const SPVCacheKey &key) const
    {
        auto it = cache_.find(key);
        if (it == cache_.end())
            return nullptr;
        return &it->second;
    }

    // 简单二进制格式：
    //   [4 bytes] magic "SPVC"
    //   [4 bytes] entry_count
    //   per entry:
    //     [2 bytes] preset_id
    //     [2 bytes] packed_key
    //     [1 byte]  pass_type
    //     [4 bytes] vs_spv_dword_count
    //     [N*4 bytes] vs_spv_data
    //     [4 bytes] fs_spv_dword_count
    //     [N*4 bytes] fs_spv_data

    static constexpr uint32_t SPV_CACHE_MAGIC = 0x43565053; // "SPVC"

    bool SPVCache::SaveToFile(const std::string &path) const
    {
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs.is_open())
            return false;

        auto write = [&](const void *data, size_t size) { ofs.write(reinterpret_cast<const char*>(data), size); };

        uint32_t magic = SPV_CACHE_MAGIC;
        uint32_t count = static_cast<uint32_t>(cache_.size());
        write(&magic, 4);
        write(&count, 4);

        for (const auto &[key, spv] : cache_)
        {
            write(&key.preset_id, 2);
            write(&key.packed_key, 2);
            uint8_t pt = static_cast<uint8_t>(key.pass_type);
            write(&pt, 1);

            uint32_t vs_count = static_cast<uint32_t>(spv.vertex_spv.size());
            write(&vs_count, 4);
            if (vs_count > 0)
                write(spv.vertex_spv.data(), vs_count * sizeof(uint32_t));

            uint32_t fs_count = static_cast<uint32_t>(spv.fragment_spv.size());
            write(&fs_count, 4);
            if (fs_count > 0)
                write(spv.fragment_spv.data(), fs_count * sizeof(uint32_t));
        }

        return ofs.good();
    }

    bool SPVCache::LoadFromFile(const std::string &path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open())
            return false;

        auto read = [&](void *data, size_t size) { ifs.read(reinterpret_cast<char*>(data), size); };

        uint32_t magic = 0;
        uint32_t count = 0;
        read(&magic, 4);
        if (magic != SPV_CACHE_MAGIC)
            return false;

        read(&count, 4);

        cache_.clear();

        for (uint32_t i = 0; i < count; ++i)
        {
            SPVCacheKey key{};
            read(&key.preset_id, 2);
            read(&key.packed_key, 2);
            uint8_t pt = 0;
            read(&pt, 1);
            key.pass_type = static_cast<PassType>(pt);

            CompiledSPV spv{};
            spv.success = true;

            uint32_t vs_count = 0;
            read(&vs_count, 4);
            if (vs_count > 0)
            {
                spv.vertex_spv.resize(vs_count);
                read(spv.vertex_spv.data(), vs_count * sizeof(uint32_t));
            }

            uint32_t fs_count = 0;
            read(&fs_count, 4);
            if (fs_count > 0)
            {
                spv.fragment_spv.resize(fs_count);
                read(spv.fragment_spv.data(), fs_count * sizeof(uint32_t));
            }

            if (!ifs.good())
                return false;

            cache_[key] = std::move(spv);
        }

        return true;
    }
}
