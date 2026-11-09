#pragma once

// MaterialData.h —— 退役转发头（2026-11 MaterialData 组件退役）。
//
// `MaterialData` 不再是 ECS 组件：它不继承 `ComponentRow`、不占 `ComponentType` 槽位，
// 本体（纯授权记录：配方覆盖 / 具名纹理资源 / 内容行绑定 / 授权代数）与世界级存储
// （`MaterialAuthoringSideTable`，键 = `EntityID`）同住旁表头文件。
// 本头只保留原 include 路径作转发，50+ 处 `#include <hgl/ecs/components/MaterialData.h>`
// 零改动继续编译。取用口在 `ECSContext`：`Get/CreateMaterialData`（查/建旁表记录）。

#include<hgl/ecs/support/material/MaterialAuthoringSideTable.h>