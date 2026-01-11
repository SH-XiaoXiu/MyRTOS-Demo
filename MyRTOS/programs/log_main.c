/**
 * @file  log_main.c
 * @brief Log程序 - 独立进程模式的日志监控工具
 */
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Process.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if MYRTOS_SERVICE_LOG_ENABLE == 1
#include "MyRTOS_Log.h"

/**
 * @brief Log程序主函数
 * @param argc 参数数量
 * @param argv 参数数组
 *        argv[1]: tag 名称（必需）
 *        argv[2]: log level（可选，默认为 debug）
 * @return 0 成功，-1 失败
 */
static int log_program_main(int argc, char *argv[]) {
    if (argc < 2) {
        MyRTOS_printf("Usage: log <tag> [level]\n");
        MyRTOS_printf("Levels: error, warn, info, debug (default: debug)\n");
        return -1;
    }

    const char *tag = argv[1];
    LogLevel_t level = LOG_LEVEL_DEBUG; // 默认级别

    // 解析日志级别参数
    if (argc >= 3) {
        if (strcasecmp(argv[2], "error") == 0) {
            level = LOG_LEVEL_ERROR;
        } else if (strcasecmp(argv[2], "warn") == 0) {
            level = LOG_LEVEL_WARN;
        } else if (strcasecmp(argv[2], "info") == 0) {
            level = LOG_LEVEL_INFO;
        } else if (strcasecmp(argv[2], "debug") == 0) {
            level = LOG_LEVEL_DEBUG;
        } else if (strcasecmp(argv[2], "none") == 0) {
            level = LOG_LEVEL_NONE;
        } else {
            MyRTOS_printf("Error: Invalid log level '%s'. Use error|warn|info|debug|none.\n", argv[2]);
            return -1;
        }
    }

    // 使用进程自带的 stdout（不需要创建新 pipe）
    StreamHandle_t stdout_stream = Stream_GetTaskStdOut(NULL);
    if (!stdout_stream) {
        MyRTOS_printf("Error: Failed to get stdout stream.\n");
        return -1;
    }

    // 添加日志监听器，输出到进程的 stdout
    LogListenerHandle_t listener_h = Log_AddListener(stdout_stream, level, tag);
    if (!listener_h) {
        MyRTOS_printf("Error: Failed to add log listener.\n");
        return -1;
    }

    MyRTOS_printf("--- Start monitoring LOGs with tag '%s' at level %d. Press any key to stop. ---\n",
                  tag, level);

    // 从 stdin 读取任意字符（阻塞等待用户按键）
    char ch;
    Stream_Read(Stream_GetTaskStdIn(NULL), &ch, 1, MYRTOS_MAX_DELAY);

    MyRTOS_printf("\n--- Stopped monitoring tag '%s'. ---\n", tag);

    // 清理资源
    Log_RemoveListener(listener_h);

    // 等待输出完成
    Task_Delay(MS_TO_TICKS(50));

    return 0;
}

/**
 * @brief Log程序定义
 */
const ProgramDefinition_t g_program_log = {
    .name = "log",
    .help = "Monitor logs by tag. Usage: log <tag> [level]",
    .main_func = log_program_main,
};

#else

// Log 服务未启用时的占位符
static int log_program_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    MyRTOS_printf("Log service is not enabled.\n");
    return -1;
}

const ProgramDefinition_t g_program_log = {
    .name = "log",
    .help = "Log service disabled",
    .main_func = log_program_main,
};

#endif // MYRTOS_SERVICE_LOG_ENABLE
