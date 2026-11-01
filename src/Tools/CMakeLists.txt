add_subdirectory(MiniPack)
add_subdirectory(GLTFConvert)

# 内置元素（预设）资产导出工具：**开发期生成器**，产出 `ShaderLibrary/asset/` 下的
# 预设文件。它不链运行时的任何生成器，也不被任何运行时 target 依赖 —— 发行包里
# 只留产物、不留这段代码。见 src/Tools/BuiltinAssets/README.md。
add_subdirectory(BuiltinAssets)

# 发行形态探针：只链接 ULRE::Runtime，两种形态下都构建 ——
# 它在 ON 树里编译也是在为 OFF 树做同样的工作，且能让「运行时反向依赖生成器」
# 在开发形态就暴露成链接错误，而不是等到打发行包时才发现。
add_subdirectory(RuntimeFormProbe)

# frag_options.glsl 发射器回归门：只链接 ULRE.ShaderMaterialData，两种形态下都构建 ——
# options 是发行形态也要用到的 program 身份，绝不能反向依赖生成器。
add_subdirectory(DomainOptions)

# 着色器/材质程序回归工具：直接链接 ULRE.ShaderProgramBuild，发行形态
# （ULRE_ENABLE_SHADER_PROGRAM_BUILD=OFF）下不构建。
IF(ULRE_ENABLE_SHADER_PROGRAM_BUILD)
    add_subdirectory(ShaderTooling)
ENDIF()

# TexConv 纹理转换工具（子模块）：依赖 ImageMagick(Magick++) 与 AMD Compressonator；
# Intel ISPC 编码器需要 ispc.exe，找不到时在 enc_intel/CMakeLists.txt 内跳过该插件
# （/Intel 明确报错；未指定后端时回落 AMD 并告警一次）。
add_subdirectory(TexConv)
