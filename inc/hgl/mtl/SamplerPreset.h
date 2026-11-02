#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/String.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/type/ManagedArray.h>

#include <vector>

// 说明：VkSamplerCreateInfo 是 Vulkan C 结构（trivially copyable 但无 operator==），
// 恰好落在 HGL 两容器静态断言的夹缝中：
//   - ValueArray<T>  要求 trivially copyable（满足）+ Find 需 operator==（不满足）
//   - ManagedArray<T> 要求 non-trivially copyable（不满足，其存指针且需 new/delete）
// 故此处按索引存储 Vulkan 结构采用 std::vector，仅承载外部 C API 结构、非引擎逻辑数据。

#include <hgl/asset/AssetAnnotation.h>

namespace hgl::graph::mtl
{
    /**
     * contract/sampler.toml 里一个 [[samplers]] 条目的字段规约（文本契约层）。
     *
     * 由 `[group.SamplerPreset]`（src/Tools/AssetGen/asset_gen.toml）生成序列化器，
     * 缺键 = 保留这里的类内缺省值；枚举一律用文本（空串 = 引擎默认），
     * 非法拼写在解析层报错（拼写真源 = hgl/mtl/VkEnumText.h）。
     * 与 VkSamplerCreateInfo 的转换在 SamplerPresetLibrary::Load 中完成。
     */
    struct HGL_ASSET_STRUCT SamplerPresetSpec
    {
        AnsiString name;                    // 组内唯一，必填
        AnsiString mag_filter;              // 空 = Linear
        AnsiString min_filter;              // 空 = Linear
        AnsiString mipmap_mode;             // 空 = Linear
        AnsiString address_mode;            // 空 = Repeat（同时作用 U/V/W）
        bool   anisotropy = false;
        float  max_anisotropy = 16.0f;      // 仅 anisotropy=true 时生效
        float  max_lod = 15.0f;
        AnsiString compare_op;              // 空 = 不做比较采样
    };

    /// sampler.toml 文件根规约：schema 版本 + 采样器条目表。
    struct HGL_ASSET_STRUCT SamplerPresetFileSpec
    {
        uint32 schema = 1;
        hgl::ManagedArray<SamplerPresetSpec> samplers;
    };

    /**
     * 全局 Sampler 预设库（进程内单例，统一注册机制的唯一权威）。
     *
     * 数据源：ShaderLibrary/contract/sampler.toml（单一 array-of-tables，出现顺序即索引）。
     * 全链路以名字为键：
     *   运行时   BindlessTextureManager::RegisterSamplers 按序 vkCreateSampler 写 binding=1
     *   ShaderGen StageSPVCompiler 生成 "#define <name>Sampler <idx>u" 宏
     * 二者引用本库，保证 name→index 唯一一致（避免两处独立读文件漂移）。
     *
     * GetIndex(name) 查不到时保底返回 0（Nearest），确保 ShaderGen 不因缺失名而失败。
     * 索引一经注册即固定：运行时不存在替换单个 sampler 槽位的路径（如需动态 sampler，
     * 应重新注册整组，见 RegisterSamplers）。
     */
    class SamplerPresetLibrary
    {
    public:
        static SamplerPresetLibrary &Instance();

        /** 解析 contract/sampler.toml。失败返回 false（保留上次成功内容）。 */
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
