# HGL 日志系统完整参考

> 头文件：`#include <hgl/log/Log.h>`  
> 位置：CMCore `inc/hgl/log/Log.h`

---

## ⚠️ 强制规则

❌ 绝对禁止：`printf` / `fprintf` / `puts` / `std::cout` / `std::cerr` / `std::clog`  
✅ 必须使用：`<hgl/log/Log.h>` 中对应宏，根据场景选择下方4种模式

---

## 📊 4种模式速查

| 场景 | 声明方式 | 使用宏 |
|------|---------|-------|
| 类成员方法 | 头文件类内 `OBJECT_LOGGER` | `LogInfo(...)` `LogError(...)` 等 |
| 全局 / 工具函数（散落少量） | 无需声明 | `GLogInfo(...)` `GLogError(...)` 等 |
| 多文件/多类共享同一模块名 | `.cpp` `DEFINE_LOGGER_MODULE(X)` + `.h` `EXTERN_LOGGER_MODULE(X)` | `MLogInfo(X,...)` `MLogError(X,...)` 等 |
| 单文件自由函数绑定模块 | `DEFINE_LOGGER_MODULE(X)` + `USE_MODULE_LOGGER(X)` | `FLogInfo(...)` `FLogError(...)` 等 |

---

## 📋 日志级别

| 级别 | 宏后缀 | Release下 | 说明 |
|------|--------|----------|------|
| Verbose | `Verbose` | 空操作 | 极详细追踪，仅Debug |
| Debug   | `Debug`   | 空操作 | 调试信息，仅Debug |
| Info    | `Info`    | 有效 | 一般信息 |
| Notice  | `Notice`  | 有效 | 重要通知 |
| Warning | `Warning` | 有效 | 警告 |
| Error   | `Error`   | 有效 | 错误 |
| Fatal   | `Fatal`   | 有效 | 致命错误 |

阈值按 `Verbose < Debug < Info < Notice < Warning < Error < Fatal` 递增；消息级别达到或高于阈值时才输出。

---

## 模块类别与运行时过滤

模块 logger 同时作为过滤类别。旧的 `DEFINE_LOGGER_MODULE(name)` 默认类别阈值为 `Verbose`，不改变原有输出行为；需要声明模块默认级别时可使用：

```cpp
DEFINE_LOGGER_MODULE_WITH_LEVEL(Render, hgl::LogLevel::Info)
```

运行时可分别调节类别默认阈值、输出器全局阈值，以及单个“类别 × 输出器”组合：

```cpp
using namespace hgl::logger;

const LogCategoryId render = LogLogRender.GetCategoryId();
const LogOutputterId console = FindLogOutputter(OS_TEXT("Console"));
const LogOutputterId file = FindLogOutputter(OS_TEXT("File"));

SetLogCategoryLevel(render, hgl::LogLevel::Debug);
SetLogOutputterLevel(console, hgl::LogLevel::Warning);
SetLogOutputterLevel(file, hgl::LogLevel::Verbose);

// 此显式组合规则覆盖类别默认值和输出器全局值。
SetLogCategoryOutputterLevel(render, file, hgl::LogLevel::Verbose);
ClearLogCategoryOutputterLevel(render, file);
```

`FindLogOutputter` 在已注册的输出器中按名称查找；内置输出器名称为 `Console`、`File`。找不到时返回 `InvalidLogOutputterId`；`SetLogOutputterLevel` 对无效输出器 ID 返回 `false`，类别规则 API 对未知类别 ID 或未注册的输出器 ID 返回 `false`。

阈值计算规则：

1. 存在类别×输出器显式规则时，只使用该规则。
2. 否则，使用类别默认阈值和输出器全局阈值中更严格的一个。
3. 只有至少一个输出器接受日志时才进行格式化；分发时仍会再次按输出器规则过滤。

模块类别与显示用的对象名/实例名独立。没有显式类别的类日志按类名归类，全局宏归入 `Global` 类别。`DEFINE_LOGGER_MODULE`、`FLogXxx`、`MLogXxx` 和 `OBJECT_LOGGER` 的现有用法保持兼容。

Release 下现有 `Verbose`/`Debug` 宏仍会在编译期移除；运行时阈值只能调节已编译进目标文件的级别，不能重新启用已剔除的调用。

---

## 模式1：类成员日志（最常用）

在头文件类定义中加 `OBJECT_LOGGER`，类方法中直接用 `LogXxx(...)` 输出。  
宏会展开为 `::hgl::logger::ObjectLogger Log{&typeid(*this)};`，自动带类名前缀。

```cpp
// MyRenderer.h
#include <hgl/log/Log.h>

class MyRenderer
{
    OBJECT_LOGGER   // 在类内添加这一行，无需分号之外的任何声明
public:
    bool Init(const OSString &config);
    void Render();
};

// MyRenderer.cpp
bool MyRenderer::Init(const OSString &config)
{
    LogInfo(u8"初始化渲染器，配置：%s", config.c_str());

    if (!LoadConfig(config))
    {
        LogError(u8"配置加载失败：%s", config.c_str());
        return false;
    }

    LogNotice(u8"渲染器初始化完成");
    return true;
}

void MyRenderer::Render()
{
    LogDebug(u8"开始渲染帧 %d", frame_index);  // Release 模式下自动忽略
    LogWarning(u8"顶点数量过多：%d", vertex_count);
}
```

**可用宏**（`this->Log.xxx` 的快捷方式）：

```
LogVerbose(fmt, ...)   // Debug-only
LogDebug(fmt, ...)     // Debug-only
LogInfo(fmt, ...)
LogNotice(fmt, ...)
LogWarning(fmt, ...)
LogError(fmt, ...)
LogFatal(fmt, ...)
```

---

## 模式2：全局日志（Global Logger）

无需任何声明。适用于全局工具函数、`main()`、静态初始化等少量散落调用。

```cpp
#include <hgl/log/Log.h>

// 可直接在任意位置使用，无需类或模块
void InitEngine()
{
    GLogInfo(u8"引擎启动");
    GLogWarning(u8"未找到配置文件，使用默认值");
}

int main()
{
    GLogInfo(u8"程序启动，版本 %s", VERSION);

    if (!InitEngine())
    {
        GLogFatal(u8"引擎初始化失败，退出");
        return -1;
    }

    GLogInfo(u8"程序正常退出");
    return 0;
}
```

**可用宏**（`::hgl::logger::GlobalLogger.xxx` 的快捷方式）：

```
GLogVerbose(fmt, ...)  // Debug-only
GLogDebug(fmt, ...)    // Debug-only
GLogInfo(fmt, ...)
GLogNotice(fmt, ...)
GLogWarning(fmt, ...)
GLogError(fmt, ...)
GLogFatal(fmt, ...)
```

---

## 模式3：模块日志（Module Logger，多类共享）

适用于一个模块横跨多个类/文件，希望所有输出都带同一模块名前缀。

**Step 1**：在模块入口 `.cpp`（或专门的 Logger.cpp）中**定义**模块 logger：

```cpp
// RenderModule.cpp
#include <hgl/log/Log.h>

DEFINE_LOGGER_MODULE(Render)
// 展开为：namespace hgl::logger { ::hgl::logger::ObjectLogger LogRender(OS_TEXT("Render")); }
```

**Step 2**：在模块头文件中**声明**（extern），供其他文件使用：

```cpp
// RenderModule.h
#include <hgl/log/Log.h>

EXTERN_LOGGER_MODULE(Render)
// 展开为：namespace hgl::logger { extern ::hgl::logger::ObjectLogger LogRender; }
```

**Step 3**：任意 `.cpp` 中 include 该头文件后，用 `MLogXxx(ModuleName, ...)` 输出：

```cpp
// RenderPass.cpp
#include "RenderModule.h"

void RenderPass::Execute()
{
    MLogInfo(Render, u8"执行渲染Pass：%s", name.c_str());
    MLogError(Render, u8"渲染Pass失败");
}

// RenderPipeline.cpp
#include "RenderModule.h"

void RenderPipeline::Build()
{
    MLogNotice(Render, u8"构建渲染管线");
}
```

**可用宏**（第一个参数是模块名，不加引号）：

```
MLogVerbose(name, fmt, ...)  // Debug-only
MLogDebug(name, fmt, ...)    // Debug-only
MLogInfo(name, fmt, ...)
MLogNotice(name, fmt, ...)
MLogWarning(name, fmt, ...)
MLogError(name, fmt, ...)
MLogFatal(name, fmt, ...)
```

---

## 模式4：文件级绑定（USE_MODULE_LOGGER + FLogXxx）

适用于单个 `.cpp` 文件内的自由函数，无需每次写模块名。  
在文件顶部 `USE_MODULE_LOGGER(X)` 绑定后，同文件内直接用 `FLogXxx(...)` 。

```cpp
// ShaderCompiler.cpp
#include <hgl/log/Log.h>

// 若 LogShader 尚未定义，先定义：
DEFINE_LOGGER_MODULE(Shader)

// 绑定到本文件（生成 GetModuleLogger() 内联函数）
USE_MODULE_LOGGER(Shader)

// 之后同文件内的自由函数无需写模块名
static bool CompileGLSL(const OSString &path)
{
    FLogInfo(u8"编译 GLSL：%s", path.c_str());

    if (/* 编译失败 */)
    {
        FLogError(u8"GLSL 编译错误：%s", error_msg);
        return false;
    }

    FLogDebug(u8"编译成功");
    return true;
}

bool LinkProgram(uint32 vert, uint32 frag)
{
    FLogInfo(u8"链接着色器程序 vert=%u frag=%u", vert, frag);
    FLogWarning(u8"未使用的 uniform：%s", uniform_name);
    return true;
}
```

**可用宏**（`GetModuleLogger().xxx` 的快捷方式，同文件内无需传模块名）：

```
FLogVerbose(fmt, ...)  // Debug-only
FLogDebug(fmt, ...)    // Debug-only
FLogInfo(fmt, ...)
FLogNotice(fmt, ...)
FLogWarning(fmt, ...)
FLogError(fmt, ...)
FLogFatal(fmt, ...)
```

---

## 格式字符串类型

所有 `LogXxx`/`GLogXxx`/`MLogXxx`/`FLogXxx` 宏均支持：

| 参数类型 | 说明 | 示例 |
|---------|------|------|
| `const u8char *fmt, ...` | UTF-8 printf 风格（推荐） | `LogInfo(u8"路径：%s", path.c_str())` |
| `const U16String &text` | UTF-16 正文直接输出 | `LogInfo(U16String(U16_TEXT("日志正文")))` |
| `const std::string &text` | 直接传 std::string（内部转换） | `LogInfo(std::string("hello"))` |

变参 printf 风格格式化以 UTF-8 格式字符串为跨平台保证接口。Windows 仍提供 UTF-16 格式化重载；POSIX 不提供该变参重载，因为 `u16char` 是 `char16_t`，不能传给要求 `wchar_t` 的 `vswprintf`。POSIX 调用方可通过 `U16String` 直接输出 UTF-16 正文。

---

## 辅助宏（快捷返回 + 日志）

```cpp
RETURN_FALSE              // 记录 GLogVerbose("return(false)") 后 return false
RETURN_ERROR(v)           // 记录 GLogInfo("return error(v)") 后 return v
RETURN_ERROR_NULL         // 记录 GLogInfo("return error(nullptr)") 后 return nullptr
RETURN_BOOL(proc)         // 执行 proc，成功返回 true，失败调用 RETURN_FALSE

IF_FALSE_RETURN(str)      // if(!str) RETURN_FALSE
```

---

## ❌ vs ✅ 常见错误对比

```cpp
// ❌ 错误：使用 printf
printf("Error: %d\n", code);

// ❌ 错误：使用 std::cout
std::cout << "Info: " << msg << std::endl;

// ❌ 错误：使用不存在的宏 LOG_INFO / LOG_ERROR
LOG_INFO("hello");   // 这些宏不存在！

// ❌ 错误：类中未加 OBJECT_LOGGER 就用 LogInfo
void MyClass::Foo() { LogInfo(u8"test"); }  // 找不到 this->Log

// ✅ 正确：类内加 OBJECT_LOGGER，方法中用 LogInfo
class MyClass { OBJECT_LOGGER ... };
void MyClass::Foo() { LogInfo(u8"test"); }

// ✅ 正确：全局函数用 GLogInfo
void FreeFunc() { GLogInfo(u8"test"); }

// ✅ 正确：模块多类共享用 MLogInfo
MLogError(Render, u8"渲染失败：%s", name.c_str());

// ✅ 正确：文件级绑定后用 FLogInfo
USE_MODULE_LOGGER(Render)
static void Helper() { FLogInfo(u8"helper called"); }
```
