#pragma once

#include <hgl/graph/asset/PrimitiveAssetSpec.h>

namespace hgl::graph
{
    /**
     * @brief 从 TOML 文件加载图元资产规约（`*.primitive.toml`）
     */
    bool LoadPrimitiveAssetSpecFromTOML(const OSString &filename, PrimitiveAssetSpec &out_spec);

    /**
     * @brief 从内存字符串解析图元资产规约
     */
    bool ParsePrimitiveAssetSpec(const char *content, size_t size,
                                 const os_char *debug_name,
                                 PrimitiveAssetSpec &out_spec);
}//namespace hgl::graph
