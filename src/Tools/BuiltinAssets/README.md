# BuiltinAssetExport —— 内置元素（预设）资产导出

**开发期生成器，不进发行包。**

把仓库内置的预设数据（环境、几何、材质…）导出成 `ShaderLibrary/asset/` 下的资产文件：

```
ShaderLibrary/asset/environment/sky34_cube_ibl.env        ← FlatBuffers（运行时读的那份）
ShaderLibrary/asset/environment/sky34_cube_ibl.env.toml   ← TOML（作者侧可 diff / 可手改）
```

## 为什么不放在运行时库里

- 这些预设是**作者侧产物**：引擎只读，从不现场生成。
- 产物甚至可能不进编辑器安装包（取决于发行策略）；生成器代码进运行时库，等于让
  发行产物背着一份永远不会执行的代码。
- 之前它挂在 `hgl::graph::ExportDefaultEnvironmentPresets()`（`src/SceneGraph`）里，
  属于「运行时库反向依赖离线数据」。现在收敛到本工程，未来**所有**内置元素都往这里加。

## 序列化红线

落盘一律走 `hgl::serialize` 统一通路，本工程**不手写** TOML 文本或 FlatBuffers 二进制：

```
AssetObject::Serialize(Archive&)      ← 字段表由 <Group>Serialize.gen.h 提供（Clang AST 生成）
        ↓
hgl::serialize::Value（有序值树）
        ↓
TomlCodec / FlatCodec                 ← 由 ArchiveFile.h 按文件名后缀分派
```

所以这里没有「TOML 一份、FlatBuffers 一份」的两套代码：同一份数据，两次
`SaveAssetObjectToFile()`，区别只在文件名后缀。

## 用法

```powershell
BuiltinAssetExport                         # 默认输出 ShaderLibrary/asset，两种格式都写
BuiltinAssetExport --out ShaderLibrary/asset
BuiltinAssetExport --toml-only             # 只写 .toml（先改文本、再单独出二进制）
BuiltinAssetExport --flat-only
```

工作目录必须是仓库根（VS 调试工作目录已指向 `${ULRE_RUNTIME_PATH}`）。
任何分组导出的数量不等于声明的期望值时以非 0 退出 —— 可直接用作 CI 门禁。

## 加一个新分组

1. 与本文件同级新增 `XxxPresets.h/.cpp`，导出 `int ExportXxxPresets(hgl::graph::builtin::PresetWriter&)`；
   每个预设就是一次 `EnvironmentSpec` / `GeometrySpec` / … 赋值 + 一次 `writer.Save()`。
2. 在 `EnvironmentPresets.h` 里同风格声明 `inline constexpr int kXxxPresetCount = N;`。
3. 在 `BuiltinAssetExport.cpp` 的 `kGroups` 里加一行：

```cpp
constexpr PresetGroup kGroups[] =
{
    { "environment", hgl::graph::builtin::kEnvironmentPresetCount,
      &hgl::graph::builtin::ExportEnvironmentPresets },
    { "geometry",    hgl::graph::builtin::kGeometryPresetCount,
      &hgl::graph::builtin::ExportGeometryPresets },
};
```

`main()` 不需要改。

## 门禁

`TestEnvironmentCodec` 的第 9 节校验仓库里那 5 份环境预设「TOML 与 FlatBuffers 同源、
字段一致、能反序列化」。改了预设数据后必须重跑本工具，否则该测试会指出产物与声明不符。
