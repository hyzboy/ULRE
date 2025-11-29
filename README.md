# ULRE - Ultra Lightweight Rendering Engine

[![Build](https://github.com/hyzboy/ULRE/actions/workflows/build.yml/badge.svg)](https://github.com/hyzboy/ULRE/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

ULRE is an experimental Vulkan-based rendering engine designed for exploring advanced rendering techniques and providing example implementations.

ULRE是一个基于Vulkan的实验性渲染引擎，用于探索先进的渲染技术并提供示例实现。

## Features / 功能特性

- **Graphics API**: Vulkan
- **Platforms**: Windows, Linux (WIP: Android, macOS, iOS)
- **Single DrawCall Rendering**: Draw entire scenes with one DrawCall
- **Material System**: Support for file-loaded and embedded materials
- **Component System**: Initial ECS architecture design

## Requirements / 系统要求

- CMake 3.28+
- C++17 compatible compiler (GCC 11+, Clang 13+, MSVC 2019+)
- Vulkan SDK 1.3+
- vcpkg (recommended for dependencies)

## Quick Start / 快速开始

```bash
# Clone with submodules
git clone --recursive https://github.com/hyzboy/ULRE.git
cd ULRE

# Install dependencies with vcpkg
vcpkg install glm jsoncpp expat

# Configure and build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

See [BUILD.md](BUILD.md) for detailed build instructions.

## Project Structure / 项目结构

```
ULRE/
├── CMCore/         # Core library / 核心库
├── CMPlatform/     # Platform abstraction / 平台抽象层
├── CMAssetsManage/ # Asset management / 资产管理
├── CMSceneGraph/   # Scene graph / 场景图
├── CMUtil/         # Utilities / 工具库
├── CM2D/           # 2D rendering / 2D渲染
├── CMGUI/          # GUI system / GUI系统
├── src/            # Engine source / 引擎源码
├── inc/            # Headers / 头文件
├── example/        # Examples / 示例
└── doc/            # Documentation / 文档
```

## Milestones / 里程碑

### Milestone 2
- Texture2DArray integrated into material instances
- Multiple render instances with different materials merged into one render
- Texture2DArray集成到材质实例中
- 不同材质的多个渲染实例合并成一次渲染

### Milestone 1
- Entire scene drawn with single DrawCall
- 单次DrawCall绘制整个场景

## Documentation / 文档

- [Build Instructions](BUILD.md) - 构建说明
- [Improvement Suggestions](doc/IMPROVEMENT_SUGGESTIONS.md) - 改进建议
- [Contributing Guide](CONTRIBUTING.md) - 贡献指南

## Examples / 示例

The `example/` directory contains various demos:

| Category | Examples |
|----------|----------|
| Basic | Triangle, Auto Instance, Billboard |
| Texture | Texture formats, Rect textures |
| Gizmo | Axis, Grid, Ray picking |
| GUI | Text rendering |
| Geometry | Polygon extrusion, Revolution |
| Environment | Atmosphere sky, Heightmap terrain |

## License / 许可证

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments / 致谢

- Vulkan SDK by Khronos Group
- GLM for mathematics
- All contributors and supporters
