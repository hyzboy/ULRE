# 新增顶点语义 SOP（实测清单，2026-08-30）

> 真源：`inc/hgl/mtl/DescriptorResourceCatalog.h` 的 `kDescriptorResourceCatalog`。
> 本清单由「追踪 VertexSize（Line width）语义的全部落点」实测得出，非推断。

## 一、先说结论：S1 改造改变了什么

**没有**把 14 处改动变成 1 处——顶点语义天然横跨 C++ ABI、GLSL 模块、运行期绑定三层，
落点数量由架构决定。S1 实际做到的是两件事：

1. **漏改从「静默出错」变成「编译/CI 失败」**（原先全靠人工纪律）
2. **删掉 11 处纯样板**：8 个 `PushVertexXxx` 包装函数 + 3 处 `SBS_X + int(XBinding::Y)` 手写配对

## 二、完整落点清单（14 项，含保障状态）

| # | 文件 | 改动 | 漏改后果 | 机器保障 |
|---|---|---|---|---|
| 1 | `inc/hgl/common/DescriptorSetTypeDef.h` | `VertexBinding` 加枚举项 | 无绑定号 | — |
| 2 | 同上 | `kDescriptorBindingMacros` 加一行（宏名显式给出） | GLSL 侧无 binding 宏 | ✅ **ctest**（`VerifyDescriptorMacros` 交叉校验） |
| 3 | `inc/hgl/mtl/DescriptorSemantic.h` | `DescriptorSemantic` 项 + `GetDescriptorSemanticLayer` case | layer=Unknown | 运行期 schema 校验 |
| 4 | `inc/hgl/graph/ssbo/SSBOTypes.h` | `SSBOType` 项 + `GetSSBOTypeName` case | 类型名 "Unknown" | — |
| 5 | `inc/hgl/graph/ShaderBufferSources.h` | `SBS_VertexX{set, name, struct}` | 无 buffer/struct 名 | ✅ 编译期（目录断言要求 `sbs != nullptr`） |
| 6 | `inc/hgl/mtl/DescriptorResourceCatalog.h` | **登记一行**（引用 1/3/4/5 的定义） | 描述符全链缺失 | ✅ **编译期**（`VertexBindingsFullyCovered` 位图判定） |
| 7 | `inc/hgl/mtl/GLSLCodeModule.h` | `GLSLCodeModuleSemantic` 项 + 名字（T1 注册表单一真源） | TOML 解析失败 | 运行期显式报错 |
| 8 | `inc/hgl/mtl/MaterialRecipe.h:170` | `GLSLCodeModuleSemantic → VertexSemantic` 映射 | need_ 判定恒 false | — |
| 9 | `src/ShaderGen/3d/DefinitionDescriptorBuilder.h` | `need_X` 标志 + case + `PushVertexResource<DescriptorSemantic::X>` | 描述符不注册 | 部分（模板保证语义已登记） |
| 10 | `src/ShaderGen/MaterialDefinitionRegistry.cpp` | `need_X` + `VertexSemantic::X` case + 模块 include | shader 缺模块 | — |
| 11 | `ShaderLibrary/vertex/s1_x.glsl` | 新模块：`@ulre provide/ssbo` + buffer 声明 + `HGL_X_LOADER` 宏 | — | @ulre 元数据校验 |
| 12 | `ShaderLibrary/vertex/s1_position_{vec2,vec2i,vec3}.glsl` | **各加 `#ifdef HGL_X_LOADER` 展开块（3 处）** | **属性静默为 0**（几何正常、属性全黑/为零——最难查） | ❌ **无保障** |
| 13 | `src/ecs/systems/render/RenderDescriptorBindingSystem.cpp` | 运行期 VAB → SSBO 绑定 | no resource 报错 | 运行期报错 |
| 14 | 数据源写入（如 `src/ecs/support/line/LineRenderPipeline.cpp` 写 Size VAB） | 写 VAB 数据 | 数据全零 | — |
| 15 | 回归门模块计数（`file_count`/`expected_count`） | 计数 +1 | 回归门 FAIL | ✅ ctest |

`ShaderLibrary/common/descriptor_macros.glsl` **不需要手改**——它是 `DescriptorMacroGen` 生成物
（改第 1/2 项后运行 `DescriptorMacroGen --emit > ShaderLibrary/common/descriptor_macros.glsl`）。

## 三、最危险的一步：第 12 项（loader 展开）

`s1_position_*` 用 `#ifdef HGL_X_LOADER` 守卫展开——**漏加即静默跳过**，没有任何编译期或
运行期报错，症状是「几何正确但属性为零」（历史事故：AutoInstance 全黑 = `s1_position_vec2`
漏 `HGL_COLOR_LOADER`）。

### 当前三模块的 loader 覆盖（2026-08-31 实测，已完全对称）

| loader | vec2 | vec2i | vec3 | 定义方 |
|---|---|---|---|---|
| `HGL_UV_LOADER` | ✅ | ✅ | ✅ | s1_uv |
| `HGL_NTB_LOADER` | ✅ | ✅ | ✅ | s1_ntb |
| `HGL_COLOR_LOADER` | ✅ | ✅ | ✅ | s1_color |
| `HGL_LUMINANCE_LOADER` | ✅ | ✅ | ✅ | s1_luminance |
| `HGL_INDEX_LOADER` | ✅ | ✅ | ✅ | s1_index |
| `HGL_SIZE_LOADER` | ✅ | ✅ | ✅ | s1_size |
| `HGL_TRANSFORMID_LOADER` | ✅ | ✅ | ✅ | s1_transform_id |
| `HGL_COLORINDEX_LOADER` | ✅ | ✅ | ✅ | s1_palette_index |

- 三模块各 8 个 loader **完全对称**（选项 A 对称化 69b4dd41e + loader 对称门禁
  `VerifyVertexLoaderConsistency` 守护）。无死残留（`HGL_JOINT_LOADER` 已删，s1_joint 已删）。

## 四、改完必做的验证

```bash
# 1. 目录/宏表自洽（编译期 + ctest）
ctest -R VerifyDescriptorMacros -V

# 2. 若改了第 1/2 项，重新生成 GLSL 宏并确认无意外差异
DescriptorMacroGen --emit > ShaderLibrary/common/descriptor_macros.glsl
git diff ShaderLibrary/common/descriptor_macros.glsl   # 应只含新增宏

# 3. 回归门（模块计数须同步）
ShaderResourceSchemaRegressionGate.exe all             # 统计须合并 stdout+stderr

# 4. 缓存稳定性
rm -rf build/cache-hot/shader-cache && 跑示例 ×2      # 产物 IDENTICAL

# 5. 视觉确认新属性真的到了 shader（第 12 项无机器保障，只能靠这步）
```

## 五、相关提交

- `b45067a8c` 目录循环断言（`RowsUnique` / `VertexBindingsFullyCovered` / `SceneRowsWellFormed`）
  + `VertexBinding` 加 `ENUM_CLASS_RANGE`
- `74b94d3b7` `PushVertexXxx` 8 函数 → `PushVertexResource<SEMANTIC>` 表驱动模板
- `774409b46` `DescriptorMacroGen` 交叉覆盖校验 + `ShaderBuildContext` 3 处配对改查目录
