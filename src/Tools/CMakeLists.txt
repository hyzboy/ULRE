add_subdirectory(MiniPack)
add_subdirectory(GLTFConvert)

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
    add_subdirectory(ShaderGen)
ENDIF()

# TexConv 纹理转换工具（子模块）：依赖 ImageMagick(Magick++) 与 AMD Compressonator；
# Intel ISPC 编码器需要 ISPCTextureCompressor 的 kernel*.obj，产物缺失时在
# TexConv/CMakeLists.txt 内自动关闭（/Intel 明确报错，默认走 AMD）。
add_subdirectory(TexConv)
