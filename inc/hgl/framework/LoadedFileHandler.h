#pragma once

#include<hgl/framework/LoadedFile.h>

namespace hgl
{
    /** @brief CN: 加载请求编号，0 表示无效。 EN: Load request identifier; 0 means invalid. */
    using FileLoadId=uint64;

    /**
     * @brief CN: 一次加载请求的票据(回调时传入，用于识别是哪一次请求)。
     *        EN: Ticket identifying one load request, passed to every callback.
     * @details CN: 票据只在回调期间有效；不要保存 filename 指针。
     *          EN: The ticket is valid only for the duration of the callback; do not keep the filename pointer.
     */
    struct FileLoadTicket
    {
        FileLoadId          id=0;                                   ///< @brief CN: 请求编号。 EN: Request identifier.
        const OSString *    filename=nullptr;                       ///< @brief CN: 源文件名，仅在回调期间有效。 EN: Source file name, valid only during the callback.
    };//struct FileLoadTicket

    /**
     * @brief CN: 文件加载结果的接收者(虚函数对象)。
     *        EN: Receiver of file-load results, implemented as a virtual-function object.
     * @details
     * CN: 业务上下文(layer、bindless 句柄、实体指针……)放在子类成员里即可。
     *     三个回调都只会在调用 LoadedFileManager::Pump() 的线程触发，
     *     因此可以直接调用要求该线程亲和性的 API(例如提交 GPU 上传)。
     *
     *     生命周期：handler 必须比它名下所有未完成的请求活得更久。
     * EN: Put per-request business context (layer index, bindless handle, entity pointer, ...)
     *     into subclass members. All three callbacks run on the thread that calls
     *     LoadedFileManager::Pump(), so thread-affine APIs (for example submitting a GPU
     *     upload) may be called directly from them.
     *
     *     Lifetime: a handler must outlive every request still pending for it.
     */
    class LoadedFileHandler
    {
    public:

        virtual ~LoadedFileHandler()=default;

        /**
         * @brief CN: 文件已读完。
         *        EN: The file has been read successfully.
         * @param file CN: 借用的文件对象；需要保留就拷贝一份 LoadedFilePtr。 EN: Borrowed file object; copy it into a LoadedFilePtr to keep it alive.
         * @note CN: 用完就让它析构/Reset()，引用归零的文件会进入待释放队列，
         *           由下一次 Collect() 真正回收。
         *       EN: When done, let it destruct or Reset() it. A zero-count file enters the
         *           pending-release queue and is reclaimed by the next Collect().
         */
        virtual void OnFileLoaded   (const FileLoadTicket &ticket,LoadedFile *file)=0;

        /** @brief CN: 读取失败(含文件不存在)。 EN: The read failed, including a missing file. */
        virtual void OnFileFailed   (const FileLoadTicket &ticket,io::AsyncFileLoadError error){}

        /** @brief CN: 请求被取消。 EN: The request was cancelled. */
        virtual void OnFileCancelled(const FileLoadTicket &ticket){}
    };//class LoadedFileHandler
}//namespace hgl