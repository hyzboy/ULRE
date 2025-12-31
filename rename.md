| 现名 | 问题 | 建议主名 | 说明 | 
|------|------|----------|------|
| SceneOrient | 语义模糊，像“朝向”而非全变换 | NodeTransform 或 TransformNodeBase | 表示节点的基础变换持有者 | 
| SceneMatrix | 持多矩阵与版本控制，不只是单一矩阵 | TransformState 或 TransformCache | 强调缓存/派生矩阵状态 | 
| RefreshMatrix() | 动机不清 | UpdateWorldTransform() | 明确作用 | 
| GetLocalToWorldMatrix | 冗长 | GetWorldMatrix() | 行业惯例 | 
| GetInverseLocalToWorldMatrix | 冗长 | GetWorldToLocalMatrix() | 行业惯例 | 
| GetInverseTransposeLocalToWorldMatrix | 冗长 | GetNormalMatrix() | 语义直觉(供法线变换) | 
| local_matrix / parent_matrix | OK | (保持) | 可保留 | 
| OriginWorldPosition / FinalWorldPosition | 区分不明显 | PrevWorldPosition / WorldPosition | Prev 表示上帧/更新前 | 
| OriginWorldNormal / FinalWorldNormal | 同上 | PrevWorldNormal / WorldNormal | 

额外建议：

- TransformManager 若只是“附加变换序列”→ Rename: ExtraTransformStack 或 AppliedTransforms
- Update()/MakeNewestData 重命名为 ComputeDerivedMatrices()

场景节点
| 现名 | 建议 | 说明 |
|------|------|------|
| SceneNode::Clear | ResetNode() 或 Reset | Clear 容易与释放资源混淆 |
| Duplication / DuplicationChildNodes | Clone / CloneChildren | 行业内普遍用 Clone |
| ComponentSet | components 或 ComponentSet（如是模板容器可保留） | 若是内部集合结构，可统一 GetComponents() 返回 span/列表 |
| ChildNodeIsEmpty | HasChildren / ChildrenEmpty | 逻辑取反更自然：HasChildren() |
| SetParent | AttachToParent / SetParent | 可保留，Add() 调子节点时统一语义 AddChild |

渲染收集/批处理层

| 现名 | 问题 | 建议主名 | 说明 |
|------|------|----------|------|
| RenderList | 功能是场景展开+分发给材质批次 | RenderCollector 或 SceneRenderCollector | 表达“收集”语义 |
| Expend / ExpendNode | 拼写错误，应为 Expand | Expand / ExpandNode | 标准拼写 |
| MaterialRenderList | 实为同材质/管线批次管理+Indirect构建 | MaterialBatch 或 PipelineMaterialBatch | 若管线相同也归并，PipelineMaterialBatch 更清晰 |
| RenderAssignBuffer | “Assign”抽象不清 | InstanceAssignmentBuffer 或 DrawInstanceMapBuffer | 表示“实例ID映射” |
| RenderNode | 语义与 SceneNode 容易混淆 | MeshDrawNode 或 DrawNode | 明确它是“可绘制条目” |
| RenderItem (内部二级统计) | 与 RenderNode 近义 | DrawBatch 或 MeshBatchItem | 表示合批后单条 indirect 调用 |
| rn_list / rn_update_l2w_list | 缩写 | draw_nodes / transform_dirty_nodes | 可读性提升 |
| ri_array | 缩写 | draw_batches | |
| Stat()/End() | 模糊 | BuildBatches() / Finalize() | 体现“统计并生成批次” |
| UpdateLocalToWorld() | 范围不清 | UpdateTransformData() | 多个对象 |
| UpdateMaterialInstance() | 同 | UpdateMaterialInstanceData() | |

执行/任务层

| 现名 | 问题 | 建议 |
|------|------|------|
| RenderTask | 泛化 | FrameRenderTask 或 ViewRenderTask |
| Renderer | 与很多库同名，过宽 | SceneRenderer 或 FrameRenderer |
| build_frame (bool) | 状态标志含糊 | command_list_dirty 或 frame_build_pending |

缓冲/字段命名
| 现名 | 建议 | 说明 |
|------|------|------|
| l2w_buffer / l2w_version / l2w_index | transform_buffer / transform_version / transform_index | 避免魔法缩写 |
| mi / mi_buffer / mi_set | material_instance / material_instance_buffer / material_instance_set | 统一 |
| assign_vab | assignment_vab 或 instance_map_vab | |
| LW2_MAX_COUNT (?) | MAX_LOCAL_TO_WORLD_COUNT | 若确需常量 |
| first_instance / instance_count (RenderItem) | first_mapping / mapping_count (若非真实 instanced draw) | 依据真实语义调整 |

函数动词统一建议

- Collect：遍历/收集（Scene → RenderCollector）

- Build：生成结构（BuildBatches / BuildIndirectCommands）

- Update：已有对象数据变化（UpdateTransformData / UpdateMaterialInstanceData）

- Write：真正写 GPU Buffer（WriteTransformBuffer / WriteMaterialInstanceBuffer）
- Render/Execute：提交指令
- Finalize：收尾冻结不可再加
- Clear/Reset：清空内部资源
- Clone：复制对象
文档/注释调整建议
1.	将“Refresh”全部替换为“Update”并添加备注：Update = Recompute + Upload If Dirty。
2.	在 TransformState 顶部类注释写出：
WorldMatrix = ParentMatrix * LocalMatrix * ExtraTransformMatrix
3.	在 MaterialBatch 注释内分段：
- 收集阶段(Collect)
- 统计+合批(BuildBatches)
- GPU缓冲写入(Write*)
- 闲时增量更新(Update*)
过渡期策略（避免一次性破坏）
1.	第一步：添加别名 + 内联转发（保留旧 API 标记弃用）。
2.	第二步：逐步替换内部使用点。
3.	第三步：移除旧名。