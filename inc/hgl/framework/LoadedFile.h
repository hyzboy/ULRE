#pragma once

#include<hgl/object/MetaObject.h>
#include<hgl/io/AsyncFileLoader.h>

namespace hgl
{
    class LoadedFileManager;

    /**
     * @brief CN: 把文件名规范化为已加载表的查表键。
     *        EN: Normalize a file name into a loaded-table key.
     * @details CN: 分隔符统一为平台原生分隔符并合并连续分隔符；Windows 下再做大小写折叠，
     *              使 "Res\\Image/A.Tex2D" 与 "res/image/a.tex2d" 命中同一份数据。
     *              该键只用于查表，不改变 LoadedFile::GetFilename() 报告给使用方的原始拼写。
     *          EN: Separators are converted to the platform-native one and repeats collapsed;
     *              on Windows the result is additionally case-folded, so differently spelled
     *              variants of one path share the same payload. The key is used for table
     *              lookups only: LoadedFile::GetFilename() keeps the original spelling.
     * @param filename CN: 原始文件名(任意分隔符/大小写)。 EN: Original file name with any separator or case.
     * @return CN: 规范键。 EN: The normalized key.
     */
    OSString MakeLoadedFileKey(const OSString &filename);

    /**
     * @brief CN: 已加载的原始文件：文件字节 + META + 引用计数。
     *        EN: A loaded raw file: byte payload, META and reference counting.
     * @details
     * CN: 统一承载三段信息：
     *      - 字节：直接读进内部的 io::AsyncFileLoadRequest，避免从读盘缓冲再拷一份；
     *      - META：本期只填 source_path(文件名)与 source_size(字节数)；
     *      - 引用计数：来自 RefObject，使用方一律通过 LoadedFilePtr(RefPtr<LoadedFile>)持有。
     *
     *     引用归零**不会**立即释放：对象挂入 LoadedFileManager 的待释放队列，
     *     由主线程调用 LoadedFileManager::Collect() 统一 Release 并 delete。
     *     期间若有人再次 Find()/Submit() 同名文件，会在队列里被"复活"(重新 RefInc)，
     *     不必重新读盘。这就是"弱表缓存"的实现方式。
     *
     *     线程约定：RefInc/RefDec 可在任意线程；字节数据只读且不可变，
     *     使用方必须在自己持有引用期间访问 GetData()。
     * EN: Carries three pieces of state: the bytes (read straight into the internal
     *     AsyncFileLoadRequest so no copy is needed), the META header (only source_path
     *     and source_size for now), and the reference count inherited from RefObject.
     *     Consumers always hold it through LoadedFilePtr (RefPtr<LoadedFile>).
     *
     *     A zero reference count does not free the object immediately: it is queued in the
     *     manager's pending-release list and only Collect() performs Release()+delete. A
     *     later Find()/Submit() for the same path resurrects it with RefInc instead of
     *     re-reading the file, which implements the weak-table cache.
     *
     *     Threading: RefInc/RefDec are safe from any thread; the byte payload is immutable,
     *     so GetData() may be read while the caller holds its own reference.
     */
    class LoadedFile:public MetaObject
    {
        friend class LoadedFileManager;

    public:

        HGL_OBJECT(LoadedFile,hgl::MetaObject)

    private:

        io::AsyncFileLoadRequest    request;                        ///< @brief CN: 读盘请求兼字节持有者。 EN: Read request that also owns the bytes.
        OSString                    path_key;                       ///< @brief CN: 已加载表的查表键(见 MakeLoadedFileKey)。 EN: Loaded-table lookup key (see MakeLoadedFileKey).
        LoadedFileManager *         owner=nullptr;                  ///< @brief CN: 所属管理器；为空则归零时直接销毁。 EN: Owning manager; when null the object destroys itself on ref-zero.
        bool                        in_pending=false;               ///< @brief CN: 是否已在待释放队列(受管理器锁保护)。 EN: Whether it is already queued for release (guarded by the manager lock).

    protected:

        /** @brief CN: 字节已由读盘线程写入，确认存在即进入 Resident。 EN: Bytes were written by the IO thread; successful validation moves it to Resident. */
        bool OnLoadPayload()override;

        /** @brief CN: 释放文件字节。 EN: Release the file bytes. */
        void OnUnloadPayload()override;

        /** @brief CN: 交给管理器的待释放队列，不在此销毁。 EN: Hand over to the manager's pending-release queue instead of destroying here. */
        void OnRefZero()override;

    public:

        /**
         * @brief CN: 创建一个尚未加载的文件对象。
         *        EN: Create a not-yet-loaded file object.
         * @param filename CN: 源文件名，同时写入 META 的 source_path。 EN: Source file name, also stored into META source_path.
         */
        explicit LoadedFile(const OSString &filename);

        ~LoadedFile() override=default;

    public:

        /** @brief CN: 取源文件名。 EN: Return the source file name. */
        const OSString &    GetFilename ()const{return meta.source_path;}

        /** @brief CN: 取源数据字节数。 EN: Return the source byte count. */
        uint64              GetSize     ()const{return meta.source_size;}

        /**
         * @brief CN: 取只读字节数据。
         *        EN: Return the read-only byte payload.
         * @return CN: 仅在成功加载后有效；使用方须自行持有引用。 EN: Valid only after a successful load; the caller must hold a reference.
         */
        const uint8 *       GetData     ()const{return request.GetData();}
    };//class LoadedFile

    /** @brief CN: 带引用计数的已加载文件指针。 EN: Reference-counted pointer to a loaded file. */
    using LoadedFilePtr=RefPtr<LoadedFile>;
}//namespace hgl