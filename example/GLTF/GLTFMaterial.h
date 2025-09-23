#pragma once

#include <string>

namespace gltf {
    // 精简材质结构。此文件保留以便后续扩展导出逻辑时使用；目前仅打印 fastgltf 的原始材质数据。
    struct TextureInfo {
        int index = -1;
        int texCoord = 0;
        bool has_tex_transform = false;
        float offset[2] = {0, 0};
        float scale[2]  = {1, 1};
        float rotation  = 0.0f;
    };

    struct Material {
        std::string name;
        bool doubleSided = false;
        float emissiveFactor[3] = {0,0,0};
        float alphaCutoff = 0.5f;
        bool unlit = false;
        // 保留更多字段的占位，后续如需导出再填充
    };
}
