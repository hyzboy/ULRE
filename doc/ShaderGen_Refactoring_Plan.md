# ShaderGen 模块分析与重构计划

## 一、模块概述

ShaderGen 是 ULRE 渲染引擎中的着色器生成模块，负责动态生成 GLSL 着色器代码并编译为 SPIR-V 字节码。该模块为 Vulkan 渲染管线提供了灵活的材质系统和着色器管理机制。

### 核心文件统计
- **总文件数**: 51 个 C++ 源文件和头文件
- **代码行数**: 约 5,799 行
- **主要目录结构**:
  - `/src/ShaderGen/` - 实现文件
  - `/inc/hgl/shadergen/` - 公共头文件
  - `/src/ShaderGen/2d/` - 2D 材质实现
  - `/src/ShaderGen/3d/` - 3D 材质实现
  - `/src/ShaderGen/common/` - 通用着色器函数

## 二、架构分析

### 2.1 核心组件

#### 1. ShaderCreateInfo 体系
**职责**: 管理单个着色器阶段（Vertex/Geometry/Fragment）的创建
- `ShaderCreateInfo` - 基类，包含通用着色器生成逻辑
- `ShaderCreateInfoVertex` - 顶点着色器创建
- `ShaderCreateInfoGeometry` - 几何着色器创建
- `ShaderCreateInfoFragment` - 片段着色器创建

#### 2. MaterialCreateInfo
**职责**: 协调完整材质的多个着色器阶段创建
- 管理着色器之间的输入输出连接
- 统一管理材质实例（Material Instance）
- 处理 LocalToWorld 变换矩阵

#### 3. MaterialDescriptorInfo
**职责**: 管理着色器描述符资源（UBO、纹理、采样器）
- 自动分配 set 和 binding 编号
- 追踪跨着色器阶段的资源使用
- 维护结构体定义的中央存储

#### 4. ShaderDescriptorInfo
**职责**: 管理单个着色器阶段的描述符信息
- 输入/输出变量管理
- 每阶段的资源列表

#### 5. GLSLCompiler
**职责**: GLSL 到 SPIR-V 的编译封装
- 动态加载外部编译器模块（GLSLCompiler.dll）
- 处理编译错误和日志
- 管理 SPIR-V 版本兼容性

#### 6. StdMaterial 体系
**职责**: 提供标准材质创建框架
- `StdMaterial` - 抽象基类
- `Std2DMaterial` - 2D 材质基类
- `Std3DMaterial` - 3D 材质基类
- 具体材质实现（如 `M_BasicLit`, `M_PureColor2D`）

### 2.2 工作流程

```
1. 创建 MaterialCreateInfo (配置材质参数)
   ↓
2. 为各着色器阶段创建 ShaderCreateInfo 对象
   ↓
3. 添加输入/输出变量、UBO、纹理等资源
   ↓
4. 添加用户自定义代码和函数
   ↓
5. MaterialDescriptorInfo 统一分配 set/binding
   ↓
6. 每个着色器阶段生成完整 GLSL 代码
   ↓
7. 调用 GLSLCompiler 编译为 SPIR-V
   ↓
8. 返回可用于 Vulkan 管线的着色器数据
```

## 三、优点评价

### 3.1 设计优点

1. **高度模块化**
   - 清晰的职责分离（着色器生成、描述符管理、编译）
   - 各组件独立性强，易于单独测试和维护

2. **灵活的材质系统**
   - 支持材质实例（Material Instance），可高效批处理
   - 支持 2D 和 3D 材质的不同需求
   - 易于扩展新材质类型

3. **自动化资源管理**
   - 自动分配描述符 set/binding 编号
   - 自动管理着色器阶段间的输入输出连接
   - 减少人工配置错误

4. **代码复用**
   - 公共着色器函数库（MFCommon.h, MFGetPosition.h 等）
   - 基于继承的材质类层次结构
   - 标准化的 UBO 定义（UBOCommon）

5. **运行时动态生成**
   - 根据配置动态组合着色器代码
   - 支持条件编译（#define）
   - 可根据硬件能力调整

### 3.2 实现优点

1. **良好的日志支持**
   - DEBUG 模式下输出生成的着色器源码
   - 编译错误详细报告

2. **外部编译器隔离**
   - 通过插件形式加载编译器
   - 易于更新或替换编译器实现

3. **标准化命名**
   - 一致的函数命名约定（如 GetPosition3D, GetNormal）
   - 清晰的变量命名（mi, l2w, camera 等）

## 四、缺点与问题

### 4.1 架构问题

1. **职责划分不清晰**
   - `ShaderCreateInfo` 同时负责代码生成和编译，违反单一职责原则
   - `MaterialCreateInfo` 承担了过多职责（配置、协调、资源管理）
   - `MaterialDescriptorInfo` 和 `ShaderDescriptorInfo` 的职责重叠

2. **紧耦合**
   - `ShaderCreateInfo` 与 `MaterialDescriptorInfo` 紧密耦合
   - 着色器生成逻辑硬编码在 `CreateShader()` 方法中
   - 难以独立测试各组件

3. **缺乏抽象层次**
   - GLSL 代码生成逻辑散落在多个类中
   - 没有统一的代码生成器接口
   - 字符串拼接方式生成代码，易出错且难维护

4. **扩展性受限**
   - 新增着色器阶段（如 Task/Mesh Shader）需要大量修改
   - 支持 HLSL 或其他着色器语言困难
   - 材质变体（Variant）管理缺失

### 4.2 代码质量问题

1. **字符串处理低效**
   - 大量使用字符串拼接操作
   - 临时字符串对象频繁创建和销毁
   - 未使用 string view 等现代 C++ 特性

2. **错误处理不完善**
   - 多处使用简单的布尔返回值
   - 错误信息不够详细
   - 缺乏异常安全保证

3. **缺少文档**
   - 类和方法缺少详细注释
   - 没有使用示例和最佳实践文档
   - 复杂逻辑缺少解释

4. **硬编码问题**
   - 着色器模板代码硬编码在字符串常量中
   - 版本号等配置硬编码（如 `#version 460 core`）
   - 魔法数字较多

5. **测试缺失**
   - 没有单元测试
   - 没有集成测试
   - 难以验证重构的正确性

### 4.3 性能问题

1. **编译时开销**
   - 每次创建材质都需要动态生成和编译着色器
   - 缺少着色器缓存机制
   - 没有并行编译支持

2. **内存管理**
   - 频繁的内存分配和释放
   - 没有使用对象池
   - 临时对象生命周期管理不佳

### 4.4 维护性问题

1. **代码重复**
   - 类似的代码模式在多个着色器阶段重复
   - 描述符添加逻辑重复
   - 输入输出处理逻辑相似

2. **命名不一致**
   - 部分使用缩写（mdi, sdi, vsc）
   - 部分使用全名
   - 匈牙利命名法使用不一致

3. **依赖管理**
   - 头文件依赖关系复杂
   - 循环依赖风险
   - 编译时间较长

## 五、重构计划

### 5.1 总体原则

1. **渐进式重构**: 每一步都保证代码可编译运行
2. **保持兼容**: 保留原有 API 直到新 API 稳定
3. **测试驱动**: 先补充测试，再进行重构
4. **文档同步**: 重构的同时更新文档

### 5.2 重构优先级

- **P0 (Critical)**: 必须立即解决的问题，影响功能正确性
- **P1 (High)**: 重要问题，影响代码质量和可维护性
- **P2 (Medium)**: 改善问题，提升用户体验
- **P3 (Low)**: 优化问题，提升性能

## 六、详细重构步骤

### 第一阶段：基础设施建设（P1）

#### Step 1: 添加测试基础设施
**目标**: 建立测试框架，为后续重构提供安全网

**具体任务**:
1. 引入测试框架（建议使用已有的测试工具）
2. 创建测试工具类（着色器代码比对、SPIR-V 验证）
3. 为现有关键功能添加回归测试
   - 测试 PureColor2D 材质生成
   - 测试 BasicLit 材质生成
   - 测试描述符分配逻辑

**验证标准**:
- 测试框架可以运行
- 至少有 5 个基本测试通过
- 可以捕获着色器编译错误

**影响范围**: 新增测试文件，不修改现有代码

---

#### Step 2: 改善日志和错误处理
**目标**: 增强调试能力和错误诊断

**具体任务**:
1. 在 `ShaderCreateInfo` 中添加详细的错误消息
   ```cpp
   // 从简单的 bool 返回值
   bool ProcDefine();
   
   // 改为详细的错误信息
   struct CompileResult {
       bool success;
       std::string error_message;
       int error_line;
   };
   CompileResult ProcDefine();
   ```

2. 在 `GLSLCompiler` 中改进错误报告
   - 包含源码行号
   - 包含完整错误上下文

3. 添加编译过程各阶段的日志输出

**验证标准**:
- 所有原有测试仍然通过
- 故意制造的错误能输出清晰的错误信息
- 日志等级可配置

**影响范围**: 修改 `ShaderCreateInfo`, `GLSLCompiler`，保持 API 兼容

---

#### Step 3: 提取常量和配置
**目标**: 消除硬编码，提高可配置性

**具体任务**:
1. 创建 `ShaderGenConfig.h` 配置类
   ```cpp
   struct ShaderGenConfig {
       std::string glsl_version = "460 core";
       uint32_t max_material_instances = 65535;
       bool enable_debug_info = true;
       // ...
   };
   ```

2. 将 `ShaderCreateInfo` 中的硬编码字符串移到配置
   - GLSL 版本号
   - 着色器阶段宏定义
   - 最大数组大小

3. 提取魔法数字为命名常量
   ```cpp
   constexpr size_t DESCRIPTOR_NAME_MAX_LENGTH = 64;
   constexpr uint32_t MI_DEFAULT_MAX_COUNT = 65535;
   ```

**验证标准**:
- 所有测试通过
- 可以通过配置改变 GLSL 版本
- 没有新的硬编码引入

**影响范围**: 新增 `ShaderGenConfig.h`，修改 `ShaderCreateInfo`

---

### 第二阶段：代码生成重构（P1）

#### Step 4: 引入代码生成器抽象
**目标**: 将字符串拼接逻辑封装，提高可测试性

**具体任务**:
1. 创建 `ShaderCodeBuilder` 类
   ```cpp
   class ShaderCodeBuilder {
   public:
       void addVersion(const std::string& version);
       void addDefine(const std::string& name, const std::string& value);
       void addStruct(const std::string& name, const std::string& body);
       void addUBO(const UBODescriptor& desc);
       void addFunction(const std::string& code);
       void addMainFunction(const std::string& code);
       
       std::string build() const;
   };
   ```

2. 在新类中实现原有的代码拼接逻辑
3. 添加单元测试验证代码生成器

**验证标准**:
- `ShaderCodeBuilder` 可以独立测试
- 生成的代码与原有方法一致
- 性能没有明显下降

**影响范围**: 新增 `ShaderCodeBuilder` 类，暂不修改现有调用

---

#### Step 5: 重构 ShaderCreateInfo 使用 CodeBuilder
**目标**: 用新的代码生成器替换直接字符串拼接

**具体任务**:
1. 修改 `ShaderCreateInfo::CreateShader()` 使用 `ShaderCodeBuilder`
2. 重构 `ProcDefine()`, `ProcUBO()` 等方法
   ```cpp
   // 从
   bool ShaderCreateInfo::ProcUBO() {
       final_shader += "\nlayout(...";
       // ...
   }
   
   // 改为
   bool ShaderCreateInfo::ProcUBO(ShaderCodeBuilder& builder) {
       for (auto& ubo : ubo_list) {
           builder.addUBO(*ubo);
       }
       return true;
   }
   ```

3. 保持原有接口不变，内部使用新实现

**验证标准**:
- 所有测试通过
- 生成的着色器代码一致
- 代码可读性提升

**影响范围**: 修改 `ShaderCreateInfo` 实现，不改变公共接口

---

#### Step 6: 优化字符串处理性能
**目标**: 减少临时字符串对象，提高性能

**具体任务**:
1. 在 `ShaderCodeBuilder` 中使用 `std::string_view` 处理常量字符串
2. 预分配字符串缓冲区大小
3. 使用 `fmt` 库或类似工具进行高效格式化
4. 减少不必要的字符串拷贝

**验证标准**:
- 性能测试显示代码生成速度提升 20%+
- 内存分配次数减少
- 所有功能测试通过

**影响范围**: 修改 `ShaderCodeBuilder` 内部实现

---

### 第三阶段：描述符管理重构（P1）

#### Step 7: 明确 MaterialDescriptorInfo 和 ShaderDescriptorInfo 职责
**目标**: 解决职责重叠问题

**具体任务**:
1. 分析两个类的使用场景
2. 重新定义职责：
   - `MaterialDescriptorInfo`: 全局资源注册和 set/binding 分配
   - `ShaderDescriptorInfo`: 单个着色器阶段的资源引用

3. 添加清晰的文档说明职责划分
4. 重构重复代码

**验证标准**:
- 职责边界清晰，无重叠
- 文档完善
- 测试覆盖新的边界

**影响范围**: 文档更新，少量代码重构

---

#### Step 8: 改进描述符添加 API
**目标**: 简化资源添加流程，减少出错

**具体任务**:
1. 创建流式（Fluent）API
   ```cpp
   // 从
   AddUBO(stage, type, struct_name, name);
   AddTexture(stage, type, texture_type, name);
   
   // 改为
   material.descriptors()
       .addUBO("camera", ShaderStage::AllGraphics)
       .addTexture("baseColor", ShaderStage::Fragment)
       .build();
   ```

2. 添加类型安全的 builder 模式
3. 提供批量添加的便捷方法

**验证标准**:
- 新 API 更易用
- 向后兼容保留旧 API
- 减少样板代码

**影响范围**: 新增 API，标记旧 API 为 deprecated

---

#### Step 9: 实现资源验证机制
**目标**: 在编译前检测资源配置错误

**具体任务**:
1. 添加 `MaterialDescriptorInfo::validate()` 方法
2. 检查常见错误：
   - 重复的资源名称
   - 缺失的结构体定义
   - set/binding 冲突
   - 着色器阶段不匹配

3. 在 `CreateShader()` 前调用验证

**验证标准**:
- 能捕获所有常见配置错误
- 错误信息清晰准确
- 不增加显著开销

**影响范围**: 修改 `MaterialDescriptorInfo`, `MaterialCreateInfo`

---

### 第四阶段：架构解耦（P1）

#### Step 10: 分离着色器生成和编译
**目标**: 降低 ShaderCreateInfo 的职责

**具体任务**:
1. 将编译逻辑从 `ShaderCreateInfo` 移出
2. 创建 `ShaderCompiler` 接口
   ```cpp
   class IShaderCompiler {
   public:
       virtual SPVData* compile(
           ShaderStage stage,
           const std::string& source
       ) = 0;
   };
   ```

3. `GLSLCompiler` 实现该接口
4. `ShaderCreateInfo` 通过接口调用编译器

**验证标准**:
- 可以轻松替换编译器实现
- 单元测试可以使用 mock 编译器
- 所有功能测试通过

**影响范围**: 新增接口，修改 `ShaderCreateInfo` 和 `GLSLCompiler`

---

#### Step 11: 重构 MaterialCreateInfo 职责
**目标**: 分离配置、构建和管理职责

**具体任务**:
1. 创建 `MaterialPipelineBuilder` 负责构建流程
   ```cpp
   class MaterialPipelineBuilder {
       MaterialConfig config_;
       MaterialDescriptorInfo descriptors_;
       std::map<ShaderStage, ShaderCreateInfo*> shaders_;
       
   public:
       MaterialPipelineBuilder& configure(const MaterialConfig& config);
       MaterialPipelineBuilder& addShaderStage(ShaderStage stage);
       MaterialPipelineBuilder& addDescriptor(...);
       MaterialCreateInfo* build();
   };
   ```

2. `MaterialCreateInfo` 变为纯数据类
3. 迁移构建逻辑到 Builder

**验证标准**:
- 职责清晰，单一职责
- 构建流程更灵活
- API 更易理解

**影响范围**: 大范围重构，但保持兼容层

---

#### Step 12: 引入依赖注入
**目标**: 解除硬编码依赖，提高可测试性

**具体任务**:
1. 创建依赖容器
   ```cpp
   struct ShaderGenDependencies {
       IShaderCompiler* compiler;
       ShaderGenConfig* config;
       ILogger* logger;
   };
   ```

2. 通过构造函数注入依赖
3. 移除全局静态依赖

**验证标准**:
- 组件可以独立实例化
- 测试可以注入 mock 对象
- 多实例不会互相干扰

**影响范围**: 修改大部分类的构造函数

---

### 第五阶段：扩展性改进（P2）

#### Step 13: 设计着色器模板系统
**目标**: 支持复杂着色器的模块化组合

**具体任务**:
1. 创建 `ShaderTemplate` 类
   ```cpp
   class ShaderTemplate {
   public:
       void addCodeBlock(const std::string& name, const std::string& code);
       void addParameter(const std::string& name, const std::string& type);
       std::string instantiate(const ParameterMap& params);
   };
   ```

2. 将常用着色器片段提取为模板
3. 支持模板继承和组合

**验证标准**:
- 可以创建自定义材质模板
- 减少代码重复
- 新材质创建更简单

**影响范围**: 新增模板系统，逐步迁移现有材质

---

#### Step 14: 实现着色器缓存机制
**目标**: 避免重复编译，提高性能

**具体任务**:
1. 创建 `ShaderCache` 类
   ```cpp
   class ShaderCache {
   public:
       SPVData* get(const std::string& source_hash);
       void put(const std::string& source_hash, SPVData* spv);
       void saveToFile(const std::string& path);
       void loadFromFile(const std::string& path);
   };
   ```

2. 计算着色器源码哈希值作为缓存键
3. 支持磁盘持久化

**验证标准**:
- 重复材质创建无需重新编译
- 应用启动时可以加载缓存
- 缓存命中率 > 90%

**影响范围**: 新增缓存模块，修改编译流程

---

#### Step 15: 支持材质变体（Variant）
**目标**: 高效管理同一材质的不同配置

**具体任务**:
1. 设计变体系统
   ```cpp
   class MaterialVariant {
       std::string base_material;
       std::map<std::string, std::string> defines;
       std::map<std::string, bool> features;
       
   public:
       MaterialVariant withDefine(const std::string& name, const std::string& value);
       MaterialVariant withFeature(const std::string& name, bool enabled);
   };
   ```

2. 支持条件编译分支
3. 共享公共代码段

**验证标准**:
- 可以轻松创建材质变体
- 变体编译高效
- 内存占用合理

**影响范围**: 新增变体系统，扩展现有材质类

---

### 第六阶段：性能优化（P2）

#### Step 16: 优化内存分配
**目标**: 减少内存分配和碎片

**具体任务**:
1. 引入对象池管理临时对象
   ```cpp
   class StringPool {
       std::vector<std::string> pool_;
   public:
       std::string* acquire();
       void release(std::string* str);
   };
   ```

2. 使用 arena allocator 管理着色器生成过程的内存
3. 减少小对象分配

**验证标准**:
- 内存分配次数减少 50%+
- 性能测试显示改善
- 无内存泄漏

**影响范围**: 内部实现优化，不影响 API

---

#### Step 17: 并行编译支持
**目标**: 利用多核加速材质创建

**具体任务**:
1. 识别可并行的编译任务
   - 独立的着色器阶段
   - 不同的材质

2. 使用线程池并行编译
   ```cpp
   class ParallelShaderCompiler {
       ThreadPool pool_;
   public:
       std::vector<std::future<SPVData*>> compileAsync(
           const std::vector<ShaderSource>& sources
       );
   };
   ```

3. 确保线程安全

**验证标准**:
- 多材质编译速度显著提升
- 无竞态条件
- 支持取消编译任务

**影响范围**: 修改编译流程，添加并发控制

---

#### Step 18: 代码生成性能优化
**目标**: 进一步提升代码生成速度

**具体任务**:
1. 性能分析，找出瓶颈
2. 优化热点路径
   - 字符串格式化
   - 容器操作
   - 循环逻辑

3. 添加性能基准测试

**验证标准**:
- 关键路径速度提升 30%+
- 性能回归测试通过
- 内存使用稳定

**影响范围**: 内部实现优化

---

### 第七阶段：文档和工具（P3）

#### Step 19: 完善 API 文档
**目标**: 降低使用门槛

**具体任务**:
1. 为所有公共类和方法添加 Doxygen 注释
2. 创建使用教程
   - 快速入门
   - 创建自定义材质
   - 高级功能

3. 生成 API 文档网站
4. 添加代码示例

**验证标准**:
- 文档覆盖率 > 90%
- 新用户可以按照教程成功创建材质
- 示例代码可运行

**影响范围**: 仅文档，不修改代码

---

#### Step 20: 开发调试工具
**目标**: 改善开发和调试体验

**具体任务**:
1. 创建着色器可视化工具
   - 显示生成的 GLSL 代码
   - 高亮语法错误
   - 显示资源绑定信息

2. 材质编辑器原型
   - 图形化配置材质参数
   - 实时预览

3. 性能分析工具
   - 编译时间统计
   - 内存使用分析

**验证标准**:
- 工具可用且稳定
- 显著改善调试效率
- 用户反馈良好

**影响范围**: 新增工具项目

---

### 第八阶段：高级特性（P3）

#### Step 21: 支持更多着色器阶段
**目标**: 支持现代 GPU 特性

**具体任务**:
1. 添加 Mesh Shader 支持
2. 添加 Task Shader 支持
3. 改进 Compute Shader 支持
4. 确保架构可扩展

**验证标准**:
- 新阶段完全集成
- 示例运行正常
- 不破坏现有功能

**影响范围**: 扩展着色器阶段枚举和相关类

---

#### Step 22: 跨语言支持探索
**目标**: 为支持 HLSL 等做准备

**具体任务**:
1. 抽象着色器语言接口
   ```cpp
   class IShaderLanguage {
   public:
       virtual std::string generateVersion() = 0;
       virtual std::string generateUBO(...) = 0;
       // ...
   };
   ```

2. 实现 GLSL 生成器
3. 原型 HLSL 生成器

**验证标准**:
- 可以切换着色器语言
- GLSL 生成器完全功能
- HLSL 基本功能可用

**影响范围**: 大范围架构调整

---

#### Step 23: 材质继承和组合
**目标**: 支持更复杂的材质关系

**具体任务**:
1. 设计材质继承机制
   ```cpp
   class DerivedMaterial : public BaseMaterial {
   public:
       DerivedMaterial() : BaseMaterial() {
           overrideFragmentShader(...);
           addNewFeature(...);
       }
   };
   ```

2. 支持材质功能组合（Mixin）
3. 自动合并着色器代码

**验证标准**:
- 可以创建材质层次结构
- 功能可以灵活组合
- 无重复代码

**影响范围**: 扩展材质类体系

---

#### Step 24: 着色器优化管道
**目标**: 生成更高效的着色器代码

**具体任务**:
1. 集成 SPIRV-Tools 优化器
2. 死代码消除
3. 常量折叠
4. 循环展开等优化

**验证标准**:
- 生成的 SPIR-V 更小更快
- 渲染性能提升
- 优化不影响正确性

**影响范围**: 添加优化管道，可选启用

---

#### Step 25: 持续集成和自动化测试
**目标**: 保证重构后代码质量

**具体任务**:
1. 建立 CI/CD 流程
   - 自动编译
   - 自动运行测试
   - 代码覆盖率报告

2. 添加性能回归测试
3. 自动生成测试报告

**验证标准**:
- CI 流程稳定运行
- 每次提交都自动测试
- 覆盖率 > 70%

**影响范围**: 基础设施，不影响代码

---

## 七、重构时间估算

假设每个 Step 为 1-3 个工作日：

| 阶段 | Steps | 估计时间 | 累计时间 |
|------|-------|---------|---------|
| 第一阶段：基础设施 | 1-3 | 5-8 天 | 1-2 周 |
| 第二阶段：代码生成 | 4-6 | 6-9 天 | 3-4 周 |
| 第三阶段：描述符管理 | 7-9 | 5-7 天 | 4-5 周 |
| 第四阶段：架构解耦 | 10-12 | 7-10 天 | 6-8 周 |
| 第五阶段：扩展性 | 13-15 | 6-9 天 | 8-10 周 |
| 第六阶段：性能优化 | 16-18 | 5-8 天 | 10-12 周 |
| 第七阶段：文档工具 | 19-20 | 4-6 天 | 12-14 周 |
| 第八阶段：高级特性 | 21-25 | 10-15 天 | 14-18 周 |

**总计**: 约 3.5-4.5 个月（单人全职），或 1.5-2 个月（小团队）

## 八、风险评估

### 8.1 技术风险

1. **兼容性风险** (中)
   - 新旧 API 共存期可能出现混淆
   - **缓解**: 清晰的迁移指南，废弃 API 警告

2. **性能风险** (低)
   - 重构可能引入性能回归
   - **缓解**: 每步都进行性能基准测试

3. **破坏性变更** (中)
   - 大规模重构可能引入 bug
   - **缓解**: 完善的测试覆盖，渐进式重构

### 8.2 项目风险

1. **时间超支** (中)
   - 实际工作量可能超出估算
   - **缓解**: 按优先级执行，允许分阶段完成

2. **资源不足** (高)
   - 可能缺乏足够的开发时间
   - **缓解**: 专注于 P0/P1 任务

3. **范围蔓延** (中)
   - 重构过程中发现新问题
   - **缓解**: 严格控制范围，新问题记录为后续任务

## 九、成功标准

### 9.1 短期目标（3 个月内）

- [ ] 完成第一至第四阶段重构
- [ ] 代码可读性和可维护性显著提升
- [ ] 新材质创建更简单（代码量减少 30%）
- [ ] 所有现有功能保持正常工作
- [ ] 测试覆盖率达到 60%+

### 9.2 中期目标（6 个月内）

- [ ] 完成第五至第七阶段重构
- [ ] 支持着色器缓存和变体
- [ ] 性能提升 30%+
- [ ] 完善的文档和示例
- [ ] 测试覆盖率达到 75%+

### 9.3 长期目标（1 年内）

- [ ] 完成所有重构任务
- [ ] 支持多种着色器语言
- [ ] 材质系统完全模块化
- [ ] 优秀的开发者体验
- [ ] 成为 ULRE 的稳定核心模块

## 十、总结

ShaderGen 模块是 ULRE 的核心组件，具有良好的设计基础，但在架构清晰度、可扩展性和性能方面仍有改进空间。通过本重构计划，我们将：

1. **保持优点**: 保留模块化设计、灵活的材质系统等优势
2. **解决问题**: 改善职责划分、降低耦合、提升性能
3. **渐进实施**: 每一步都可编译运行，降低风险
4. **提升质量**: 增加测试、完善文档、改善工具

最终目标是打造一个**高性能、易扩展、开发友好**的着色器生成系统，为 ULRE 的长期发展奠定坚实基础。

---

**文档版本**: 1.0  
**创建日期**: 2025-12-03  
**最后更新**: 2025-12-03  
**作者**: ULRE 开发团队
