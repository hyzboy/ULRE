#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/String.h>
#include <hgl/type/UnorderedMap.h>

#include <vector>

// 说明：VkSamplerCreateInfo 是 Vulkan C 结构（trivially copyable 但无 operator==），
// 恰好落在 HGL 两容器静态断言的夹缝中：
//   - ValueArray<T>  要求 trivially copyable（满足）+ Find 需 operator==（不满足）
//   - ManagedArray<T> 要求 non-trivially copyable（不满足，其存指针且需 new/delete）
// 故此处按索引存储 Vulkan 结构采用 std::vector，仅承载外部 C API 结构、非引擎逻辑数据。

namespace hgl::graph::mtl
{
    /**
     * 全局 Sampler 预设库（进程内单例，统一注册机制的唯一权威）。
     *
     * 数据源：ShaderLibrary/sampler.toml（单一 array-of-tables，出现顺序即索引）。
     * 全链路以名字为键：
     *   运行时   BindlessTextureManager::RegisterSamplers 按序 vkCreateSampler 写 binding=1
     *   ShaderGen MaterialShaderCompiler 生成 "#define <name>Sampler <idx>u" 宏
     * 二者引用本库，保证 name→index 唯一一致（避免两处独立读文件漂移）。
     *
     * GetIndex(name) 查不到时保底返回 0（Nearest），确保 ShaderGen 不因缺失名而失败。
     * 特殊 sampler（Terrain 等）通过 RebuildSampler 运行时重建，索引/宏不变。
     */
    class SamplerPresetLibrary
    {
    public:
        static SamplerPresetLibrary &Instance();

        /** 解析 sampler.toml。失败返回 false（保留上次成功内容）。 */
        bool Load(const OSString &path);

        void Clear();

        /** 名字 → 索引（数组顺序）；查不到或空名保底 0。 */
        uint32 GetIndex(const char *name) const;

        /** 预设总数。 */
        uint32 GetCount() const;

        /** 按索引取 VkSamplerCreateInfo；越界返回 nullptr。 */
        const VkSamplerCreateInfo *GetCreateInfo(uint32 index) const;

    private:
        SamplerPresetLibrary() = default;

        std::vector<VkSamplerCreateInfo> infos_;              // 索引 = 数组顺序
        hgl::UnorderedMap<AnsiString, uint32> name_to_index_; // 名字 → 索引
    };
}//namespace hgl::graph::mtl
