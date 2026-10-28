#pragma once

#include<hgl/graph/module/TextureManager.h>

namespace hgl::graph
{
    /**
     * @brief CN: 从内存中的纹理文件字节同步创建一张 2D 纹理。
     *        EN: Synchronously create a 2D texture from texture-file bytes held in memory.
     * @param tm            CN: 纹理管理器。 EN: Texture manager.
     * @param data          CN: 完整的 .Tex2D 文件字节(含文件头)。 EN: Complete .Tex2D file bytes, header included.
     * @param size          CN: 字节数。 EN: Byte count.
     * @param auto_mipmaps  CN: 源无 mip 链时是否自动生成。 EN: Whether to build mipmaps when the source has no chain.
     * @param source_name   CN: 调试用的来源名(通常是文件路径)，会写进纹理名；空则退化为 "Texture"。
     *                      EN: Debug-only source name (usually the file path) used as the texture name;
     *                          an empty value falls back to "Texture".
     * @return CN: 成功返回纹理指针，失败返回 nullptr。 EN: The created texture, or nullptr on failure.
     * @note CN: 像素字节在 Load 期间被同步拷入 staging 缓冲，返回后调用方即可释放 data。
     *       EN: Pixel bytes are copied into a staging buffer during Load, so the caller may free
     *           data as soon as this returns.
     */
    Texture2D *CreateTexture2DFromMemory(TextureManager *tm,
                                         const void *data,
                                         int64 size,
                                         bool auto_mipmaps,
                                         const OSString &source_name=OSString());

    /**
     * @brief CN: 从内存中的纹理文件字节异步提交一张 2D 纹理(进入上传队列)。
     *        EN: Asynchronously submit a 2D texture built from in-memory texture-file bytes.
     * @param tm              CN: 纹理管理器。 EN: Texture manager.
     * @param data            CN: 完整的 .Tex2D 文件字节(含文件头)。 EN: Complete .Tex2D file bytes, header included.
     * @param size            CN: 字节数。 EN: Byte count.
     * @param auto_mipmaps    CN: 源无 mip 链时是否自动生成。 EN: Whether to build mipmaps when the source has no chain.
     * @param priority        CN: 上传优先级。 EN: Upload priority.
     * @param bindless_handle CN: 需要绑定的 bindless 槽位，0=不绑定。 EN: Bindless slot to update, 0 for none.
     * @param callback        CN: 上传完成回调，可为 nullptr。 EN: Upload completion callback, may be nullptr.
     * @param user_data       CN: 回调上下文。 EN: Callback context.
     * @param source_name     CN: 调试用的来源名(通常是文件路径)，会写进纹理名；空则退化为 "Texture"。
     *                        EN: Debug-only source name (usually the file path) used as the texture name;
     *                            an empty value falls back to "Texture".
     * @return CN: 上传任务编号，0 表示失败。 EN: Upload task identifier, or 0 on failure.
     * @note CN: 像素字节在提交前被同步拷入 staging 缓冲，返回后调用方即可释放 data。
     *       EN: Pixel bytes are copied into a staging buffer before returning, so the caller may
     *           free data immediately afterwards.
     */
    uint64 CreateTexture2DFromMemoryAsync(TextureManager *tm,
                                          const void *data,
                                          int64 size,
                                          bool auto_mipmaps,
                                          UploadPriority priority,
                                          uint32 bindless_handle,
                                          void (*callback)(TextureUploadTask *,void *),
                                          void *user_data,
                                          const OSString &source_name=OSString());

    /**
     * @brief CN: 从内存中的纹理文件字节写入 Texture2DArray 的指定层。
     *        EN: Write in-memory texture-file bytes into one layer of a Texture2DArray.
     * @param tm    CN: 纹理管理器。 EN: Texture manager.
     * @param ta    CN: 目标数组纹理。 EN: Destination array texture.
     * @param layer CN: 目标层号。 EN: Destination layer index.
     * @param data  CN: 完整的 .Tex2D 文件字节(含文件头)。 EN: Complete .Tex2D file bytes, header included.
     * @param size  CN: 字节数。 EN: Byte count.
     * @param source_name CN: 仅用于失败日志的来源名(通常是文件路径)，空则不打印来源。
     *                    EN: Source name used only in failure logs (usually the file path); empty omits it.
     * @return CN: 成功返回 true。 EN: True on success.
     * @note CN: 与文件版本一致：尺寸/格式必须与数组完全匹配，mip 级数策略见其文档。
     *       EN: Same rules as the file variant: extent and format must match exactly, and the
     *           mip-chain policy is documented on that function.
     */
    bool LoadTexture2DLayerFromMemory(TextureManager *tm,
                                      Texture2DArray *ta,
                                      const uint32 layer,
                                      const void *data,
                                      int64 size,
                                      const OSString &source_name=OSString());
}//namespace hgl::graph