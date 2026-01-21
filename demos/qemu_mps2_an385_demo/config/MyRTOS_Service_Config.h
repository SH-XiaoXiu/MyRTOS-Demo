/**
 * @brief MyRTOS 服务配置文件 (QEMU Demo)
 *
 * 简化配置：禁用 VTS 和 Process 服务
 */

#ifndef MYRTOS_SERVICE_CONFIG_H
#define MYRTOS_SERVICE_CONFIG_H

/*===========================================================================*
 *                      模块功能开关                                          *
 *===========================================================================*/

/** @brief 启用 IO 流服务模块 */
#define MYRTOS_SERVICE_IO_ENABLE            1

/** @brief 启用异步I/O服务模块 */
#define MYRTOS_SERVICE_ASYNC_IO_ENABLE      1

/** @brief 启用软件定时器服务模块 */
#define MYRTOS_SERVICE_TIMER_ENABLE         1

/** @brief 启用日志服务模块 */
#define MYRTOS_SERVICE_LOG_ENABLE           1

/** @brief 启用系统监控服务模块 */
#define MYRTOS_SERVICE_MONITOR_ENABLE       1

/** @brief 启用虚拟终端服务模块 */
#define MYRTOS_SERVICE_VTS_ENABLE           1

/** @brief 启用进程管理服务模块 */
#define MYRTOS_SERVICE_PROCESS_ENABLE       1

/*===========================================================================*
 *                      模块参数配置                                          *
 *===========================================================================*/

#if MYRTOS_SERVICE_IO_ENABLE == 1
/** @brief Stream_Printf 和 Stream_VPrintf 使用的内部格式化缓冲区大小 (字节) */
#define MYRTOS_IO_PRINTF_BUFFER_SIZE        128
/** @brief 管道默认缓冲区大小 */
#define MYRTOS_DEFAULT_PIPE_BUFFER_SIZE     256
#endif

#if MYRTOS_SERVICE_ASYNC_IO_ENABLE == 1
/** @brief 异步I/O请求队列的深度 */
#define MYRTOS_ASYNCIO_QUEUE_LENGTH         32
/** @brief 异步I/O队列发送等待时长(ms) */
#define MYRTOS_ASYNCIO_QUEUE_SEND_TIMEOUT   50
/** @brief 单条异步消息内容的最大长度 (字节) */
#define MYRTOS_ASYNCIO_MSG_MAX_SIZE         128
/** @brief 异步I/O后台任务的堆栈大小 (字节) */
#define MYRTOS_ASYNCIO_TASK_STACK_SIZE      1024
/** @brief 异步I/O后台任务的优先级 */
#define MYRTOS_ASYNCIO_TASK_PRIORITY        2
#endif

#if MYRTOS_SERVICE_LOG_ENABLE == 1
/** @brief 日志输出级别 */
#define MYRTOS_LOG_DEFAULT_LEVEL            LOG_LEVEL_DEBUG
/** @brief 日志框架支持的最大监听器数量 */
#define MYRTOS_LOG_MAX_LISTENERS            4
/** @brief 格式化单条完整日志消息的临时缓冲区大小 */
#define MYRTOS_LOG_FORMAT_BUFFER_SIZE       256
/** @brief 日志输出的默认投递方式 (0: 同步, 1: 异步) */
#define MYRTOS_LOG_USE_ASYNC_OUTPUT         0
#endif

/** @brief 日志标签最大长度 */
#define MYRTOS_LOG_MAX_TAG_LEN              16

#if MYRTOS_SERVICE_TIMER_ENABLE == 1
/** @brief 定时器服务任务命令队列的深度 */
#define MYRTOS_TIMER_COMMAND_QUEUE_SIZE     10
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#define VTS_TASK_PRIORITY                   5
#define VTS_TASK_STACK_SIZE                 256
#define VTS_RW_BUFFER_SIZE                  128
#define VTS_PIPE_BUFFER_SIZE                512
#define VTS_LINE_BUFFER_SIZE                64
#define SIG_INTERRUPT                       (1 << 0)
#define SIG_CHILD_EXIT                      (1 << 1)
#define SIG_SUSPEND                         (1U << 2)
#define SIG_BACKGROUND                      (1U << 3)
#endif

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
/** @brief 最大同时运行的进程数量 */
#define MYRTOS_PROCESS_MAX_INSTANCES        8
/** @brief 每个进程最大文件描述符数量 */
#define MYRTOS_PROCESS_MAX_FD               16
/** @brief 最大可注册程序数量 */
#define MYRTOS_PROCESS_MAX_PROGRAMS         16
/** @brief 程序启动器任务的默认栈大小 */
#define MYRTOS_PROCESS_LAUNCHER_STACK       4096
/** @brief 程序启动器任务的默认优先级 */
#define MYRTOS_PROCESS_LAUNCHER_PRIORITY    2
#endif

/*===========================================================================*
 *                      Shell 配置参数                                        *
 *===========================================================================*/

/** @brief Shell历史记录条数 (0表示禁用历史功能) */
#define SHELL_HISTORY_SIZE                  10
/** @brief Shell单行命令最大长度 */
#define SHELL_MAX_LINE_LENGTH               128

/*===========================================================================*
 *                      依赖关系检查                                          *
 *===========================================================================*/

#if defined(MYRTOS_SERVICE_LOG_ENABLE) && MYRTOS_SERVICE_LOG_ENABLE == 1
  #if !defined(MYRTOS_SERVICE_IO_ENABLE) || MYRTOS_SERVICE_IO_ENABLE == 0
    #error "MYRTOS_SERVICE_LOG_ENABLE requires MYRTOS_SERVICE_IO_ENABLE"
  #endif
#endif

#if defined(MYRTOS_SERVICE_VTS_ENABLE) && MYRTOS_SERVICE_VTS_ENABLE == 1
  #if !defined(MYRTOS_SERVICE_IO_ENABLE) || MYRTOS_SERVICE_IO_ENABLE == 0
    #error "MYRTOS_SERVICE_VTS_ENABLE requires MYRTOS_SERVICE_IO_ENABLE"
  #endif
#endif

#if defined(MYRTOS_SERVICE_PROCESS_ENABLE) && MYRTOS_SERVICE_PROCESS_ENABLE == 1
  #if !defined(MYRTOS_SERVICE_IO_ENABLE) || MYRTOS_SERVICE_IO_ENABLE == 0
    #error "MYRTOS_SERVICE_PROCESS_ENABLE requires MYRTOS_SERVICE_IO_ENABLE"
  #endif
#endif

/*===========================================================================*
 *                      空宏定义 (模块禁用时)                                  *
 *===========================================================================*/

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

#if MYRTOS_SERVICE_MONITOR_ENABLE == 0
#define Monitor_Init(config) (-1)
#define Monitor_GetNextTask(prev_h) ((void *) 0)
#define Monitor_GetTaskInfo(task_h, stats) (-1)
#define Monitor_GetHeapStats(stats) ((void) 0)
#define Monitor_TaskSwitchIn(tcb)
#define Monitor_TaskSwitchOut(tcb)
#endif

#endif /* MYRTOS_SERVICE_CONFIG_H */
