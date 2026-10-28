#pragma once

#include<hgl/framework/LoadedFileHandler.h>
#include<hgl/concurrency/SwapMailbox.h>
#include<hgl/sync/Mutex.h>
#include<hgl/thread/ShutdownMode.h>
#include<hgl/type/Queue.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ValueArray.h>

namespace hgl
{
    /**
     * @brief CN: 加载管理器的运行统计快照。
     *        EN: Snapshot of the load manager's runtime statistics.
     * @details CN: 除 io 外的字段只由调用 Pump()/Collect() 的线程更新，读取时无需加锁。
     *          EN: All fields except io are updated only by the thread calling
     *              Pump()/Collect(), so reading them needs no locking.
     */
    struct LoadedFileManagerStatistics
    {
        uint64 submitted=0;             ///< @brief CN: Submit 接受的请求数。 EN: Requests accepted by Submit.
        uint64 cache_hits=0;            ///< @brief CN: 直接命中已加载表(含待释放区复活)的请求数。 EN: Requests served from the loaded table, including resurrection from the pending-release area.
        uint64 dispatched_loaded=0;     ///< @brief CN: 已回调 OnFileLoaded 的次数。 EN: Number of OnFileLoaded callbacks delivered.
        uint64 dispatched_failed=0;     ///< @brief CN: 已回调 OnFileFailed 的次数。 EN: Number of OnFileFailed callbacks delivered.
        uint64 dispatched_cancelled=0;  ///< @brief CN: 已回调 OnFileCancelled 的次数。 EN: Number of OnFileCancelled callbacks delivered.
        uint64 alive_files=0;           ///< @brief CN: 已加载文件表中的文件数(含待释放)。 EN: Files in the loaded table, including pending-release ones.
        uint64 pending_release=0;       ///< @brief CN: 当前待释放队列长度。 EN: Current pending-release queue length.
        uint64 released_files=0;        ///< @brief CN: Collect 累计真正释放的文件数。 EN: Files actually released by Collect so far.
        io::AsyncFileLoaderStatistics io;  ///< @brief CN: 底层读盘统计。 EN: Underlying read statistics.
    };//struct LoadedFileManagerStatistics

    /**
     * @brief CN: 通用异步文件加载管理器(两段式)：IO 线程读盘 + 调用线程提交。
     *        EN: Generic asynchronous file-load manager (two stages): IO-thread reads plus caller-thread commit.
     * @details
     * CN: 第一段(读取)由 CMCore 的 io::AsyncFileLoader 在 IO 线程池中把整个文件读进
     *     LoadedFile 自带的请求缓冲；第二段(提交)由调用线程周期性调用 Pump()，
     *     在其中回调 LoadedFileHandler，便于把结果投递到需要线程亲和性的目标(GPU 队列、
     *     音频线程等)。
     *
     *     已加载文件表与待释放队列共用同一把锁 table_lock：
     *      - 引用归零只入待释放队列，不立即销毁；
     *      - Collect() 时若计数仍为 0 才真正 Release()+delete；
     *      - 排队期间被 Find()/Submit() 命中则 RefInc 复活，Collect() 放过它。
     *     由此得到的语义是"弱表缓存"：只要还有人持有 LoadedFilePtr，同名文件就不会重复读盘。
     *
     *     表键由 MakeLoadedFileKey() 生成：分隔符规范化 + Windows 下大小写折叠。因此
     *     "Res\\Image/A.Tex2D" 与 "res/image/a.tex2d" 视为同一个文件；回调里报告的文件名
     *     仍是该文件首次 Submit 时的原始拼写。
     *
     *     线程约束：
     *      - Submit/Cancel/Pump/Collect/Stop 只允许在同一(主)线程调用；
     *      - Find 以及 LoadedFile 的 RefInc/RefDec 可在任意线程调用。
     *
     *     管理器必须比所有 LoadedFile 活得更久；析构时会解除关联(见析构函数说明)。
     * EN: Stage one (read) happens on AsyncFileLoader worker threads, which read a whole
     *     file into the request buffer owned by LoadedFile. Stage two (commit) happens when
     *     the caller invokes Pump() periodically; handlers run there so results can be
     *     routed to thread-affine destinations (GPU queues, audio threads, ...).
     *
     *     The loaded-file table and the pending-release queue share one table_lock: a
     *     reference count reaching zero only enqueues the file, Collect() performs the real
     *     Release()+delete when the count is still zero, and a hit through Find()/Submit()
     *     while queued resurrects it with RefInc. The result is a weak table: as long as
     *     someone holds a LoadedFilePtr the same path is never read twice.
     *
     *     Table keys come from MakeLoadedFileKey(): separator normalization plus case
     *     folding on Windows, so "Res\\Image/A.Tex2D" and "res/image/a.tex2d" are one
     *     entry. The file name reported to handlers keeps the spelling used by the first
     *     Submit of that file.
     *
     *     Threading: Submit/Cancel/Pump/Collect/Stop must be called from one thread
     *     (normally the main thread); Find and reference counting are thread-safe.
     *     The manager must outlive every LoadedFile; destruction detaches them.
     */
    class LoadedFileManager:public io::AsyncFileLoadHandler
    {
        friend class LoadedFile;

        /**
         * @brief CN: 一次提交请求的私有状态。 EN: Private state of one submitted request.
         */
        enum class Outcome:uint8
        {
            Loading=0,      ///< @brief CN: 仍在读盘。 EN: Still reading.
            Loaded,         ///< @brief CN: 读取成功(可能来自缓存)。 EN: Read succeeded, possibly from cache.
            Failed,         ///< @brief CN: 读取或负载校验失败。 EN: Read or payload validation failed.
            Cancelled       ///< @brief CN: 被取消。 EN: Cancelled.
        };//enum class Outcome

        /**
         * @brief CN: 一次请求的完整上下文，贯穿读取段与提交段。
         *        EN: Full context of one request spanning both stages.
         * @details CN: Job 由 Submit 创建、由 Dispatch 销毁；IO 线程只把它贴到邮箱上，
         *              由回调线程填写 outcome。
         *          EN: Created by Submit and destroyed by Dispatch. IO threads only post it
         *              through the mailbox; the callback thread fills in its outcome.
         */
        struct Job
        {
            FileLoadId                  id=0;                       ///< @brief CN: 请求编号。 EN: Request identifier.
            LoadedFilePtr               file;                       ///< @brief CN: 本次请求的文件对象。 EN: File object for this request.
            LoadedFileHandler *         handler=nullptr;            ///< @brief CN: 结果接收者(不拥有)。 EN: Result receiver, not owned.
            Outcome                     outcome=Outcome::Loading;   ///< @brief CN: 当前结果。 EN: Current outcome.
            io::AsyncFileLoadError      error=io::AsyncFileLoadError::None; ///< @brief CN: 失败原因。 EN: Failure reason.
            bool                        from_cache=false;           ///< @brief CN: 命中已加载表，未走读盘。 EN: Served from the loaded table without reading.
            bool                        cancel_requested=false;     ///< @brief CN: 调用方已请求取消。 EN: Caller requested cancellation.
        };//struct Job

    private:

        io::AsyncFileLoader             loader;                     ///< @brief CN: 底层读盘线程池。 EN: Underlying read worker pool.
        SwapMailbox<Job *>              done_mailbox;               ///< @brief CN: 多写单读：IO 线程 → 调用线程。 EN: Multiple writers, one reader: IO threads to caller thread.

        FileLoadId                      next_id=1;                  ///< @brief CN: 下一个请求编号。 EN: Next request identifier.
        UnorderedMap<FileLoadId,Job *>  jobs;                       ///< @brief CN: 尚未回调完的请求(调用线程独占)。 EN: Requests not yet dispatched (caller-thread only).
        Queue<Job *>                    ready;                      ///< @brief CN: 已从邮箱取出、待回调的请求(调用线程独占)。 EN: Requests taken from the mailbox and awaiting dispatch (caller-thread only).

        Mutex                           table_lock;                 ///< @brief CN: 保护下面三项。 EN: Guards the three members below.
        UnorderedMap<OSString,LoadedFile *> files;                  ///< @brief CN: 已加载文件表(弱引用，不增加计数)。 EN: Loaded-file table holding weak, non-owning pointers.
        ValueArray<LoadedFile *>        pending_release;            ///< @brief CN: 引用归零、等待 Collect 的文件。 EN: Files whose count reached zero and await Collect.
        uint64                          released_files=0;           ///< @brief CN: 累计真正释放的文件数。 EN: Total files actually released.

        LoadedFileManagerStatistics     stats;                      ///< @brief CN: 调用线程独占的计数。 EN: Counters owned by the caller thread.
        bool                            started=false;              ///< @brief CN: 是否已启动。 EN: Whether the manager has been started.

    private:

        bool OnFileLoaded   (uint worker_index,io::AsyncFileLoadRequest *request)override;
        void OnFileError    (uint worker_index,io::AsyncFileLoadRequest *request,io::AsyncFileLoadError error)override;
        void OnFileCancelled(uint worker_index,io::AsyncFileLoadRequest *request)override;

        /** @brief CN: IO 线程：写结果标记并投递邮箱。 EN: IO thread: record the outcome and post to the mailbox. */
        void Post(Job *job,Outcome outcome,io::AsyncFileLoadError error=io::AsyncFileLoadError::None);

        /** @brief CN: 引用归零回调(任意线程，持 table_lock 入队)。 EN: Reference-zero callback; enqueues while holding table_lock. */
        void OnFileRefZero(LoadedFile *file);

        /** @brief CN: 回调一个请求并销毁 Job。 EN: Dispatch one request and destroy its Job. */
        void Dispatch(Job *job);

        /** @brief CN: Release()+delete，必须在 table_lock 之外调用。 EN: Release()+delete; must run outside table_lock. */
        void ReleaseFile(LoadedFile *file);

    public:

        LoadedFileManager();

        /**
         * @brief CN: 析构：停止读盘线程并丢弃尚未回调的请求。
         *        EN: Destructor: stop read threads and drop requests that were never dispatched.
         * @details CN: 仍被外部持有的 LoadedFile 会被解除与本管理器的关联，之后它们引用归零
         *              时按 AutoDestroy 语义自行销毁；计数已为 0 的在析构中立即销毁。
         *          EN: Files still held elsewhere are detached from this manager and destroy
         *              themselves when their count reaches zero; files already at zero are
         *              destroyed here.
         * @note CN: 调用方必须保证析构期间没有其他线程同时对 LoadedFile 做 RefDec。
         *       EN: The caller must guarantee no other thread RefDecs a LoadedFile during destruction.
         */
        ~LoadedFileManager() override;

        /**
         * @brief CN: 启动 IO 线程池，仅可成功一次。
         *        EN: Start the IO worker pool; succeeds only once.
         * @param io_worker_count CN: IO 线程数，必须大于 0。 EN: Number of IO workers; must be greater than 0.
         * @return CN: 成功返回 true。 EN: True on success.
         */
        bool Start(uint io_worker_count);

        /**
         * @brief CN: 停止读盘；停止后仍可 Pump 取走已完成的结果。
         *        EN: Stop reading; completed results can still be drained through Pump.
         * @param mode CN: Drain 读完已接受的请求；CancelPending 取消仍在排队中的请求。
         *             EN: Drain processes accepted requests; CancelPending cancels still-queued requests.
         * @return CN: 所有 IO 线程成功退出时返回 true。 EN: True when every IO thread joined successfully.
         */
        bool Stop(thread::ShutdownMode mode=thread::ShutdownMode::Drain);

        /**
         * @brief CN: 提交一个文件加载请求。
         *        EN: Submit one file-load request.
         * @param filename CN: 文件名；查表前会经 MakeLoadedFileKey() 规范化。 EN: File name; normalized through MakeLoadedFileKey() before the lookup.
         * @param handler  CN: 结果接收者，必须活到该请求回调完成。 EN: Result receiver that must outlive this request's callback.
         * @param priority CN: 读盘优先级(数值越小越优先)。 EN: Read priority; lower values are served first.
         * @return CN: 请求编号，0 表示失败。 EN: Request identifier, or 0 on failure.
         * @note CN: 命中已加载表时不读盘，结果在下一次 Pump() 中回调。
         *       EN: A table hit skips reading and is dispatched by the next Pump().
         */
        FileLoadId Submit(const OSString &filename,
                          LoadedFileHandler *handler,
                          io::AsyncFilePriority priority=io::AsyncFilePriority::Normal);

        /**
         * @brief CN: 取消一个尚未回调的请求。
         *        EN: Cancel a request that has not been dispatched yet.
         * @return CN: false 表示编号无效(已回调或从未存在)。 EN: False if the identifier is unknown or already dispatched.
         * @note CN: 取消是协作式的：不能中断正在进行的系统读取，但即使读取已完成，
         *           回调也会被转成 OnFileCancelled。
         *       EN: Cancellation is cooperative and cannot interrupt a blocking OS read, but a
         *           request that already finished is still reported as OnFileCancelled.
         */
        bool Cancel(FileLoadId id);

        /**
         * @brief CN: 在调用线程回调已完成的请求。
         *        EN: Dispatch completed requests on the calling thread.
         * @param max_dispatch CN: 本次最多回调的请求数。 EN: Maximum number of requests to dispatch this call.
         * @return CN: 本次实际回调的请求数。 EN: Number of requests actually dispatched.
         * @note CN: 每帧限量可把完成事件平滑摊到多帧，避免一次性提交过多 GPU 上传。
         *       EN: Limiting per frame spreads completion work across frames, avoiding an upload burst.
         */
        int Pump(int max_dispatch=0x7fffffff);

        /**
         * @brief CN: 释放待释放队列中引用计数仍为 0 的文件。
         *        EN: Release queued files whose reference count is still zero.
         * @return CN: 本次释放的文件数。 EN: Number of files released.
         * @note CN: 建议在帧末调用(使用方已放手引用之后)。
         *       EN: Call at frame end, after consumers have dropped their references.
         */
        int Collect();

        /**
         * @brief CN: 在已加载表中查找；命中时 RefInc，可复活待释放的文件。
         *        EN: Look up the loaded table; a hit RefIncs and thus resurrects pending-release files.
         * @return CN: 命中返回持有引用的指针，否则为空。 EN: A reference-holding pointer on hit, empty otherwise.
         * @note CN: 任意线程可调用。 EN: Safe to call from any thread.
         */
        LoadedFilePtr Find(const OSString &filename);

        /** @brief CN: 尚未回调完的请求数(仅调用线程)。 EN: Requests not yet dispatched (caller thread only). */
        int GetPendingJobCount()const{return jobs.GetCount();}

        /**
         * @brief CN: 取运行统计快照。
         *        EN: Take a statistics snapshot.
         * @note CN: 可在任意线程调用；内部会短暂持有 table_lock 取表长度。
         *       EN: Callable from any thread; internally holds table_lock briefly.
         */
        LoadedFileManagerStatistics GetStatistics();
    };//class LoadedFileManager
}//namespace hgl