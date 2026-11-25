#pragma once

/**
 * 兼容入口：GPU 缓冲结构注解宏已迁到契约层（UBO/SSBO 结构是跨层 ABI 契约）。
 * 新代码请直接 include <hgl/graphics/contracts/BufferAnnotation.h>。
 */
#include<hgl/graphics/contracts/BufferAnnotation.h>