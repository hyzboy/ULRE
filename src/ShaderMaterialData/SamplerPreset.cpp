#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/VkEnumText.h>

#include <hgl/asset/generated/SamplerPresetSerialize.gen.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/sync/Mutex.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/type/Smart.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        /// 全局采样器库写锁：并发 Load()/Clear() 串行化，防止双写损坏容器。
        /// 读侧（GetIndex/GetCount/GetCreateInfo）保持无锁：假定「编译并发期
        /// 库不变」——Load 只在引擎初始化或单线程工具阶段调用（现状如此，
        /// 头文件注释已声明该契约）。
        hgl::Mutex g_sampler_library_write_mutex;
    }//anonymous namespace

    SamplerPresetLibrary &SamplerPresetLibrary::Instance()
    {
        // C++11 函数级 static（magic static）构造本身线程安全，无需额外加锁。
        static SamplerPresetLibrary instance;
        return instance;
    }

    void SamplerPresetLibrary::Clear()
    {
        hgl::MutexLock lock(&g_sampler_library_write_mutex);
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
        // 写锁罩住整函数：临时容器构建 + 末尾成员替换与并发 Load/Clear 互斥。
        hgl::MutexLock write_lock(&g_sampler_library_write_mutex);

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

        // 字段规约来自 [group.SamplerPreset] 生成的序列化器（asset_gen.toml）。
        // 文本层只管「文本 → 值树 → 规约结构」；枚举拼写仍是字符串，在这里
        // 才转换 VkSamplerCreateInfo：未知名 = 加载失败（不静默落默认值），
        // 拼写真源 = inc/hgl/mtl/VkEnumText.h。
        SamplerPresetFileSpec root;
        hgl::serialize::Archive archive;
        hgl::serialize::TomlParseError parse_error;
        if (!hgl::serialize::LoadFromTomlText(buffer.data(), static_cast<size_t>(size),
                                              root, &archive, &parse_error))
        {
            AnsiString why = parse_error.Format();
            if (why.Length() == 0)
            {
                why = AnsiString(archive.FailedPath());
                if (why.Length() == 0)
                    why = AnsiString(archive.FailedField());
                why += ": ";
                why += AnsiString(archive.ErrorMessage());
            }
            GLogError("[SamplerPreset] 解析失败: %s (%s)", path.c_str(), why.c_str());
            return false;
        }

        if (root.schema != 1)
        {
            GLogError("[SamplerPreset] 不支持的 schema: %u (%s)", root.schema, path.c_str());
            return false;
        }

        const int count = root.samplers.GetCount();
        if (count <= 0)
        {
            GLogError("[SamplerPreset] 缺少 samplers 数组: %s", path.c_str());
            return false;
        }

        // 先解析到临时容器，全部成功后原子替换（失败保留上次成功内容）。
        std::vector<VkSamplerCreateInfo> new_infos;
        hgl::UnorderedMap<AnsiString, uint32> new_map;
        new_infos.reserve(static_cast<size_t>(count));

        for (int i = 0; i < count; ++i)
        {
            const SamplerPresetSpec &spec = *root.samplers[i];

            const AnsiString name = spec.name;
            if (name.Length() == 0)
            {
                GLogError("[SamplerPreset] 第 %d 项缺少 name", i);
                return false;
            }
            if (new_map.ContainsKey(name))
            {
                GLogError("[SamplerPreset] 重复的 sampler 名: %s", name.c_str());
                return false;
            }

            // 缺省值 = SamplerPresetSpec 的类内缺省（与历史硬编码默认一致）。
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
            sci.maxLod                  = spec.max_lod;
            sci.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
            sci.unnormalizedCoordinates = VK_FALSE;

            // 枚举取值一律走 inc/hgl/mtl/VkEnumText.h 的单一真源；空串 = 保持缺省。
            if (spec.mag_filter.Length() > 0)
            {
                if (!ParseVkFilter(spec.mag_filter.c_str(), sci.magFilter))
                {
                    GLogError("[SamplerPreset] %s: 非法 mag_filter: %s", name.c_str(), spec.mag_filter.c_str());
                    return false;
                }
            }
            if (spec.min_filter.Length() > 0)
            {
                if (!ParseVkFilter(spec.min_filter.c_str(), sci.minFilter))
                {
                    GLogError("[SamplerPreset] %s: 非法 min_filter: %s", name.c_str(), spec.min_filter.c_str());
                    return false;
                }
            }
            if (spec.mipmap_mode.Length() > 0)
            {
                if (!ParseVkSamplerMipmapMode(spec.mipmap_mode.c_str(), sci.mipmapMode))
                {
                    GLogError("[SamplerPreset] %s: 非法 mipmap_mode: %s", name.c_str(), spec.mipmap_mode.c_str());
                    return false;
                }
            }
            if (spec.address_mode.Length() > 0)
            {
                VkSamplerAddressMode am = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                if (!ParseVkSamplerAddressMode(spec.address_mode.c_str(), am))
                {
                    GLogError("[SamplerPreset] %s: 非法 address_mode: %s", name.c_str(), spec.address_mode.c_str());
                    return false;
                }
                sci.addressModeU = am;
                sci.addressModeV = am;
                sci.addressModeW = am;
            }
            if (spec.anisotropy)
            {
                sci.anisotropyEnable = VK_TRUE;
                sci.maxAnisotropy = spec.max_anisotropy;
            }
            if (spec.compare_op.Length() > 0)
            {
                VkCompareOp op = VK_COMPARE_OP_NEVER;
                if (!ParseVkCompareOp(spec.compare_op.c_str(), op))
                {
                    GLogError("[SamplerPreset] %s: 非法 compare_op: %s", name.c_str(), spec.compare_op.c_str());
                    return false;
                }
                sci.compareEnable = VK_TRUE;
                sci.compareOp = op;
            }

            new_map.Add(name, static_cast<uint32>(i));
            new_infos.push_back(sci);
        }

        infos_ = new_infos;
        name_to_index_ = new_map;

        GLogInfo("[SamplerPreset] 加载 %u 个 sampler: %s", static_cast<uint32>(count), path.c_str());
        return true;
    }
}//namespace hgl::graph::mtl
