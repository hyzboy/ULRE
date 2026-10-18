# 构建系统与模块结构审计（2026-10）

> 状态：**提案**（尚未实施）
> 范围：CMake 模块化、include 边界、构建工作链、测试与文档组织
> 不涉及：渲染算法、ECS 数据布局、ShaderGen 语义（各有独立文档）

---

## 0. 结论摘要

| # | 问题 | 类型 | 危害等级 | 改动量 |
|---|------|------|---------|--------|
| 1 | 全局 `include_directories`，模块间零隔离 | 设计失误 | 高 | 中 |
| 2 | `add_cm_library` 不声明模块间依赖（fat `${ULRE}` 列表） | 设计失误 | 高 | 中 |
| 3 | 单一扁平输出目录 + 逐 target applocal | 设计失误 | 高 | 小 |
| 4 | 巨型单文件（432KB / 267KB / 128KB） | 设计失误 | 中 | 大 |
| 5 | 零 CI | 工作链 | 高 | 小 |
| 6 | 19 个 submodule，3 个长期脏 | 工作链 | 中 | 小 |
| 7 | 根目录 7 个构建/临时目录 | 工作链 | 低 | 小 |
| 8 | `doc/` 无索引、目录失衡 | 工作链 | 中 | 小 |
| 9 | `add_cm_library` 的 include 参数重复登记 | 冗余 | 低 | 极小 |
| 10 | `add_subdirectory` 顺序有语义但无注释 | 冗余 | 低 | 极小 |
| 11 | 各模块手写文件清单已漂移 | 冗余 | 中 | 小 |

**性价比排序**：3 → 5 → 8 → 2 → 1 → 11 → 6 → 7 → 9 → 10 → 4

---

## 1. 全局 `include_directories`，模块间零隔离

### 现象

`CMCMakeModule/use_cm_module.cmake`：

```cmake
macro(use_cm_module module)
    add_subdirectory(CM${module})
    include(CM${module}/path_config.cmake)
    include_directories(CM${module}/inc)   # ← 每个模块的 inc 都进全局 include 路径
endmacro()
```

根 `CMakeLists.txt:16-17` 同样全局加入：

```cmake
include_directories(${ULRE_3RDPTY_ROOT_PATH}/NvTriStrip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
```

结果：**任何 target 都能 include 任何模块的任何头**，编译器层面没有边界。

### 证据

**证据 A —— 工具直接编译库私有实现**

`src/Tools/ShaderGen/` 下的三个回归工具原本必须写：

```cpp
#include "../../ShaderGen/compile/MaterialShaderEmitter.h"
#include "../../ShaderGen/builder/GenericMaterialBuilder.h"
#include "../../ShaderGen/meshgen/MeshTemplateEmitter.h"
#include "../../ShaderGen/document/DocumentFragmentBuilder.h"
```

`MaterialShaderEmitter.h`、`GenericMaterialBuilder.h`、`MeshTemplateEmitter.h` 都在 `src/ShaderGen/`（**库私有**，不在 `inc/`），却在工具里被直接编译。这等于把 ShaderGen 的内部结构当成了对外 ABI：

- 私有重构会直接编译失败工具
- 工具与库必须使用完全一致的编译宏，否则产生 ODR 风险
- 走 `../../` 跨级回跳，路径依赖当前目录层级

（本轮已改为 `"ShaderGen/..."`，走 `CreateProject` 已有的 `${CMAKE_SOURCE_DIR}/src` 私有 include 路径。**这只是消除了 `../..`，没有消除越界本身**——工具仍在编库私有头。）

**证据 B —— 该设计自己的自白**

`CMCMakeModule/common.cmake`：

> CMCore 的公开头文件（OrderedSet.h 等）直接包含 absl 头文件，而
> `use_cm_module` 会把各模块 inc 目录全局加入所有 target 的 include 路径。
> 因此必须把 absl 的 include 目录也全局暴露，否则链接了 CMCore 但未
> 直接链接 absl 的 target 在编译时会报 C1083。

即：因为 include 是全局的，所以**依赖也必须全局**，否则会出现"头文件可见但它的依赖不可见"的割裂状态。这是全局 include 模型的必然代价。

**证据 C —— 跨模块变量泄漏**

`src/Vulkan/CMakeLists.txt:245` 定义了 `GEOMETRY_VERTEX_DATA_FILES`，而 `src/SceneGraph/CMakeLists.txt` 的 `add_cm_library` 里引用了它。因为 `src/CMakeLists.txt` 的 `add_subdirectory` 顺序是 Vulkan → ShaderGen → SceneGraph，父作用域变量确实泄漏过去，导致 SceneGraph 的源文件列表里混进了 Vulkan 的 `VertexAttrib.cpp` / `VertexDataManager.cpp`。

（本轮已删除该引用。）

### 建议

`add_cm_library` 其实已经写了 `target_include_directories(... PUBLIC ...)`，说明本来就想走 per-target 传播，只是被全局 `include_directories` 架空了。

**试点路径**（不要一次全改）：

1. 选一个叶子模块（`ULRE.InlineGeometry` 或 `ULRE.Work`），删掉它在 `use_cm_module` 中的全局 include 依赖，只靠 `target_link_libraries` 传播
2. 修正因此暴露的缺失依赖声明（这一步会自然产出第 2 条所需的依赖信息）
3. 逐步向外扩到 `ULRE.SceneGraph`、`ULRE.ECS`

风险：`common.cmake` 里 absl 的全局 include 是补丁，试点时会被打破，需要一并改成 absl 的 target 传播。

---

## 2. `add_cm_library` 不声明模块间依赖

### 现象

`CMCMakeModule/cm_modules.cmake`：

```cmake
macro(add_cm_library module_name project_folder include_path)
    set(SOURCE_FILES "${ARGN}")
    if(WIN32)
        add_library(${module_name} STATIC ${SOURCE_FILES})
        target_link_libraries(${module_name} PRIVATE ${HGL_MATH_LIB})
    else()
        add_library(${module_name} SHARED ${SOURCE_FILES})
    endif()
    ...
endmacro()
```

宏**只有 3 个固定参数**（名字、文件夹、include 路径），**没有依赖参数**。所以：

- `ULRE.SceneGraph` 完全不声明它依赖 `ULRE.Vulkan` / `CMCore` / `CMMath`
- `ULRE.ECS` 同理

于是所有消费者只能这样写：

```cmake
target_link_libraries(some_example ${ULRE})
```

`${ULRE}` 是根 `CMakeLists.txt:39-55` 的 15 项"全家桶"，一次链上整个引擎。

### 危害

**"谁需要哪个库"在列表里完全看不出来。** 三个具体后果：

1. **改一处依赖要动所有 consumer。** 想把 `CMAudio` 从默认链接里拿掉？影响全部 353 个可执行目标。
2. **误删风险，且已实际发生。** 本轮清理时把 `${HGL_MATH_LIB}` 判定为"从未定义的死变量"删除——因为它在列表里孤零零地待着，看不出谁需要它。实际它定义在 `CMCMakeModule/math.cmake:22/25/29`（`glm::glm` / `GLM`），且是**消费者获取 glm 头文件的唯一通道**：`add_cm_library` 用 `PRIVATE` 链接它，不向消费者传播；而 `inc/hgl/ecs/core/Context.h`、30+ 个 example 都直接 `#include <glm/...>`。已回滚（提交 `e6cda6a25`）并加注释防止再次误删。
   > 本机之所以删了也能编过，只是因为 Vulkan SDK 自带 glm，且 `vulkan.cmake` 全局 `include_directories(SYSTEM ${Vulkan_INCLUDE_DIRS})` 顺带覆盖了它。在"Vulkan SDK 不含 glm"（走 `find_package(glm CONFIG REQUIRED)`）或 Linux 上会直接断链。
3. **缺符号只在最终链接时暴露。** 静态库不校验符号完整性，错误延迟到 exe 链接阶段，定位成本高。

`${RENDER_LIBRARY}`（`vulkan.cmake:12`，UNIX 分支为 `xcb`）同理：`ULRE.Vulkan` 在非 Windows 上是 SHARED 且自己不链 xcb，消费者的 xcb 只能来自这份列表。

### 建议

给宏加依赖参数，或干脆在各模块 `CMakeLists.txt` 里显式 `target_link_libraries`：

```cmake
# 方案 A：扩展现有宏
add_cm_library(ULRE.SceneGraph "ULRE" ${SG_INCLUDE_PATH}
    DEPENDS ULRE.Vulkan ULRE.ShaderGen CMMath CMCore
    ${SG_SOURCE} ...)

# 方案 B：保持宏不变，各模块自己声明
add_cm_library(ULRE.SceneGraph "ULRE" ${SG_INCLUDE_PATH} ${SG_SOURCE} ...)
target_link_libraries(ULRE.SceneGraph PUBLIC ULRE.Vulkan ULRE.ShaderGen CMMath CMCore)
```

同时把 `HGL_MATH_LIB` 的链接从 `PRIVATE` 改为 `PUBLIC`（glm 是 header-only INTERFACE target，PRIVATE 对静态库消费者毫无意义）。

注意：`cm_modules.cmake` 位于 **CMCMakeModule 子模块**内，改动需要子模块提交 + gitlink 更新，会影响共用该模块的其他项目。

---

## 3. 单一扁平输出目录 + 逐 target applocal

### 现象

`CMCMakeModule/output_path.cmake` 把**所有** target 的所有产物指向同一个目录：

```cmake
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY})
# 再对 Debug/Release/MinSizeRel/RelWithDebInfo 各设一遍
```

实测 `build/out/Windows_64_Debug/` 内容：

| 项 | 数量 |
|---|---|
| 可执行文件 | **353** |
| DLL | **38** |
| 总文件 | **832** |

全部平铺在同一个目录里。

同时 vcpkg 的 applocal 是**逐可执行目标的后置构建步骤**，形如：

```
E:\vcpkg\vcpkg.exe z-applocal --target-binary=.../XXX.exe \
    --installed-bin-dir=E:/vcpkg/installed/x64-windows/debug/bin
```

353 个 exe ⇒ 353 个后置步骤，**并发往同一个目录拷贝同一批 38 个 DLL**。

### 证据（可复现）

同一份代码，只改并行度：

| 构建方式 | 结果 |
|---|---|
| `msbuild ULRE.slnx /p:Configuration=Debug /m`（第 1 次） | `ShaderResourceSchemaRegressionGate` 报 `MSB3073 exit 32` |
| `/m`（第 2 次） | `BitsArrayTest`、`UnorderedSetBasicOperations`、`LogOutputWaysExample` 报 `MSB3073 exit 32` |
| `/m`（第 3 次） | 同上三个 CMCore 示例 |
| `/m:1`（串行） | **零命中** |

三次并行构建命中三组**不同**目标，串行构建零命中 ⇒ 确定是并发写同一目录的竞态，而非某个 target 本身有问题。

单独重建失败目标（`msbuild .../ShaderResourceSchemaRegressionGate.vcxproj`）也直接成功，进一步排除代码问题。

### 危害

- 在 CI 上表现为**随机红灯**，极易被误判成"测试不稳定"或"flaky test"，进而掩盖真实回归
- 本地 `/m` 构建需要重跑才能通过，浪费排查时间
- 353 个 exe 平铺也让"哪个产物属于哪个模块"完全不可读

### 建议

**方案 A（推荐，改动最小）**：关闭 applocal，构建末尾一次性部署。

```cmake
set(VCPKG_APPLOCAL_DEPS OFF)
add_custom_target(deploy_vcpkg_deps ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin"
            "${OUTPUT_DIRECTORY_DEBUG}"
    COMMENT "一次性部署 vcpkg 运行期依赖")
```

**方案 B**：完全不拷贝，调试器环境变量加 PATH。

```cmake
set_target_properties(x PROPERTIES VS_DEBUGGER_ENVIRONMENT
    "PATH=${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin;%PATH%")
```

**方案 C（治本，改动大）**：按模块分子目录输出（`out/Debug/ULRE/`、`out/Debug/examples/`），但这会牵动所有依赖相对路径的脚本，不建议现在做。

---

## 4. 巨型单文件

### 现状

| 文件 | 大小 | 说明 |
|---|---|---|
| `src/ecs/tests/TestCSMIncrementalPass.cpp` | 432 KB | 51 个 case 挤在一个 TU |
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | 267 KB | 7 个 regression group 全在一个 TU |
| `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` | 128 KB | — |
| `inc/hgl/ecs/core/Context.h` | 70 KB | 被大量 TU 包含 |
| `inc/hgl/ecs/support/TransformDataStorage.h` | 42 KB | — |
| `inc/hgl/vk/VertexAttribDataAccess.h` | 41 KB | — |

### 危害

- 单个 432KB TU：改一个字符重编整个文件，增量编译收益归零
- 无法并行编译（一个文件一个 cl.exe 进程）
- MSVC 对超大 TU 的优化会退化（寄存器分配、内联决策）
- 头文件（`Context.h` 70KB）的代价会乘以所有包含它的 TU

### 建议

`TestCSMIncrementalPass` 的 51 个 case 天然可按 case 拆分：保留一个 `main` 做调度，case 分布到 `case_*.cpp`。`ShaderResourceSchemaRegressionGate` 的 7 个 group（glsl / interface / descriptor / cache / materialization / pipeline）也天然可切分。

这项收益明确但工作量最大，建议放到最后。

---

## 5. 零 CI

### 现状

`.github/workflows/` **目录存在但是空的**。

已有资产其实相当充足：

| 项 | 数量 |
|---|---|
| ctest 用例 | 84 |
| shadergen 契约测试 | 13（`ctest -L shadergen`） |
| 构建期校验 | `descriptor_macros_verify`（`ALL` target）、`VerifyVertexLoaderConsistency` |
| 独立契约测试 | `TestCSMIncrementalPass`（51 项）、`TestRenderStrategyTable`（6 项） |

但全部靠人手跑。结果是既有红灯长期挂着无人处理：

- `TexImage.lib` `LNK1104` ×4（`unit_df` / `unit_encoders` / `unit_image` / `TexConvCore`）
- vcpkg `z-applocal` 的并发竞态（见第 3 条）

### 建议（最小闭环）

一个 Windows job 即可：

```yaml
- cmake --build build --config Debug
- ctest --test-dir build -C Debug -L "shadergen;baseline" --output-on-failure
```

先把能绿的固化住，再逐步把 `TexImage.lib` 这类既有红灯转成已知豁免（`set_tests_properties(... DISABLED)` 或 `WILL_FAIL`）而不是沉默。

注意：如果第 3 条不先修，CI 会引入随机红灯，所以**顺序上应先修 3 再上 5**。

---

## 6. submodule 与仓库卫生

### 现状

`.gitmodules` 声明 **19** 个 submodule，实际 gitlink **17** 个（差异来自嵌套 submodule）。其中 **3 个长期处于 dirty 状态**：

| submodule | 脏内容 |
|---|---|
| `res` | `ABeautifulGame.StaticMesh/`、`image/Environments/field.hdr`、`sky_34_2k.zip`、`sky_34_cubemap_2k.7z`、`image/gradient/`、`image/num/`、`image/pbr/` 下多个目录 |
| `CMScript` | `CMScript.sln`、`src/WasmVM/wasm-micro-runtime/` |
| `src/Tools/GLTFConvert` | 嵌套 submodule `meshoptimizer` 未初始化 |

### 危害

`git status` 永远不干净。直接后果：**真正的误入库文件会淹没在噪音里**。

实证：本轮清理要处理的 `comp.spv`（18KB 临时编译输出）、`DartConfiguration.tcl`（CTest 生成）、`bin/PipelineCompiler.exe`（896KB 预编译件），都需要单独 `git ls-files` 逐个确认才能判定——正是因为第一眼看到的"脏"全是 submodule 噪音。

### 建议

1. `res` 是纯资产库（贴图/HDR/模型），不应该走 submodule + 无 LFS 的方式。要么接 Git LFS，要么拆成独立资产仓库按需拉取
2. 本地降噪：`git config status.submoduleSummary false`
3. `.gitmodules` 里补上缺失的嵌套 `meshoptimizer`，或从 `GLTFConvert` 里移除该引用

---

## 7. 根目录构建/临时目录堆积

### 现状

| 目录 | 文件数 | gitignore |
|---|---|---|
| `build/` | 构建目录 | ✅ |
| `out/` | 8 | ✅（与 `build/out` 重复的第二套输出路径） |
| `Cache/` | 309 | ✅ |
| `tmp/` | 98 | ✅ |
| `CMakeFiles/` | 45 | ✅ |
| `Tools/` | 210 | ✅（来源不明） |
| `plans/` | 0（空目录） | ❌ 但 git 不跟踪空目录 |

### 危害

不污染仓库，但污染**任何递归扫描**。

实证：本轮审计时用脚本扫全仓 `CMakeLists.txt` / `*.cmake` 查找未定义变量，被根 `CMakeFiles/` 下的生成脚本（`CMakeCXXCompiler.cmake`、`CTestScript.cmake`）干扰，报出 3 个假的"未定义变量"：`extension`、`lang`、`notes_files`。

同样地，`CMAudio/Plug-Ins/Audio.FluidSynth/FluidLite` 下有**无限嵌套的符号链接**（`example/FluidLite/example/FluidLite/example/...`），任何 `Get-ChildItem -Recurse` 扫到都会卡死或抛 `The name of the file cannot be resolved by the system`。这类问题都源于"构建产物与源码混在同一个树里"。

### 建议

统一到 `build/`，删除 `out/`、`Tools/`、`plans/`；给审计脚本固化排除清单（或写成 `scripts/audit/` 下的工具，把排除规则沉淀下来而不是每次手写）。

---

## 8. `doc/` 无索引、目录失衡

### 现状

| 位置 | 内容 |
|---|---|
| `doc/*.md` | 48 篇 |
| `doc/future/` | 22 篇未实施设计文档 |
| `doc/analysis/ecs-audit-2026-10/` | 6 篇，编号 `01` `02` `03` `06` `07` `08`（**04、05 缺失**） |
| `doc/material/shader-system/` | 空子目录 |
| `doc/material/shadergen/` | 空子目录 |
| `doc/refactor/` | 空 |

**没有 `INDEX.md`，没有任何状态标注。**

编号断档（04、05 缺失）本身就是无索引的直接后果：没人能说清这两篇是被合并了、改名了，还是从未写。

### 危害（已实际发生）

本轮清理差点把 `doc/ambient-provider-types-inventory.md` 当垃圾删掉。它其实是在提交 `93331545f`（"DynamicAmbient 工作收尾"）里入库的、用户明确要求的交付物——但**没有任何索引指向它**，从文件名和目录位置也看不出它和当前主线的关系。最后靠 `git log -- <file>` 才发现。

同样的问题存在于 `doc/future/` 的 22 篇：无法判断哪些已实施、哪些已废弃、哪些仍有效。

### 建议

1. 建 `doc/INDEX.md`，每篇一行，标注 `状态`（已实施 / 已废弃 / 提案中 / 交付物）+ 一句话摘要
2. 删除 3 个空目录（`doc/material/shader-system`、`doc/material/shadergen`、`doc/refactor`）
3. `future/` 里的文档要么标注实施状态，要么归档到 `doc/archive/`
4. 补上或明确注销 `ecs-audit-2026-10` 的 04、05 号

---

## 9. 其他小冗余

### 9.1 `add_cm_library` 的 include 参数重复登记

```cmake
if(include_path)
    target_include_directories(${module_name} PRIVATE ${include_path})   # ← 被下面完全覆盖
    target_include_directories(${module_name} PUBLIC
        $<BUILD_INTERFACE:${include_path}>
        $<INSTALL_INTERFACE:inc>)
endif()
```

`PUBLIC` 已包含 `PRIVATE` 的全部语义，第一行是纯冗余。

### 9.2 `add_subdirectory` 顺序有语义但无注释

`src/CMakeLists.txt` 全文只有 9 行：

```cmake
add_subdirectory(Vulkan)
add_subdirectory(ShaderGen)
add_subdirectory(SceneGraph)
add_subdirectory(InlineGeometry)
add_subdirectory(Work)
add_subdirectory(ecs)
add_subdirectory(Tools)
#add_subdirectory(GUI)
#add_subdirectory(JoltPhysics/Build)
```

这个顺序**不是随意的**：父作用域变量按顺序泄漏（第 1 条证据 C），target 存在性也按顺序成立。但没有一行注释说明，任何人重排都会引入难查的问题。

（根 `CMakeLists.txt:66-67` 关于 CMQT 顺序的注释是正面例子，应当推广。）

### 9.3 各模块手写文件清单已漂移

每个模块的 `CMakeLists.txt` 都手写 `SET(XXX_FILES ...)` + `SOURCE_GROUP(...)`。已经出现漂移：

| 问题 | 状态 |
|---|---|
| `src/ShaderGen/document/DocumentFragmentBuilder.h` 从未登记 | 本轮已补 |
| `src/GUI` 登记了已不存在的 `Layout.h` | 本轮已删 |
| `src/SceneGraph` 的 `VK_GEOMETRY_FILES` 与 `GEOMETRY_CORE_FILES` 内容完全重复 | 本轮已删 |

建议加配置期校验（列出的文件必须存在），比 `file(GLOB ... CONFIGURE_DEPENDS)` 更可控：

```cmake
foreach(f IN LISTS ALL_LISTED_FILES)
    if(NOT EXISTS ${f})
        message(FATAL_ERROR "CMakeLists 登记了不存在的文件: ${f}")
    endif()
endforeach()
```

---

## 附：可复现的验证命令

```powershell
# 1. 输出目录规模
(Get-ChildItem build\out\Windows_64_Debug -Filter *.exe -File).Count   # 353
(Get-ChildItem build\out\Windows_64_Debug -Filter *.dll -File).Count   # 38

# 2. 并行构建竞态（跑 3 次以上，命中的 target 每次不同）
cd build
& $msb ULRE.slnx /p:Configuration=Debug /m /v:m /nologo 2>&1 |
    Select-String ': error ' | Sort-Object -Unique

# 3. 串行构建（应只剩 TexImage.lib LNK1104 ×4）
& $msb ULRE.slnx /p:Configuration=Debug /m:1 /v:m /nologo 2>&1 |
    Select-String ': error ' | Sort-Object -Unique

# 4. 测试基线
ctest --test-dir build -C Debug -L shadergen          # 13/13
& build\out\Windows_64_Debug\TestCSMIncrementalPass.exe   # 51/51（须在仓库根运行）
& build\out\Windows_64_Debug\TestRenderStrategyTable.exe  # 6/6

# 5. 全局 include 的实际暴露面
Select-String -Path CMCMakeModule\use_cm_module.cmake -Pattern 'include_directories'

# 6. 宏无依赖参数
Select-String -Path CMCMakeModule\cm_modules.cmake -Pattern 'macro\(add_cm_library'
```

---

## 附：本轮已实施的相关清理

提交 `c8eb23ce6`（清理：误入库产物、死代码、CMake 冗余与路径逃逸）：

- 删 3 个死源文件（`ChainLink.cpp`、`DistanceField2D.cpp`、`DistanceField3D.cpp`）
- 删 12 个零引用公共头
- 删误入库的 `comp.spv`、`DartConfiguration.tcl`、`bin/PipelineCompiler.exe`，`.gitignore` 加防复发
- SceneGraph：删重复的 `VK_GEOMETRY_FILES`、清 8 个空/泄漏变量
- ShaderGen：补登记 `DocumentFragmentBuilder.h`
- `src/Tools/ShaderGen` 三处 `../../ShaderGen/...` 改为 `ShaderGen/...`
- 删空目录 `src/SceneGraph/env`、`ShaderLibrary/mtl`、`ShaderLibrary/2d`

提交 `e6cda6a25`（修正）：恢复 `${HGL_MATH_LIB}` / `${RENDER_LIBRARY}` 并注明其为隐式传递依赖（见第 2 条的误删记录）。

**均未涉及本文第 1~9 条的根因。**
