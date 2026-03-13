#pragma once

#include <hgl/shadergen/PresetShaderCompiler.h>
#include <map>
#include <string>
#include <vector>

namespace hgl::graph
{
    /**
     * SPVCache — 构建期 SPV 缓存
     *
     * 功能（第一版）：
     *   1. 内存中的 Map<SPVCacheKey, CompiledSPV> 查表
     *   2. Store(key, spv) / Lookup(key) → CompiledSPV*
     *   3. SaveToFile(path) / LoadFromFile(path) — 序列化支持
     */
    class SPVCache
    {
    public:

        /// 存入缓存
        void Store(const SPVCacheKey &key, CompiledSPV spv);

        /// 查找缓存，未命中返回 nullptr
        const CompiledSPV *Lookup(const SPVCacheKey &key) const;

        /// 缓存条目数
        size_t Size() const { return cache_.size(); }

        /// 清空缓存
        void Clear() { cache_.clear(); }

        /// 序列化到文件
        bool SaveToFile(const std::string &path) const;

        /// 从文件反序列化
        bool LoadFromFile(const std::string &path);

    private:

        std::map<SPVCacheKey, CompiledSPV> cache_;
    };
}
