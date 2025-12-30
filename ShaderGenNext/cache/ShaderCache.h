#pragma once

#include "Result.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace hgl::shader_next {

/**
 * 缓存策略
 */
enum class CachePolicy {
    MemoryOnly,      // 仅内存缓存
    DiskOnly,        // 仅磁盘缓存
    MemoryAndDisk,   // 内存 + 磁盘
    Distributed      // 分布式缓存（可选，用于团队协作）
};

/**
 * 压缩类型
 */
enum class CompressionType {
    None,
    LZ4,         // 快速压缩
    Zstandard,   // 平衡压缩
    LZMA         // 高压缩比
};

/**
 * 缓存选项
 */
struct CacheOptions {
    bool memory_cache = true;
    bool disk_cache = true;
    std::string cache_dir = ".shader_cache";
    size_t max_memory_size = 256 * 1024 * 1024;  // 256 MB
    CompressionType compression = CompressionType::LZ4;
    int compression_level = 5;  // 1-9
    
    // 磁盘缓存选项
    size_t max_disk_size = 2LL * 1024 * 1024 * 1024;  // 2 GB
    int max_age_days = 30;  // 缓存有效期
    
    // 分布式缓存选项（可选）
    bool enable_network_cache = false;
    std::string network_cache_url;
};

/**
 * 缓存统计信息
 */
struct CacheStats {
    size_t total_requests = 0;
    size_t memory_hits = 0;
    size_t disk_hits = 0;
    size_t network_hits = 0;
    size_t misses = 0;
    
    size_t memory_size = 0;
    size_t disk_size = 0;
    
    double hit_rate() const {
        if (total_requests == 0) return 0.0;
        return double(memory_hits + disk_hits + network_hits) / total_requests;
    }
    
    double memory_hit_rate() const {
        if (total_requests == 0) return 0.0;
        return double(memory_hits) / total_requests;
    }
};

/**
 * 缓存清理策略
 */
enum class CleanupPolicy {
    LRU,         // 最近最少使用
    LFU,         // 最不常使用
    FIFO,        // 先进先出
    ByAge,       // 按年龄清理
    BySize       // 按大小清理
};

/**
 * 缓存条目
 */
struct CacheEntry {
    uint64_t hash;
    std::vector<uint8_t> data;
    uint64_t timestamp;
    size_t access_count;
    size_t compressed_size;
    
    CacheEntry() = default;
    CacheEntry(uint64_t h, std::vector<uint8_t> d, uint64_t ts = 0)
        : hash(h), data(std::move(d)), timestamp(ts), access_count(0),
          compressed_size(data.size()) {}
};

/**
 * ShaderCache - 多层着色器缓存系统
 * 
 * 架构:
 * L1: 内存缓存 (最快，容量小)
 * L2: 磁盘缓存 (快速，容量中等)
 * L3: 网络缓存 (慢，但可共享)
 * 
 * 使用示例:
 * ```cpp
 * auto cache = ShaderCache::create(CacheOptions{
 *     .memory_cache = true,
 *     .disk_cache = true,
 *     .cache_dir = ".shader_cache",
 *     .max_memory_size = 256_MB
 * });
 * 
 * // 查询缓存
 * auto result = cache->get(shader_hash);
 * if (result.isSome()) {
 *     auto cached_data = result.unwrap();
 *     // 使用缓存的数据
 * } else {
 *     // 编译并存入缓存
 *     auto compiled = compiler->compile(source);
 *     cache->put(shader_hash, compiled);
 * }
 * ```
 */
class ShaderCache {
private:
    CacheOptions options_;
    CacheStats stats_;
    
    // L1: 内存缓存
    std::unordered_map<uint64_t, CacheEntry> memory_cache_;
    
    // L2: 磁盘缓存路径
    std::string disk_cache_path_;
    
public:
    explicit ShaderCache(CacheOptions options);
    ~ShaderCache();
    
    static std::shared_ptr<ShaderCache> create(CacheOptions options = CacheOptions{}) {
        return std::make_shared<ShaderCache>(std::move(options));
    }
    
    // 查询缓存
    Option<std::vector<uint8_t>> get(uint64_t hash);
    
    // 存入缓存
    Result<void, Error> put(uint64_t hash, const std::vector<uint8_t>& data);
    
    // 批量预热缓存
    Result<void, Error> preheat(const std::vector<uint64_t>& hashes);
    
    // 检查是否存在
    bool contains(uint64_t hash) const;
    
    // 清除特定条目
    bool remove(uint64_t hash);
    
    // 清空所有缓存
    void clear();
    
    // 清理策略
    void cleanup(CleanupPolicy policy);
    
    // 统计信息
    const CacheStats& getStats() const { return stats_; }
    void resetStats() { stats_ = CacheStats{}; }
    
    // 配置管理
    const CacheOptions& getOptions() const { return options_; }
    void setOptions(CacheOptions options);
    
private:
    // L1: 内存缓存操作
    Option<std::vector<uint8_t>> getFromMemory(uint64_t hash);
    void putToMemory(uint64_t hash, const std::vector<uint8_t>& data);
    void evictFromMemory();
    
    // L2: 磁盘缓存操作
    Option<std::vector<uint8_t>> getFromDisk(uint64_t hash);
    Result<void, Error> putToDisk(uint64_t hash, const std::vector<uint8_t>& data);
    std::string getDiskCachePath(uint64_t hash) const;
    
    // L3: 网络缓存操作（可选）
    Option<std::vector<uint8_t>> getFromNetwork(uint64_t hash);
    Result<void, Error> putToNetwork(uint64_t hash, const std::vector<uint8_t>& data);
    
    // 压缩/解压缩
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const;
    
    // 清理辅助函数
    void cleanupByLRU(size_t target_size);
    void cleanupByAge(int max_age_days);
};

/**
 * 缓存键生成器
 */
class CacheKeyGenerator {
public:
    // 根据源码和编译选项生成哈希
    static uint64_t generate(
        const std::string& source,
        const std::string& compile_options
    );
    
    // 64位 FNV-1a 哈希
    static uint64_t hash64(const void* data, size_t len);
    
    // 结合多个哈希值
    static uint64_t combine(uint64_t h1, uint64_t h2);
};

} // namespace hgl::shader_next
