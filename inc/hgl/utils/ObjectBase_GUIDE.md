/**
 * ObjectBase 框架集成指南
 * 
 * 目标：追踪所有Vulkan对象的生命周期，在崩溃时恢复信息
 * 
 * =============================================================
 */

/*
 * 【文件结构】
 * 
 * inc/hgl/utils/ObjectBase.h              - 核心基类
 *   ├─ ObjectBase                         - 基类
 *   ├─ ObjectIdGenerator                  - ID生成器
 *   ├─ ObjectRegistry                     - 全局注册表
 *   └─ 便利宏 (HGL_CHECK_OBJECT_VALID等)
 * 
 * inc/hgl/utils/ObjectCrashRecovery.h    - 崩溃恢复
 *   ├─ ObjectSnapshot                     - 对象快照
 *   └─ ObjectCrashRecovery                - 恢复工具
 * 
 * inc/hgl/utils/ObjectBaseExamples.h     - 使用示例
 *   ├─ VKFenceBase
 *   ├─ VKBufferBase
 *   ├─ VKImageBase
 *   ├─ VKCommandBufferBase
 *   └─ MaterialBase
 * 
 * =============================================================
 */

/*
 * 【快速开始】
 * 
 * ① 包含头文件
 * 
 *    #include<hgl/utils/ObjectBase.h>
 * 
 * ② 为需要追踪的类创建基类版本
 * 
 *    // 原代码：
 *    class Fence
 *    {
 *    public:
 *        Fence(VkDevice dev, VkFence f) : device(dev), fence(f) {}
 *        ~Fence() { vkDestroyFence(device, fence, nullptr); }
 *    };
 * 
 *    // 改为：
 *    class Fence : public hgl::utils::ObjectBase
 *    {
 *    public:
 *        Fence(
 *            VkDevice dev,
 *            VkFence f,
 *            const std::source_location& loc = std::source_location::current()
 *        ) : ObjectBase(hgl::core::ObjectTypeTag::VKFence, "Fence", loc),
 *            device(dev), fence(f)
 *        {
 *        }
 * 
 *        virtual ~Fence() override
 *        {
 *            HGL_OBJECT_DESTROY_LOCATION();  // 记录销毁位置
 *            vkDestroyFence(device, fence, nullptr);
 *        }
 *    };
 * 
 * ③ 创建对象时自动追踪（无需手动操作）
 * 
 *    auto fence = new Fence(device, vk_fence);
 *    // 自动：
 *    // - 分配唯一ID
 *    // - 记录创建位置 (文件、行号、函数)
 *    // - 注册到全局注册表
 * 
 * ④ 销毁对象时自动处理
 * 
 *    delete fence;
 *    // 自动：
 *    // - 记录销毁位置
 *    // - 从注册表注销
 *    // - 标记为已销毁
 * 
 * =============================================================
 */

/*
 * 【核心特性】
 * 
 * ① 自动ID分配
 *    - 每个对象获得唯一64位ID
 *    - 从1开始自增
 *    - 无锁原子操作
 * 
 * ② 源位置追踪
 *    - 创建位置：文件、行号、函数
 *    - 销毁位置：文件、行号、函数（可选）
 *    - 使用C++20 source_location
 * 
 * ③ 魔数验证
 *    - 每个对象有魔数: 0xDEADBEEFCAFEBABE
 *    - 验证指针是否指向有效ObjectBase
 *    - 防止访问已删除对象
 * 
 * ④ 全局注册表
 *    - 追踪所有活跃对象
 *    - 实时查询对象数量
 *    - 检测泄漏
 * 
 * ⑤ 崩溃恢复
 *    - 快照保存到文件
 *    - 内存扫描恢复
 *    - HTML报告生成
 * 
 * =============================================================
 */

/*
 * 【常用API】
 * 
 * ① 对象查询
 *    obj->get_object_id()            - 获取ID
 *    obj->get_object_type()          - 获取类型
 *    obj->get_object_name()          - 获取名称
 *    obj->get_creation_location()    - 获取创建位置
 *    obj->get_destruction_location() - 获取销毁位置
 *    obj->is_valid()                 - 检查有效性
 *    obj->is_destroyed()             - 检查是否已销毁
 *    obj->to_string()                - 获取信息字符串
 * 
 * ② 全局操作（宏）
 *    HGL_LIST_ALL_OBJECTS()          - 列出所有对象
 *    HGL_REPORT_LEAKS()              - 检测泄漏
 *    HGL_GET_OBJECT_COUNT()          - 获取对象总数
 *    HGL_CHECK_OBJECT_VALID(obj)     - 验证对象有效性
 *    HGL_FIND_OBJECT(id, type)       - 从ID查找对象
 * 
 * ③ 崩溃恢复
 *    ObjectCrashRecovery::save_snapshot()    - 保存快照
 *    ObjectCrashRecovery::load_and_analyze() - 加载分析
 *    ObjectCrashRecovery::generate_html_report() - 生成报告
 * 
 * =============================================================
 */

/*
 * 【使用例子】
 * 
 * ① 创建并追踪对象
 * 
 *    class TextureManager
 *    {
 *    public:
 *        class TextureImpl : public ObjectBase
 *        {
 *        public:
 *            TextureImpl(VkImage img, const std::source_location& loc)
 *                : ObjectBase(ObjectTypeTag::VKImage, "Texture", loc)
 *                , image(img)
 *            {
 *            }
 *            
 *            virtual ~TextureImpl() override
 *            {
 *                HGL_OBJECT_DESTROY_LOCATION();
 *                if (image) vkDestroyImage(dev, image, nullptr);
 *            }
 *            
 *            VkImage image;
 *        };
 *        
 *        Texture* create_texture()
 *        {
 *            VkImage img = ...;
 *            // 自动记录创建位置
 *            return new TextureImpl(img);
 *        }
 *    };
 * 
 * ② 调试泄漏
 * 
 *    int main()
 *    {
 *        hgl::utils::ObjectRegistry& registry = 
 *            hgl::utils::ObjectRegistry::get_instance();
 *        
 *        // ... 运行代码 ...
 *        
 *        printf("Total objects: %zu\n", HGL_GET_OBJECT_COUNT());
 *        HGL_LIST_ALL_OBJECTS();     // 列出所有
 *        size_t leaks = HGL_REPORT_LEAKS(); // 报告泄漏
 *        
 *        return leaks > 0 ? 1 : 0;
 *    }
 * 
 * ③ 崩溃后恢复信息
 * 
 *    void crash_handler()
 *    {
 *        // 程序崩溃前自动调用
 *        printf("Program crash detected!\n");
 *        printf("Current active objects:\n");
 *        HGL_LIST_ALL_OBJECTS();
 *        
 *        // 也可以保存快照用于离线分析
 *        // ObjectCrashRecovery::save_snapshot("crash.snap");
 *    }
 * 
 * =============================================================
 */

/*
 * 【性能考虑】
 * 
 * ① 开销
 *    - 每个对象额外 ~100 字节内存
 *    - 创建/销毁：O(log n) 注册表操作，非常快
 *    - 查询：O(1) 哈希表查找
 *    - 无锁ID生成：原子操作
 * 
 * ② 优化建议
 *    - 在Release版本中可选择关闭追踪（通过编译开关）
 *    - 注册表大小自动扩展
 *    - 只在需要时调用列出/报告方法
 * 
 * ③ 线程安全
 *    - 原子ID生成
 *    - 互斥锁保护注册表
 *    - 销毁标记原子操作
 *    - 安全地用于多线程
 * 
 * =============================================================
 */

/*
 * 【迁移清单】
 * 
 * 对于SwapChainModule中的Fence泄漏 (18处)：
 * 
 * ✓ 1. SwapChainModule.cpp:147 - RenderCmdBuffer
 *      class RenderCmdBuffer : public ObjectBase
 * 
 * ✓ 2. SwapChainModule.cpp:224 - Fence
 *      class Fence : public ObjectBase
 * 
 * ✓ 3. TextureManager.cpp - Textures
 *      class Texture : public ObjectBase
 * 
 * ✓ 4. MaterialManager.cpp - Materials
 *      class Material : public ObjectBase
 * 
 * ✓ 5. RenderPassManager.cpp - RenderPass
 *      class RenderPass : public ObjectBase
 * 
 * 优先级：Fence（24处）> Material（8处）> Others
 * 
 * =============================================================
 */

/*
 * 【编译选项】
 * 
 * 可以添加CMake选项来启用/禁用：
 * 
 *    option(HGL_ENABLE_OBJECT_TRACKING "Enable object lifecycle tracking" ON)
 * 
 *    if(HGL_ENABLE_OBJECT_TRACKING)
 *        target_compile_definitions(hgl PRIVATE HGL_ENABLE_OBJECT_TRACKING)
 *    endif()
 * 
 * 在ObjectBase.h中：
 * 
 *    #ifdef HGL_ENABLE_OBJECT_TRACKING
 *        // 启用追踪代码
 *    #else
 *        // 精简版本，无追踪开销
 *    #endif
 * 
 * =============================================================
 */
