#pragma once

#include<hgl/asset/LoadedFile.h>
#include<hgl/graph/module/TextureManager.h>

namespace hgl::graph
{
    /**
     * @brief CN: 把已加载文件提交到纹理上传队列的主线程提交者。
     *        EN: Main-thread committer that submits loaded files to the texture upload queue.
     * @details
     * CN: 两段式异步纹理加载的"第二段"入口：第一段由 hgl::LoadedFileManager 在 IO 线程
     *     读完整个 .Tex2D 文件，第二段在这里(Pump 回调所在线程)把内存字节解析成
     *     TextureCreateInfo 并提交到 TextureManager 的上传队列，由 GPU 侧异步完成。
     *
     *     关键性质：VkTextureLoader 在 Load 期间已把像素**同步拷贝**进映射好的 staging
     *     缓冲，所以这些函数返回后，LoadedFile 的字节不再被需要，调用方可立即放手引用。
     *
     *     线程契约：**只能在主线程(即调用 LoadedFileManager::Pump 的线程)调用**。
     *     未来若把上传提交拆到多 transfer queue family 的多线程模块，需要在此处
     *     同步该模块的线程约束。
     * EN: The "stage two" entry point of the two-stage async texture pipeline. Stage one is
     *     LoadedFileManager reading the whole .Tex2D file on IO threads; here, on the thread
     *     that runs Pump() callbacks, the memory bytes are parsed into a
     *     TextureCreateInfo and submitted to the TextureManager upload queue for the GPU.
     *
     *     Key property: VkTextureLoader copies the pixels into a mapped staging buffer during
     *     Load, so once these calls return the LoadedFile bytes are no longer needed and the
     *     caller may drop its reference immediately.
     *
     *     Thread contract: main thread only, that is the thread calling
     *     LoadedFileManager::Pump(). If upload submission is later spread over multiple
     *     transfer-queue-family threads, update this contract accordingly.
     */
    class TextureLoadCommitter
    {
        TextureManager *tex_manager=nullptr;            ///< @brief CN: 纹理管理器(不拥有)。 EN: Texture manager, not owned.

    public:

        /** @brief CN: 构造一个未绑定纹理管理器的提交者，随后必须调用 SetTextureManager。 EN: Construct an unbound committer; SetTextureManager must be called afterwards. */
        TextureLoadCommitter()=default;

        /**
         * @brief CN: 构造并立即绑定纹理管理器。
         *        EN: Construct and bind a texture manager immediately.
         * @param tm CN: 纹理管理器，必须比本对象活得更久。 EN: Texture manager; must outlive this object.
         */
        explicit TextureLoadCommitter(TextureManager *tm):tex_manager(tm){}

        /** @brief CN: 绑定纹理管理器；用于从属对象在 Init 阶段(没有构造函数上下文)完成绑定。 EN: Bind the texture manager; used by owned objects that bind during Init when no constructor context exists. */
        void SetTextureManager(TextureManager *tm){tex_manager=tm;}

        /** @brief CN: 是否已绑定纹理管理器。 EN: Whether a texture manager is bound. */
        bool IsValid()const{return tex_manager!=nullptr;}

        /**
         * @brief CN: 用已加载文件的字节创建并异步上传一张 2D 纹理。
         *        EN: Create and asynchronously upload a 2D texture from loaded file bytes.
         * @param file            CN: 已加载的 .Tex2D 文件，调用方持有引用即可，本函数不保留。 EN: Loaded .Tex2D file; the caller keeps the reference and this call does not retain it.
         * @param auto_mipmaps    CN: 源无 mip 链时是否自动生成。 EN: Whether to build mipmaps when the source has no chain.
         * @param priority        CN: GPU 上传优先级。 EN: GPU upload priority.
         * @param bindless_handle CN: 需要更新的 bindless 槽位，0=不绑定。 EN: Bindless slot to update, 0 for none.
         * @param callback        CN: 上传完成回调，可为 nullptr。 EN: Upload completion callback, may be nullptr.
         * @param user_data       CN: 回调上下文。 EN: Callback context.
         * @return CN: 上传任务编号，0 表示失败。 EN: Upload task identifier, or 0 on failure.
         */
        uint64 CommitTexture2D(const LoadedFile *file,
                               bool auto_mipmaps,
                               UploadPriority priority=UploadPriority::Normal,
                               uint32 bindless_handle=0,
                               void (*callback)(GpuUploadTask *,void *)=nullptr,
                               void *user_data=nullptr);

        /**
         * @brief CN: 提交一张"磁盘直写 staging"的 2D 纹理(Direct-to-Staging 零拷贝路径)。
         *        EN: Submit a 2D texture whose bytes were written straight into a staging buffer by disk IO.
         * @details CN: 仅适用于 file->IsDirectBuffer() 的 LoadedFile：字节已经在 `staging`
         *              指向的映射内存里(文件头在最前)。本函数不 memcpy、不分配 CPU 内存，
         *              只解析头部并把 staging 挂进上传任务。
         *              staging 的 Unmap 总在内部完成；返回 0 表示失败且 staging 未被接管，
         *              调用方负责 delete；返回非 0 表示上传任务已拥有它。
         *          EN: Only for LoadedFile instances with IsDirectBuffer(): the bytes already live
         *              in the mapped memory behind `staging` (header first). No memcpy, no CPU
         *              allocation; this parses the header and hands the staging buffer to the
         *              upload task. The buffer is always unmapped internally. Return 0 means the
         *              staging was NOT consumed (caller must delete it); non-zero means the
         *              upload task owns it until the upload finishes.
         * @param file            CN: 直写模式的已加载文件(其数据指针必须等于 staging 的 Map 指针)。
         *                        EN: Direct-mode loaded file whose data pointer is the staging map pointer.
         * @param staging         CN: file 字节所在的那个 DeviceBuffer。 EN: The DeviceBuffer backing file's bytes.
         * @param auto_mipmaps    CN: 源无 mip 链时是否自动生成。 EN: Whether to build mipmaps when the source has no chain.
         * @param priority        CN: GPU 上传优先级。 EN: GPU upload priority.
         * @param bindless_handle CN: 需要更新的 bindless 槽位，0=不绑定。 EN: Bindless slot to update, 0 for none.
         * @param callback        CN: 上传完成回调，可为 nullptr。 EN: Upload completion callback, may be nullptr.
         * @param user_data       CN: 回调上下文。 EN: Callback context.
         * @return CN: 上传任务编号，0 表示失败(staging 交还调用方)。 EN: Upload task id, or 0 on failure (staging returned to caller).
         */
        uint64 CommitTexture2DDirect(const LoadedFile *file,
                                     DeviceBuffer *staging,
                                     bool auto_mipmaps,
                                     UploadPriority priority=UploadPriority::Normal,
                                     uint32 bindless_handle=0,
                                     void (*callback)(GpuUploadTask *,void *)=nullptr,
                                     void *user_data=nullptr);

        /**
         * @brief CN: 用已加载文件的字节写入 Texture2DArray 的指定层。
         *        EN: Write loaded file bytes into one layer of a Texture2DArray.
         * @param file  CN: 已加载的 .Tex2D 文件。 EN: Loaded .Tex2D file.
         * @param array CN: 目标数组纹理。 EN: Destination array texture.
         * @param layer CN: 目标层号。 EN: Destination layer index.
         * @return CN: 成功返回 true；尺寸/格式不符或解析失败返回 false。 EN: True on success; false on extent/format mismatch or parse failure.
         * @note CN: 该路径是同步提交(与 TextureManager::LoadTexture2DArray 一致)，仍然不阻塞
         *           磁盘 IO——字节已经在内存里。
         *       EN: This path submits synchronously, like TextureManager::LoadTexture2DArray,
         *           but never blocks on disk IO because the bytes are already in memory.
         */
        bool CommitTexture2DArrayLayer(const LoadedFile *file,
                                       Texture2DArray *array,
                                       uint32 layer);
    };//class TextureLoadCommitter
}//namespace hgl::graph