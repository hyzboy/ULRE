# SKILL: HGL 类型与库完整参考手册

> **何时使用本文档**：当你需要字符串、集合、IO、哈希、智能指针等基础功能时，查阅本文档找到正确的HGL API，**不要使用STL**。

---

## 目录

1. [字符串（String）](#1-字符串string)
2. [集合类型（Collections）](#2-集合类型collections)
3. [哈希（Hash）](#3-哈希hash)
4. [IO 输入输出](#4-io-输入输出)
5. [智能指针与内存](#5-智能指针与内存)
6. [日志](#6-日志)
7. [时间](#7-时间)
8. [常见错误 vs 正确写法](#8-常见错误-vs-正确写法)

---

## 1. 字符串（String）

### 头文件
```cpp
#include <hgl/type/String.h>
#include <hgl/type/StrChar.h>   // 字符工具函数
#include <hgl/type/StringList.h> // 字符串列表
```

### 类型说明

| 类型 | 字符类型 | 用途 |
|------|----------|------|
| `AnsiString` | `char` | 英文标识符、资源名、调试信息 |
| `UTF8String` / `U8String` | `u8char` | UTF-8编码文本、中文内容 |
| `WideString` | `wchar_t` | Windows宽字符（平台相关） |
| `OSString` | 平台相关 | 跨平台文件路径（Windows=wchar_t，其他=char） |

### 字符串宏

```cpp
// UTF-8字符串字面量
U8_TEXT("你好世界")     // 等价于 (u8char *)"你好世界"

// OS路径字符串字面量（自动适配平台）
OS_TEXT("/path/to/file")  // Windows上 L"/path/to/file"，其他平台 "/path/to/file"
```

### 构造与赋值

```cpp
// 从 C 字符串构造
AnsiString s1 = "hello";
AnsiString s2("world");
AnsiString s3(c_str_ptr, length);   // 从指针+长度构造

// UTF-8
UTF8String u8s = u8"你好";
UTF8String u8s2 = U8_TEXT("资源名称");

// 数字转字符串
AnsiString num_str = AnsiString::numberOf(42);
AnsiString num_str2 = AnsiString::numberOf(3.14f);
AnsiString hex_str = AnsiString::hexOf(0xDEADBEEF);
```

### 字符串操作

```cpp
AnsiString a = "hello";
AnsiString b = "world";

// 拼接
AnsiString c = a + "_" + b;               // "hello_world"
AnsiString d = a + AnsiString::numberOf(42); // "hello42"

// 长度和数据
int len = a.GetLength();    // 字节长度（不含终止符）
const char *ptr = a.c_str(); // C风格指针
const char *data = a.data(); // 同 c_str()
bool empty = a.IsEmpty();

// 比较
bool eq = (a == b);
bool eq2 = a.EqualIgnoreCase(b);  // 大小写不敏感

// 查找
int pos = a.FindChar('l');         // 找字符，找不到返回 -1
int pos2 = a.Find("ell");          // 找子串
bool has = a.Contains("ell");

// 截取
AnsiString sub = a.SubString(1, 3); // 从索引1取3个字符
AnsiString left = a.Left(3);        // 前3个字符
AnsiString right = a.Right(3);      // 后3个字符

// 大小写
AnsiString upper = a.ToUpper();
AnsiString lower = a.ToLower();

// Trim
AnsiString trimmed = a.Trim();      // 去两端空白
```

### 字符串列表

```cpp
#include <hgl/type/StringList.h>

AnsiStringList list;
list.Add("item1");
list.Add("item2");
list.Add("item3");

int count = list.GetCount();
const AnsiString &item = list[0];

// 合并
AnsiString joined = list.Join(",");  // "item1,item2,item3"

// 分割
AnsiStringList parts;
SplitString(parts, "a,b,c", ',');
```

---

## 2. 集合类型（Collections）

### 动态数组 `ArrayList<T>` → 替代 `std::vector<T>`

```cpp
#include <hgl/type/ArrayList.h>

hgl::ArrayList<int> arr;

// 添加
arr.Add(1);
arr.Add(2);
arr.Add(3);

// 访问
int count = arr.GetCount();
int *data = arr.GetData();      // 原始指针
int val = arr[0];               // 下标访问

// 查找
int idx = arr.Find(2);          // 找到返回索引，否则 -1

// 删除
arr.Delete(0);                  // 按索引删除
arr.DeleteMove(0);              // 删除并移动后续元素
arr.Clear();                    // 清空

// 预分配
arr.SetCount(100);              // 设置大小（不初始化）
arr.SetAllocCount(100);         // 预分配容量

// 遍历
for (int i = 0; i < arr.GetCount(); i++) {
    int v = arr[i];
}
```

### 链表 `List<T>` → 替代 `std::list<T>`

```cpp
#include <hgl/type/List.h>

// 通常优先使用 ArrayList；只在需要频繁中间插入删除时用 List
```

### 哈希映射 `UnorderedMap<K,V>` → 替代 `std::unordered_map<K,V>`

```cpp
#include <hgl/type/UnorderedMap.h>

hgl::UnorderedMap<AnsiString, int> map;

// 插入
map.Add("key1", 100);
map.Add("key2", 200);

// 查找（返回值指针，找不到返回 nullptr）
int *val = map.Find("key1");
if (val) {
    // 使用 *val
}

// 检查是否存在
bool exists = map.KeyExist("key1");

// 删除
map.Delete("key1");

// 获取数量
int count = map.GetCount();

// 遍历（使用迭代器）
hgl::UnorderedMap<AnsiString,int>::iterator it = map.GetBegin();
while (!map.IsEnd(it)) {
    const AnsiString &k = it->key;
    int v = it->value;
    ++it;
}
```

### 有序集合 `SortedSet<T>` → 替代 `std::set<T>`

```cpp
#include <hgl/type/SortedSet.h>

hgl::SortedSet<AnsiString> set;
set.Add("b");
set.Add("a");
set.Add("c");
// 自动排序：a, b, c

bool has = set.Find("a");
set.Delete("b");
int count = set.GetCount();
```

### 有序映射（替代 `std::map<K,V>`）

```cpp
#include <hgl/type/Map.h>

// Map 是有序的（按key排序）
hgl::Map<AnsiString, int> ordered_map;
ordered_map.Add("b", 2);
ordered_map.Add("a", 1);
// 按字母顺序排列
```

### 双向映射

```cpp
#include <hgl/type/BidirectionalMap.h>

hgl::BidirectionalMap<AnsiString, int> bimap;
bimap.Add("one", 1);
int *val = bimap.FindByLeft("one");   // 通过key找value
AnsiString *key = bimap.FindByRight(1); // 通过value找key
```

---

## 3. 哈希（Hash）

### ⚠️ 禁止手写FNV1a！必须使用以下API

```cpp
#include <hgl/util/hash/FNV1a.h>
```

### FNV1a 标准用法

```cpp
// --- 32位哈希 ---
uint32 hash32 = hgl::hash::FNV1aInit<uint32>();
hash32 = hgl::hash::FNV1aAppendBytes(hash32, data, byte_len);
hash32 = hgl::hash::FNV1aAppendValueBytes(hash32, some_value);

// --- 64位哈希（推荐） ---
uint64 hash64 = hgl::hash::FNV1aInit<uint64>();
hash64 = hgl::hash::FNV1aAppendBytes(hash64, data, byte_len);
hash64 = hgl::hash::FNV1aAppendValueBytes(hash64, some_value);

// --- 追加字符串 ---
AnsiString name = "some_name";
hash64 = hgl::hash::FNV1aAppendBytes(hash64, name.data(), name.GetLength());

// --- 一次性哈希整个缓冲区 ---
uint64 h = hgl::hash::FNV1a<uint64>(data_ptr, byte_count);
```

### 典型场景：对结构体哈希

```cpp
uint64 HashMyStruct(const MyStruct &s)
{
    uint64 hash = hgl::hash::FNV1aInit<uint64>();
    hash = hgl::hash::FNV1aAppendValueBytes(hash, s.field_a);
    hash = hgl::hash::FNV1aAppendValueBytes(hash, s.field_b);
    hash = hgl::hash::FNV1aAppendBytes(hash, s.name.data(), s.name.GetLength());
    return hash;
}
```

### 通用哈希接口

```cpp
#include <hgl/util/hash/Hash.h>

// 获取数据哈希值
uint64 h = hgl::hash::Hash64(data_ptr, byte_count);
```

---

## 4. IO 输入输出

### 文件读取

```cpp
#include <hgl/io/FileInputStream.h>
#include <hgl/io/DataInputStream.h>

// 打开文件（失败返回 nullptr）
hgl::io::FileInputStream *fis = hgl::io::OpenFileInputStream(OS_TEXT("path/to/file"));
if (!fis) return false;

// 读取原始字节
uint8 buf[256];
int64 bytes_read = fis->Read(buf, sizeof(buf));

// 包装为 DataInputStream 读结构化数据
hgl::io::DataInputStream dis(fis);
int32 val;
dis.ReadInt32(val);

float f;
dis.ReadFloat(f);

delete fis;
```

### 文件写入

```cpp
#include <hgl/io/FileOutputStream.h>
#include <hgl/io/DataOutputStream.h>

hgl::io::FileOutputStream *fos = hgl::io::CreateFileOutputStream(OS_TEXT("path/to/file"));
if (!fos) return false;

hgl::io::DataOutputStream dos(fos);
dos.WriteInt32(42);
dos.WriteFloat(3.14f);

delete fos;
```

### 内存流（替代 `std::ostringstream` / `std::istringstream`）

```cpp
#include <hgl/io/MemoryOutputStream.h>
#include <hgl/io/MemoryInputStream.h>

// 写内存流
hgl::io::MemoryOutputStream mos;
hgl::io::DataOutputStream dos(&mos);
dos.WriteInt32(1);
dos.WriteInt32(2);

const void *buf = mos.GetData();
int64 size = mos.Tell();

// 读内存流
hgl::io::MemoryInputStream mis(buf, size);
hgl::io::DataInputStream dis(&mis);
int32 a, b;
dis.ReadInt32(a);  // a == 1
dis.ReadInt32(b);  // b == 2
```

### 加载字符串文件

```cpp
#include <hgl/io/LoadString.h>

// 加载文本文件为字符串
AnsiString content;
if (hgl::io::LoadStringFromFile(OS_TEXT("file.txt"), content)) {
    // 使用 content
}
```

### 文件系统操作

```cpp
#include <hgl/filesystem/FileSystem.h>

// 检查文件是否存在
bool exists = hgl::filesystem::FileExist(OS_TEXT("path/to/file"));

// 获取文件大小
int64 size = hgl::filesystem::GetFileSize(OS_TEXT("path/to/file"));

// 目录操作
hgl::filesystem::MakeDirectory(OS_TEXT("path/to/dir"));
bool is_dir = hgl::filesystem::DirectoryExist(OS_TEXT("path/to/dir"));
```

---

## 5. 智能指针与内存

```cpp
#include <hgl/type/Smart.h>

// 引用计数智能指针（类似 shared_ptr）
// 查阅 Smart.h 获取具体API，此处以实际代码为准

// 内存工具
#include <hgl/type/MemoryUtil.h>

// 对齐工具
#include <hgl/type/AlignUtil.h>
```

### 对象管理器

```cpp
#include <hgl/type/ObjectManager.h>

// 当需要管理一批具名对象时使用
hgl::ObjectManager<AnsiString, MyObject> manager;
manager.Add("name", new MyObject());
MyObject *obj = manager.Find("name");
manager.Delete("name");
```

---

## 6. 日志

```cpp
#include <hgl/log/Log.h>

// 基本日志
LOG_INFO("启动完成");
LOG_WARNING("资源未找到");
LOG_ERROR("严重错误");

// 带参数（使用字符串拼接）
LOG_INFO("加载材质: " + material_name);
LOG_ERROR("错误码: " + AnsiString::numberOf(error_code));

// 不要使用 std::cout 或 printf（调试除外）
```

---

## 7. 时间

```cpp
#include <hgl/time/Time.h>

// 获取当前时间戳（毫秒）
int64 ms = hgl::GetMilliSecond();

// 获取当前时间戳（微秒）
int64 us = hgl::GetMicroSecond();
```

---

## 8. 常见错误 vs 正确写法

### ❌ 错误：使用 `std::string`
```cpp
// 错误
#include <string>
std::string name = "material_a";
std::string key = name + "_" + std::to_string(42);
```
### ✅ 正确：使用 `AnsiString`
```cpp
// 正确
#include <hgl/type/String.h>
AnsiString name = "material_a";
AnsiString key = name + "_" + AnsiString::numberOf(42);
```

---

### ❌ 错误：使用 `std::vector`
```cpp
// 错误
#include <vector>
std::vector<int> ids;
ids.push_back(1);
size_t count = ids.size();
```
### ✅ 正确：使用 `ArrayList`
```cpp
// 正确
#include <hgl/type/ArrayList.h>
hgl::ArrayList<int> ids;
ids.Add(1);
int count = ids.GetCount();
```

---

### ❌ 错误：手写FNV1a
```cpp
// 错误 - 禁止手写任何哈希实现
uint32_t fnv1a_hash(const void *data, size_t len) {
    uint32_t hash = 2166136261u;
    const uint8_t *ptr = (const uint8_t *)data;
    for (size_t i = 0; i < len; ++i) {
        hash ^= ptr[i];
        hash *= 16777619u;
    }
    return hash;
}
```
### ✅ 正确：使用 `hgl::hash::FNV1a`
```cpp
// 正确
#include <hgl/util/hash/FNV1a.h>
uint64 hash = hgl::hash::FNV1aInit<uint64>();
hash = hgl::hash::FNV1aAppendBytes(hash, data, len);
```

---

### ❌ 错误：使用 `std::unordered_map`
```cpp
// 错误
#include <unordered_map>
std::unordered_map<std::string, int> cache;
cache["key"] = 1;
auto it = cache.find("key");
if (it != cache.end()) { int v = it->second; }
```
### ✅ 正确：使用 `UnorderedMap`
```cpp
// 正确
#include <hgl/type/UnorderedMap.h>
hgl::UnorderedMap<AnsiString, int> cache;
cache.Add("key", 1);
int *v = cache.Find("key");
if (v) { /* 使用 *v */ }
```

---

### ❌ 错误：使用 `std::ifstream`
```cpp
// 错误
#include <fstream>
std::ifstream f("file.txt");
std::string line;
std::getline(f, line);
```
### ✅ 正确：使用 HGL IO
```cpp
// 正确
#include <hgl/io/LoadString.h>
AnsiString content;
hgl::io::LoadStringFromFile(OS_TEXT("file.txt"), content);
```

---

### ❌ 错误：使用 `std::thread` / `std::mutex`
```cpp
// 错误 - 不要直接使用STL线程
#include <thread>
#include <mutex>
std::thread t([]{ /* ... */ });
std::mutex m;
```
### ✅ 正确：使用HGL线程库
```cpp
// 查阅 inc/hgl/ 下的线程相关头文件
// 或在代码库中搜索现有的线程使用模式
```

---

## 参考文件位置

- 字符串类型：`inc/hgl/type/String.h`、`inc/hgl/type/StrChar.h`
- 集合类型：`inc/hgl/type/ArrayList.h`、`inc/hgl/type/UnorderedMap.h`、`inc/hgl/type/SortedSet.h`、`inc/hgl/type/Map.h`
- 哈希：`inc/hgl/util/hash/FNV1a.h`、`inc/hgl/util/hash/Hash.h`
- IO：`inc/hgl/io/FileInputStream.h`、`inc/hgl/io/FileOutputStream.h`、`inc/hgl/io/MemoryInputStream.h`、`inc/hgl/io/MemoryOutputStream.h`
- 日志：`inc/hgl/log/Log.h`
- 时间：`inc/hgl/time/Time.h`
- 智能指针：`inc/hgl/type/Smart.h`
- 内存：`inc/hgl/type/MemoryUtil.h`、`inc/hgl/type/AlignUtil.h`
- 文件系统：`inc/hgl/filesystem/FileSystem.h`
