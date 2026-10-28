#include<hgl/framework/LoadedFileManager.h>

namespace hgl
{
    LoadedFileManager::LoadedFileManager()
        :loader(this)
    {
    }

    LoadedFileManager::~LoadedFileManager()
    {
        Stop(thread::ShutdownMode::CancelPending);

        // 1. 丢弃所有未回调的请求；Job 持有的引用随之放开，归零的文件进入待释放队列
        {
            ValueArray<Job *> all;
            jobs.GetValueArray(all);

            for(int i=0;i<all.GetCount();i++)
                delete all[i];

            jobs.Clear();
            ready.Clear();
        }

        // 2. 解除所有文件与本管理器的关联；计数为 0 的立即销毁，其余等持有者放手后自行销毁
        ValueArray<LoadedFile *> dead;

        {
            MutexLock guard(&table_lock);

            for(int i=0;i<pending_release.GetCount();i++)
            {
                LoadedFile *file=pending_release[i];

                if(file->GetRefCount()==0)
                    dead.Add(file);
                else
                    file->owner=nullptr;

                file->in_pending=false;
            }

            pending_release.Clear();

            ValueArray<LoadedFile *> alive;
            files.GetValueArray(alive);

            for(int i=0;i<alive.GetCount();i++)
                alive[i]->owner=nullptr;

            files.Clear();
        }

        for(int i=0;i<dead.GetCount();i++)
            ReleaseFile(dead[i]);
    }

    bool LoadedFileManager::Start(uint io_worker_count)
    {
        if(started)
            return(false);

        started=loader.Start(io_worker_count);
        return started;
    }

    bool LoadedFileManager::Stop(thread::ShutdownMode mode)
    {
        const bool result=loader.Stop(mode);

        done_mailbox.Close();                       // 已接受的结果仍可被 Pump 取走
        return result;
    }

    // ---------------------------------------------------------------- IO 线程侧

    void LoadedFileManager::Post(Job *job,Outcome outcome,io::AsyncFileLoadError error)
    {
        job->outcome=outcome;
        job->error=error;
        done_mailbox.Post(job);
    }

    bool LoadedFileManager::OnFileLoaded(uint,io::AsyncFileLoadRequest *request)
    {
        Job *job=static_cast<Job *>(request->user_data);

        if(!job)
            return(false);

        job->outcome=Outcome::Loaded;
        return done_mailbox.Post(job);
    }

    void LoadedFileManager::OnFileError(uint,io::AsyncFileLoadRequest *request,io::AsyncFileLoadError error)
    {
        Job *job=static_cast<Job *>(request->user_data);

        if(job)
            Post(job,Outcome::Failed,error);
    }

    void LoadedFileManager::OnFileCancelled(uint,io::AsyncFileLoadRequest *request)
    {
        Job *job=static_cast<Job *>(request->user_data);

        if(job)
            Post(job,Outcome::Cancelled);
    }

    // ---------------------------------------------------------------- 引用归零 / 释放

    void LoadedFileManager::OnFileRefZero(LoadedFile *file)
    {
        MutexLock guard(&table_lock);

        if(file->in_pending)                        // 已在队列里：不重复入队
            return;

        file->in_pending=true;
        pending_release.Add(file);
    }

    void LoadedFileManager::ReleaseFile(LoadedFile *file)
    {
        file->Release();                            // Object 析构前必须 Release，否则会报违规日志
        delete file;
    }

    int LoadedFileManager::Collect()
    {
        ValueArray<LoadedFile *> dead;

        {
            MutexLock guard(&table_lock);

            for(int i=0;i<pending_release.GetCount();i++)
            {
                LoadedFile *file=pending_release[i];

                file->in_pending=false;

                if(file->GetRefCount()!=0)          // 排队期间被复活，放过
                    continue;

                LoadedFile *registered=nullptr;

                if(files.Get(file->path_key,registered)&&registered==file)
                    files.DeleteByKey(file->path_key);      // 摘表后 Find 再也找不到它，锁外释放是安全的

                dead.Add(file);
            }

            pending_release.Clear();
            released_files+=(uint64)dead.GetCount();
        }

        for(int i=0;i<dead.GetCount();i++)
            ReleaseFile(dead[i]);

        return dead.GetCount();
    }

    LoadedFilePtr LoadedFileManager::Find(const OSString &filename)
    {
        MutexLock guard(&table_lock);

        LoadedFile *file=nullptr;

        if(!files.Get(MakeLoadedFileKey(filename),file)||!file)
            return LoadedFilePtr();

        return LoadedFilePtr(file);                 // RefInc：计数为 0 的待释放文件在此复活
    }

    // ---------------------------------------------------------------- 提交 / 回调

    FileLoadId LoadedFileManager::Submit(const OSString &filename,LoadedFileHandler *handler,io::AsyncFilePriority priority)
    {
        if(!started||!handler||filename.IsEmpty())
            return(0);

        Job *job=new Job;
        job->id=next_id;
        job->handler=handler;

        LoadedFilePtr cached=Find(filename);

        if(cached)
        {
            job->file=cached;
            job->from_cache=true;
            job->outcome=Outcome::Loaded;

            ++next_id;
            ++stats.submitted;
            ++stats.cache_hits;

            jobs.Add(job->id,job);
            ready.Push(job);                        // 无需读盘，下一次 Pump 回调
            return job->id;
        }

        LoadedFile *file=new LoadedFile(filename);

        file->owner=this;

        if(!file->Init())
        {
            ReleaseFile(file);
            delete job;
            return(0);
        }

        job->file=LoadedFilePtr(file);              // 计数 0→1：读盘期间由 Job 持有
        file->request.user_data=job;

        jobs.Add(job->id,job);

        if(!loader.Submit(&file->request,priority))
        {
            jobs.DeleteByKey(job->id);
            delete job;                             // 引用归零 → 入待释放队列，等 Collect
            return(0);
        }

        ++stats.submitted;
        return next_id++;
    }

    bool LoadedFileManager::Cancel(FileLoadId id)
    {
        Job *job=nullptr;

        if(!jobs.Get(id,job)||!job)
            return(false);

        job->cancel_requested=true;

        if(!job->from_cache)
            job->file->request.Cancel();            // 协作式：读盘线程在检查点响应

        return(true);
    }

    void LoadedFileManager::Dispatch(Job *job)
    {
        jobs.DeleteByKey(job->id);

        FileLoadTicket ticket;
        ticket.id=job->id;
        ticket.filename=&job->file->GetFilename();

        Outcome outcome=job->outcome;
        io::AsyncFileLoadError error=job->error;

        if(outcome==Outcome::Loaded&&job->cancel_requested)
            outcome=Outcome::Cancelled;

        LoadedFile *publish=nullptr;                // 需要交给 handler 的文件

        if(outcome==Outcome::Loaded)
        {
            if(job->from_cache)
                publish=job->file.Get();
            else
            {
                LoadedFile *file=job->file.Get();

                file->GetMeta().source_size=(uint64)file->request.GetDataSize();

                if(!file->LoadPayload())
                {
                    outcome=Outcome::Failed;
                    error=io::AsyncFileLoadError::ReadFailed;
                }
                else
                {
                    MutexLock guard(&table_lock);

                    LoadedFile *existing=nullptr;

                    // 同名文件抢先读完(两次 Submit 在途重叠)：沿用已登记的那份，本份随 Job 一起走进待释放队列
                    if(files.Get(file->path_key,existing)&&existing&&existing!=file)
                        publish=existing;
                    else
                    {
                        files.Add(file->path_key,file);
                        publish=file;
                    }
                }
            }
        }

        if(publish)
        {
            LoadedFilePtr use(publish);             // 在锁外 RefInc，避免与归零回调互相阻塞

            job->handler->OnFileLoaded(ticket,use.Get());
            ++stats.dispatched_loaded;

            use.Reset();                            // 用完即放：没人再持有的文件归零入队
        }
        else
        {
            switch(outcome)
            {
                case Outcome::Failed:   job->handler->OnFileFailed(ticket,error);
                                        ++stats.dispatched_failed;
                                        break;

                default:                job->handler->OnFileCancelled(ticket);
                                        ++stats.dispatched_cancelled;
                                        break;
            }
        }

        delete job;                                 // Job 里的引用随之放开
    }

    int LoadedFileManager::Pump(int max_dispatch)
    {
        if(done_mailbox.TrySwap())
        {
            ValueArray<Job *> &batch=done_mailbox.GetReceive();

            for(int i=0;i<batch.GetCount();i++)
                ready.Push(batch[i]);

            done_mailbox.ReleaseReceive();
        }

        int count=0;

        while(count<max_dispatch)
        {
            Job *job=nullptr;

            if(!ready.Pop(job)||!job)
                break;

            Dispatch(job);
            ++count;
        }

        return count;
    }

    LoadedFileManagerStatistics LoadedFileManager::GetStatistics()
    {
        LoadedFileManagerStatistics result=stats;

        {
            MutexLock guard(&table_lock);

            result.alive_files=(uint64)files.GetCount();
            result.pending_release=(uint64)pending_release.GetCount();
            result.released_files=released_files;
        }

        result.io=loader.GetStatistics();
        return result;
    }
}//namespace hgl