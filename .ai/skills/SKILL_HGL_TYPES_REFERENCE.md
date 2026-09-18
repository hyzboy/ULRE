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
8. [C标准库字符函数替代（CharType）](#8-c标准库字符函数替代chartype)
9. [C风格字符串操作函数](#9-c风格字符串操作函数)
10. [数字字符串转换](#10-数字字符串转换)
11. [十六进制字符串转换](#11-十六进制字符串转换)
12. [字符串修剪与截取（TrimClip）](#12-字符串修剪与截取trimclip)
13. [内存操作（MemoryUtil）](#13-内存操作memoryutil)
14. [位操作（BitOperations）](#14-位操作bitoperations)
15. [比较与限制工具（CompareUtil）](#15-比较与限制工具compareutil)
16. [常见错误 vs 正确写法](#16-常见错误-vs-正确写法)

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

> 详细完整参考：[SKILL_FILESYSTEM_IO_REFERENCE.md](SKILL_FILESYSTEM_IO_REFERENCE.md)

### 文件读取

```cpp
#include <hgl/io/FileInputStream.h>
#include <hgl/io/DataInputStream.h>

// ── 推荐：RAII 辅助类 OpenFileInputStream ──────────────────────
// 注意：OpenFileInputStream 是 RAII 类，不是全局函数！
hgl::io::OpenFileInputStream opener(OS_TEXT("path/to/file.bin"));
if (!opener) return false;

// 读取原始字节
uint8 buf[256];
int64 bytes_read = opener->Read(buf, sizeof(buf));
int64 file_size  = opener->GetSize();

// 包装为 DataInputStream 读结构化数据
hgl::io::DataInputStream dis(opener);  // opener 隐式转换为 FileInputStream*
int32 val;
dis.ReadInt32(val);
float f;
dis.ReadFloat(f);
// opener 析构时自动 delete FileInputStream
```

### 文件写入

```cpp
#include <hgl/io/FileOutputStream.h>
#include <hgl/io/DataOutputStream.h>

// ── 工厂函数（失败返回 nullptr，需手动 delete） ─────────────────
hgl::io::FileOutputStream *fos =
    hgl::io::CreateFileOutputStream(OS_TEXT("path/to/file.bin")); // 默认 CreateTrunc
if (!fos) return false;

hgl::io::DataOutputStream dos(fos);
dos.WriteInt32(42);
dos.WriteFloat(3.14f);
delete fos;

// ── 其他打开模式 ──────────────────────────────────────────────
hgl::io::FileOutputStream fos2;
fos2.OpenAppend(OS_TEXT("log.txt"));   // 追加
fos2.Create    (OS_TEXT("new.bin"));   // 创建（已存在则失败）
fos2.Close();
```

### 内存流（替代 `std::ostringstream` / `std::istringstream`）

```cpp
#include <hgl/io/MemoryOutputStream.h>
#include <hgl/io/MemoryInputStream.h>

// 写内存流（自动扩容）
hgl::io::MemoryOutputStream mos;
hgl::io::DataOutputStream dos(&mos);
dos.WriteInt32(1);
dos.WriteInt32(2);
const void *buf = mos.GetData();
int64 size = mos.Tell();

// 读内存流
hgl::io::MemoryInputStream mis(buf, size);
hgl::io::DataInputStream dis2(&mis);
int32 a, b;
dis2.ReadInt32(a);  // a == 1
dis2.ReadInt32(b);  // b == 2
```

### 加载文本文件

```cpp
#include <hgl/io/LoadString.h>

// 注意：函数在 hgl:: 命名空间，参数是 (输出字符串, 路径)
hgl::U8String content;
int result = hgl::LoadStringFromTextFile(content, OS_TEXT("file.txt"));
if (result < 0) GLogError(u8"加载失败");
```

### 文件系统操作

```cpp
#include <hgl/filesystem/FileSystem.h>

// 检查文件是否存在
bool exists = hgl::filesystem::FileExist(OS_TEXT("path/to/file"));

// 获取文件信息（大小等）
hgl::filesystem::FileInfo fi;
hgl::filesystem::GetFileInfo(OS_TEXT("path/to/file"), fi);
int64 file_size = fi.size;           // 注意：不是 GetFileSize()，该函数不存在

// 目录操作（注意：不是 MakeDirectory，是 MakePath）
bool ok     = hgl::filesystem::MakePath   (OS_TEXT("path/to/dir"));   // 递归创建目录
bool is_dir = hgl::filesystem::IsDirectory(OS_TEXT("path/to/dir"));   // 判断目录（不是 DirectoryExist）
hgl::filesystem::DeleteTree(OS_TEXT("path/to/dir"));                  // 递归删除目录树
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

> 详细完整参考：[SKILL_LOGGING_REFERENCE.md](SKILL_LOGGING_REFERENCE.md)

❌ 禁止：`printf` / `std::cout` / `std::cerr` / `LOG_INFO` / `LOG_ERROR`（这些宏不存在）  
✅ 必须使用 `<hgl/log/Log.h>` 中的宏，根据场景选择：

```cpp
#include <hgl/log/Log.h>

// ── 模式1：类成员日志（最常用）──────────────────────────────
// 头文件类定义中：
class MyClass {
    OBJECT_LOGGER   // 添加 Log 成员
    ...
};
// 类方法中：
void MyClass::DoWork() {
    LogInfo(u8"开始处理，数量：%d", count);
    LogError(u8"处理失败：%s", name.c_str());
    LogWarning(u8"资源不足");
}

// ── 模式2：全局/自由函数 ────────────────────────────────────
void FreeFunc() {
    GLogInfo(u8"全局信息");
    GLogError(u8"全局错误：%d", code);
}

// ── 模式3：模块多类共享（.cpp 定义 / .h 声明）────────────────
// Render.cpp:
DEFINE_LOGGER_MODULE(Render)
// RenderModule.h:
EXTERN_LOGGER_MODULE(Render)
// 任意 .cpp 中使用（需 include 含 EXTERN 的 .h）：
MLogInfo(Render, u8"渲染开始");
MLogError(Render, u8"渲染失败：%s", msg);

// ── 模式4：单文件绑定（自由函数文件顶部绑定）────────────────
DEFINE_LOGGER_MODULE(Shader)
USE_MODULE_LOGGER(Shader)   // 绑定到本文件
// 同文件内无需写模块名：
FLogInfo(u8"编译着色器");
FLogError(u8"编译失败：%s", path.c_str());
```

**日志级别**（均有对应宏后缀，如 `LogInfo`/`GLogInfo`/`MLogInfo(X,...)`/`FLogInfo`）：  
`Verbose`（仅Debug）→ `Debug`（仅Debug）→ `Info` → `Notice` → `Warning` → `Error` → `Fatal`

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

## 8. C标准库字符函数替代（CharType）

> 替代 `<ctype.h>` / `<cctype>` 中的 `isalpha`、`isdigit`、`isspace` 等。

### 头文件
```cpp
#include <hgl/type/CharType.h>
```

### 字符分类函数（对照 ctype.h）

| C标准库 (`<ctype.h>`) | HGL等价函数 | 说明 |
|----------------------|-------------|------|
| `isalpha(c)` | `hgl::is_alpha(ch)` | 是否为字母（a-z / A-Z） |
| `islower(c)` | `hgl::is_lower_alpha(ch)` | 是否为小写字母（a-z） |
| `isupper(c)` | `hgl::is_upper_alpha(ch)` | 是否为大写字母（A-Z） |
| `isdigit(c)` | `hgl::is_digit(ch)` | 是否为十进制数字（0-9） |
| `isxdigit(c)` | `hgl::is_hex_digit(ch)` | 是否为十六进制字符（0-9, a-f, A-F） |
| `isspace(c)` | `hgl::is_space(ch)` | 是否为空白字符 |
| `isalnum(c)` | `hgl::is_alpha(ch) \|\| hgl::is_digit(ch)` | 是否为字母或数字（无直接等价） |
| 无 | `hgl::is_float_char(ch)` | 是否可出现在浮点数文本中（含 `+-./eEfF`） |
| 无 | `hgl::is_integer_char(ch)` | 是否可出现在整数文本中（含 `+-`） |

所有函数均为模板，支持 `char`、`wchar_t`、`char8_t` 等任意整型字符类型。

```cpp
#include <hgl/type/CharType.h>

char c = 'A';

if (hgl::is_alpha(c))        { /* 字母 */ }
if (hgl::is_upper_alpha(c))  { /* 大写字母 */ }
if (hgl::is_lower_alpha(c))  { /* 小写字母 */ }
if (hgl::is_digit(c))        { /* 数字 */ }
if (hgl::is_hex_digit(c))    { /* 十六进制字符 */ }
if (hgl::is_space(c))        { /* 空白字符 */ }
if (hgl::is_float_char(c))   { /* 浮点数字符 */ }
if (hgl::is_integer_char(c)) { /* 整数字符 */ }
```

---

## 9. C风格字符串操作函数

> 替代 `<string.h>` / `<cstring>` 中的 `strlen`、`strcmp`、`strcpy`、`strcat`、`strstr`、`strchr` 等。
>
> ⚠️ 所有HGL字符串函数均为模板，支持 `char`、`wchar_t`、`u8char` 等多种字符类型，且对 `nullptr` 安全。

### 9.1 字符串长度（Str.Length.h）

```cpp
#include <hgl/type/Str.Length.h>
```

| C标准库 | HGL等价 | 说明 |
|---------|---------|------|
| `strlen(s)` | `hgl::strlen(str)` | 字符串长度（nullptr 安全，返回 0） |
| `strnlen(s, n)` | `hgl::strlen(str, max_len)` | 最大 max_len 个字符的长度 |

```cpp
#include <hgl/type/Str.Length.h>

const char *s = "hello";
int len = hgl::strlen(s);               // 5
int limited = hgl::strlen(s, 3);        // 3

// 支持宽字符
const wchar_t *ws = L"world";
int wlen = hgl::strlen(ws);             // 5
```

### 9.2 字符串比较（Str.Comp.h）

```cpp
#include <hgl/type/Str.Comp.h>
```

| C标准库 | HGL等价 | 说明 |
|---------|---------|------|
| `strcmp(s1, s2)` | `hgl::strcmp(src, dst)` | 字符串比较，返回 <0/0/>0 |
| `strncmp(s1, s2, n)` | `hgl::strcmp(src, dst, count)` | 最多 n 个字符比较 |
| `strcasecmp(s1, s2)` | `hgl::stricmp(src, dst)` | 大小写无关比较 |
| `strncasecmp(s1, s2, n)` | `hgl::stricmp(src, dst, count)` | 大小写无关、最多 n 个字符 |
| 无 | `hgl::strcmp_ordering(src, dst)` | 返回 `std::strong_ordering` |
| 无 | `hgl::stricmp_ordering(src, dst)` | 大小写无关，返回 `std::strong_ordering` |
| 无 | `hgl::strcmp(src, src_size, dst, dst_size)` | 带长度的比较 |
| 无 | `hgl::strcmp_content(src, src_size, dst, dst_size)` | 内容比较（ordering） |
| 无 | `hgl::stricmp_content(src, src_size, dst, dst_size)` | 大小写无关内容比较 |

```cpp
#include <hgl/type/Str.Comp.h>

const char *a = "hello";
const char *b = "HELLO";

int r = hgl::strcmp(a, b);                  // 非0（不等）
int ri = hgl::stricmp(a, b);               // 0（大小写无关相等）

// 带长度比较
int r2 = hgl::strcmp(a, 5, b, 5);

// C++20 ordering 版本
auto ord = hgl::strcmp_ordering(a, b);     // std::strong_ordering::greater
auto ordi = hgl::stricmp_ordering(a, b);   // std::strong_ordering::equal
```

### 9.3 字符串复制（Str.Copy.h）

```cpp
#include <hgl/type/Str.Copy.h>
```

| C标准库 | HGL等价 | 说明 |
|---------|---------|------|
| `strcpy(dst, src)` | `hgl::strcpy(dst, count, src)` | 复制，count 为 dst 总容量（含终止符） |
| `strncpy(dst, src, n)` | `hgl::strcpy(dst, dst_count, src, count)` | 限制源和目标长度的复制 |
| `strcat(dst, src)` | `hgl::strcat(dst, max_count, src, count)` | 追加字符串 |
| 无 | `hgl::strcat(dst, max_count, ch)` | 追加单个字符 |
| `strdup(str)` | `hgl::create_copy(str, size=-1)` | 堆上创建字符串副本，需手动 `delete[]` |

```cpp
#include <hgl/type/Str.Copy.h>

char buf[64];

// 复制（count = 目标缓冲区总容量）
int written = hgl::strcpy(buf, 64, "hello");  // written = 5

// 限制源长度
int written2 = hgl::strcpy(buf, 64, "hello world", 5);  // 仅复制 "hello"

// 追加
hgl::strcat(buf, 64, "_suffix", 7);

// 追加单个字符
hgl::strcat(buf, 64, '!');

// 创建堆上副本（用完需 delete[]）
char *copy = hgl::create_copy("hello");
// ...
delete[] copy;
```

### 9.4 字符串大小写转换（Str.Case.h）

```cpp
#include <hgl/type/Str.Case.h>
```

| C标准库 | HGL等价 | 说明 |
|---------|---------|------|
| 无（需逐字符 `toupper`） | `hgl::to_upper_char(s)` | 就地转大写 |
| 无（需逐字符 `tolower`） | `hgl::to_lower_char(s)` | 就地转小写 |
| 无 | `hgl::to_upper_char(src, dst)` | 转大写并写入 dst |
| 无 | `hgl::to_lower_char(src, dst)` | 转小写并写入 dst |
| 无 | `hgl::upper_cpy(dst, src)` | 复制并转大写，返回写入字符数 |
| 无 | `hgl::lower_cpy(dst, src)` | 复制并转小写，返回写入字符数 |
| 无 | `hgl::upper_clip_cpy(dst, src)` | 复制 + 转大写 + 去除空格 |
| 无 | `hgl::lower_clip_cpy(dst, src)` | 复制 + 转小写 + 去除空格 |

```cpp
#include <hgl/type/Str.Case.h>

char s[] = "Hello World";

// 就地大小写转换
hgl::to_upper_char(s);   // s == "HELLO WORLD"
hgl::to_lower_char(s);   // s == "hello world"

// 复制并转换
char dst[64];
hgl::to_upper_char("hello", dst);    // dst == "HELLO"
hgl::to_lower_char("HELLO", dst);    // dst == "hello"

// 复制 + 转换 + 去除空格
hgl::upper_clip_cpy(dst, "hello world");  // dst == "HELLOWORLD"
hgl::lower_clip_cpy(dst, "HELLO WORLD"); // dst == "helloworld"
```

### 9.5 字符串查找（Str.Search.h）

```cpp
#include <hgl/type/Str.Search.h>
```

| C标准库 | HGL等价 | 说明 |
|---------|---------|------|
| `strstr(s, needle)` | `hgl::strstr(haystack, haystack_len, needle, needle_len)` | 查找子串 |
| 无 | `hgl::strrstr(haystack, haystack_len, needle, needle_len)` | 反向查找子串 |
| 无 | `hgl::stristr(haystack, haystack_len, needle, needle_len)` | 大小写无关查找子串 |
| `strchr(s, c)` | `hgl::strchr(str, ch)` | 查找字符（从前向后） |
| 无 | `hgl::strchr(str, ch, n)` | 查找字符，最多前 n 个字符 |
| 无 | `hgl::strchr(str, set, set_count)` | 查找字符集中任意字符 |
| `strrchr(s, c)` | `hgl::strrchr(str, ch)` | 从后向前查找字符 |
| 无 | `hgl::strrchr(str, length, ch)` | 指定长度从后向前查找 |
| 无 | `hgl::strechr(str, ch)` | 找到字符后，返回其后的位置 |
| 无 | `hgl::replace_extname(new_fn, old_fn, max_len, new_ext)` | 替换文件扩展名 |
| 无 | `hgl::replace(str, old_ch, new_ch)` | 替换所有指定字符 |

> ⚠️ HGL 的 `strstr`/`strrchr` 等函数需要显式传入长度参数，比C标准库更安全。

```cpp
#include <hgl/type/Str.Search.h>

const char *text = "hello world hello";
const int text_len = hgl::strlen(text);

// 查找子串
const char *p = hgl::strstr(text, text_len, "world", 5);
// p 指向 "world hello"

// 反向查找子串
const char *last = hgl::strrstr(text, text_len, "hello", 5);
// last 指向最后的 "hello"

// 大小写无关查找
const char *ci = hgl::stristr(text, text_len, "WORLD", 5);

// 查找字符
const char *pos = hgl::strchr(text, 'o');         // 第一个 'o'
const char *rpos = hgl::strrchr(text, text_len, 'o'); // 最后一个 'o'

// 替换文件扩展名
char new_name[256];
hgl::replace_extname(new_name, "model.obj", 256, "fbx");
// new_name == "model.fbx"

// 替换字符
char buf[] = "a.b.c.d";
hgl::replace(buf, '.', '/');
// buf == "a/b/c/d"
```

---

## 10. 数字字符串转换

> 替代 `<cstdlib>` / `<cstdio>` 中的 `atoi`、`atof`、`strtol`、`sprintf` 等，以及 `itoa`、`utoa`。

### 头文件
```cpp
#include <hgl/type/Str.Number.h>
```

### 字符串→数字（返回 bool 表示成功/失败）

| C标准库 | HGL等价 | 说明 |
|---------|---------|------|
| `atoi(s)` / `strtol(s, ...)` | `hgl::stoi(str, out_result)` | 十进制整数字符串 → 有符号整数 |
| `strtoul(s, ...)` | `hgl::stou(str, out_result)` | 十进制整数字符串 → 无符号整数 |
| `strtoul(s, ..., 16)` | `hgl::xtou(str, out_result)` | 十六进制字符串 → 无符号整数 |
| `strtoul(s, ..., 8)` | `hgl::stoo(str, out_result)` | 八进制字符串 → 无符号整数 |
| `atof(s)` / `strtod(s, ...)` | `hgl::stof(str, out_result)` | 浮点数字符串 → 浮点数 |
| 无 | `hgl::etof(str, out_result)` | 科学计数法字符串 → 浮点数 |
| 无 | `hgl::stob(str, out_value)` | `"true"`/`"false"` → bool |

所有函数均为模板，支持任意字符类型（`char`、`wchar_t` 等）。

```cpp
#include <hgl/type/Str.Number.h>

// 字符串 → 整数
int32 i;
if (hgl::stoi("42", i))      { /* i == 42 */ }
if (hgl::stoi("-123", i))    { /* i == -123 */ }

// 字符串 → 无符号整数
uint32 u;
if (hgl::stou("255", u))     { /* u == 255 */ }

// 十六进制字符串 → 整数
uint32 hex;
if (hgl::xtou("FF", hex))    { /* hex == 255 */ }
if (hgl::xtou("0xFF", hex))  { /* hex == 255 */ }

// 八进制字符串 → 整数
uint32 oct;
if (hgl::stoo("17", oct))    { /* oct == 15 */ }

// 字符串 → 浮点数
float f;
if (hgl::stof("3.14", f))    { /* f == 3.14f */ }
if (hgl::stof("-1.5e2", f))  { /* f == -150.0f */ }

// 科学计数法
double d;
if (hgl::etof("1.23E+4", d)) { /* d == 12300.0 */ }

// 字符串 → bool
bool b;
if (hgl::stob("true", b))    { /* b == true */ }
if (hgl::stob("false", b))   { /* b == false */ }

// 带长度参数的版本
int32 i2;
if (hgl::stoi("42abc", 2, i2))  { /* 只解析前2字符, i2 == 42 */ }
```

### 数字→字符串

| C标准库 | HGL等价 | 说明 |
|---------|---------|------|
| `itoa(n, buf, 10)` / `sprintf(buf, "%d", n)` | `hgl::itos(str, size, num)` | 有符号整数 → 十进制字符串 |
| `utoa(n, buf, 10)` / `sprintf(buf, "%u", n)` | `hgl::utos(str, size, value)` | 无符号整数 → 十进制字符串 |
| `sprintf(buf, "%x", n)` | `hgl::utos(str, size, value, 16)` | 无符号整数 → 十六进制字符串 |

```cpp
#include <hgl/type/Str.Number.h>

char buf[32];

// 整数 → 字符串
hgl::itos(buf, 32, -42);       // buf == "-42"
hgl::utos(buf, 32, 255u);      // buf == "255"
hgl::utos(buf, 32, 255u, 16);  // buf == "ff"（十六进制）
hgl::utos(buf, 32, 255u, 16, true);  // buf == "FF"（大写十六进制）
```

---

## 11. 十六进制字符串转换

```cpp
#include <hgl/type/Str.Hex.h>
```

### 解析十六进制字符串

```cpp
// 十六进制字符串 → 字节数组
uint8 data[4];
hgl::ParseHexStr(data, "DEADBEEF", 4);   // data = {0xDE, 0xAD, 0xBE, 0xEF}

// 十六进制字符串 → 固定类型
uint32 value;
hgl::ParseHexStr(value, "DEADBEEF");     // value = 0xDEADBEEF
```

### 生成十六进制字符串

```cpp
char str[9];

// 数值 → 十六进制字符串
hgl::Hex2String(str, (uint32)0xDEADBEEF);          // str == "DEADBEEF"
hgl::Hex2String(str, (uint32)0xDEADBEEF, false);   // str == "deadbeef"

// 字节数组 → 十六进制字符串
uint8 data[] = {0xDE, 0xAD, 0xBE, 0xEF};
char hexstr[9];
hgl::DataToUpperHexStr(hexstr, data, 4);   // hexstr == "DEADBEEF"
hgl::DataToLowerHexStr(hexstr, data, 4);   // hexstr == "deadbeef"

// 带间隔符
char hexstr2[12];
hgl::DataToUpperHexStr(hexstr2, data, 4, ':'); // hexstr2 == "DE:AD:BE:EF"

// 固定类型 → 十六进制字符串
uint32 v = 0xDEADBEEF;
char out[9];
hgl::ToUpperHexStr(out, v);   // out == "DEADBEEF"
hgl::ToLowerHexStr(out, v);   // out == "deadbeef"
```

---

## 12. 字符串修剪与截取（TrimClip）

```cpp
#include <hgl/type/Str.TrimClip.h>
```

> ⚠️ 这些函数操作的是 **指针+长度** 对，不修改原始字符串，返回新起始指针，长度通过引用参数更新。

| 函数 | 说明 |
|------|------|
| `hgl::trimleft(src, len)` | 去除前端空白字符（返回新起始指针，更新 len） |
| `hgl::trimright(src, len)` | 去除尾端空白字符（更新 len） |
| `hgl::trim(src, len)` | 去除两端空白字符 |
| `hgl::clipleft(src, len)` | 截取前端非空白部分（更新 len 为截取长度） |
| `hgl::clipright(src, len)` | 截取尾端非空白部分 |

可以传入自定义谓词函数替代默认的 `is_space`。

```cpp
#include <hgl/type/Str.TrimClip.h>

const char *s = "   hello world   ";
int len = hgl::strlen(s);   // 17

// 去两端空白
const char *trimmed = hgl::trim(s, len);
// trimmed 指向 "hello world   "，len == 11

// 去前端空白
std::size_t l2 = hgl::strlen(s);
const char *left = hgl::trimleft(s, l2);
// left 指向 "hello world   "，l2 == 14

// 截取前端非空白部分
const char *text = "hello world";
int tlen = hgl::strlen(text);
const char *word = hgl::clipleft(text, tlen);
// word 指向 "hello world"，tlen == 5（"hello"的长度）
```

---

## 13. 内存操作（MemoryUtil）

> 替代 `<cstring>` 中的 `memcpy`、`memmove`、`memset`、`memcmp`。
> 所有函数均为模板，类型安全，trivially_copyable 类型自动优化为 C 标准库底层实现。

```cpp
#include <hgl/type/MemoryUtil.h>
```

### 内存复制

| C标准库 | HGL等价 | 说明 |
|---------|---------|------|
| `memcpy(&dst, &src, sizeof(T))` | `hgl::mem_copy(dst, src)` | 复制单个对象 |
| `memcpy(dst, src, n*sizeof(T))` | `hgl::mem_copy(dst, src, count)` | 复制 count 个元素（不可重叠） |
| `memmove(dst, src, n*sizeof(T))` | `hgl::mem_move(dst, src, count)` | 复制 count 个元素（支持重叠） |
| 无 | `hgl::convert_copy(dst, src, count)` | 跨类型转换复制（`D(*src)` 逐元素转换） |

```cpp
#include <hgl/type/MemoryUtil.h>

int src[] = {1, 2, 3, 4, 5};
int dst[5];

// 复制数组
hgl::mem_copy(dst, src, 5);    // dst == {1,2,3,4,5}

// 支持重叠区域的复制
hgl::mem_move(src + 1, src, 4); // 向右偏移1位

// 跨类型复制（int → float）
float fdst[5];
hgl::convert_copy(fdst, src, 5);  // fdst == {1.0f, 2.0f, ...}

// 复制单个对象
MyStruct a, b;
hgl::mem_copy(a, b);  // a = b（类型安全）
```

### 内存填充

| C标准库 | HGL等价 | 说明 |
|---------|---------|------|
| `memset(data, 0, n)` | `hgl::mem_zero(data, count)` | 数组清零 |
| `memset(&data, 0, sizeof(T))` | `hgl::mem_zero(data)` | 单个对象清零 |
| `memset(data, v, n)`（仅字节） | `hgl::mem_fill(data, value, count)` | 类型安全填充（支持任意类型值） |
| 无 | `hgl::mem_fill_pattern(data, pattern, count)` | 用同一对象填充整个数组 |

```cpp
int arr[10];
hgl::mem_zero(arr, 10);         // 全部清零

hgl::mem_fill(arr, 42, 10);     // 全部设为 42

int pattern = 99;
hgl::mem_fill_pattern(arr, &pattern, 10);  // 全部设为 99

MyStruct s;
hgl::mem_zero(s);               // 对象清零
```

### 内存比较

| C标准库 | HGL等价 | 说明 |
|---------|---------|------|
| `memcmp(&a, &b, sizeof(T))` | `hgl::mem_compare(a, b)` | 比较单个对象，返回 <0/0/>0 |
| `memcmp(a, b, n*sizeof(T))` | `hgl::mem_compare(a, b, count)` | 比较 count 个元素 |
| 无 | `hgl::mem_compare_ordering(a, b)` | 返回 `std::strong_ordering` |
| 无 | `hgl::mem_compare_ordering(a, b, count)` | 数组比较，返回 `std::strong_ordering` |

```cpp
int a = 1, b = 2;
int r = hgl::mem_compare(a, b);    // < 0

auto ord = hgl::mem_compare_ordering(a, b);
if (ord == std::strong_ordering::less) { /* ... */ }
```

---

## 14. 位操作（BitOperations）

```cpp
#include <hgl/type/BitOperations.h>
```

> 基于 C++20 `<bit>` 标准库封装，所有函数均为模板，支持任意整型类型。

### 宏定义

```cpp
HGL_BIT(n)      // 1 << n （32位）
HGL_64BIT(n)    // 1LL << n （64位）
```

### 位检测与设置

| 函数 | 说明 |
|------|------|
| `hgl::is_bit_set(value, offset)` | 检测指定位是否为1 |
| `hgl::is_bit_clear(value, offset)` | 检测指定位是否为0 |
| `hgl::set_bit(value, offset)` | 设置指定位为1 |
| `hgl::clear_bit(value, offset)` | 设置指定位为0 |
| `hgl::set_bit_value(value, offset, b)` | 设置指定位为指定值 |
| `hgl::toggle_bit(value, offset)` | 翻转指定位 |

```cpp
#include <hgl/type/BitOperations.h>

uint32 flags = 0b00001010;

bool b = hgl::is_bit_set(flags, 1);    // true（第1位为1）
bool c = hgl::is_bit_clear(flags, 0);  // true（第0位为0）

hgl::set_bit(flags, 0);     // flags = 0b00001011
hgl::clear_bit(flags, 1);   // flags = 0b00001001
hgl::toggle_bit(flags, 3);  // flags = 0b00000001
hgl::set_bit_value(flags, 7, true);  // 设置第7位为1
```

### 位计数与位位置

| 函数 | 说明 |
|------|------|
| `hgl::popcount(value)` | 统计1的个数（C++20 `std::popcount`） |
| `hgl::popcount(value, bit_count)` | 统计指定位数范围内1的个数 |
| `hgl::bit_offset(value)` | 最低位1的位置（0起）；无1则返回-1 |
| `hgl::bit_width(value)` | 最高位1的位置（使用 `std::bit_width`） |
| `hgl::has_single_bit(value)` | 是否恰好只有1位为1（是否为2的幂） |
| `hgl::bit_floor(value)` | 向下取整到最大不超过 value 的2的幂 |
| `hgl::bit_ceil(value)` | 向上取整到最小不小于 value 的2的幂 |
| `hgl::rotl(value, shift)` | 循环左移 |
| `hgl::rotr(value, shift)` | 循环右移 |

```cpp
uint32 v = 0b00101100;

int ones   = hgl::popcount(v);         // 3（有3个1）
int lowest = hgl::bit_offset(v);       // 2（最低位1在第2位）
int width  = hgl::bit_width(v);        // 6（最高位1在第5位，返回6）
bool pow2  = hgl::has_single_bit(v);   // false

uint32 ceil  = hgl::bit_ceil(v);       // 32（大于0b101100的最小2的幂）
uint32 floor = hgl::bit_floor(v);      // 32（不超过0b101100的最大2的幂）

uint32 rotated = hgl::rotl(v, 2);      // 循环左移2位
uint32 rotatedr = hgl::rotr(v, 2);     // 循环右移2位
```

---

## 15. 比较与限制工具（CompareUtil）

```cpp
#include <hgl/type/CompareUtil.h>
```

| 函数 | C标准库/STL等价 | 说明 |
|------|----------------|------|
| `hgl::clamp(value, min_v, max_v)` | `std::clamp` | 限制值在 [min_v, max_v] 范围内 |
| `hgl::abs(value)` | `std::abs` | 绝对值（无符号类型直接返回） |
| `hgl::min(a, b)` | `std::min` | 返回较小值 |
| `hgl::max(a, b)` | `std::max` | 返回较大值 |
| `hgl::update_min(a, b)` | 无直接等价 | 若 b < a，则将 a 更新为 b |
| `hgl::update_max(a, b)` | 无直接等价 | 若 b > a，则将 a 更新为 b |
| `hgl::min_element_value(data, count, default)` | `*std::min_element` | 数组中的最小值 |
| `hgl::max_element_value(data, count, default)` | `*std::max_element` | 数组中的最大值 |

```cpp
#include <hgl/type/CompareUtil.h>

// 限制范围
float v = hgl::clamp(1.5f, 0.0f, 1.0f);   // v == 1.0f
int clamped = hgl::clamp(-5, 0, 100);      // clamped == 0

// 绝对值
int a = hgl::abs(-42);    // 42
float f = hgl::abs(-3.14f);  // 3.14f

// 最大/最小
int lo = hgl::min(3, 7);   // 3
int hi = hgl::max(3, 7);   // 7

// 就地更新最大/最小（常用于遍历求极值）
int cur_max = 0;
int arr[] = {5, 2, 8, 1, 9, 3};
for (int x : arr) hgl::update_max(cur_max, x);  // cur_max == 9

int cur_min = INT_MAX;
for (int x : arr) hgl::update_min(cur_min, x);  // cur_min == 1

// 数组最大/最小
int min_val = hgl::min_element_value(arr, 6, INT_MAX);  // 1
int max_val = hgl::max_element_value(arr, 6, INT_MIN);  // 9
```

---

## 16. 常见错误 vs 正确写法

### ❌ 错误：使用 `strlen` / `strcmp` / `strcpy`
```cpp
// 错误
#include <cstring>
size_t len = strlen(str);
int r = strcmp(s1, s2);
strcpy(dst, src);
```
### ✅ 正确：使用 HGL 字符串函数
```cpp
// 正确
#include <hgl/type/Str.Length.h>
#include <hgl/type/Str.Comp.h>
#include <hgl/type/Str.Copy.h>
int len = hgl::strlen(str);
int r = hgl::strcmp(s1, s2);
hgl::strcpy(dst, sizeof(dst), src);
```

---

### ❌ 错误：使用 `isdigit` / `isalpha`
```cpp
// 错误
#include <cctype>
if (isdigit(c)) { ... }
if (isalpha(c)) { ... }
```
### ✅ 正确：使用 `hgl::CharType`
```cpp
// 正确
#include <hgl/type/CharType.h>
if (hgl::is_digit(c))  { ... }
if (hgl::is_alpha(c))  { ... }
```

---

### ❌ 错误：使用 `atoi` / `atof`
```cpp
// 错误
#include <cstdlib>
int n = atoi("42");
float f = atof("3.14");
```
### ✅ 正确：使用 `hgl::stoi` / `hgl::stof`（返回 bool 表示成功）
```cpp
// 正确
#include <hgl/type/Str.Number.h>
int32 n; hgl::stoi("42", n);
float f; hgl::stof("3.14", f);
```

---

### ❌ 错误：使用 `memcpy` / `memset` / `memcmp`
```cpp
// 错误
#include <cstring>
memcpy(dst, src, count * sizeof(T));
memset(arr, 0, count * sizeof(T));
memcmp(&a, &b, sizeof(T));
```
### ✅ 正确：使用 HGL 内存函数
```cpp
// 正确
#include <hgl/type/MemoryUtil.h>
hgl::mem_copy(dst, src, count);
hgl::mem_zero(arr, count);
hgl::mem_compare(a, b);
```

---

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
- 内存：`inc/hgl/type/MemoryUtil.h`、`inc/hgl/type/AlignUtil.h`、`inc/hgl/type/MemoryAlloc.h`
- 文件系统：`inc/hgl/filesystem/FileSystem.h`
- **C标准库字符函数替代**：`CMCoreType/inc/hgl/type/CharType.h`
- **字符串长度**：`CMCoreType/inc/hgl/type/Str.Length.h`
- **字符串比较**：`CMCoreType/inc/hgl/type/Str.Comp.h`
- **字符串复制**：`CMCoreType/inc/hgl/type/Str.Copy.h`
- **字符串大小写**：`CMCoreType/inc/hgl/type/Str.Case.h`
- **字符串查找**：`CMCoreType/inc/hgl/type/Str.Search.h`
- **字符串数字转换**：`CMCoreType/inc/hgl/type/Str.Number.h`
- **十六进制字符串**：`CMCoreType/inc/hgl/type/Str.Hex.h`
- **字符串修剪截取**：`CMCoreType/inc/hgl/type/Str.TrimClip.h`
- **字符串之间查找**：`CMCoreType/inc/hgl/type/Str.Between.h`
- **字符串数字数组**：`CMCoreType/inc/hgl/type/Str.NumberArray.h`
- **字符串字符串数组**：`CMCoreType/inc/hgl/type/Str.StringArray.h`
- **内存操作**：`CMCoreType/inc/hgl/type/MemoryUtil.h`
- **位操作**：`CMCoreType/inc/hgl/type/BitOperations.h`
- **比较与限制工具**：`CMCoreType/inc/hgl/type/CompareUtil.h`
- **对象工具**：`CMCoreType/inc/hgl/type/ObjectUtil.h`
- **类型限制**：`CMCoreType/inc/hgl/type/TypeLimits.h`
- **枚举工具**：`CMCoreType/inc/hgl/type/EnumUtil.h`
- **字节缓冲**：`CMCoreType/inc/hgl/type/StdByteBuffer.h`
