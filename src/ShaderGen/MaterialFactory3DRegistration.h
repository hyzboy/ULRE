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
