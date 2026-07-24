# ULRE Copilot 编码规范

## ⚠️ 强制规则：禁止使用STL，必须使用HGL自有库

本项目是自研游戏引擎，拥有完整的自有基础库（字符串、集合、IO、线程、哈希等）。
**Agent生成代码时必须使用HGL库，严禁引入STL头文件和类型。**

---

## ❌ 绝对禁止（Never Include）

```cpp
// 以下头文件一律禁止
#include <string>
#include <string_view>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>   // 用 <hgl/type/Smart.h> 代替
```

❌ 禁止手写FNV1a或任何哈希函数实现  
❌ 禁止使用 `std::string`、`std::vector`、`std::thread`、`std::mutex` 等STL类型  
❌ 禁止使用 `new`/`delete` 手动内存管理（使用HGL的智能指针或池）  
❌ 禁止使用 `printf`、`std::cout`、`std::cerr`、`fprintf` 输出日志（使用HGL日志宏）

---

## ✅ STL → HGL 替换对照表

| 禁止使用 (STL)               | 必须使用 (HGL)                          | 头文件                              |
|------------------------------|----------------------------------------|-------------------------------------|
| `std::string`                | `AnsiString`                           | `<hgl/type/String.h>`               |
| `std::string`（UTF-8内容）   | `UTF8String` / `U8String`              | `<hgl/type/String.h>`               |
| 跨平台文件路径字符串          | `OSString`                             | `<hgl/type/String.h>`               |
| `std::wstring`               | `WideString`                           | `<hgl/type/String.h>`               |
| `std::string_view`（字节）   | `AnsiStringView`                       | `<hgl/type/String.h>`               |
| `std::vector<T>`             | `hgl::ArrayList<T>`                    | `<hgl/type/ArrayList.h>`            |
| `std::list<T>`               | `hgl::List<T>`                         | `<hgl/type/List.h>`                 |
| `std::map<K,V>`              | `hgl::Map<K,V>`（有序）                | `<hgl/type/Map.h>`                  |
| `std::unordered_map<K,V>`    | `hgl::UnorderedMap<K,V>`               | `<hgl/type/UnorderedMap.h>`         |
| `std::set<T>`                | `hgl::SortedSet<T>`                    | `<hgl/type/SortedSet.h>`            |
| `std::unordered_set<T>`      | `hgl::OrderedSet<T>`                   | `<hgl/type/OrderedSet.h>`           |
| `std::shared_ptr<T>`         | HGL智能指针                            | `<hgl/type/Smart.h>`                |
| 手写FNV1a                    | `hgl::hash::FNV1aInit` + Append系列    | `<hgl/util/hash/FNV1a.h>`           |
| 其他哈希函数                  | `hgl::hash::Hash`                      | `<hgl/util/hash/Hash.h>`            |
| `std::ifstream`              | `hgl::io::FileInputStream`             | `<hgl/io/FileInputStream.h>`        |
| `std::ofstream`              | `hgl::io::FileOutputStream`            | `<hgl/io/FileOutputStream.h>`       |
| `std::istringstream`         | `hgl::io::MemoryInputStream`           | `<hgl/io/MemoryInputStream.h>`      |
| `std::ostringstream`         | `hgl::io::MemoryOutputStream`          | `<hgl/io/MemoryOutputStream.h>`     |
| 文件系统操作                  | `hgl::filesystem::*`                   | `<hgl/filesystem/FileSystem.h>`     |
| 日志输出                      | `LogInfo` / `GLogInfo` / `FLogInfo` 等宏 | `<hgl/log/Log.h>`                   |

---

## 🔑 FNV1a 哈希正确用法（禁止手写，必须使用此API）

```cpp
#include <hgl/util/hash/FNV1a.h>

// 初始化
uint64 hash = hgl::hash::FNV1aInit<uint64>();

// 追加原始字节数据
hash = hgl::hash::FNV1aAppendBytes(hash, data_ptr, byte_count);

// 追加值类型（自动按字节展开）
hash = hgl::hash::FNV1aAppendValueBytes(hash, some_int_or_struct);

// 追加字符串内容
hash = hgl::hash::FNV1aAppendBytes(hash, str.data(), str.size());
```

---

## 🔤 字符串正确用法

```cpp
#include <hgl/type/String.h>

// 构造
AnsiString s = "hello";
AnsiString s2 = AnsiString("prefix_") + AnsiString::numberOf(42u);

// UTF-8字符串（引擎内部文本、资源名等）
UTF8String u8s = u8"你好";
// 或使用宏
U8String name = U8_TEXT("MaterialName");

// 跨平台文件路径
OSString path = OS_TEXT("/path/to/file");

// 获取 C 风格指针
const char *c_str = s.c_str();   // AnsiString
const u8char *u8_ptr = u8s.data(); // UTF8String
```

---

## 📦 集合类正确用法

```cpp
#include <hgl/type/ArrayList.h>
#include <hgl/type/UnorderedMap.h>

// 动态数组（替代 std::vector）
hgl::ArrayList<int> list;
list.Add(1);
list.Add(2);
const int count = list.GetCount();
int *ptr = list.GetData();

// 哈希Map（替代 std::unordered_map）
hgl::UnorderedMap<AnsiString, int> map;
map.Add("key", 42);
int *val = map.Find("key");  // 找不到返回 nullptr
```

---

## 📁 IO 正确用法

```cpp
#include <hgl/io/FileInputStream.h>
#include <hgl/io/MemoryOutputStream.h>

// 读文件
hgl::io::FileInputStream *fis = hgl::io::OpenFileInputStream(filename);
if (fis) {
    // 读取操作
    delete fis;
}

// 内存流（替代 std::ostringstream）
hgl::io::MemoryOutputStream mos;
// 写入数据后获取缓冲
const void *buf = mos.GetData();
int64 len = mos.Tell();
```

---

## 📝 日志正确用法

❌ 永远不要使用 `printf`、`std::cout`、`std::cerr`、`fprintf`、`puts` 等输出日志。  
✅ 必须使用 `<hgl/log/Log.h>` 中的宏，根据使用场景选择以下4种模式之一：

### 模式1：类成员日志（最常用）

在头文件类定义中声明 `OBJECT_LOGGER`，类方法中用 `LogXxx(...)` 输出：

```cpp
// MyClass.h
#include <hgl/log/Log.h>

class MyClass
{
    OBJECT_LOGGER   // 展开为：::hgl::logger::ObjectLogger Log{&typeid(*this)};
public:
    void DoWork();
};

// MyClass.cpp
void MyClass::DoWork()
{
    LogInfo(u8"开始工作");
    LogWarning(u8"资源不足，剩余 %d", count);
    LogError(u8"初始化失败：%s", name);
}
```

可用宏：`LogVerbose` `LogDebug` `LogInfo` `LogNotice` `LogWarning` `LogError` `LogFatal`  
（`LogVerbose`/`LogDebug` 在 Release 模式下自动为空操作）

---

### 模式2：全局 / 自由函数（少量散落调用）

无需任何声明，直接使用 `GLogXxx(...)` 调用全局 logger：

```cpp
#include <hgl/log/Log.h>

void SomeGlobalFunc()
{
    GLogInfo(u8"全局信息");
    GLogError(u8"全局错误：%d", code);
}
```

可用宏：`GLogVerbose` `GLogDebug` `GLogInfo` `GLogNotice` `GLogWarning` `GLogError` `GLogFatal`

---

### 模式3：模块日志（多文件/多类共享模块名）

适合一个模块有多个类的场景，所有输出都带模块名前缀：

```cpp
// Render.cpp（或模块入口 .cpp）
#include <hgl/log/Log.h>
DEFINE_LOGGER_MODULE(Render)   // 定义 hgl::logger::LogRender

// RenderHelper.h（其他需要此 logger 的头文件）
#include <hgl/log/Log.h>
EXTERN_LOGGER_MODULE(Render)   // extern 声明

// 任意 .cpp 中直接使用（需已 include 含 EXTERN 的头文件）
MLogInfo(Render, u8"渲染帧开始");
MLogError(Render, u8"着色器编译失败：%s", name);
```

可用宏：`MLogVerbose(name,...)` `MLogDebug(name,...)` `MLogInfo(name,...)` `MLogNotice(name,...)`  
`MLogWarning(name,...)` `MLogError(name,...)` `MLogFatal(name,...)`

---

### 模式4：文件级模块绑定（自由函数、单文件绑定）

在 `.cpp` 顶部用 `USE_MODULE_LOGGER` 绑定，之后用 `FLogXxx(...)` 无前缀调用：

```cpp
// Render.cpp — 先定义模块
#include <hgl/log/Log.h>
DEFINE_LOGGER_MODULE(Render)
USE_MODULE_LOGGER(Render)   // 生成 GetModuleLogger() 内联函数

// 之后同文件内的自由函数直接使用 FLogXxx
static void CompileShader(const OSString &path)
{
    FLogInfo(u8"编译着色器：%s", path.c_str());
    FLogError(u8"编译失败");
}
```

可用宏：`FLogVerbose` `FLogDebug` `FLogInfo` `FLogNotice` `FLogWarning` `FLogError` `FLogFatal`

---

### 选择指南

| 场景 | 使用模式 |
|------|---------|
| 类方法中输出 | `OBJECT_LOGGER` + `LogXxx` |
| 全局/工具函数（少量） | `GLogXxx` |
| 多类共享同一模块名 | `DEFINE_LOGGER_MODULE` + `EXTERN_LOGGER_MODULE` + `MLogXxx` |
| 单文件自由函数绑定模块 | `DEFINE_LOGGER_MODULE` + `USE_MODULE_LOGGER` + `FLogXxx` |

> 📌 详细参考：`.ai/skills/SKILL_LOGGING_REFERENCE.md`

---

## 🔍 遇到不确定的HGL类型时

1. 优先查阅 `.ai/skills/SKILL_HGL_TYPES_REFERENCE.md` 获取完整示例
2. 搜索 `inc/hgl/type/`、`inc/hgl/io/`、`inc/hgl/util/` 等目录下的头文件
3. 参照已有代码中的 `#include <hgl/...>` 用法模式

---

## 📚 任务开始前请查阅 `.ai/skills/`

遇到新任务时，先查看 `.ai/skills/SKILL_INDEX.md` 了解可用的工作流指南：
- 添加新渲染组件/系统 → `SKILL_ADD_NEW_RENDER_COMPONENT.md`
- HGL类型完整参考 → `SKILL_HGL_TYPES_REFERENCE.md`
- **HGL日志系统** → `SKILL_LOGGING_REFERENCE.md`
- 系统分组和启用 → `SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md`
- 执行阶段排序 → `SKILL_EXECUTION_PHASE_ORDERING.md`
- RenderGraph用法 → `SKILL_RENDERGRAPH_USAGE.md`
