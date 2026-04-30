#pragma once

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <cstddef>

namespace hgl::graph::mtl
{
    namespace contract { struct PhysicalDeviceProfileLite; }
    struct MaterialCreateConfig;
    class  MaterialCreateInfo;
    struct MaterialVariantDesc;

    /// 工厂回调签名。所有 M_*.cpp 通过此签名注册自己的 Create* 函数。
    /// cfg 的实际类型由 preset 决定；adapter 函数负责 static_cast。
    /// desc 是 MaterialLibrary 路由层已查找好的 variant 描述符，adapter 直接使用无需重复 QueryVariant。
    using PresetFactoryFn = MaterialCreateInfo *(*)(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
        const MaterialVariantKey                  &key,
        MaterialCreateConfig                      *cfg);

    /// 基于注册表的材质工厂。
    /// 各 M_*.cpp 在翻译单元加载时通过 ULRE_REGISTER_PRESET_FACTORY 自动注册。
    class MaterialFactory3D
    {
    public:
        /// 注册 preset → 工厂回调。重复注册被忽略并打 warning。
        /// name 仅用于日志，不参与查找。
        static bool Register(MaterialPreset preset, const char *name, PresetFactoryFn fn);

        /// 按 preset 创建 MaterialCreateInfo*。未注册返回 nullptr 并打 warning。
        /// desc 是路由层已查找好的 variant 描述符，直接透传给工厂，无需工厂再次 QueryVariant。
        static MaterialCreateInfo *Create(
            MaterialPreset                             preset,
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialVariantDesc                 *desc,
            const MaterialVariantKey                  &key,
            MaterialCreateConfig                      *cfg);

        /// 已注册 preset 的个数（调试用）。
        static size_t RegisteredCount();

        /// 根据 preset 返回注册时的名字（调试用）。未注册返回 nullptr。
        static const char *GetRegisteredName(MaterialPreset preset);
    };
}
