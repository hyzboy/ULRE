# AssetGen 资产代码重新生成与维护指南

本文档介绍在 ULRE 引擎中，当资产对象结构（C++ 头文件）、序列化字段或属性注解发生变更时，如何重新生成派生序列化代码与元数据。

---

## 1. 核心原则

> **C++ 头文件中的结构体声明是唯一真源（Single Source of Truth）。**

所有派生产物（字段遍历、编解码描述表、属性元数据、FlatBuffers schema 及 flatc 头文件）均由 `asset_gen.py` 根据 Clang AST 自动化提取并生成，**严禁手动编辑生成的代码文件**。

### 派生产物一览

| 产物 | 生成路径 | 用途 |
|---|---|---|
| **唯一字段遍历** | `inc/hgl/asset/generated/<Group>Serialize.gen.h` | 声明顺序遍历所有字段，与具体序列化格式无关 |
| **编解码门面 & 描述表** | `inc/hgl/asset/generated/<Group>Codec.gen.h` | FlatBuffers `TableDesc` 纯数据表及 TOML/FBS 统一读写门面 |
| **属性元数据表** | `inc/hgl/asset/generated/<Group>Meta.gen.h` | 供编辑器属性检查器（Inspector GUI）、文档导出使用 |
| **FlatBuffers 契约** | `inc/hgl/asset/generated/<group>_schema.fbs` | 发行版二进制资产契约 |
| **flatc 派生 C++ 头** | `inc/hgl/asset/generated/<group>_schema_generated.h` | `flatc` 编译得到的底层二进制结构代码 |

---

## 2. 什么时候需要重新生成？

遇到以下任一修改时，必须重新运行代码生成：

1. **增删改字段**：修改了带有 `HGL_ASSET_STRUCT` 或 `HGL_ASSET_CLASS` 标记的结构体成员（添加新成员、删除老成员、改变成员类型、修改类内默认值）。
2. **新增资产结构**：在现有头文件或新头文件中声明了新的资产对象，并在 `src/Tools/AssetGen/asset_gen.toml` 中配置了扫描路径。
3. **修改属性注解**：调整了字段上的 `HGL_PROPERTY(...)` 标签（如调整 `Range`、修改 `Tooltip`、添加 `Transient` 或 `EditOnly`）。
4. **修改枚举映射**：在 `asset_gen.toml` 中添加或修改了 `[[enum]]` 的 TOML 别名拼写。
5. **CI 门禁告警**：CI 提示 `check-asset-gen.ps1` 失败，表明代码与生成头文件不同步。

---

## 3. 环境与依赖准备

执行代码生成需要以下开发工具环境：

1. **Python 3.11+**：必须包含标准库 `tomllib`（若使用 Python 3.10 及以下需安装 `pip install tomli`）。
2. **LLVM Clang (`clang++`)**：用于解析 C++ 结构体并提取 AST。
   - 默认推荐安装于 `C:\Program Files\LLVM\bin\clang++.exe`。
3. **FlatBuffers 编译器 (`flatc`)**：用于将 `.fbs` 编译为 C++ 头文件。
   - 通常位于 vcpkg 目录：`D:\vcpkg\installed\x64-windows\tools\flatbuffers\flatc.exe`。
4. **第三方 Include 头文件**：
   - glm、flatbuffers：位于 vcpkg 头文件目录（如 `D:\vcpkg\installed\x64-windows\include`）。
   - Vulkan SDK：位于 `%VULKAN_SDK%\Include`。

> 💡 **提示**：仓库提供的批处理和 PowerShell 脚本已配置智能路径探测，只要工具安装在标准路径或配置了环境变量，通常无需手动配置任何路径。

### 支持的环境变量（可选配置）

| 环境变量 | 说明 |
|---|---|
| `HGL_CLANG` | 指定 `clang++.exe` 的绝对路径 |
| `HGL_FLATC` | 指定 `flatc.exe` 的绝对路径 |
| `HGL_ASSETGEN_INCLUDE_DIRS` | 额外的 include 目录（多个路径用分号 `;` 分隔） |
| `VCPKG_ROOT` | vcpkg 安装根目录（用于自动定位 flatc 和 include） |
| `VULKAN_SDK` | Vulkan SDK 根目录 |

---

## 4. 重新运行代码生成

### 方式一：一键脚本（推荐）

在仓库根目录下，我们提供了开箱即用的一键脚本，自动完成路径探测、工具链自测和代码落盘：

#### 1. Windows 命令提示符 (CMD) 或直接双击
```cmd
:: 默认执行：单元测试 + 生成代码并落盘写入 (--write)
run_assetgen.cmd

:: 仅做一致性校验，不写入文件（相当于 CI --check 模式）
run_assetgen.cmd -Check

:: 仅更新指定分组（例如调试 Geometry）
run_assetgen.cmd -Only Geometry
```

#### 2. PowerShell 终端
```powershell
# 重新生成并覆盖目标文件
.\scripts\run-assetgen.ps1

# 比对校验模式
.\scripts\run-assetgen.ps1 -Check

# 指定单个分组
.\scripts\run-assetgen.ps1 -Only Geometry
```

---

### 方式二：手动执行 Python 命令

如果需要显式指定工具链路径或在自定义脚本中集成：

```powershell
python src/Tools/AssetGen/asset_gen.py --repo-root . `
  --clang "C:\Program Files\LLVM\bin\clang++.exe" `
  --flatc "D:\vcpkg\installed\x64-windows\tools\flatbuffers\flatc.exe" `
  --include-dir "D:\vcpkg\installed\x64-windows\include" `
  --include-dir "D:\VulkanSDK\1.4.363.0\Include" `
  --write
```

常用参数说明：
- `--write`：将生成的代码覆盖写回文件（默认如果不带 `--write` 和 `--check` 只会打印摘要）。
- `--check`：比对生成内容与已有文件，如有差异返回退出码 1，不修改硬盘文件。
- `--only <GroupName>`：仅处理特定的分组名称（对应 `asset_gen.toml` 中的 `[group.<Name>]`）。
- `--no-flatc`：仅更新 `.fbs` 与 C++ 序列化头，跳过调用 `flatc` 编译。

---

## 5. 生成后的验证闭环

完成代码重新生成后，按以下步骤完成验证闭环：

### 步骤 1：确认生成器单元测试通过
一键脚本运行过程中会自动执行单元测试：
```powershell
python src/Tools/AssetGen/test_asset_gen.py
```
应输出 `OK`。

### 步骤 2：运行序列化与资产相关测试
使用 CTest 或 Visual Studio 编译并运行相关测试：
```powershell
ctest -C Debug -R "TestSerializeArchive|TestAssetReflection|TestSceneFile|TestGeometryAssetFile" --output-on-failure
```
确保全内存往返测试、TOML 编解码测试及二进制反序列化测试全部通过。

### 步骤 3：查看 Git 变更并提交
在终端检查变更内容：
```powershell
git status
git diff inc/hgl/asset/generated/
```
确认只有预期修改的字段和描述表发生了变动，然后进行 Git 提交。

---

## 6. 常见问题与排查

### 1. 报错 `找不到 clang++` 或 `找不到 flatc`
- **原因**：工具未安装在默认路径，且未加入 `PATH` 或环境变量。
- **解决**：在命令中添加 `-Clang "你的路径\clang++.exe" -Flatc "你的路径\flatc.exe"`，或配置环境变量 `HGL_CLANG` / `HGL_FLATC`。

### 2. 报错 `'glm/glm.hpp' file not found`
- **原因**：Clang 解析头文件时找不到第三方库头文件。
- **解决**：传入 `-IncludeDir "D:\vcpkg\installed\x64-windows\include"` 或设置环境变量 `HGL_ASSETGEN_INCLUDE_DIRS`。

### 3. 报错 `不支持的类型: ...`
- **原因**：在资产结构中使用了未经支持的 STL 容器（如 `std::vector`）或未注册的复杂自定义类型。
- **解决**：遵循引擎规范，使用 `hgl::ValueArray<T>`、`hgl::ManagedArray<T>` 等 HGL 自有容器，或在 `inc/hgl/serialize/Traits.h` 中为新基础类型实现特化。
