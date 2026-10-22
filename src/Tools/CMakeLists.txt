add_subdirectory(MiniPack)
add_subdirectory(GLTFConvert)

# ShaderGen 回归门/工具：直接链接 ULRE.ShaderGen，发行形态（ULRE_ENABLE_SHADERGEN=OFF）下不构建。
IF(ULRE_ENABLE_SHADERGEN)
    add_subdirectory(ShaderGen)
ENDIF()

# TexConv 纹理转换工具（子模块）：依赖 ImageMagick(Magick++) 与 AMD Compressonator；
# Intel ISPC 编码器需要 ISPCTextureCompressor 的 kernel*.obj，产物缺失时在
# TexConv/CMakeLists.txt 内自动关闭（/Intel 明确报错，默认走 AMD）。
add_subdirectory(TexConv)
