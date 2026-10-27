# SKILL: 统一任务架构与优先级异步文件 IO

本文档说明 CMCore 的任务执行/队列解耦模型，以及如何使用优先级异步文件加载服务，并将图片、音频等结果投递到不同目标线程。

This guide describes CMCore's decoupled task execution and queue model, plus priority-based asynchronous file loading and routing results to different destination threads.

## 1. 架构与职责

```mermaid
flowchart LR
    P[多个业务生产者] --> Q[独立任务队列 / 优先级队列]
    Q --> E[SingleWorker / WorkerPool / PolledWorker]
    E --> H[WorkHandler]
    H --> R[按 target_id 投递到目标线程队列]
    R --> T1[渲染 / 主线程]
    R --> T2[音频 / 专用线程]
```

| 组件 | 职责 | 生命周期/线程要点 |
|---|---|---|
| `WorkHandler<W>` | 仅定义 `OnWork(worker_index, work)` 与可选 `OnCancel(work)` | 不拥有任务；多个 worker 可并发调用同一处理器 |
| `TaskQueue<T>` | 多生产者、多消费者 FIFO 队列 | 队列可单独创建并注入执行器；关闭后拒绝新任务 |
| `SingleWorker<W>` | 在一个后台线程消费队列 | 接收外部 `TaskQueue<W*>`；`Stop()` 关闭队列并排空 |
| `WorkerPool<W>` | 多个后台线程竞争消费一个队列 | 可使用内部队列，或注入外部队列；业务处理器负责同步共享状态 |
| `PolledWorker<W>` | 由调用方线程显式调用 `Process()` 消费 | 适用于主线程帧循环；同一队列应保持单消费者 |
| `PriorityTaskQueue<T, Levels>` | 固定等级的 FIFO 优先级队列 | 数值小者优先、同级 FIFO；关闭和取消语义明确 |

生产者、接收队列、执行器和处理器可以分别管理。队列允许多个生产者提交；执行器负责消费与生命周期；处理器只写业务工作。`WorkerPool` 和 `SingleWorker` 的外部队列构造方式适合需要独立管理投递端的场景。执行器停止时会关闭注入的队列，因此应由该队列的协调者统一安排关闭顺序。

Producers, queues, executors and handlers can be managed independently. Queues accept submissions, executors own worker lifecycle, and handlers implement business work. The external-queue constructors of `WorkerPool` and `SingleWorker` support separate posting ownership. Stopping either executor closes its injected queue, so the queue coordinator should control shutdown ordering.

## 2. 通道拓扑选择

| 生产者 → 消费者 | 建议队列 | 适用示例 |
|---|---|---|
| 单 → 单 | `SpscQueue<T>`（非阻塞、容量固定）或 `TaskQueue<T>` | 音频回调与专用消费线程间的一对一数据通道 |
| 多 → 单 | `SwapMailbox<T>`（批次交换）或 `TaskQueue<T>` | 多个 IO worker 向渲染/主线程提交完成结果 |
| 多 → 多 | `TaskQueue<T>` / `PriorityTaskQueue<T, Levels>` | 多线程共同提交后台任务、多个 worker 竞争处理 |
| 单 → 多 | 一个共享 `TaskQueue<T>` 由 `WorkerPool` 消费；按键分片可由业务路由 | 单个调度点分发并行任务 |

`SpscQueue` 的单生产者/单消费者约束必须真实成立。若多个 IO worker 都能完成并分发任务，目标队列至少需要支持多生产者（例如 `SwapMailbox` 或 `TaskQueue`）；若必须使用 SPSC，可为每个 worker 建独立 SPSC 队列，再由目标线程做 fan-in。

The SPSC constraint must be true in practice. When multiple IO workers may complete and dispatch work, destination queues must support multiple producers, such as `SwapMailbox` or `TaskQueue`. If SPSC is required, use one SPSC queue per worker and fan them in on the destination thread.

## 3. 优先级队列语义

```cpp
#include <hgl/concurrency/PriorityTaskQueue.h>

hgl::PriorityTaskQueue<MyTask *,4> queue;
queue.Submit(task,0); // highest priority
queue.Submit(other,3); // lowest priority
```

- 优先级范围是 `[0, LevelCount)`，数值越小越先出队。
- 每一级自身维持 FIFO；提交时为更新峰值统计、取出时为选择最高优先级，都会扫描固定数量的等级，操作成本为 `O(LevelCount)`；等级数固定且较少（例如 4 级）时是有界的小扫描，不是堆排序。
- `Close()` 拒绝新任务并允许消费者排空；`CloseAndCancel()` 原子关闭并取出尚未出队的任务。
- 队列内任务是平凡可复制值；常用方式是排队 `Task*`，对象仍由调用方管理。
- 严格高优先级优先不保证公平：持续的高优先级负载可能使低优先级饥饿。优先级只影响尚未出队的任务，不会抢占正在执行的 IO。

`AsyncFileLoader` 固定提供四级常用优先级，也允许调用方直接传入 `uint` 自定义 0–3 的级别：

| API 名称 | 数值 | 用途示例 |
|---|---:|---|
| `AsyncFilePriority::Critical` | 0 | 首屏、当前可见 UI/纹理 |
| `AsyncFilePriority::High` | 1 | 当前场景必需资源 |
| `AsyncFilePriority::Normal` | 2 | 常规贴图、音效 |
| `AsyncFilePriority::Low` | 3 | 远处资源、背景音乐预加载 |

一个全局加载器队列让图片和音频请求在同一优先级域竞争 IO；完成后的目标线程由请求的 `target_id` 决定。若不同存储设备、资源预算或服务等级需要完全隔离，可创建多个加载器/队列，而不是让一个优先级字段承担所有资源策略。

The loader uses one global priority domain so image and audio requests compete for the same IO workers; `target_id` chooses the destination after loading. Use separate loaders/queues when storage devices, memory budgets or service classes need strict isolation instead of overloading a single priority field.

## 4. 异步文件加载与目标线程分流

`AsyncFileLoader` 读取整个文件到请求自带的 `MemoryBlock`。`OnFileLoaded` 在 IO worker 上运行，应尽量只做轻量的路由投递，**不要在这里调用要求渲染线程或音频线程亲和性的 API**。

`AsyncFileLoader` reads each whole file into the request's `MemoryBlock`. `OnFileLoaded` runs on an IO worker and should normally only route the request; do **not** call APIs that require render-thread or audio-thread affinity from this callback.

```cpp
#include <hgl/concurrency/SwapMailbox.h>
#include <hgl/concurrency/TaskQueue.h>
#include <hgl/io/AsyncFileLoader.h>

class ResourceRouter final : public hgl::io::AsyncFileLoadHandler
{
public:
    hgl::SwapMailbox<hgl::io::AsyncFileLoadRequest *> render_queue;
    hgl::TaskQueue<hgl::io::AsyncFileLoadRequest *> audio_queue;

    bool OnFileLoaded(uint,hgl::io::AsyncFileLoadRequest *request) override
    {
        switch(request->target_id)
        {
            case 1: return render_queue.Post(request); // MPSC: IO workers -> one render consumer
            case 2: return audio_queue.Submit(request); // MPMC queue -> audio worker(s)
            default: return false;
        }
    }

    void OnFileError(uint,hgl::io::AsyncFileLoadRequest *,
                     hgl::io::AsyncFileLoadError) override
    {
        // Record/report the failure using the project's HGL logging and error path.
    }

    void OnFileCancelled(uint,hgl::io::AsyncFileLoadRequest *) override
    {
        // Notify the resource owner that the request will not be delivered.
    }
};

ResourceRouter router;
hgl::io::AsyncFileLoader loader(&router);
hgl::io::AsyncFileLoadRequest image(image_path,1);
hgl::io::AsyncFileLoadRequest music(music_path,2);

if(loader.Start(2))
{
    loader.Submit(&image,hgl::io::AsyncFilePriority::Critical);
    loader.Submit(&music,hgl::io::AsyncFilePriority::Low);
    loader.Stop(hgl::thread::ShutdownMode::Drain);
}
```

实际资源处理时，渲染线程从 `render_queue` 批次取出图片请求，在渲染线程创建/上传 GPU 资源；音频队列由音频 `SingleWorker` 或 `WorkerPool` 消费并执行解码/缓冲处理。测试示例 `CMCore/examples/thread/async_file_loader_test.cpp` 用小型临时文件模拟这两条路由，不依赖 Vulkan 或音频设备。

In the real resource pipeline, the render thread consumes image requests from `render_queue` and creates/uploads GPU resources there. An audio `SingleWorker` or `WorkerPool` consumes the audio queue and performs decoding/buffer work. `CMCore/examples/thread/async_file_loader_test.cpp` simulates both routes with a small temporary file and does not require Vulkan or an audio device.

## 5. 请求、缓冲区与关闭生命周期

1. 每个 `AsyncFileLoadRequest` 是一次性请求对象；不要并发重复提交同一对象。调用方需保持对象有效，直到错误/取消回调返回；成功投递后，还要保持其有效，直到目标线程完成处理。
2. `OnFileLoaded` 返回 `true` 表示目标队列已接受请求。目标处理完成后调用 `request->ReleaseData()` 释放文件缓冲区，再结束请求生命周期。
3. 返回 `false` 表示拒绝投递：加载器释放缓冲区并调用 `OnFileError(..., DeliveryRejected)`。
4. `Cancel()` 是协作式取消。若任务尚未开始，worker 会跳过 IO；读取中取消会在下一次读操作边界检查并释放部分缓冲区。它不能中断阻塞中的系统调用，也不能撤回已经成功投递到目标队列的请求。
5. `Stop(Drain)` 停止接收新请求并等待已接受的请求读完和交付。`Stop(CancelPending)` 对仍在优先队列中的请求调用 `OnFileCancelled`，该取消回调在调用 `Stop` 的线程上运行；已经被 worker 取出的请求继续运行。
6. 错误回调和成功回调可能在多个 IO worker 上并发执行；`AsyncFileLoadHandler` 的共享状态必须自行同步。不要在这些回调内调用同一加载器的 `Stop()`。
7. 若文件很大或请求可能无限堆积，需由上层做并发/内存预算与背压。当前加载器一次性读完整个文件，优先队列没有容量上限，也不是分块流式读取器。

1. Treat each request as one-shot; do not submit the same object concurrently. Keep it alive through error/cancel callbacks, and after successful routing until destination processing completes.
2. `OnFileLoaded == true` means the destination accepted the request. Call `request->ReleaseData()` after destination processing to release the file buffer.
3. `OnFileLoaded == false` rejects delivery; the loader releases the buffer and reports `DeliveryRejected`.
4. `Cancel()` is cooperative. A queued task is skipped; an active read checks cancellation between reads and releases partial data. It cannot interrupt a blocking OS call or retract a request already delivered.
5. `Stop(Drain)` stops submissions and waits for accepted reads and deliveries. `Stop(CancelPending)` calls `OnFileCancelled` for tasks still queued; those callbacks run on the thread calling `Stop`. Requests already dequeued continue.
6. Success/error callbacks can run concurrently on multiple IO workers. Synchronize shared handler state and do not call the same loader's `Stop()` from a callback.
7. For large files or unbounded request production, apply application-level concurrency/memory budgets and backpressure. The current loader reads whole files and its priority queue is unbounded; it is not a chunked streaming reader.

## 6. 统计与诊断

`GetStatistics()` 返回线程安全的累计指标和队列快照：

- `submitted` / `popped`：被接受与被 worker 取出的任务数。
- `loaded` / `failed` / `cancelled`：成功交付、失败与取消数。
- `loaded_bytes`：成功交付的文件字节总数。
- `pending` / `peak_pending`：当前排队量与队列峰值。
- `queue_wait_microseconds`：被取出任务累计排队等待时间。
- `read_microseconds` / `read_samples`：文件打开和读取阶段累计耗时及采样数。平均读取耗时可在 `read_samples > 0` 时计算为总耗时除以采样数。

These are cumulative, low-contention counters rather than a tracing system. Queue wait includes time from successful submission to dequeue; read duration covers open/size/allocation/read processing and excludes destination callback execution. Track per-resource timing, histograms, cancellation reasons or per-worker utilization in a higher-level profiler if those are needed.

## 7. 验证示例

`CMCore/examples/thread/` 中的程序覆盖：

- `work_handler_test.cpp`：轮询执行、单后台 worker、外部队列注入到 `WorkerPool`。
- `priority_task_queue_test.cpp`：优先级顺序、同级 FIFO、超时等待、关闭排空、取消和统计。
- `async_file_loader_test.cpp`：图片/音频目标队列分流、成功加载、取消与聚合统计。

在仓库现有 Windows Debug 构建目录中，可通过以下命令构建并运行这些目标：

```powershell
cmake --build build --config Debug --target WorkHandlerTest PriorityTaskQueueTest AsyncFileLoaderTest WorkerPoolTest
ctest --test-dir build -C Debug -R "^(WorkHandlerTest|PriorityTaskQueueTest|AsyncFileLoaderTest|WorkerPoolTest)$" --output-on-failure
```
