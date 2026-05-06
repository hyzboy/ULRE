#pragma once
#include <hgl/shadergen/MaterialFactory3D.h>

/// 在翻译单元静态初始化阶段向 MaterialFactory3D 注册一个 preset。
///
/// 用法（置于 M_*.cpp 末尾的文件作用域，namespace 之外）：
///   ULRE_REGISTER_PRESET_FACTORY(PureColor3D, "PureColor3D", PureColor3D_Adapter)
///
/// @param preset   hgl::graph::mtl::MaterialPreset 枚举值（不带 namespace 前缀）
/// @param name     日志字符串，建议与 preset 名保持一致
/// @param fn       adapter 函数，签名须匹配 PresetFactoryFn
///
/// ⚠ 依赖 static initializer 顺序：
///   - Registry() 采用 Meyers singleton，首次访问时安全构造。
///   - 若使用 LTO 或 /OPT:REF 可能剔除未引用的翻译单元；
///     此时需在 CMake 中对 ShaderGen 添加 /WHOLEARCHIVE（MSVC）
///     或 --whole-archive（GCC/Clang）链接选项。
#define ULRE_REGISTER_PRESET_FACTORY(preset, name, fn)                              \
    namespace {                                                                      \
        struct AutoReg_##preset {                                                    \
            AutoReg_##preset() {                                                     \
                ::hgl::graph::mtl::MaterialFactory3D::Register(                     \
                    ::hgl::graph::mtl::MaterialPreset::preset, name,                 \
                    &(fn));                                                           \
            }                                                                        \
        };                                                                           \
        static AutoReg_##preset s_auto_reg_##preset;                                 \
    }

/// 从 Owned Creator 自动生成 adapter 并注册。
///
/// 适用于常见签名：
///   Create{Preset}Owned(profile, TypedConfig*, const MaterialVariantDesc&, const MaterialVariantKey&)
///
/// 用法：
///   ULRE_REGISTER_PRESET_FACTORY_FROM_OWNED(
///       Gizmo3D,
///       hgl::graph::mtl::Material3DCreateConfig)
///
/// 约定：
///   - 日志 name 自动使用 #preset
///   - Owned creator 自动拼接为 ::hgl::graph::mtl::Create##preset##Owned
#define ULRE_REGISTER_PRESET_FACTORY_FROM_OWNED(preset, cfg_type)                   \
    namespace {                                                                      \
        static std::unique_ptr<::hgl::graph::mtl::MaterialCreateInfo>               \
            AutoAdapter_##preset(                                                    \
                const ::hgl::graph::mtl::contract::PhysicalDeviceProfileLite *profile, \
                const ::hgl::graph::mtl::MaterialVariantDesc *desc,                 \
                const ::hgl::graph::mtl::MaterialVariantKey &key,                   \
                ::hgl::graph::mtl::MaterialCreateConfig *cfg)                       \
        {                                                                            \
            return ::hgl::graph::mtl::Create##preset##Owned(                        \
                profile, static_cast<cfg_type *>(cfg), *desc, key);                 \
        }                                                                            \
    }                                                                                \
    ULRE_REGISTER_PRESET_FACTORY(preset, #preset, AutoAdapter_##preset)
