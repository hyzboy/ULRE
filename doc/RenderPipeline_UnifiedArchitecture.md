/**
 * RenderPipeline 统一架构指南
 * 
 * 本指南说明如何使用新的 RenderPipelineBase 接口来统一所有渲染管道。
 * 
 * =============================================================================
 * 1. 架构概览
 * =============================================================================
 * 
 * 旧架构（混乱）:
 *   PrimitiveBatchPipeline - 直接被 4 个 System 调用
 *   TextRenderPipeline     - 直接被 3 个 System 调用
 *   LineRenderSystem       - 自己是 System，持有 LineRenderManager
 *   QuadRenderSystem       - 自己是 System，持有 QuadRenderManager
 *   
 * 新架构（统一）:
 *   RenderPipelineBase (虚基类)
 *     ├─ PrimitiveBatchPipeline (改造为派生类)
 *     ├─ TextRenderPipeline (改造为派生类)
 *     ├─ LineRenderPipeline (新类，从 LineRenderSystem 分离出来)
 *     ├─ QuadRenderPipeline (新类，从 QuadRenderSystem 分离出来)
 *     └─ ParticleRenderPipeline (未来扩展)
 *     
 * ECSContext 中统一管理：
 *   std::unordered_map<std::string, std::unique_ptr<RenderPipelineBase>> render_pipelines
 *   
 * SystemGroup 与 Pipeline 一一对应：
 *   Group "Primitive"  ← PrimitiveBatchPipeline
 *   Group "Text"       ← TextRenderPipeline
 *   Group "Line"       ← LineRenderPipeline
 *   Group "Quad"       ← QuadRenderPipeline
 * 
 * =============================================================================
 * 2. 生命周期和执行流程
 * =============================================================================
 * 
 * 每帧执行（在 RenderGraph 中）：
 * 
 *   PrepareSubWorld() / PrepareRenderPassSetup()
 *   ├─ RenderBeginFrame
 *   ├─ RenderCollect
 *   │  └─ for each pipeline: pipeline->PrepareFrame(); pipeline->RunCollect();
 *   ├─ RenderCull
 *   │  └─ for each pipeline: pipeline->RunCull();
 *   ├─ RenderSort
 *   │  └─ for each pipeline: pipeline->RunSort();
 *   ├─ RenderBatch
 *   │  └─ for each pipeline: pipeline->RunBuild();
 *   ├─ RenderBufferCommit
 *   ├─ RenderBufferUpload
 *   │  └─ for each pipeline: pipeline->RunSync();
 *   └─ RenderFrameSync
 *   
 *   BeginRenderPass()
 *   ├─ RenderDrawSubmit 
 *   │  └─ for each pipeline: 
 *   │     pipeline->GetRenderPrimitives(primitives);
 *   │     RenderDrawSubmitSystem::RecordDrawCalls(primitives);
 *   │     pipeline->Render(cmd);  // 自定义绘制命令
 *   ├─ ...other passes...
 *   └─ EndRenderPass()
 * 
 * =============================================================================
 * 3. 迁移步骤 - 以 PrimitiveBatchPipeline 为例
 * =============================================================================
 * 
 * STEP 1: 添加虚函数覆盖
 * ─────────────────────
 * 
 *   class PrimitiveBatchPipeline : public RenderPipelineBase {
 *   public:
 *       const std::string& GetName() const override { return "Primitive"; }
 *       ECSContext* GetWorld() const override { return world; }
 *       
 *       bool PrepareFrame() override;
 *       void RunCollect() override;
 *       void RunCull() override;
 *       void RunSort() override;
 *       void RunBuild() override;
 *       void RunSync() override;
 *       void GetRenderPrimitives(...) override;
 *       void Render(RenderCmdBuffer* cmd) override;
 *   };
 * 
 * STEP 2: 调整 System 的调用方式
 * ───────────────────────────────
 * 
 *   旧代码（RenderPrimitiveCullSystem::Update）:
 *     auto pipeline = context->GetPrimitiveBatchPipeline();
 *     pipeline->RunCulling();
 *   
 *   新代码:
 *     auto pipeline = context->GetRenderPipeline("Primitive");
 *     if (pipeline) pipeline->RunCull();
 *   
 *   或者（向后兼容）:
 *     context->GetPrimitiveBatchPipeline()->RunCulling();
 *     // 内部调用 render_pipelines["Primitive"]->RunCull()
 * 
 * STEP 3: 在 SystemGroup 安装程序中注册 Pipeline
 * ───────────────────────────────────────────────
 * 
 *   bool InstallPrimitiveGroup(ECSContext* context, IRenderTarget* target)
 *   {
 *       // 创建 Pipeline
 *       auto primitive_pipeline = std::make_unique<PrimitiveBatchPipeline>();
 *       primitive_pipeline->SetWorld(context);
 *       
 *       // 注册到 Context
 *       context->RegisterRenderPipeline("Primitive", std::move(primitive_pipeline));
 *       
 *       // 注册 Systems（Collect, Cull, Sort, Build, Finalize, Submit）
 *       context->RegisterRenderSystem<RenderPrimitiveCollectSystem>();
 *       context->RegisterRenderSystem<RenderPrimitiveCullSystem>();
 *       // ... 等等
 *       
 *       return true;
 *   }
 * 
 * =============================================================================
 * 4. 新建一个 Pipeline 的完整例子 - LineRenderPipeline
 * =============================================================================
 * 
 *   // inc/hgl/ecs/support/LineRenderPipeline.h
 *   class LineRenderPipeline : public RenderPipelineBase {
 *   private:
 *       ECSContext* world = nullptr;
 *       hgl::graph::LineRenderManager* line_manager = nullptr;
 *       uint32_t prepared_frame_index = UINT32_MAX;
 *       
 *   public:
 *       const std::string& GetName() const override { return "Line"; }
 *       ECSContext* GetWorld() const override { return world; }
 *       void SetWorld(ECSContext* w) { world = w; }
 *       
 *       bool PrepareFrame() override { /* 初始化 line_manager */ }
 *       void RunCollect() override { /* 遍历 LinesComponent */ }
 *       void RunBuild() override { /* 构建批次，标脏缓冲 */ }
 *       void GetRenderPrimitives(...) override { /* 返回 line 原语 */ }
 *       void Render(RenderCmdBuffer* cmd) override { /* 录制绘制命令 */ }
 *   };
 * 
 * =============================================================================
 * 5. 与 SystemGroup 的协作
 * =============================================================================
 * 
 *   // 注册 SystemGroup（包含 Phase 范围）
 *   SystemGroup primitive_group("Primitive", ExecutionPhase::RenderCollect, 
 *                                ExecutionPhase::RenderStat);
 *   SystemGroupRegistry::Get().RegisterGroup(primitive_group);
 *   
 *   // 注册 Installer（创建 Pipeline 和 Systems）
 *   SystemGroupRegistry::Get().RegisterGroupInstaller("Primitive", 
 *       [](ECSContext* ctx, IRenderTarget* rt) {
 *           return InstallPrimitiveGroup(ctx, rt);
 *       });
 *   
 *   // 运行时启用/禁用整个 group
 *   context->SetSystemGroupEnabled("Primitive", false);  // 禁用所有 Primitive 系统及其 Pipeline
 * 
 * =============================================================================
 * 6. 优点
 * =============================================================================
 * 
 * ✅ 统一接口：所有渲染管道都有相同的方法签名
 * ✅ 易于扩展：新增渲染类型只需新建 Pipeline 派生类和对应 System
 * ✅ 清晰的生命周期：Pipeline 生命周期与 SystemGroup 一致
 * ✅ 去耦合：System 不需要知道 Pipeline 的具体实现，只需调用虚函数
 * ✅ 可测试性：Pipeline 可独立单元测试，无需完整 System 框架
 * ✅ 性能：Pipeline 各阶段可用细粒度控制（启用/禁用各个阶段）
 * 
 * =============================================================================
 * 7. 迁移时间表
 * =============================================================================
 * 
 * Phase 1: 添加 RenderPipelineBase 基类和管理接口 ✅ (本周期完成)
 * Phase 2: PrimitiveBatchPipeline 实现 RenderPipelineBase
 * Phase 3: TextRenderPipeline 实现 RenderPipelineBase  
 * Phase 4: LineRenderSystem 内部分离为 LineRenderPipeline
 * Phase 5: Quad/Billboard/Particle 等新 Pipeline 的统一实现
 */
