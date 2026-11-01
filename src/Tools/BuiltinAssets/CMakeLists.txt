cmake_minimum_required(VERSION 3.28)

# 内置元素（预设）资产导出工具
#
# 定位：**开发期生成器**，产出 `ShaderLibrary/asset/` 下的预设文件（.toml + 二进制）。
# 它不属于运行时库，也不该进发行包 —— 引擎只读这些产物，从不现场生成。
# 未来所有内置元素（几何预设、材质预设…）都在本工程内加一组，见 EnvironmentPresets.cpp
# 与 BuiltinAssetExport.cpp 里的 `kGroups`。
#
# 依赖：ULRE.SceneGraph（EnvironmentAsset 等资产句柄）+ 基础库。
# 序列化一律走 hgl::serialize 统一通路（ArchiveFile.h），本工程不含任何格式专用代码。
add_executable(BuiltinAssetExport
    BuiltinAssetExport.cpp
    EnvironmentPresets.cpp
    PresetWriter.cpp)

target_link_libraries(BuiltinAssetExport
    PRIVATE ULRE.SceneGraph
            ULRE.Debug
            CMCore
            CMCoreType)

set_property(TARGET BuiltinAssetExport PROPERTY FOLDER "ULRE/Tools/BuiltinAssets")

if(MSVC)
    set_target_properties(BuiltinAssetExport PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()
