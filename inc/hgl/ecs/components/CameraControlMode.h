#pragma once

namespace hgl::ecs
{
    /// 相机控制模式 —— 相机**参数**的一部分（作者设置、CameraSystem 处理输入）。
    ///
    /// A7c：由 `CameraComponent` 的嵌套枚举提升到命名空间级。原因：示例侧相机作者流程
    /// 改为经**世界级访问器**取相机（`ECSContext::GetCamera` / `GetOrCreateCamera`），
    /// 调用点不再出现组件类型名；控制模式是相机的公共参数，不该逼作者引用组件类。
    /// 单一真源：只在这里定义一次，`CameraComponent::control_mode` 直接用它 ——
    /// 不留 `CameraComponent::ControlMode` 别名（那是同一身份的第二个名字）。
    enum class CameraControlMode
    {
        FirstPerson,    ///< 第一人称模式（WASD 移动 + 鼠标旋转）
        ViewModel,      ///< 视图模型模式（左键旋转 + 滚轮缩放 + 右键平移）
        LookAt,         ///< 观察模式（中键平移 + 滚轮距离）
        Free            ///< 自由模式
    };
}//namespace hgl::ecs
