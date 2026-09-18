# HGL 文件系统与IO流完整参考

> CMCore 子库，头文件位于：  
> - 文件系统：`inc/hgl/filesystem/`  
> - IO流：`inc/hgl/io/`

---

## 目录

1. [文件系统（FileSystem.h）](#1-文件系统filesystemh)
2. [路径/文件名操作（Filename.h / Path）](#2-路径文件名操作filenameh--path)
3. [文件枚举（EnumFile / CollectFiles）](#3-文件枚举enumfile--collectfiles)
4. [基础IO流接口（InputStream / OutputStream）](#4-基础io流接口inputstream--outputstream)
5. [文件流（FileInputStream / FileOutputStream）](#5-文件流fileinputstream--fileoutputstream)
6. [内存流（MemoryInputStream / MemoryOutputStream）](#6-内存流memoryinputstream--memoryoutputstream)
7. [格式化IO（DataInputStream / DataOutputStream）](#7-格式化iodatainputstream--dataoutputstream)
8. [文本IO（TextInputStream / LoadString）](#8-文本iotextinputstream--loadstring)
9. [随机访问文件（RandomAccessFile）](#9-随机访问文件randomaccessfile)
10. [内存映射文件（MMapFile）](#10-内存映射文件mmapfile)
11. [STL → HGL 对照表](#11-stl--hgl-对照表)
12. [❌ vs ✅ 常见错误对比](#12--vs--常见错误对比)

---

## 1. 文件系统（FileSystem.h）

```cpp
#include <hgl/filesystem/FileSystem.h>
```

### 文件操作

```cpp
using namespace hgl::filesystem;

// 文件存在性检测
bool exists = FileExist(OS_TEXT("path/to/file.txt"));

// 复制 / 移动 / 改名 / 删除
bool ok = FileCopy  (OS_TEXT("src.txt"), OS_TEXT("dst.txt"));
bool ok = FileMove  (OS_TEXT("src.txt"), OS_TEXT("dst.txt"));
bool ok = FileRename(OS_TEXT("old.txt"), OS_TEXT("new.txt"));
bool ok = FileDelete(OS_TEXT("file.txt"));

// 文件比较（按内容）
bool same = FileComp(OS_TEXT("a.bin"), OS_TEXT("b.bin"));

// 权限检测
bool can_r = FileCanRead (OS_TEXT("file.txt"));
bool can_w = FileCanWrite(OS_TEXT("file.txt"));
bool can_x = FileCanExec (OS_TEXT("file.txt"));

// 最后修改时间（uint64，平台相关）
uint64 mtime = FileLastWriteTime(OS_TEXT("file.txt"));
```

### 整体文件内存加载/保存

```cpp
// 加载整个文件到内存（返回 malloc 分配的指针，需手动 free）
int64 size = 0;
void *data = LoadFileToMemory(OS_TEXT("file.bin"), size);
// 也可以追加零结尾（append_zero=true，适合文本）
void *text = LoadFileToMemory(OS_TEXT("file.txt"), size, true);
hgl_free(data);

// 保存内存块到文件
int64 written = SaveMemoryToFile(OS_TEXT("out.bin"), data, size);

// 保存多块内存到一个文件
void  *parts[2]   = { header_ptr, body_ptr };
int64  sizes[2]   = { header_size, body_size };
int64 written = SaveMemoryToFile(OS_TEXT("out.bin"), parts, sizes, 2);

// 加载文件局部（offset + size）
void *partial = LoadFileToMemory(OS_TEXT("file.bin"), offset, buf, partial_size);

// 保存到文件指定偏移位置
bool ok = SaveMemoryToFile(OS_TEXT("file.bin"), offset, data, size);
```

### 目录操作

```cpp
// 判断是否是目录
bool is_dir = IsDirectory(OS_TEXT("path/to/dir"));

// 创建目录（递归，类似 mkdir -p）
bool ok = MakePath(OS_TEXT("path/to/new/dir"));

// 删除空目录
bool ok = DeletePath(OS_TEXT("path/to/dir"));

// 删除整个目录树（包含所有文件）
DeleteTree(OS_TEXT("path/to/dir"));
```

### 路径查询

```cpp
OSString current, program, program_path, appdata;

GetCurrentPath      (current);      // 当前工作目录
GetCurrentProgram   (program);      // 当前程序完整路径
GetCurrentProgramPath(program_path);// 当前程序所在目录
GetLocalAppdataPath (appdata);      // 用户 AppData 路径
```

### 文件信息结构

```cpp
hgl::filesystem::FileInfo fi;
if (GetFileInfo(OS_TEXT("file.txt"), fi))
{
    // fi.name        — 仅文件名（不含路径）
    // fi.fullname    — 完整路径
    // fi.size        — 字节数
    // fi.is_file     — 是文件
    // fi.is_directory— 是目录
    // fi.is_hidden   — 是隐藏文件
    // fi.can_read    — 可读
    // fi.can_write   — 可写
    // fi.mtime       — 最后修改时间
}
```

### 在多目录中查找文件

```cpp
OSStringList search_paths;
search_paths.Add(OS_TEXT("/usr/share/data"));
search_paths.Add(OS_TEXT("./assets"));

// 回调：每次找到文件（或确认不存在）时调用
auto callback = [](const OSString &filename, void *ud, bool exist) -> bool {
    if (exist) { /* 处理 filename */ }
    return true; // 继续查找
};

uint found = FindFileOnPaths(OS_TEXT("texture.png"), search_paths, nullptr, callback);
```

---

## 2. 路径/文件名操作（Filename.h / Path）

```cpp
#include <hgl/filesystem/Filename.h>  // 自由函数
#include <hgl/filesystem/Path.h>      // Path 类（RAII）
```

### Filename.h 自由函数

```cpp
using namespace hgl::filesystem;

OSString path = OS_TEXT("/data/assets/texture.png");

// 获取文件名部分（含扩展名）："texture.png"
OSString filename  = GetFilename(path);

// 获取扩展名（默认含点）：".png"
OSString ext       = GetExtension(path);
OSString ext_nodot = GetExtension(path, false);  // "png"

// 获取不含扩展名的文件名（Stem）："texture"
OSString stem      = GetStem(path);

// 获取父级路径："/data/assets"
OSString parent    = GetParentPath(path);

// 获取路径最后一个组件："texture.png"
OSString last      = GetLastPathComponent(path);

// 规范化路径（清理 ./ ../ 双斜杠等）
OSString normalized = NormalizeFilename(path);

// 替换扩展名
OSString replaced  = ReplaceExtension(path, OS_TEXT(".dds"));

// 删除扩展名
OSString no_ext    = RemoveExtension(path);

// 拼接目录和文件名
OSString full      = JoinPathWithFilename(OS_TEXT("/data/assets"), OS_TEXT("texture.png"));

// 拆分路径（目录 + 文件名）
OSString dir, file;
SplitPath(path, dir, file);
```

### Path 类（RAII 路径操作）

```cpp
#include <hgl/filesystem/Path.h>

hgl::filesystem::Path p(OS_TEXT("/data/assets"));

// 追加子路径（/ 运算符）
hgl::filesystem::Path full = p / OS_TEXT("textures") / OS_TEXT("sky.dds");

// 获取各部分
OSString filename  = full.GetFilename();   // "sky.dds"
OSString ext       = full.GetExtension();  // ".dds"
OSString stem      = full.GetStem();       // "sky"
hgl::filesystem::Path parent = full.GetParent();

// 规范化
full.Normalize();

// 替换扩展名
full.ReplaceExtension(OS_TEXT(".png"));

// 文件系统查询
bool exists = full.Exists();
bool is_dir = full.IsDirectory();
bool ok     = full.CreateDirectory();

// 转换
const OSString &str = full.ToOSString();
const os_char  *c   = full.c_str();
```

---

## 3. 文件枚举（EnumFile / CollectFiles）

### CollectFiles（模式匹配收集）

```cpp
#include <hgl/filesystem/CollectFiles.h>

hgl::ValueArray<hgl::filesystem::FileInfo> results;

// 收集所有 .png 文件（通配符，不递归）
int count = hgl::filesystem::CollectFiles(
    results,
    OS_TEXT("/data/textures"),
    OS_TEXT("*.png")
);

// 递归搜索
int count = hgl::filesystem::CollectFiles(
    results,
    OS_TEXT("/data"),
    OS_TEXT("*.png"),
    false,   // use_regex = false（通配符）
    true     // sub_folder = true（递归）
);

// 正则表达式模式
int count = hgl::filesystem::CollectFiles(
    results,
    OS_TEXT("/data"),
    OS_TEXT(".*\\.png$"),
    true,    // use_regex = true
    false
);

for (int i = 0; i < results.GetCount(); i++)
{
    const auto &fi = results[i];
    GLogInfo(u8"找到文件：%s 大小：%llu", fi.fullname, fi.size);
}
```

### EnumFile（可继承扩展枚举）

```cpp
#include <hgl/filesystem/EnumFile.h>

class MyEnumFile : public hgl::filesystem::EnumFile
{
    OBJECT_LOGGER

protected:
    void ProcFile(hgl::filesystem::EnumFileConfig *efc,
                  hgl::filesystem::FileInfo &fi) override
    {
        LogInfo(u8"文件：%s", fi.fullname);
    }

    void ProcFolderBegin(hgl::filesystem::EnumFileConfig *parent,
                         hgl::filesystem::EnumFileConfig *cur,
                         hgl::filesystem::FileInfo &fi) override
    {
        LogInfo(u8"进入目录：%s", fi.fullname);
    }
};

// 使用：
MyEnumFile ef;
hgl::filesystem::EnumFileConfig cfg(OS_TEXT("/data/assets"));
cfg.proc_file   = true;
cfg.proc_folder = true;
cfg.sub_folder  = true;
ef.Enum(&cfg);
```

---

## 4. 基础IO流接口（InputStream / OutputStream）

```cpp
#include <hgl/io/InputStream.h>
#include <hgl/io/OutputStream.h>
```

所有流类的公共接口：

| 方法 | 说明 |
|------|------|
| `Read(void*, int64)` | 读取数据，返回实际读取字节数 |
| `Write(const void*, int64)` | 写入数据，返回实际写入字节数 |
| `Peek(void*, int64)` | 预读数据（不移动指针） |
| `Seek(int64, SeekOrigin)` | 移动访问指针（Begin/Current/End） |
| `Tell()` | 当前位置 |
| `GetSize()` | 流总大小 |
| `Available()` | 剩余可读/写字节数 |
| `Restart()` | 复位到开头 |
| `Skip(int64)` | 跳过指定字节 |
| `Close()` | 关闭流 |
| `CanSeek()` | 是否支持随机访问 |
| `CanSize()` | 是否可查询大小 |

---

## 5. 文件流（FileInputStream / FileOutputStream）

```cpp
#include <hgl/io/FileInputStream.h>
#include <hgl/io/FileOutputStream.h>
```

### 文件读取

```cpp
// 方式1：RAII 辅助类（推荐）
hgl::io::OpenFileInputStream opener(OS_TEXT("path/to/file.bin"));
if (!opener) {
    GLogError(u8"打开文件失败");
    return false;
}
hgl::io::FileInputStream *fis = opener;  // 隐式转换

uint8 buf[256];
int64 bytes = fis->Read(buf, sizeof(buf));
int64 size  = fis->GetSize();
// opener 析构时自动关闭并释放

// 方式2：手动管理
hgl::io::FileInputStream fis2;
if (!fis2.Open(OS_TEXT("file.bin"))) {
    GLogError(u8"打开失败");
    return false;
}
fis2.Close();
```

### 文件写入

```cpp
// 方式1：工厂函数（CreateTrunc，失败返回nullptr）
hgl::io::FileOutputStream *fos =
    hgl::io::CreateFileOutputStream(OS_TEXT("out.bin")); // 默认 CreateTrunc
if (!fos) return false;
fos->Write(data, size);
delete fos;

// 方式2：RAII 辅助类
hgl::io::OpenFileOutputStream writer(OS_TEXT("out.bin"), hgl::io::FileOpenMode::CreateTrunc);
if (!writer) return false;
writer->Write(data, size);
// 析构时自动关闭

// 方式3：手动管理（可选打开模式）
hgl::io::FileOutputStream fos2;
fos2.Create    (OS_TEXT("new.bin"));      // 创建（已存在则失败）
fos2.CreateTrunc(OS_TEXT("overwrite.bin"));// 创建（已存在则覆盖）
fos2.Open      (OS_TEXT("exist.bin"));    // 只写打开
fos2.OpenAppend(OS_TEXT("log.txt"));      // 追加模式
fos2.Close();
```

---

## 6. 内存流（MemoryInputStream / MemoryOutputStream）

```cpp
#include <hgl/io/MemoryInputStream.h>
#include <hgl/io/MemoryOutputStream.h>
```

### MemoryOutputStream（写入内存，替代 `std::ostringstream`）

```cpp
hgl::io::MemoryOutputStream mos;

// 直接写入原始字节
mos.Write(data, size);

// 获取已写入数据
const void *buf  = mos.GetData();
int64 written    = mos.Tell();

// 清空（不释放内存，复位写指针）
mos.Clear();

// 关联已有缓冲区（外部管理内存）
uint8 my_buf[1024];
mos.Link(my_buf, sizeof(my_buf));
// 写入后
mos.Unlink();

// 创建内部缓冲区（自动扩容）
mos.Create(initial_size);

// 获取当前内容的独立拷贝（调用者负责 delete[]）
int64 copy_len;
void *copy = mos.CreateCopyData(&copy_len);
delete[] static_cast<uint8*>(copy);
```

### MemoryInputStream（从内存读取，替代 `std::istringstream`）

```cpp
hgl::io::MemoryInputStream mis(data_ptr, data_size);

// 读取
uint8 out[16];
int64 bytes = mis.Read(out, sizeof(out));

// 预读（不移动指针）
mis.Peek(out, sizeof(out));

// 当前指针位置的原始指针
const void *cur_ptr = mis.TellPointer();

// 更新数据源（不复位读指针）
mis.Update(new_ptr, new_size);
mis.Unlink(); // 解除关联
```

---

## 7. 格式化IO（DataInputStream / DataOutputStream）

DataInputStream/DataOutputStream 是包装层，包裹任意 InputStream/OutputStream，提供类型化读写。

```cpp
#include <hgl/io/DataInputStream.h>
#include <hgl/io/DataOutputStream.h>
```

### 常用读取方法（DataInputStream）

```cpp
hgl::io::DataInputStream dis(stream_ptr);

// 基础类型
bool b;   dis.ReadBool  (b);
int8  i8; dis.ReadInt8  (i8);
int16 i16;dis.ReadInt16 (i16);
int32 i32;dis.ReadInt32 (i32);
int64 i64;dis.ReadInt64 (i64);
uint32 u; dis.ReadUint32(u);
float  f; dis.ReadFloat (f);
double d; dis.ReadDouble(d);

// 模板自适应（自动匹配类型）
MyStruct s;
dis.Read(s);  // 按 sizeof(MyStruct) 读取原始字节

// 数组
int32 arr[10];
int64 count = dis.ReadArrays(arr, 10);  // 返回实际读入个数

// 字符串（前缀长度的字符串，默认4字节长度）
hgl::U8String  u8str;  dis.ReadUTF8String    (u8str);
hgl::U16String u16str; dis.ReadUTF16LEString (u16str);

// 跳过
dis.Skip(4);
dis.Seek(100, hgl::io::SeekOrigin::Begin);
```

### 常用写入方法（DataOutputStream）

```cpp
hgl::io::DataOutputStream dos(stream_ptr);

dos.WriteBool  (true);
dos.WriteInt8  (42);
dos.WriteInt16 (1000);
dos.WriteInt32 (0x12345678);
dos.WriteInt64 (big_val);
dos.WriteFloat (3.14f);
dos.WriteDouble(3.14159265);

// 模板自适应
MyStruct s = { ... };
dos.Write(s);

// 数组
int32 arr[10] = { ... };
dos.WriteArrays(arr, 10);

// 字符串（前缀4字节长度）
dos.WriteUTF8String   (u8str);
dos.WriteUTF16LEString(u16str);
// 前缀2字节长度（Short系列）
dos.WriteUTF8ShortString(u8str);
// 前缀1字节长度（Tiny系列）
dos.WriteUTF8TinyString (u8str);
```

### 组合用法示例

```cpp
// 写入结构化二进制文件
hgl::io::FileOutputStream fos;
fos.CreateTrunc(OS_TEXT("data.bin"));
hgl::io::DataOutputStream dos(&fos);
dos.WriteInt32(version);
dos.WriteUTF8String(name);
dos.WriteFloat(value);
fos.Close();

// 读取结构化二进制文件
hgl::io::OpenFileInputStream opener(OS_TEXT("data.bin"));
if (!opener) return false;
hgl::io::DataInputStream dis(opener);
int32 ver;  dis.ReadInt32(ver);
hgl::U8String n; dis.ReadUTF8String(n);
float v;    dis.ReadFloat(v);
```

---

## 8. 文本IO（TextInputStream / LoadString）

### 加载整个文本文件

```cpp
#include <hgl/io/LoadString.h>

// 加载文本文件到 U8String（自动识别编码，默认 UTF-8）
hgl::U8String content;
int result = hgl::LoadStringFromTextFile(content, OS_TEXT("file.txt"));
if (result < 0) GLogError(u8"加载失败");

// 加载为 U16String
hgl::U16String wide;
hgl::LoadStringFromTextFile(wide, OS_TEXT("file.txt"));

// 从内存块加载文本（已有原始字节）
int result = hgl::LoadStringFromText(content, raw_data, raw_size);
```

### 加载文本文件为字符串列表（按行分割）

```cpp
#include <hgl/io/LoadStringList.h>

hgl::U8StringList lines;
int count = hgl::LoadStringListFromTextFile(lines, OS_TEXT("list.txt"));
// count 为读取的行数

for (int i = 0; i < lines.GetCount(); i++)
    GLogInfo(u8"第%d行：%s", i, lines[i].c_str());
```

### TextInputStream（流式逐行解析）

```cpp
#include <hgl/io/TextInputStream.h>

// 实现回调处理每行
struct MyLineHandler : public hgl::io::TextInputStream::ParseCallback<u8char>
{
    bool OnLine(u8char *text, const int len) override
    {
        // 处理每行文本
        GLogInfo(u8"行：%s", text);
        return true;  // 返回 false 停止解析
    }
};

hgl::io::OpenFileInputStream opener(OS_TEXT("large.txt"));
if (!opener) return false;
hgl::io::TextInputStream tis(opener);

MyLineHandler handler;
tis.SetParseCallback<u8char>(&handler);
int lines = tis.Run();  // 返回解析的总行数
```

---

## 9. 随机访问文件（RandomAccessFile）

同时支持读写，共用一个访问指针（DataInputStream + DataOutputStream 可同时包裹）：

```cpp
#include <hgl/io/RandomAccessFile.h>

hgl::io::RandomAccessFile raf;
if (!raf.Open(OS_TEXT("data.bin"))) return false;

// 读取
int32 val;
raf.Read(&val, sizeof(val));

// 写入
raf.Write(&val, sizeof(val));

// 跳转位置
raf.Seek(100, hgl::io::SeekOrigin::Begin);
int64 pos  = raf.Tell();
int64 size = raf.GetSize();

// 指定位置读写（不移动共用指针）
raf.Read (offset, buf, size);
raf.Write(offset, buf, size);

raf.Close();
```

---

## 10. 内存映射文件（MMapFile）

适合超大文件的只读映射（避免全量 LoadFileToMemory）：

```cpp
#include <hgl/io/MMapFile.h>

// 只读映射（最常用）
hgl::MMapFile::Error err;
hgl::MMapFile *mf = hgl::OpenMMapFileOnlyRead(OS_TEXT("large.bin"), &err);
if (!mf) {
    GLogError(u8"内存映射失败，错误码：%d", (int)err);
    return false;
}
const void *data = mf->data();
size_t      size = mf->size();
// 直接访问 data 指针即可读取文件内容
delete mf;  // 析构时自动解除映射

// 可写映射（现有文件指定大小）
hgl::MMapFile *wmf = hgl::OpenMMapFile(OS_TEXT("file.bin"), file_size);
void *rw_data = wmf->data();
memcpy(rw_data, buf, size);
delete wmf;

// 创建新文件并映射
hgl::MMapFile *nmf = hgl::CreateMMapFile(OS_TEXT("new.bin"), new_size);
delete nmf;
```

---

## 11. STL → HGL 对照表

| 禁止使用 (STL) | 必须使用 (HGL) | 头文件 |
|---------------|---------------|--------|
| `std::ifstream` | `hgl::io::FileInputStream` + `OpenFileInputStream` | `<hgl/io/FileInputStream.h>` |
| `std::ofstream` | `hgl::io::FileOutputStream` + `CreateFileOutputStream` | `<hgl/io/FileOutputStream.h>` |
| `std::istringstream` | `hgl::io::MemoryInputStream` | `<hgl/io/MemoryInputStream.h>` |
| `std::ostringstream` | `hgl::io::MemoryOutputStream` | `<hgl/io/MemoryOutputStream.h>` |
| `std::fstream` | `hgl::io::RandomAccessFile` | `<hgl/io/RandomAccessFile.h>` |
| `std::filesystem::path` | `hgl::filesystem::Path` | `<hgl/filesystem/Path.h>` |
| `std::filesystem::exists` | `hgl::filesystem::FileExist` | `<hgl/filesystem/FileSystem.h>` |
| `std::filesystem::create_directories` | `hgl::filesystem::MakePath` | `<hgl/filesystem/FileSystem.h>` |
| `std::filesystem::remove_all` | `hgl::filesystem::DeleteTree` | `<hgl/filesystem/FileSystem.h>` |
| `std::filesystem::copy` | `hgl::filesystem::FileCopy` | `<hgl/filesystem/FileSystem.h>` |
| `mmap` (posix) | `hgl::OpenMMapFileOnlyRead` / `hgl::OpenMMapFile` | `<hgl/io/MMapFile.h>` |
| `ReadFile` (WinAPI) | `hgl::filesystem::LoadFileToMemory` 或 `FileInputStream` | `<hgl/filesystem/FileSystem.h>` |

---

## 12. ❌ vs ✅ 常见错误对比

```cpp
// ❌ 错误：使用 std::ifstream
std::ifstream ifs("file.bin", std::ios::binary);
ifs.read(buf, size);

// ✅ 正确：使用 OpenFileInputStream（RAII）
hgl::io::OpenFileInputStream opener(OS_TEXT("file.bin"));
if (!opener) return false;
opener->Read(buf, size);

// ❌ 错误：以为 OpenFileInputStream 是全局函数
auto *fis = hgl::io::OpenFileInputStream(filename);   // 编译错误！OpenFileInputStream 是 RAII 类

// ✅ 正确：OpenFileInputStream 是 RAII 辅助类
hgl::io::OpenFileInputStream opener(filename);
if (!opener) return false;
hgl::io::FileInputStream *fis = opener;

// ❌ 错误：使用不存在的函数 GetFileSize / MakeDirectory / DirectoryExist
int64 sz = hgl::filesystem::GetFileSize(path);   // 不存在！
hgl::filesystem::MakeDirectory(path);            // 不存在！
hgl::filesystem::DirectoryExist(path);           // 不存在！

// ✅ 正确：
hgl::filesystem::FileInfo fi;
hgl::filesystem::GetFileInfo(path, fi);
int64 sz = fi.size;                              // fi.size 是文件大小
hgl::filesystem::MakePath(path);                 // 创建目录
hgl::filesystem::IsDirectory(path);              // 判断目录

// ❌ 错误：使用不存在的函数 LoadStringFromFile
hgl::io::LoadStringFromFile(path, str);           // 不存在！

// ✅ 正确：
hgl::LoadStringFromTextFile(str, path);           // 注意命名空间和参数顺序！

// ❌ 错误：使用 std::ostringstream 拼装二进制
std::ostringstream ss;
ss.write(reinterpret_cast<const char*>(&val), sizeof(val));

// ✅ 正确：
hgl::io::MemoryOutputStream mos;
hgl::io::DataOutputStream dos(&mos);
dos.WriteInt32(val);
const void *buf = mos.GetData();
int64 len = mos.Tell();

// ❌ 错误：使用 std::filesystem::path
std::filesystem::path p = std::filesystem::path("data") / "textures" / "sky.dds";

// ✅ 正确：
hgl::filesystem::Path p(OS_TEXT("data"));
p = p / OS_TEXT("textures") / OS_TEXT("sky.dds");
```
