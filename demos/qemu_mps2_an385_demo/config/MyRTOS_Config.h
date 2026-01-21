#ifndef MYRTOS_CONFIG_H
#define MYRTOS_CONFIG_H

/*===========================================================================*
 *                      芯片设备头文件                                        *
 *===========================================================================*/
#include "CMSDK_CM3.h"

/*===========================================================================*
 *                      Boot 模块配置                                         *
 *===========================================================================*/

// 是否使用 MyRTOS Boot 模块
#define MYRTOS_USE_BOOT_MODULE 1

/*===========================================================================*
 *                      核心内核配置                                          *
 *===========================================================================*/

// CPU核心时钟频率 (单位: Hz)
// MPS2-AN385 使用 25MHz 时钟
#define MYRTOS_CPU_CLOCK_HZ (25000000UL)

// RTOS 系统节拍（Tick）的频率 (单位: Hz)
#define MYRTOS_TICK_RATE_HZ (1000UL)

// 最大任务优先级数
#define MYRTOS_MAX_PRIORITIES (32)

// 系统支持的最大并发任务数
#define MYRTOS_MAX_CONCURRENT_TASKS (64)

// 无限期阻塞等待的Tick计数值
#define MYRTOS_MAX_DELAY (0xFFFFFFFFUL)

/*===========================================================================*
 *                      内存配置                                              *
 *===========================================================================*/

// RTOS内核管理的内存堆大小 (单位: 字节)
// QEMU 模拟的 MPS2-AN385 有 4MB RAM
#define MYRTOS_MEMORY_POOL_SIZE (256 * 1024)

// 内存分配的对齐字节数
#define MYRTOS_HEAP_BYTE_ALIGNMENT (8)

/*===========================================================================*
 *                         配置错误检查                                       *
 *===========================================================================*/

#if MYRTOS_MAX_PRIORITIES > 32
#error "MY_RTOS_MAX_PRIORITIES must be less than or equal to 32."
#endif

#if MYRTOS_MAX_CONCURRENT_TASKS > 64
#error "MYRTOS_MAX_CONCURRENT_TASKS must be less than or equal to 64."
#endif

#if (MYRTOS_CPU_CLOCK_HZ / MYRTOS_TICK_RATE_HZ) > 0xFFFFFF
#error "SysTick reload value is too large."
#endif

#endif /* MYRTOS_CONFIG_H */
