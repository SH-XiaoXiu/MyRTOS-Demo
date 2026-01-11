/**
 *  @brief MyRTOS 扩展服务层 - 全局配置文件
 */
#ifndef MYRTOS_SERVICE_CONFIG_H
#define MYRTOS_SERVICE_CONFIG_H

/*==================================================================================================
 *                                    模块功能开关
 *================================================================================================*/

/** @brief 启用 IO 流服务模块 (日志和Shell模块的基础) */
#define MYRTOS_SERVICE_IO_ENABLE 1

/** @brief 启用异步I/O服务模块 异步日志等功能的基础。*/
#define MYRTOS_SERVICE_ASYNC_IO_ENABLE      1

/** @brief 启用软件定时器服务模块 */
#define MYRTOS_SERVICE_TIMER_ENABLE 1

/** @brief 启用日志服务模块 (依赖 IO 流) */
#define MYRTOS_SERVICE_LOG_ENABLE 1

/** @brief 启用系统监控服务模块 */
#define MYRTOS_SERVICE_MONITOR_ENABLE 1

/** @brief 启用虚拟终端服务模块 */
#define MYRTOS_SERVICE_VTS_ENABLE 1

/** @brief 启用进程管理服务模块 */
#define MYRTOS_SERVICE_PROCESS_ENABLE 1


/*==================================================================================================
 *                                    模块参数配置
 *                            (仅在对应模块开启时，以下配置才有效)
 *================================================================================================*/

#if MYRTOS_SERVICE_IO_ENABLE == 1
/** @brief Stream_Printf 和 Stream_VPrintf 使用的内部格式化缓冲区大小 (字节) */
#define MYRTOS_IO_PRINTF_BUFFER_SIZE 128
#endif

#if MYRTOS_SERVICE_ASYNC_IO_ENABLE == 1
/** @brief 异步I/O请求队列的深度。若队列满，新的打印请求将被丢弃。 */
#define MYRTOS_ASYNCIO_QUEUE_LENGTH         32
/** @brief 异步I/O队列发送等待时长(ms)。建议给一定时长, 防止消息被丢弃 */
#define MYRTOS_ASYNCIO_QUEUE_SEND_TIMEOUT     50
/** @brief 单条异步消息内容的最大长度 (字节)。超过部分将被截断。 */
#define MYRTOS_ASYNCIO_MSG_MAX_SIZE         128
/** @brief 异步I/O后台任务的堆栈大小 (字节)。 */
#define MYRTOS_ASYNCIO_TASK_STACK_SIZE      1024
/** @brief 异步I/O后台任务的优先级。通常应设为较低优先级，避免抢占关键业务。*/
#define MYRTOS_ASYNCIO_TASK_PRIORITY        2
#endif


#if MYRTOS_SERVICE_LOG_ENABLE == 1
/** @brief 日志输出级别 */
#define MYRTOS_LOG_DEFAULT_LEVEL LOG_LEVEL_DEBUG

/** @brief 日志框架支持的最大监听器数量*/
#define MYRTOS_LOG_MAX_LISTENERS            4

/** @brief 格式化单条完整日志消息的临时缓冲区大小。如果消息被截断，会在末尾加省略号"..."*/
#define MYRTOS_LOG_FORMAT_BUFFER_SIZE       256

/**
 * @brief 日志输出的默认投递方式。
 * @details 0: 同步 (Stream_Printf), 1: 异步 (MyRTOS_AsyncPrintf)。
 */
#define MYRTOS_LOG_USE_ASYNC_OUTPUT         1

#endif

#define MYRTOS_LOG_MAX_TAG_LEN 16


#if MYRTOS_SERVICE_TIMER_ENABLE == 1
/** @brief 定时器服务任务命令队列的深度 (能缓存多少个命令) */
#define MYRTOS_TIMER_COMMAND_QUEUE_SIZE 10
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#define VTS_TASK_PRIORITY 5
#define VTS_TASK_STACK_SIZE 256
#define VTS_RW_BUFFER_SIZE 128
#define VTS_PIPE_BUFFER_SIZE 512
#define VTS_LINE_BUFFER_SIZE 64
#define VTS_RW_BUFFER_SIZE 128 // 内部读写缓冲区大小
#define SIG_INTERRUPT    (1 << 0) // 由VTS发送，用于中断 (Ctrl+C)
#define SIG_CHILD_EXIT   (1 << 1) // 由子任务在退出前发送
#define SIG_SUSPEND      (1U << 2) // VTS发送, 用于挂起 (Ctrl+Z)
#define SIG_BACKGROUND   (1U << 3) // VTS发送, 用于转到后台 (Ctrl+B)
#endif

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
/** @brief 最大同时运行的进程数量 */
#define MYRTOS_PROCESS_MAX_INSTANCES 8
/** @brief 每个进程最大文件描述符数量 (包括stdin/stdout/stderr) */
#define MYRTOS_PROCESS_MAX_FD 16
/** @brief 最大可注册程序数量 (静态程序表大小) */
#define MYRTOS_PROCESS_MAX_PROGRAMS 16
/** @brief 程序启动器任务的默认栈大小 (字节) */
#define MYRTOS_PROCESS_LAUNCHER_STACK 512
/** @brief 程序启动器任务的默认优先级 */
#define MYRTOS_PROCESS_LAUNCHER_PRIORITY 2
#endif


/*==================================================================================================
 *                                      依赖关系检查
 *                                (禁止不合理的配置组合)
 *================================================================================================*/

#if defined(MYRTOS_SERVICE_LOG_ENABLE) && !defined(MYRTOS_SERVICE_IO_ENABLE)
#error "配置错误: 日志模块 (MYRTOS_LOG_ENABLE) 依赖于 IO流模块 (MYRTOS_IO_ENABLE)!"
#endif

#if defined(MYRTOS_SERVICE_VTS_ENABLE) && !defined(MYRTOS_SERVICE_IO_ENABLE)
#error "配置错误: 虚拟终端模块 (MYRTOS_VTS_ENABLE) 依赖于 IO流模块 (MYRTOS_IO_ENABLE)!"
#endif

#if defined(MYRTOS_SERVICE_PROCESS_ENABLE) && MYRTOS_SERVICE_PROCESS_ENABLE == 1
  #if !defined(MYRTOS_SERVICE_IO_ENABLE) || MYRTOS_SERVICE_IO_ENABLE == 0
    #error "配置错误: 进程管理模块 (MYRTOS_PROCESS_ENABLE) 依赖于 IO流模块 (MYRTOS_IO_ENABLE)!"
  #endif
#endif

/*==================================================================================================
 *                                      空宏定义
 *                  (如果模块被禁用, 定义空宏以保证上层代码无需修改即可编译)
 *================================================================================================*/
// --- 日志模块 ---
#if MYRTOS_SERVICE_LOG_ENABLE == 0
#define LOG_E(tag, format, ...) ((void) 0)
#define LOG_W(tag, format, ...) ((void) 0)
#define LOG_I(tag, format, ...) ((void) 0)
#define LOG_D(tag, format, ...) ((void) 0)
#define LOG_V(tag, format, ...) ((void) 0)
#define Log_Init(initial_level, default_stream) (0)
#define Log_SetLevel(level) ((void) 0)
#define Log_SetStream(stream) ((void) 0)
#define Log_Output(level, tag, format, ...) ((void) 0)
#define Log_VOutput(level, tag, format, args) ((void) 0)
#endif

// --- 定时器模块 ---
#if MYRTOS_SERVICE_TIMER_ENABLE == 0
#define TimerHandle_t void *
#define TimerService_Init(prio, stack) (0)
#define Timer_Create(name, period, is_p, cb, arg) ((void *) 0)
#define Timer_Start(timer, ticks) (-1)
#define Timer_Stop(timer, ticks) (-1)
#define Timer_Delete(timer, ticks) (-1)
#define Timer_ChangePeriod(timer, period, ticks) (-1)
#define Timer_GetArg(timer) ((void *) 0)
#endif

// --- 监控模块 ---
#if MYRTOS_SERVICE_MONITOR_ENABLE == 0
#define Monitor_Init(config) (-1)
#define Monitor_GetNextTask(prev_h) ((void *) 0)
#define Monitor_GetTaskInfo(task_h, stats) (-1)
#define Monitor_GetHeapStats(stats) ((void) 0)
#endif
#endif

