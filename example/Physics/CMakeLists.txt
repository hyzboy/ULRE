# JoltPhysics 对接范例 1~4（GUI 可视化）
# 场景搭建来自 src/ecs/tests/PhysicsScenarios.cpp（与无头测试共用同一真源）
CreateProject(Physics01BoxStacking     01_BoxStacking.cpp     PhysicsAppCommon.h ../../src/ecs/tests/PhysicsScenarios.cpp)
CreateProject(Physics02CapsuleCompound 02_CapsuleCompound.cpp PhysicsAppCommon.h ../../src/ecs/tests/PhysicsScenarios.cpp)
CreateProject(Physics03KinematicPlatform 03_KinematicPlatform.cpp PhysicsAppCommon.h ../../src/ecs/tests/PhysicsScenarios.cpp)
CreateProject(Physics04RaycastSensor   04_RaycastSensor.cpp   PhysicsAppCommon.h ../../src/ecs/tests/PhysicsScenarios.cpp)
