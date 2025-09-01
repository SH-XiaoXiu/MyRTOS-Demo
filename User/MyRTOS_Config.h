#ifndef MYRTOS_CONFIG_H
#define MYRTOS_CONFIG_H

/*===========================================================================*
 *                      核心内核配置                         *
 *===========================================================================*/

// CPU核心时钟频率 (单位: Hz)
// 用于计算 SysTick 的重载值，应与 SystemCoreClock 一致
#define MYRTOS_CPU_CLOCK_HZ                (168000000UL)

// RTOS 系统节拍（Tick）的频率 (单位: Hz)
// 推荐值为 1000，即每 1ms 产生一次Tick中断
// 更高的频率会提高时间片调度的精度和任务延时的分辨率，但也会增加系统开销
#define MYRTOS_TICK_RATE_HZ                (1000UL)

// 最大任务优先级数
// 任务的优先级范围是从 0 (最低) 到 (MY_RTOS_MAX_PRIORITIES - 1) (最高)
// 例如，设置为 32，则优先级范围为 0-31
#define MYRTOS_MAX_PRIORITIES              (32)

// 系统支持的最大并发任务数
// 这个值决定了任务ID池的大小
// 例如，设置为 64，系统最多可以同时并发64个任务
#define MYRTOS_MAX_CONCURRENT_TASKS        (64)

// 定义用于无限期阻塞等待的Tick计数值
// 通常是一个无符号整数的最大值
#define MYRTOS_MAX_DELAY                   (0xFFFFFFFFUL)
/*===========================================================================*
 *                      内存配置                 *
 *===========================================================================*/

// RTOS内核管理的内存堆的总大小 (单位: 字节)
// 所有通过 MyRTOS_Malloc() 分配的内存（如TCB, 任务栈, 队列存储区）都来自这个池
// 大小需要根据您的应用仔细估算
#define MYRTOS_MEMORY_POOL_SIZE               (64 * 1024)

// 内存分配的对齐字节数
// 对于32位处理器，通常设置为 8 以确保最佳性能和兼容性
// 必须是2的幂
#define MYRTOS_HEAP_BYTE_ALIGNMENT                 (8)


/*===========================================================================*
 *                         配置错误检查                  *
 *===========================================================================*/

#if MYRTOS_MAX_PRIORITIES > 32
#error "MY_RTOS_MAX_PRIORITIES must be less than or equal to 32."
#endif

#if MYRTOS_MAX_CONCURRENT_TASKS > 64
#error "MYRTOS_MAX_CONCURRENT_TASKS must be less than or equal to 64 due to the current taskIdBitmap implementation."
#endif

#if (MYRTOS_CPU_CLOCK_HZ / MYRTOS_TICK_RATE_HZ) > 0xFFFFFF
#error "SysTick reload value is too large. Either decrease MY_RTOS_CPU_CLOCK_HZ or increase MY_RTOS_TICK_RATE_HZ."
#endif


#endif /* MYRTOS_CONFIG_H */
