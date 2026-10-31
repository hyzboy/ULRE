#pragma once

#include <vulkan/vulkan.h>

#include <cstring>

// ── Vk 枚举 ↔ 引擎文本名：材质 / 采样器 TOML 的唯一解析真源 ───────────────
// 覆盖：
//   材质 TOML  pipeline.cull_mode / pipeline.depth_compare_op / pipeline.blend_src|blend_dst
//   contract/sampler.toml mag_filter / min_filter / mipmap_mode / address_mode / compare_op
//
// 口径（T1：一个枚举的名字↔值只有一份实现）：
//   ① 拼写以本表为唯一真源，**精确匹配**（与历史解析器一致，不再让 "Nearest"/"LessOrEqual"
//      这类字面量散落在解析文件里）；
//   ② 未知名一律**失败**（Parse* 返回 false），由调用方报错——不静默落默认值；
//   ③ 每个枚举配一对 Get*Name（反查，供诊断/日志）与 Parse*（正查）。
//
// 历史：此前 MaterialDefinitionFile.cpp 有 3 份手写解析器（cull_mode/compare_op/blend_src|dst）、
//   SamplerPreset.cpp 有 4 份（filter/mipmap_mode/address_mode/compare_op），
//   另有 src/Vulkan/VKString.{h,cpp} 一套同类注册表（全仓 0 消费者），已于 2026-10-11 整删。

namespace hgl::graph::mtl
{
    template<typename E>
    struct VkEnumText
    {
        E           value;
        const char *text;
    };

    template<typename E,size_t N>
    inline bool FindVkEnumText(const VkEnumText<E> (&table)[N],const char *text,E &out) noexcept
    {
        if(!text||!*text)
            return false;

        for(size_t i=0;i<N;++i)
            if(std::strcmp(text,table[i].text)==0)
            {
                out=table[i].value;
                return true;
            }

        return false;
    }

    template<typename E,size_t N>
    inline const char *FindVkEnumTextName(const VkEnumText<E> (&table)[N],const E &value) noexcept
    {
        for(size_t i=0;i<N;++i)
            if(table[i].value==value)
                return table[i].text;

        return nullptr;
    }

    // ── VkCullModeFlags ─────────────────────────────────────────────────
    inline constexpr const VkEnumText<VkCullModeFlags> kCullModeText[]=
    {
        { VK_CULL_MODE_NONE,           "None"         },
        { VK_CULL_MODE_FRONT_BIT,      "Front"        },
        { VK_CULL_MODE_BACK_BIT,       "Back"         },
        { VK_CULL_MODE_FRONT_AND_BACK, "FrontAndBack" },
    };

    inline bool ParseVkCullMode(const char *text,VkCullModeFlags &out) noexcept
    { return FindVkEnumText(kCullModeText,text,out); }

    inline const char *GetVkCullModeName(const VkCullModeFlags &value) noexcept
    { return FindVkEnumTextName(kCullModeText,value); }

    // ── VkCompareOp ─────────────────────────────────────────────────────
    inline constexpr const VkEnumText<VkCompareOp> kCompareOpText[]=
    {
        { VK_COMPARE_OP_NEVER,            "Never"          },
        { VK_COMPARE_OP_LESS,             "Less"           },
        { VK_COMPARE_OP_EQUAL,            "Equal"          },
        { VK_COMPARE_OP_LESS_OR_EQUAL,    "LessOrEqual"    },
        { VK_COMPARE_OP_GREATER,          "Greater"        },
        { VK_COMPARE_OP_NOT_EQUAL,        "NotEqual"       },
        { VK_COMPARE_OP_GREATER_OR_EQUAL, "GreaterOrEqual" },
        { VK_COMPARE_OP_ALWAYS,           "Always"         },
    };

    inline bool ParseVkCompareOp(const char *text,VkCompareOp &out) noexcept
    { return FindVkEnumText(kCompareOpText,text,out); }

    inline const char *GetVkCompareOpName(const VkCompareOp &value) noexcept
    { return FindVkEnumTextName(kCompareOpText,value); }

    // ── VkBlendFactor ───────────────────────────────────────────────────
    inline constexpr const VkEnumText<VkBlendFactor> kBlendFactorText[]=
    {
        { VK_BLEND_FACTOR_ZERO,                 "Zero"              },
        { VK_BLEND_FACTOR_ONE,                  "One"               },
        { VK_BLEND_FACTOR_SRC_ALPHA,            "SrcAlpha"          },
        { VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  "OneMinusSrcAlpha"  },
        { VK_BLEND_FACTOR_DST_ALPHA,            "DstAlpha"          },
        { VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,  "OneMinusDstAlpha"  },
        { VK_BLEND_FACTOR_SRC_COLOR,            "SrcColor"          },
        { VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,  "OneMinusSrcColor"  },
        { VK_BLEND_FACTOR_DST_COLOR,            "DstColor"          },
        { VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,  "OneMinusDstColor"  },
        { VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,   "SrcAlphaSaturate"  },
    };

    inline bool ParseVkBlendFactor(const char *text,VkBlendFactor &out) noexcept
    { return FindVkEnumText(kBlendFactorText,text,out); }

    inline const char *GetVkBlendFactorName(const VkBlendFactor &value) noexcept
    { return FindVkEnumTextName(kBlendFactorText,value); }

    // ── VkFilter ────────────────────────────────────────────────────────
    inline constexpr const VkEnumText<VkFilter> kFilterText[]=
    {
        { VK_FILTER_NEAREST, "Nearest" },
        { VK_FILTER_LINEAR,  "Linear"  },
    };

    inline bool ParseVkFilter(const char *text,VkFilter &out) noexcept
    { return FindVkEnumText(kFilterText,text,out); }

    inline const char *GetVkFilterName(const VkFilter &value) noexcept
    { return FindVkEnumTextName(kFilterText,value); }

    // ── VkSamplerMipmapMode ─────────────────────────────────────────────
    inline constexpr const VkEnumText<VkSamplerMipmapMode> kSamplerMipmapModeText[]=
    {
        { VK_SAMPLER_MIPMAP_MODE_NEAREST, "Nearest" },
        { VK_SAMPLER_MIPMAP_MODE_LINEAR,  "Linear"  },
    };

    inline bool ParseVkSamplerMipmapMode(const char *text,VkSamplerMipmapMode &out) noexcept
    { return FindVkEnumText(kSamplerMipmapModeText,text,out); }

    inline const char *GetVkSamplerMipmapModeName(const VkSamplerMipmapMode &value) noexcept
    { return FindVkEnumTextName(kSamplerMipmapModeText,value); }

    // ── VkSamplerAddressMode ────────────────────────────────────────────
    inline constexpr const VkEnumText<VkSamplerAddressMode> kSamplerAddressModeText[]=
    {
        { VK_SAMPLER_ADDRESS_MODE_REPEAT,              "Repeat"            },
        { VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,     "MirroredRepeat"    },
        { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,       "ClampToEdge"       },
        { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,     "ClampToBorder"     },
        { VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,"MirrorClampToEdge" },
    };

    inline bool ParseVkSamplerAddressMode(const char *text,VkSamplerAddressMode &out) noexcept
    { return FindVkEnumText(kSamplerAddressModeText,text,out); }

    inline const char *GetVkSamplerAddressModeName(const VkSamplerAddressMode &value) noexcept
    { return FindVkEnumTextName(kSamplerAddressModeText,value); }
}//namespace hgl::graph::mtl
