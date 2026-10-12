#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/VkEnumText.h>

#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/type/Smart.h>
#include <toml/toml.hpp>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
    }//anonymous namespace

    SamplerPresetLibrary &SamplerPresetLibrary::Instance()
    {
        static SamplerPresetLibrary instance;
        return instance;
    }

    void SamplerPresetLibrary::Clear()
    {
        infos_.clear();
        name_to_index_.Clear();
    }

    uint32 SamplerPresetLibrary::GetIndex(const char *name) const
    {
        // ~0u = 显式无效值：调用方必须处理查不到的情况。
        // 此前保底 0（=Nearest）会把"未声明采样器"静默错位成 Nearest，
        // 视觉问题难排查——显式无效让调用方报错暴露。
        if (!name || !name[0])
            return ~0u;
        const AnsiString key(name);
        const uint32 *idx = name_to_index_.GetValuePointer(key);
        return idx ? *idx : ~0u;
    }

    uint32 SamplerPresetLibrary::GetCount() const
    {
        return static_cast<uint32>(infos_.size());
    }

    const VkSamplerCreateInfo *SamplerPresetLibrary::GetCreateInfo(uint32 index) const
    {
        const size_t i = static_cast<size_t>(index);
        if (i >= infos_.size())
            return nullptr;
        return &infos_[i];
    }

    bool SamplerPresetLibrary::Load(const OSString &path)
    {
        hgl::io::OpenFileInputStream opener(path);
        if (!opener)
        {
            GLogError("[SamplerPreset] 打开失败: %s", path.c_str());
            return false;
        }
        const int64 size = opener->GetSize();
        if (size <= 0)
        {
            GLogError("[SamplerPreset] 文件为空: %s", path.c_str());
            return false;
        }

        hgl::AutoDeleteArray<char> buffer(size_t(size) + 1);
        if (!buffer || opener->Read(buffer.data(), size) != size)
        {
            GLogError("[SamplerPreset] 读取失败: %s", path.c_str());
            return false;
        }
        buffer[size_t(size)] = 0;

        toml::value root;
        try
        {
            root = toml::parse_str(std::string(buffer.data(), size_t(size)));
        }
        catch (const toml::exception &)
        {
            GLogError("[SamplerPreset] TOML 解析失败: %s", path.c_str());
            return false;
        }

        if (!root.is_table()
         || !root.contains("samplers")
         || !root.at("samplers").is_array())
        {
            GLogError("[SamplerPreset] 缺少 samplers 数组: %s", path.c_str());
            return false;
        }

        // 先解析到临时容器，全部成功后原子替换（失败保留上次成功内容）。
        std::vector<VkSamplerCreateInfo> new_infos;
        hgl::UnorderedMap<AnsiString, uint32> new_map;

        const toml::array &arr = root.at("samplers").as_array();
        new_infos.reserve(arr.size());
        const uint32 count = static_cast<uint32>(arr.size());
        for (uint32 i = 0; i < count; ++i)
        {
            const toml::value &item = arr[i];
            if (!item.is_table()
             || !item.contains("name") || !item.at("name").is_string())
            {
                GLogError("[SamplerPreset] 第 %u 项缺少 name", i);
                return false;
            }

            const std::string name = item.at("name").as_string();
            if (name.empty())
            {
                GLogError("[SamplerPreset] 第 %u 项 name 为空", i);
                return false;
            }

            VkSamplerCreateInfo sci{};
            sci.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sci.pNext                   = nullptr;
            sci.flags                   = 0;
            sci.magFilter               = VK_FILTER_LINEAR;
            sci.minFilter               = VK_FILTER_LINEAR;
            sci.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sci.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sci.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sci.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sci.mipLodBias              = 0.0f;
            sci.anisotropyEnable        = VK_FALSE;
            sci.maxAnisotropy           = 1.0f;
            sci.compareEnable           = VK_FALSE;
            sci.compareOp               = VK_COMPARE_OP_NEVER;
            sci.minLod                  = 0.0f;
            sci.maxLod                  = 15.0f;   // 统一 max_lod=15，不再按纹理 mip 派生
            sci.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
            sci.unnormalizedCoordinates = VK_FALSE;

            // 枚举取值一律走 inc/hgl/mtl/VkEnumText.h 的单一真源；未知名 = 加载失败（不静默落默认值）。
            if (item.contains("mag_filter") && item.at("mag_filter").is_string())
            {
                const std::string value = item.at("mag_filter").as_string();
                if (!ParseVkFilter(value.c_str(), sci.magFilter))
                {
                    GLogError("[SamplerPreset] %s: 非法 mag_filter: %s", name.c_str(), value.c_str());
                    return false;
                }
            }
            if (item.contains("min_filter") && item.at("min_filter").is_string())
            {
                const std::string value = item.at("min_filter").as_string();
                if (!ParseVkFilter(value.c_str(), sci.minFilter))
                {
                    GLogError("[SamplerPreset] %s: 非法 min_filter: %s", name.c_str(), value.c_str());
                    return false;
                }
            }
            if (item.contains("mipmap_mode") && item.at("mipmap_mode").is_string())
            {
                const std::string value = item.at("mipmap_mode").as_string();
                if (!ParseVkSamplerMipmapMode(value.c_str(), sci.mipmapMode))
                {
                    GLogError("[SamplerPreset] %s: 非法 mipmap_mode: %s", name.c_str(), value.c_str());
                    return false;
                }
            }
            if (item.contains("address_mode") && item.at("address_mode").is_string())
            {
                const std::string value = item.at("address_mode").as_string();
                VkSamplerAddressMode am = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                if (!ParseVkSamplerAddressMode(value.c_str(), am))
                {
                    GLogError("[SamplerPreset] %s: 非法 address_mode: %s", name.c_str(), value.c_str());
                    return false;
                }
                sci.addressModeU = am;
                sci.addressModeV = am;
                sci.addressModeW = am;
            }
            if (item.contains("anisotropy") && item.at("anisotropy").is_boolean()
                && item.at("anisotropy").as_boolean())
            {
                sci.anisotropyEnable = VK_TRUE;
                sci.maxAnisotropy = 16.0f;
                if (item.contains("max_anisotropy") && item.at("max_anisotropy").is_floating())
                    sci.maxAnisotropy = static_cast<float>(item.at("max_anisotropy").as_floating());
            }
            if (item.contains("max_lod"))
            {
                if (item.at("max_lod").is_floating())
                    sci.maxLod = static_cast<float>(item.at("max_lod").as_floating());
                else if (item.at("max_lod").is_integer())
                    sci.maxLod = static_cast<float>(item.at("max_lod").as_integer());
            }
            if (item.contains("compare_op") && item.at("compare_op").is_string())
            {
                const std::string value = item.at("compare_op").as_string();
                VkCompareOp op = VK_COMPARE_OP_NEVER;
                if (!ParseVkCompareOp(value.c_str(), op))
                {
                    GLogError("[SamplerPreset] %s: 非法 compare_op: %s", name.c_str(), value.c_str());
                    return false;
                }
                sci.compareEnable = VK_TRUE;
                sci.compareOp = op;
            }

            const AnsiString key(name.c_str());
            if (new_map.ContainsKey(key))
            {
                GLogError("[SamplerPreset] 重复的 sampler 名: %s", name.c_str());
                return false;
            }

            new_map.Add(key, i);
            new_infos.push_back(sci);
        }

        infos_ = new_infos;
        name_to_index_ = new_map;

        GLogInfo("[SamplerPreset] 加载 %u 个 sampler: %s", count, path.c_str());
        return true;
    }
}//namespace hgl::graph::mtl
