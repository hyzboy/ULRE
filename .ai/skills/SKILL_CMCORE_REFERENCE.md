# SKILL: CMCore 完整参考手册

> **何时使用本文档**：当你需要多线程、时间、字符编码、队列/栈/LRU缓存、内存管理、智能指针等功能时，查阅本文档找到正确的 HGL API。**严禁使用 STL 的 `<thread>`, `<mutex>`, `<atomic>`, `<chrono>`, `<queue>`, `<stack>`**。

---

## 目录

1. [线程/并发（Threading）](#1-线程并发threading)
2. [时间（Time）](#2-时间time)
3. [字符编码（Charset / Endian / UTF）](#3-字符编码charset--endian--utf)
4. [集合类型扩展（CMCore Collections）](#4-集合类型扩展cmcore-collections)
5. [内存管理（Memory Management）](#5-内存管理memory-management)
6. [智能指针（Smart Pointers）](#6-智能指针smart-pointers)
7. [对象基类（Object）](#7-对象基类object)
8. [文件系统扩展（Filesystem Extensions）](#8-文件系统扩展filesystem-extensions)
9. [IO 扩展（IO Extensions）](#9-io-扩展io-extensions)
10. [STL → HGL 线程/时间替换速查表](#10-stl--hgl-线程时间替换速查表)

---

## 1. 线程/并发（Threading）

### 1.1 原子类型 `Atomic.h` → 替代 `<atomic>`

```cpp
#include <hgl/thread/Atomic.h>

// 原子类型（封装 std::atomic，允许此头文件内部使用）
hgl::atom<int>   counter{0};
hgl::atom_bool   flag{false};
hgl::atom_int    index{0};
hgl::atom_int32  ref_cnt{1};
hgl::atom_uint64 timestamp{0};

// 用法与 std::atomic 一致
counter.fetch_add(1, std::memory_order_acq_rel);
bool prev = flag.exchange(true);
int val = index.load(std::memory_order_acquire);
index.store(42, std::memory_order_release);
```

可用别名：`atom_bool`, `atom_char`, `atom_uchar`, `atom_int`, `atom_uint`, `atom_int32`, `atom_uint32`, `atom_int64`, `atom_uint64`, `atom_float`

---

### 1.2 线程类 `Thread.h` → 替代 `std::thread`

```cpp
#include <hgl/thread/Thread.h>

// 继承 Thread，重写 Execute()
class MyThread : public hgl::Thread
{
    OBJECT_LOGGER
public:
    bool Execute() override
    {
        // 在这里写线程循环体，返回 true 继续，返回 false 退出
        LogInfo(u8"线程帧执行");
        return true;
    }
};

// 使用
MyThread *t = new MyThread();
t->Start();         // 启动线程
// ...
t->WaitExit();      // 等待线程退出（内部调用 Exit()）
// Thread 默认在退出时自动 delete 自身（DeletedAfterExit()=true）

// 多线程管理
hgl::MultiThreadManage<MyThread> mgr;
mgr.Add(new MyThread());
mgr.Add(new MyThread());
mgr.Start();        // 启动所有线程
mgr.Close();        // 等待所有线程退出
```

---

### 1.3 互斥锁 `ThreadMutex.h` → 替代 `std::mutex` / `std::lock_guard`

```cpp
#include <hgl/thread/ThreadMutex.h>

hgl::ThreadMutex mutex;

// 手动加锁/解锁
mutex.Lock();
// ... 访问共享数据 ...
mutex.Unlock();

// 尝试加锁（非阻塞）
if (mutex.TryLock()) {
    // ... 
    mutex.Unlock();
}

// RAII 自动释放锁（替代 std::lock_guard）
{
    hgl::ThreadMutexLock lock(&mutex);  // 构造时自动 Lock()
    // ... 访问共享数据 ...
}   // 析构时自动 Unlock()

// 带数据的线程安全对象
hgl::ThreadMutexObject<MyData> safe_obj;
safe_obj->DoSomething();    // 通过 operator-> 访问数据（不加锁，需手动锁定）
```

---

### 1.4 读写锁 `RWLock.h` → 替代 `std::shared_mutex`

```cpp
#include <hgl/thread/RWLock.h>

hgl::RWLock rw;

// 读锁（共享访问，多个读线程可同时持有）
rw.ReadLock();
// ... 只读访问 ...
rw.ReadUnlock();

// 写锁（独占访问）
rw.WriteLock();
// ... 修改数据 ...
rw.WriteUnlock();

// RAII 读锁（替代 std::shared_lock）
{
    hgl::OnlyReadLock read_guard(rw);   // 构造时 ReadLock()
    // ...
}   // 析构时 ReadUnlock()

// RAII 写锁（替代 std::unique_lock<shared_mutex>）
{
    hgl::OnlyWriteLock write_guard(rw); // 构造时 WriteLock()
    // ...
}   // 析构时 WriteUnlock()
```

---

### 1.5 信号量 `Semaphore.h`

```cpp
#include <hgl/thread/Semaphore.h>

hgl::Semaphore sem(10);  // 初始容量 10

// 生产者
sem.Post(1);                // 发送 1 个信号

// 消费者
sem.Acquire();              // 阻塞等待一个信号
sem.Acquire(5.0);           // 等待最多 5 秒
bool ok = sem.TryAcquire(); // 非阻塞尝试
```

---

### 1.6 条件变量 `CondVar.h`

```cpp
#include <hgl/thread/CondVar.h>

hgl::ThreadMutex mutex;
hgl::CondVar cond;

// 等待线程
mutex.Lock();
cond.Wait(&mutex);          // 释放 mutex 并等待信号，唤醒后重新持有 mutex
mutex.Unlock();

// 通知线程
cond.Signal();              // 唤醒一个等待线程
cond.Broadcast();           // 唤醒所有等待线程
```

---

### 1.7 双缓冲数据交换 `SwapData.h`

适用场景：多个线程投递数据，一个线程消费数据，零拷贝交换。

```cpp
#include <hgl/thread/SwapData.h>

// 简单双缓冲
hgl::SwapData<MyStruct> swap;

// 投递方（多个线程）
{
    MyStruct &post = swap.GetPost();    // 加锁获取写缓冲
    post.value = 42;
    swap.ReleasePost();                 // 解锁
}

// 消费方（单线程）
swap.Swap();                            // 交换前后台
MyStruct &recv = swap.GetReceive();     // 获取读缓冲（无锁）

// 带信号的版本（消费方阻塞等待）
hgl::SemSwapData<MyStruct> sem_swap;

// 投递方
{
    MyStruct &post = sem_swap.GetPost();
    post.value = 99;
    sem_swap.ReleasePost();
    sem_swap.PostSem();                 // 发送信号通知消费方
}

// 消费方
sem_swap.WaitSemSwap(5.0);             // 等待最多 5 秒并自动交换
MyStruct &data = sem_swap.GetReceive();
```

---

### 1.8 线程安全集合交换 `SwapColl.h`

```cpp
#include <hgl/thread/SwapColl.h>

// SwapList（基于 ValueArray 的线程安全列表）
hgl::SwapList<int> list;

// 投递方（可多线程）
list.Add(1);
list.Add(2);

// 消费方
hgl::ValueArray<int> &proc = list.GetProcList(); // 加锁合并 + 解锁，返回处理列表
for (int i = 0; i < proc.GetCount(); i++) { /* 处理 proc[i] */ }

// SemSwapList（带信号等待）
hgl::SemSwapList<int> sem_list;
sem_list.Add(42);                       // 自动发送信号

// 消费方
sem_list.WaitProc(5.0);               // 等待最多 5 秒并合并
hgl::ValueArray<int> &items = *sem_list;
```

---

### 1.9 环形缓冲 `RingBuffer.h`

```cpp
#include <hgl/thread/RingBuffer.h>

hgl::RingBuffer<uint8> ring(4096);      // 4096字节环形缓冲

// 写线程
ring.SafeWrite(data, size);

// 读线程  
ring.SafeRead(buf, size);
int avail = ring._GetReadSize();
```

---

## 2. 时间（Time）

### 2.1 基础时间函数 `Time.h`

```cpp
#include <hgl/time/Time.h>

// 系统时间（Unix 纪元起）
uint64 ms  = hgl::GetTimeMs();          // 毫秒
uint64 us  = hgl::GetTimeUs();          // 微秒
double sec = hgl::GetTimeSec();         // 秒（高精度浮点）
double local = hgl::GetLocalTimeSec();  // 本地时间（含时区）

// 程序运行时间
uint64 up_ms  = hgl::GetUptimeMs();
uint64 up_us  = hgl::GetUptimeUs();
double up_sec = hgl::GetUptimeSec();

// 休眠
hgl::SleepSecond(0.1);                 // 休眠 100 毫秒

// Unix 时间戳
uint64 unix_sec = hgl::GetUnixTimestampSec();
uint64 unix_ms  = hgl::GetUnixTimestampMs();
uint64 unix_us  = hgl::GetUnixTimestampUs();

// 计时示例
double start = hgl::GetUptimeSec();
// ... 执行操作 ...
double elapsed = hgl::GetUptimeSec() - start;
```

---

### 2.2 时间戳类 `Timestamp.h`

```cpp
#include <hgl/time/Timestamp.h>

// 创建时间戳
hgl::Timestamp now = hgl::Timestamp::Now();
hgl::Timestamp t = hgl::Timestamp::FromUnixSeconds(1700000000ULL);
hgl::Timestamp t2 = hgl::Timestamp::FromUnixMilliseconds(1700000000000ULL);

// 格式转换
uint64 unix_sec = now.ToUnixSeconds();
uint64 unix_ms  = now.ToUnixMilliseconds();
uint64 win_ts   = now.ToWindows();          // Windows FILETIME
uint64 uuid7    = now.ToUUIDv7();           // UUIDv7 时间戳

// 时间运算
hgl::Timestamp tomorrow = now.AddDays(1);
hgl::Timestamp later    = now.AddHours(2).AddMinutes(30);
hgl::Timestamp earlier  = now.SubSeconds(3600);

// 原地修改
now.AddDaysInplace(7);

// 时间差（返回整数）
int64 diff_sec  = now.DiffSeconds(t);
int64 diff_days = now.DiffDays(t);

// 比较
bool newer = (now > t);
bool same  = (now == t2);

// 算术运算符（加减秒数）
hgl::Timestamp next = now + 60;     // 加 60 秒
int64 delta = now - t;              // 秒数差
```

---

### 2.3 时间结构 `DateTime.h`

```cpp
#include <hgl/time/DateTime.h>

// 当天时间
hgl::TimeOfDay tod;         // 默认构造，读取当前时间
int h = tod.GetHour();
int m = tod.GetMinute();
int s = tod.GetSecond();
int day_sec = tod.GetDaySeconds(); // 今天过了多少秒

// 完整日期时间（DateTime 继承自 TimeOfDay）
hgl::DateTime dt;           // 读取当前日期时间
int year  = dt.GetYear();
int month = dt.GetMonth();
int day   = dt.GetDay();
```

---

## 3. 字符编码（Charset / Endian / UTF）

### 3.1 字节序工具 `Endian.h`

```cpp
#include <hgl/Endian.h>

// 字节序交换（任意类型）
uint16 v16 = hgl::endian::EndianSwap(value16);
uint32 v32 = hgl::endian::EndianSwap(value32);
uint64 v64 = hgl::endian::EndianSwap(value64);

// 平台统一转换（将数据转换为大/小端存储）
hgl::endian::ToBigEndian(&data, count);         // 原地转换为大端
hgl::endian::ToLittleEndian(&data, count);      // 原地转换为小端
hgl::endian::ToBigEndian(dst, src, count);      // 带类型转换的目标拷贝

// BOM 检测
const void *file_data = ...;
hgl::endian::ByteOrderMask bom = hgl::endian::CheckBOM(file_data);
if (bom == hgl::endian::ByteOrderMask::UTF8)   { /* UTF-8 with BOM */ }
if (bom == hgl::endian::ByteOrderMask::UTF16LE){ /* UTF-16 LE */ }
```

---

### 3.2 字符集转换 `Charset.h`

```cpp
#include <hgl/Charset.h>

// 内置字符集
hgl::CharSet cs = hgl::UTF8CharSet;
hgl::CharSet cs2 = hgl::UTF16LECharSet;

// 转换到 UTF-16
u16char *u16_buf = nullptr;
int len = hgl::to_utf16(hgl::UTF8CharSet, &u16_buf, (const void *)u8_data, byte_size);
// 用完后 delete[] u16_buf

// 转换到 UTF-8
u8char *u8_buf = nullptr;
int len2 = hgl::to_utf8(hgl::UTF16LECharSet, &u8_buf, utf16_data, char_count);
// 用完后 delete[] u8_buf

// BOM → CharSet
hgl::endian::BOMFileHeader bom_hdr = ...;
hgl::CharSet detected;
hgl::BOM2CharSet(&detected, &bom_hdr);
```

---

### 3.3 UTF-8 ↔ UTF-16 转换 `utf.h`

```cpp
#include <hgl/utf.h>

// U8String → U16String
hgl::U8String u8s = U8_TEXT("你好世界");
hgl::U16String u16s = hgl::to_u16(u8s);

// U16String → U8String
hgl::U8String back = hgl::to_u8(u16s);

// 跨平台路径转换
hgl::OSString os_path = hgl::ToOSString(u8_text);    // UTF-8 → OSString（Win: UTF-16；非Win: 原样）
hgl::U8String u8_path = hgl::ToU8String(os_path);    // OSString → UTF-8

// 原始字符指针转换
u16char *u16_ptr = hgl::u8_to_u16(u8_ptr);           // 返回 new[] 分配的缓冲，需 delete[]
u8char  *u8_ptr2 = hgl::u16_to_u8(u16_ptr);          // 返回 new[] 分配的缓冲，需 delete[]
```

---

## 4. 集合类型扩展（CMCore Collections）

### 4.1 队列 `Queue.h` → 替代 `std::queue`

> 仅支持 trivially copyable 类型；非平凡类型请用 `Queue<T*>`

```cpp
#include <hgl/type/Queue.h>

hgl::Queue<int> q;

// 入队
q.Push(1);
q.Push(2);
q.Push(3);

int buf[] = {4, 5, 6};
q.Push(buf, 3);             // 批量入队

// 出队
int val;
bool ok = q.Pop(val);       // 取出并前进

// 只读（不前进）
bool has = q.Peek(val);

int count = q.GetCount();
bool empty = (q.GetCount() == 0);

q.Clear();                  // 清空（保留内存）
q.Free();                   // 清空并释放内存
```

---

### 4.2 栈 `Stack.h` → 替代 `std::stack`

> 仅支持 trivially copyable 类型；非平凡类型请用 `Stack<T*>`

```cpp
#include <hgl/type/Stack.h>

hgl::Stack<int> s;

// 压栈/弹栈
s.Push(10);
s.Push(20);

int top;
s.Pop(top);                 // 弹出（移除并返回）
int &peek = s.Peek();       // 查看栈顶（不移除）

int count = s.GetCount();
bool empty = s.IsEmpty();
const int *data = s.GetData();
```

---

### 4.3 LRU 缓存 `LRUCache.h`

```cpp
#include <hgl/type/LRUCache.h>

// 继承 LRUCache，重写 Create/Clear
class TexCache : public hgl::LRUCache<OSString, Texture*>
{
protected:
    bool Create(const OSString &key, Texture *&val) override
    {
        val = LoadTexture(key);
        return val != nullptr;
    }
    void Clear(const OSString &key, Texture *&val) override
    {
        delete val;
        val = nullptr;
    }
public:
    TexCache(int capacity) : LRUCache(capacity) {}
};

TexCache cache(64);  // 最多缓存 64 个纹理

Texture *t = nullptr;
cache.Get(OS_TEXT("rock.png"), t);      // 如果不存在则自动 Create
cache.Find(OS_TEXT("rock.png"), t);     // 如果不存在则不创建，返回 false

int used = cache.GetCount();
int free = cache.GetFreeCount();
```

---

### 4.4 平铺有序映射 `FlatOrderedMap.h`

适用于静态/半静态数据、需要序列化的场景（K、V 均须 trivially copyable）：

```cpp
#include <hgl/type/FlatOrderedMap.h>

hgl::FlatOrderedMap<int, float> fmap;

// 增删查
fmap.Add(3, 3.14f);
fmap.Add(1, 1.0f);       // 自动有序排列

float *v = fmap.Find(1);  // 找不到返回 nullptr
fmap.Delete(3);

int count = fmap.GetCount();

// 直接访问底层连续数组（序列化友好）
const int   *keys   = fmap.GetKeys();
const float *values = fmap.GetValues();
```

---

### 4.5 平铺有序集合 `FlatOrderedSet.h`

适用于需要连续内存且元素偶尔修改的场景：

```cpp
#include <hgl/type/FlatOrderedSet.h>

hgl::FlatOrderedSet<int> fset;

fset.Add(5);
fset.Add(2);
fset.Add(8);
// 自动有序: 2, 5, 8

bool has = fset.Find(5);    // 二分查找，O(log n)
fset.Delete(2);

int count = fset.GetCount();
const int *data = fset.GetData();   // 连续数组指针（可直接序列化）
```

---

### 4.6 有序映射 `OrderedMap.h`（基于 absl::btree_map）

适用于频繁插入删除且需有序遍历的场景：

```cpp
#include <hgl/type/OrderedMap.h>

hgl::OrderedMap<AnsiString, int> omap;

omap.Add("b", 2);
omap.Add("a", 1);       // 自动按 key 排序

int *v = omap.Find("a");    // 找不到返回 nullptr
omap.Delete("b");

// 有序遍历
for (auto &pair : omap) { /* pair.key, pair.value */ }

// 前缀查找
auto range = omap.FindRange("prefix");  // lower/upper bound
```

---

### 4.7 有序集合 `OrderedSet.h`（基于 absl::btree_set）

```cpp
#include <hgl/type/OrderedSet.h>

hgl::OrderedSet<int> oset;

oset.Add(3);
oset.Add(1);    // 自动有序: 1, 3

bool has = oset.Find(3);
oset.Delete(1);
int count = oset.GetCount();

for (auto v : oset) { /* 有序遍历 */ }
```

---

### 4.8 字符串视图 `StringView.h`

非拥有型只读字符串视图（不分配内存）：

```cpp
#include <hgl/type/StringView.h>

hgl::AnsiStringView view("hello", 5);  // 不拷贝数据
const char *ptr = view.data();
int len = (int)view.size();

// 从 AnsiString 构造
AnsiString s = "world";
hgl::AnsiStringView sv(s.c_str(), s.GetLength());
```

---

## 5. 内存管理（Memory Management）

### 5.1 内存块 `MemoryBlock.h`

```cpp
#include <hgl/type/MemoryBlock.h>

hgl::MemoryBlock mb;

mb.Reserve(4096);                       // 预留 4096 字节
void *ptr = mb.Get();                   // 获取数据指针
void *ptr2 = mb.Get(128);              // 从偏移 128 开始

mb.Write(0, src_data, 64);             // 向偏移 0 写入 64 字节
mb.Move(128, 0, 64);                   // 块内移动
mb.Clear();                            // 清零内容（保留分配）
mb.Free();                             // 释放内存
uint64 size = mb.GetSize();
```

---

### 5.2 块分配器 `BlockAllocator.h`

适用于固定单元大小的分配场景（GPU 缓冲区管理等）：

```cpp
#include <hgl/type/BlockAllocator.h>

hgl::BlockAllocator alloc(1024);        // 最多管理 1024 个块单元

hgl::BlockAllocator::UserNode *node = alloc.Acquire(8); // 申请 8 个连续块
if (node) {
    int start = node->GetStart();      // 起始块索引
    int count = node->GetCount();      // 占用块数量
    
    alloc.Release(node);               // 释放（自动合并相邻空闲块）
}
```

---

## 6. 智能指针（Smart Pointers）

### 6.1 `Smart.h` — SharedPtr / WeakPtr

```cpp
#include <hgl/type/Smart.h>

// SharedPtr（强引用）
hgl::SharedPtr<MyObject> sp(new MyObject());
sp->DoSomething();

// 拷贝（增加引用计数）
hgl::SharedPtr<MyObject> sp2 = sp;

// WeakPtr（弱引用，不阻止析构）
hgl::WeakPtr<MyObject> wp = sp.GetWeak();

// 从 WeakPtr 提升为 SharedPtr
hgl::SharedPtr<MyObject> locked = wp.Lock();
if (locked) {
    locked->DoSomething();
}

// AutoDelete（离开作用域后自动 delete）
hgl::AutoDelete<MyObject> ad(new MyObject());
ad->DoSomething();

// AutoDeleteArray（delete[]）
hgl::AutoDeleteArray<uint8> buf(new uint8[1024]);
buf[0] = 0xFF;
```

---

## 7. 对象基类（Object）

### 7.1 `Object.h` — 禁止拷贝/移动的基类

```cpp
#include <hgl/object/Object.h>

// 继承 Object：自动禁止拷贝和移动语义（NO_COPY + NO_MOVE）
class MyResource : public hgl::Object
{
public:
    MyResource() = default;
    virtual ~MyResource() override = default;
    
    void Use() { /* ... */ }
};

// 错误：不能拷贝/移动
// MyResource a;
// MyResource b = a;  // 编译错误
```

---

## 8. 文件系统扩展（Filesystem Extensions）

### 8.1 文件枚举 `EnumFile.h`

```cpp
#include <hgl/filesystem/EnumFile.h>

// 继承 EnumFile，重写回调方法
class MyFileEnumerator : public hgl::filesystem::EnumFile
{
    OBJECT_LOGGER
protected:
    void ProcFile(hgl::filesystem::EnumFileConfig *cfg, hgl::filesystem::FileInfo &fi) override
    {
        LogInfo(u8"找到文件: %s", fi.fullname);
    }
    void ProcFolderBegin(hgl::filesystem::EnumFileConfig *parent, hgl::filesystem::EnumFileConfig *cur, hgl::filesystem::FileInfo &fi) override
    {
        LogInfo(u8"进入目录: %s", fi.name);
    }
};

MyFileEnumerator enumerator;
hgl::filesystem::EnumFileConfig cfg(OS_TEXT("/path/to/dir"));
cfg.proc_file   = true;
cfg.proc_folder = true;
cfg.sub_folder  = true;     // 递归子目录
enumerator.Enum(&cfg);
```

---

### 8.2 路径操作 `Filename.h`

```cpp
#include <hgl/filesystem/Filename.h>

// 路径拼接（推荐使用 JoinPath 系列）
OSString full = hgl::filesystem::JoinPathWithFilename(
    OS_TEXT("/base/dir"),
    OS_TEXT("file.txt")
);
// → "/base/dir/file.txt"

// 获取文件名（去掉目录）
OSString name = hgl::filesystem::GetFilename(OS_TEXT("/path/to/file.txt"));
// → "file.txt"

// 获取扩展名
OSString ext = hgl::filesystem::GetExtName(OS_TEXT("file.txt"));
// → "txt"（不含点）

// 获取不含扩展名的文件名（stem）
OSString stem = hgl::filesystem::GetMainFilename(OS_TEXT("file.txt"));
// → "file"

// 获取目录部分
OSString dir = hgl::filesystem::GetFilePath(OS_TEXT("/path/to/file.txt"));
// → "/path/to"

// 替换扩展名
OSString renamed = hgl::filesystem::ChangeExtName(
    OS_TEXT("model.fbx"),
    OS_TEXT("hgl")
);
// → "model.hgl"
```

---

## 9. IO 扩展（IO Extensions）

### 9.1 文本逐行解析 `TextInputStream.h`

```cpp
#include <hgl/io/TextInputStream.h>
#include <hgl/io/FileInputStream.h>

// 实现解析回调
class MyLineParser : public hgl::io::TextInputStream::ParseCallback<char>
{
public:
    bool OnLine(char *text, int len) override
    {
        // 处理一行文本 text（长度 len，不含换行符）
        AnsiString line(text, len);
        GLogInfo(u8"读到行: %s", line.c_str());
        return true;    // 返回 false 则停止解析
    }
};

// 使用
hgl::io::FileInputStream fis;
fis.Open(OS_TEXT("config.txt"));

hgl::io::TextInputStream tis(&fis);

MyLineParser parser;
tis.SetParseCallback<char>(&parser);
int lines = tis.Run();
```

---

### 9.2 文本输出流 `TextOutputStream.h`

```cpp
#include <hgl/io/TextOutputStream.h>
#include <hgl/io/FileOutputStream.h>

// 创建 UTF-8 文本输出流
hgl::io::FileOutputStream *fos = hgl::io::CreateFileOutputStream(OS_TEXT("out.txt"));

hgl::io::UTF8TextOutputStream *tos =
    hgl::io::CreateTextOutputStream<char>(fos);  // fos 的所有权转移给 tos

tos->WriteBOM();                                // 写入 UTF-8 BOM（可选）
tos->WriteLine(U8_TEXT("第一行"));
tos->WriteLine(U8_TEXT("第二行"));

// 写入字符串列表
U8StringList lines;
lines.Add(U8_TEXT("item1"));
lines.Add(U8_TEXT("item2"));
tos->WriteText(lines);                         // 每项写一行

delete tos;  // 析构时自动 delete fos
```

---

### 9.3 加载文本文件快捷函数 `LoadString.h`

```cpp
#include <hgl/io/LoadString.h>

// 加载整个文本文件为 U8String
U8String content;
int chars = hgl::LoadStringFromTextFile(content, OS_TEXT("readme.txt"));
if (chars < 0) { GLogError(u8"加载失败"); }

// 加载为 U16String
U16String wcontent;
hgl::LoadStringFromTextFile(wcontent, OS_TEXT("readme.txt"));
```

---

### 9.4 加载文本文件为行列表 `LoadStringList.h`

```cpp
#include <hgl/io/LoadStringList.h>

U8StringList lines;
int count = hgl::LoadStringListFromTextFile(lines, OS_TEXT("list.txt"));
for (int i = 0; i < count; i++) {
    GLogInfo(u8"行 %d: %s", i, lines[i].c_str());
}
```

---

### 9.5 内存映射文件 `MMapFile.h`

```cpp
#include <hgl/io/MMapFile.h>

// 只读映射（最常用）
hgl::MMapFile::Error err;
hgl::MMapFile *mf = hgl::OpenMMapFileOnlyRead(OS_TEXT("big_file.bin"), &err);
if (!mf) { GLogError(u8"映射失败: %d", (int)err); }

const void *data = mf->data();
size_t size = mf->size();
// 直接访问 data，无需 Read()
delete mf;  // 解除映射

// 创建可写映射（新建文件）
hgl::MMapFile *wmf = hgl::CreateMMapFile(OS_TEXT("new_file.bin"), 1024*1024);
uint8 *wdata = (uint8 *)wmf->data();
wdata[0] = 0xFF;
delete wmf;
```

---

## 10. STL → HGL 线程/时间替换速查表

| 禁止使用 (STL)                        | 必须使用 (HGL)                              | 头文件                              |
|--------------------------------------|---------------------------------------------|-------------------------------------|
| `std::thread`                        | `hgl::Thread`                               | `<hgl/thread/Thread.h>`             |
| `std::mutex`                         | `hgl::ThreadMutex`                          | `<hgl/thread/ThreadMutex.h>`        |
| `std::lock_guard<mutex>`             | `hgl::ThreadMutexLock`                      | `<hgl/thread/ThreadMutex.h>`        |
| `std::unique_lock<mutex>`            | `hgl::ThreadMutexLock`                      | `<hgl/thread/ThreadMutex.h>`        |
| `std::shared_mutex`                  | `hgl::RWLock`                               | `<hgl/thread/RWLock.h>`             |
| `std::shared_lock`                   | `hgl::OnlyReadLock`                         | `<hgl/thread/RWLock.h>`             |
| `std::unique_lock<shared_mutex>`     | `hgl::OnlyWriteLock`                        | `<hgl/thread/RWLock.h>`             |
| `std::atomic<T>`                     | `hgl::atom<T>`                              | `<hgl/thread/Atomic.h>`             |
| `std::atomic<int>`                   | `hgl::atom_int`                             | `<hgl/thread/Atomic.h>`             |
| `std::atomic<bool>`                  | `hgl::atom_bool`                            | `<hgl/thread/Atomic.h>`             |
| `std::counting_semaphore`            | `hgl::Semaphore`                            | `<hgl/thread/Semaphore.h>`          |
| `std::condition_variable`            | `hgl::CondVar`                              | `<hgl/thread/CondVar.h>`            |
| `std::queue<T>`                      | `hgl::Queue<T>`                             | `<hgl/type/Queue.h>`                |
| `std::stack<T>`                      | `hgl::Stack<T>`                             | `<hgl/type/Stack.h>`                |
| `std::chrono::high_resolution_clock` | `hgl::GetUptimeSec()` / `hgl::GetTimeMs()`  | `<hgl/time/Time.h>`                 |
| `std::chrono::system_clock`          | `hgl::GetUnixTimestampSec()`                | `<hgl/time/Time.h>`                 |
| 自定义时间戳类型                       | `hgl::Timestamp`                            | `<hgl/time/Timestamp.h>`            |
| `this_thread::sleep_for`             | `hgl::SleepSecond(seconds)`                 | `<hgl/time/Time.h>`                 |
| `std::map<K,V>`（有序，高频增删）     | `hgl::OrderedMap<K,V>`（B树）               | `<hgl/type/OrderedMap.h>`           |
| `std::set<T>`（有序，高频增删）       | `hgl::OrderedSet<T>`（B树）                 | `<hgl/type/OrderedSet.h>`           |
| `std::map<K,V>`（静态/半静态）        | `hgl::FlatOrderedMap<K,V>`                  | `<hgl/type/FlatOrderedMap.h>`       |
| `std::set<T>`（静态/半静态）          | `hgl::FlatOrderedSet<T>`                    | `<hgl/type/FlatOrderedSet.h>`       |
| `std::string_view`                   | `hgl::AnsiStringView` / `StringView<T>`     | `<hgl/type/StringView.h>`           |
