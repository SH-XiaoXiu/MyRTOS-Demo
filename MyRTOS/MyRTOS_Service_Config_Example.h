#if 0
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

/** @brief 启用软件定时器服务模块 */
#define MYRTOS_SERVICE_TIMER_ENABLE 1

/** @brief 启用日志服务模块 (依赖 IO 流) */
#define MYRTOS_SERVICE_LOG_ENABLE 1

/** @brief 启用 Shell 服务模块 (依赖 IO 流) */
#define MYRTOS_SERVICE_SHELL_ENABLE 1

/** @brief 启用系统监控服务模块 */
#define MYRTOS_SERVICE_MONITOR_ENABLE 1

/** @brief 启用虚拟终端服务模块 */
#define MYRTOS_SERVICE_VTS_ENABLE 1


/*==================================================================================================
 *                                    模块参数配置
 *                            (仅在对应模块开启时，以下配置才有效)
 *================================================================================================*/

#if MYRTOS_SERVICE_IO_ENABLE == 1
/** @brief Stream_Printf 和 Stream_VPrintf 使用的内部格式化缓冲区大小 (字节) */
#define MYRTOS_IO_PRINTF_BUFFER_SIZE 128
#endif

#if MYRTOS_SERVICE_LOG_ENABLE == 1
#define MYRTOS_LOG_MAX_LEVEL LOG_LEVEL_INFO
#define MYRTOS_LOG_FORMAT                                                                                              \
    "[%c][%s]",                                                                                                        \
            (level == LOG_LEVEL_ERROR ? 'E'                                                                            \
                                      : (level == LOG_LEVEL_WARN ? 'W' : (level == LOG_LEVEL_INFO ? 'I' : 'D'))),      \
            tag
#define MYRTOS_LOG_QUEUE_LENGTH 32
#define MYRTOS_LOG_MSG_MAX_SIZE 128
#define MYRTOS_LOG_TASK_STACK_SIZE 2048
#define MYRTOS_LOG_TASK_PRIORITY 1
#endif

#if MYRTOS_SERVICE_TIMER_ENABLE == 1
/** @brief 定时器服务任务命令队列的深度 (能缓存多少个命令) */
#define MYRTOS_TIMER_COMMAND_QUEUE_SIZE 10
#endif

#if MYRTOS_SERVICE_SHELL_ENABLE == 1
/** @brief Shell 命令支持的最大参数数量 (包括命令本身) */
#define SHELL_MAX_ARGS 10
/** @brief Shell 命令行输入缓冲区的最大长度 (字节) */
#define SHELL_CMD_BUFFER_SIZE 64
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#define VTS_TASK_PRIORITY 5
#define VTS_TASK_STACK_SIZE 256
#define VTS_RW_BUFFER_SIZE 128
#define VTS_PIPE_BUFFER_SIZE 512
#define VTS_MAX_BACK_CMD_LEN 16 // "back"命令序列的最大长度
#define VTS_RW_BUFFER_SIZE 128 // 内部读写缓冲区大小
#endif


/*==================================================================================================
 *                                      依赖关系检查
 *                                (禁止不合理的配置组合)
 *================================================================================================*/

#if defined(MYRTOS_SERVICE_LOG_ENABLE) && !defined(MYRTOS_SERVICE_IO_ENABLE)
#error "配置错误: 日志模块 (MYRTOS_LOG_ENABLE) 依赖于 IO流模块 (MYRTOS_IO_ENABLE)!"
#endif

#if defined(MYRTOS_SERVICE_SHELL_ENABLE) && !defined(MYRTOS_SERVICE_IO_ENABLE)
#error "配置错误: Shell模块 (MYRTOS_SHELL_ENABLE) 依赖于 IO流模块 (MYRTOS_IO_ENABLE)!"
#endif

#if defined(MYRTOS_SERVICE_VTS_ENABLE) && !defined(MYRTOS_SERVICE_IO_ENABLE)
#error "配置错误: 虚拟终端模块 (MYRTOS_VTS_ENABLE) 依赖于 IO流模块 (MYRTOS_IO_ENABLE)!"
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

// --- Shell 模块 ---
#if MYRTOS_SERVICE_SHELL_ENABLE == 0
#define Shell_Init(config, commands, count) ((void *) 0)
#define Shell_Start(shell_h, name, prio, stack) (-1)
#define Shell_GetStream(shell_h) ((void *) 0)
#endif

// --- 监控模块 ---
#if MYRTOS_SERVICE_MONITOR_ENABLE == 0
#define Monitor_Init(config) (-1)
#define Monitor_GetNextTask(prev_h) ((void *) 0)
#define Monitor_GetTaskInfo(task_h, stats) (-1)
#define Monitor_GetHeapStats(stats) ((void) 0)
#endif

#endif // MYRTOS_SERVICE_CONFIG_H

#endif
