add_subdirectory(MiniPack)
add_subdirectory(GLTFConvert)

add_subdirectory(ShaderGen)

# TexConv 纹理转换工具（子模块）：依赖 ImageMagick(Magick++) 与 AMD Compressonator；
# Intel ISPC 编码器需要 ISPCTextureCompressor 的 kernel*.obj，产物缺失时在
# TexConv/CMakeLists.txt 内自动关闭（/Intel 明确报错，默认走 AMD）。
add_subdirectory(TexConv)
