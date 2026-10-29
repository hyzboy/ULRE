# 通用异步文件加载管理器(LoadedFileManager)——进度交接文档

> 记录时间:2026-10-28 17:40 · 工作目录:`D:\ULRE`
> 状态:**设计已定稿,第 1 步代码已写出但尚未编译、尚未注册到 CMake、尚无测试。**

---

## 1. 目标与已确认的决策

起点:`CMCore/examples/thread/async_file_loader_test.cpp`(最近 24 小时 CMCore 提交:`PriorityTaskQueue`、`WorkHandler/SingleWorker`、`AsyncFileLoader` 及耗时指标),参照 `example/Texture/AsyncTextureUpload.cpp` 与 `TextureArrayStreaming.cpp`,建立通用异步文件加载管理器。

| # | 决策 |
|---|---|
| 1 | 代码**先放在 ULRE 里**(不放 `CMAssetsManage`,该模块以后整合包裹系统时再升级) |
| 2 | 读取直接用 CMCore 的 `AsyncFileLoader`,**不接 `AssetSource`** |
| 3 | **只支持单个文件加载**;**本期不做优先级**、批量、Group、解码阶段 |
| 4 | 回调用**对象虚函数**(`LoadedFileHandler`),业务上下文放 handler 子类成员 |
| 5 | URI 第一版直接用 `OSString` 文件名 |
| 6 | 统一的"已加载原始文件"对象 `LoadedFile`,基于 `MetaObject`/`RefObject`:META 只放文件名(`source_path`)和长度(`source_size`),引用计数用 `RefObject` 自带 |
| 7 | 纹理侧用完就释放引用;**引用归零 → 进待释放队列,不立即销毁**;`Collect()` 统一释放。第一版 `Collect` 立即释放全部归零项 |
| 8 | 待释放期间被再次加载同名文件 → `RefInc` 复活,不重新读盘;`Find/Submit` 与待释放队列**共用同一把锁** |
| 9 | 两段式:读取段(IO 线程池)+ 提交段。**本期提交段只做单一主线程 `Pump()`**;提交段将来由另一批多线程模块接管(对应不同 transfer queue family),本期不管 |

---

## 2. 架构

```
主线程                IO 线程池 (CMCore AsyncFileLoader)                    主线程
Submit(path,handler) ─▶ 读盘到 LoadedFile 内部的 request ─▶ SwapMailbox ─▶ Pump() ─▶ handler->OnFileLoaded
        │                                                                        │
   命中已加载表 → 直接进 ready 队列(不读盘)                       使用方持有 LoadedFilePtr(RefPtr)
                                                                                 │ 用完 Reset
                                              引用归零 → LoadedFile::OnRefZero → 待释放队列
                                                                                 │
                                                                   Collect(): 计数仍为 0 → Release()+delete
```

关键点:

- **字节直接读进 `LoadedFile` 内部的 `AsyncFileLoadRequest`**(其 `data` 是私有的、只有 `AsyncFileLoader` 是友元,取不出 `MemoryBlock`;把 request 作为 `LoadedFile` 成员就避免了一次拷贝,也不用改 CMCore)。
- `Job`(管理器内部)= `{id, LoadedFilePtr file, handler, outcome, error, from_cache, cancel_requested}`,`request.user_data` 指向 Job。
- IO 线程回调(`OnFileLoaded/OnFileError/OnFileCancelled`)只做"写 outcome + `done_mailbox.Post(job)`";所有用户回调都在调用 `Pump()` 的线程触发。
- `Pump()` 登记已加载表时若发现同名文件已被抢先登记,沿用已登记那份,本份随 Job 进待释放队列。
- 防重复入队:`LoadedFile::in_pending` 标志(受 `table_lock` 保护)。复活后再次归零不会重复入队;`Collect` 对计数 > 0 的项直接放过并清标志。
- 线程约束:`Submit/Cancel/Pump/Collect` 限同一(主)线程;`Find` 及引用计数操作可任意线程。

---

## 3. 已写出的文件(均为 UTF-8 BOM + CRLF)

| 文件 | 内容 |
|---|---|
| `inc/hgl/framework/LoadedFile.h` | `LoadedFile : MetaObject`、`LoadedFilePtr = RefPtr<LoadedFile>`;覆盖 `OnLoadPayload / OnUnloadPayload / OnRefZero` |
| `inc/hgl/framework/LoadedFileHandler.h` | `FileLoadId`、`FileLoadTicket`、`LoadedFileHandler`(`OnFileLoaded` 纯虚;`OnFileFailed/OnFileCancelled` 默认空) |
| `inc/hgl/framework/LoadedFileManager.h` | `LoadedFileManager : io::AsyncFileLoadHandler`、`LoadedFileManagerStatistics` |
| `src/Work/LoadedFile.cpp` | `LoadedFile` 实现 |
| `src/Work/LoadedFileManager.cpp` | 管理器实现 |

管理器公共 API:`Start(io_workers)`、`Stop(ShutdownMode)`、`Submit(filename, handler) → FileLoadId`、`Cancel(id)`、`Pump(max_dispatch)`、`Collect()`、`Find(filename) → LoadedFilePtr`、`GetStatistics()`、`GetPendingJobCount()`。

---

## 4. 尚未完成的事项(按顺序)

### 4.1 注册到 CMake(**必须先做,否则不会被编译**)
`src/Work/CMakeLists.txt` 当前只有:
```cmake
SET(WORK_SOURCE ${WORK_INCLUDE_PATH}/WorkObject.h
                ${WORK_INCLUDE_PATH}/WorkManager.h
                ${WORK_INCLUDE_PATH}/AppFramework.h
                AppFramework.cpp WorkObject.cpp WorkManager.cpp)
add_cm_library(ULRE.Work "ULRE" ${WORK_SOURCE})
```
需要加入三个头和 `LoadedFile.cpp`、`LoadedFileManager.cpp`。该文件是 UTF-8 BOM + CRLF,编辑时保持。

### 4.2 编译并修正
代码**从未编译**。可能需要核对的点:
- `UnorderedMap<OSString, LoadedFile*>`(依赖 `std::hash<hgl::String<T>>` 特化,已在 `String.h:1222` 看到)。
- `ValueArray::operator[]` 与 `UnorderedMap::GetValueArray(ValueArray&)` 的用法。
- `SwapMailbox::ReleaseReceive()` 是否会清空接收批次(`Pump` 假定会)。
- `AsyncFileLoader::Submit(request, AsyncFilePriority)` 重载调用。
- `HGL_OBJECT` 宏在 `namespace hgl` 内的使用。
- `Object` 析构前必须 `Release()`,否则 `Log.Error` 报"对象析构前未调用Release()"——管理器所有销毁路径已走 `ReleaseFile()`,编译后留意日志。

### 4.3 单元测试(仿照 `CMCore/examples/thread/async_file_loader_test.cpp`)
建议放 `example/Loader/`(用 `example/CMakeLists.txt` 里的 `CreateProject` 宏,并 `add_subdirectory`),`if(BUILD_TESTING)` 下 `add_test`。覆盖:
1. 读取成功 → `Pump` 回调 `OnFileLoaded`,字节与 meta(文件名/长度)正确。
2. 文件不存在 → `OnFileFailed(OpenFailed)`。
3. 取消(提交前/后)→ `OnFileCancelled`,且无 `OnFileLoaded`。
4. handler 不保留 → 文件归零入待释放队列 → `Collect()` 释放,`alive_files` 归零。
5. **复活**:归零入队后 `Find`(或再次 `Submit`)命中,`Collect` 不释放;再次放手后 `Collect` 才释放。
6. 重复入队保护(复活后再归零只入队一次)。
7. 同名文件在途重叠两次 `Submit`。
8. `Stop(Drain)` 后仍可 `Pump` 取走结果;`CancelPending`;管理器析构不泄漏、不崩溃。
9. `Pump(max_dispatch)` 限量。

### 4.4 `TextureLoadCommitter`(纹理提交模块,主线程)
- 位置:`inc/hgl/graph/module/TextureLoadCommitter.h` + `src/SceneGraph/module/TextureLoadCommitter.cpp`,并在 `src/SceneGraph/CMakeLists.txt` 的 `GRAPH_MODULE_FILES` 里登记。
- **重要发现(推翻了此前"要改 `TextureUploadTask` 持有 `LoadedFilePtr`"的假设)**:`TextureLoader::Load(io::InputStream*)` 已存在,`io::MemoryInputStream` 也存在;而 `VkTextureLoader` 在 `Load` 内部是**同步把字节拷进映射好的 staging `DeviceBuffer`**(`OnBegin` 创建并 `Map`)。所以只要 `Load` 返回,`LoadedFile` 的字节就不再被需要,**调用方可立即释放引用,`TextureUploadTask` 不需要改**。
- 做法:
  - 在 `src/SceneGraph/texture/VKTexture2DLoader.cpp` 新增"从内存创建异步上传"函数:`MemoryInputStream` → `loader.Load(&stream)` → `CreateTextureCreateInfo` → `tm->CreateTexture2DAsync(...)`(逻辑与现有 `CreateTexture2DFromFileAsync` 相同,只是输入源不同)。
  - 在 `src/SceneGraph/texture/VKTexture2DArrayLoader.cpp` 把 `LoadTexture2DLayerFromFile` 的函数体拆成"`Load` + 公共的层提交逻辑",再加内存版本(同样用 `MemoryInputStream`)。注意这两个文件已经依赖 `VkTextureLoader<Texture2D,Texture2DLoader>::OnExtent` 的显式特化,新代码须放在同样的 TU 里。
  - `TextureLoadCommitter` 持有 `TextureManager*`,提供 `CommitTexture2D(const LoadedFile*, auto_mipmaps, UploadPriority, bindless_handle, callback, user_data) → task_id` 与 `CommitTexture2DArrayLayer(const LoadedFile*, Texture2DArray*, layer) → bool`。
  - `MemoryInputStream` 构造需要非 const `void*`,用 `const_cast`。
- 线程契约写进头文件注释:**当前只能在主线程调用**;`TextureUploadTask`/多 transfer queue family 多线程化留给未来。

### 4.5 两个示例改造(编译通过后)
- 保持 `TextureManager::LoadTexture2DAsync` / `LoadTexture2DArray` 的**原签名不变**(它们的返回值语义——立即返回 `task_id`、同步 `bool`——被现有代码依赖)。改造发生在示例里,新增的路径是"管理器 + handler + committer"。
- **`TextureArrayStreaming`**:`WorkObject` 里持有 `LoadedFileManager`(`Init` 里 `Start(2)`,`Tick` 里 `Pump` → 之后 `Collect`,析构里 `Stop`)。50 次 `Submit`,handler 子类成员放 `layer`/格位;`OnFileLoaded` 里调用 committer 写层并创建实体。去掉 `load_timer/load_aborted`;节奏用 `Pump(max_dispatch)` 或每帧限量代替。
- **`AsyncTextureUpload`**:五个文件加载改成 `Submit` + handler,handler 保存 commit 后的 `task_id`(状态打印改读 handler 里的 id);取消演示改用 `Cancel(FileLoadId)`;`bindless_handle`、完成回调放 handler 成员。
- 帧内顺序:`Pump`(提交到 GPU 队列)→ 使用方放手引用 → `Collect`。
- GPU 示例可能无法在无显示环境运行验证,至少要保证编译通过。

---

## 5. 注意事项 / 陷阱

1. **`RefObject` 的 `ref_count` 初值为 0**。新建 `LoadedFile` 必须立刻被某个 `RefPtr` 接管(管理器在 `Submit` 里 `job->file=LoadedFilePtr(file)` 完成),否则第一个 `RefPtr` 析构就会触发归零。
2. **`MetaObject` 默认策略 `KeepMetaUnloadPayload` 的实际效果**是归零即 `Release()`→`Disposed`(不可再用),并非"可再 `LoadPayload` 复用"。所以本方案覆盖 `OnRefZero`,自行管理"入队 / 复活 / 最终释放"。
3. `LoadedFile::OnRefZero` 在 `owner==nullptr` 时走老路径 `Release(); delete this;`——管理器析构时会把仍被外部持有的文件解除关联。管理器析构与其他线程同时 `RefDec` 的竞态未做处理(已在文档/注释中约定使用方保证)。
4. `Find` 在锁内 `RefInc`,`OnRefZero` 在锁内入队,`Collect` 在锁内"检查计数 + 摘表",锁外 `Release+delete`。摘表后无人能再 `Find` 到该文件,所以锁外释放安全。
5. handler 生命周期必须长于其所有未完成请求。
6. `AsyncFileLoader` 的取消是协作式的,不能中断正在进行的系统读取。
7. `CMAssetsManage` 里不少旧头文件出现乱码(疑似 GBK/UTF-8 混用),本次未触碰。新增文件统一 UTF-8 BOM + CRLF(与最近提交的"统一编码"约定一致)。

---

## 6. 仓库当前状态(迁移时注意)

- 本次新增的 5 个文件**未 `git add`、未提交**。
- 工作区里还有与本任务**无关**的未提交改动,迁移时不要混入:`.ai/skills/SKILL_CMCORE_REFERENCE.md`(已修改)、`CMScript/`、`res/`、`src/Tools/GLTFConvert/`(未跟踪)、`src/Tools/TexConv`(已修改)。
- `CMCore`、`CMAssetsManage` 为独立子仓库(各自有 git 历史),本次**没有修改它们**。
- 已有 `D:\ULRE\build` 构建目录(不确定是否与迁移后的环境一致,建议在新机器重新配置)。

---

## 7. 关键参考位置

| 内容 | 路径 |
|---|---|
| 起点示例 | `CMCore/examples/thread/async_file_loader_test.cpp` |
| 底层读盘 | `CMCore/inc/hgl/io/AsyncFileLoader.h` |
| 优先级队列 | `CMCore/inc/hgl/concurrency/PriorityTaskQueue.h` |
| 邮箱 | `CMCore/inc/hgl/concurrency/SwapMailbox.h` |
| 引用计数/元信息 | `CMCore/inc/hgl/object/RefObject.h`、`MetaObject.h`;实现 `CMCore/src/Object/` |
| 纹理异步上传 | `inc/hgl/graph/module/TextureUploadQueue.h`、`TextureUploadTask.h`、`TextureManager.h`;`src/SceneGraph/module/` |
| 纹理文件加载 | `src/SceneGraph/texture/VKTextureLoader.h`、`VKTexture2DLoader.cpp`、`VKTexture2DArrayLoader.cpp`;`inc/hgl/graph/texture/TextureLoader.h` |
| 两个目标示例 | `example/Texture/AsyncTextureUpload.cpp`、`TextureArrayStreaming.cpp` |
