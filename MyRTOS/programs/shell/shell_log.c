/**
 * @file  shell_log.c
 * @brief 日志命令（logall, log, loglevel）
 */
#include "shell.h"

#if MYRTOS_SERVICE_LOG_ENABLE == 1

#include "MyRTOS_Log.h"
#include "MyRTOS_IO.h"
#include "MyRTOS.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#else
// VTS service stubs when disabled
static inline StreamHandle_t VTS_GetBackgroundStream(void) { return NULL; }
static inline void VTS_ReturnToRootFocus(void) {}
static inline int VTS_SetFocus(StreamHandle_t input_stream, StreamHandle_t output_stream) {
    (void)input_stream;
    (void)output_stream;
    return 0;
}
static inline void VTS_SetTerminalMode(int mode) { (void)mode; }
#define VTS_MODE_RAW 0
#endif

// 用于 logall 命令的全局日志监听器句柄
static LogListenerHandle_t g_logall_listener_h = NULL;

// logall 命令：切换全局日志监控
static int cmd_logall(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    if (argc != 2) {
        MyRTOS_printf("Usage: logall <on|off>\n");
        return -1;
    }
    if (strcmp(argv[1], "on") == 0) {
        if (g_logall_listener_h != NULL) {
            MyRTOS_printf("Global log monitoring is already enabled.\n");
            return 0;
        }
        // 添加一个监听所有tag(*)的监听器到VTS后台流
        g_logall_listener_h = Log_AddListener(VTS_GetBackgroundStream(), LOG_LEVEL_DEBUG, NULL);
        if (g_logall_listener_h) {
            MyRTOS_printf("Global log monitoring enabled.\n");
        } else {
            MyRTOS_printf("Error: Failed to enable global log monitoring.\n");
        }
    } else if (strcmp(argv[1], "off") == 0) {
        if (g_logall_listener_h == NULL) {
            MyRTOS_printf("Global log monitoring is not active.\n");
            return 0;
        }
        // 移除监听器
        if (Log_RemoveListener(g_logall_listener_h) == 0) {
            g_logall_listener_h = NULL;
            MyRTOS_printf("Global log monitoring disabled.\n");
        } else {
            MyRTOS_printf("Error: Failed to disable global log monitoring.\n");
        }
    } else {
        MyRTOS_printf("Invalid argument. Use 'on' or 'off'.\n");
        return -1;
    }
    return 0;
}

// log 命令：监控指定标签或任务的实时日志
static int cmd_log(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    if (argc < 2 || argc > 3) {
        MyRTOS_printf("Usage: log <taskName_or_tag> [level]\n");
        return -1;
    }

    const char *tag = argv[1];
    LogLevel_t level = LOG_LEVEL_DEBUG; // 默认级别

    if (argc == 3) {
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

    StreamHandle_t pipe = Pipe_Create(1024);
    if (!pipe) {
        MyRTOS_printf("Error: Failed to create pipe for logging.\n");
        return -1;
    }

    LogListenerHandle_t listener_h = Log_AddListener(pipe, level, tag);
    if (!listener_h) {
        MyRTOS_printf("Error: Failed to add log listener.\n");
        Pipe_Delete(pipe);
        return -1;
    }

    VTS_SetFocus(Stream_GetTaskStdIn(NULL), pipe);
    VTS_SetTerminalMode(VTS_MODE_RAW);
    Stream_Printf(pipe, "--- Start monitoring LOGs with tag '%s' at level %d. Press any key to stop. ---\n",
                  tag, level);

    char ch;
    Stream_Read(Stream_GetTaskStdIn(NULL), &ch, 1, MYRTOS_MAX_DELAY);
    Stream_Printf(pipe, "\n--- Stopped monitoring tag '%s'. ---\n", tag);
    Task_Delay(MS_TO_TICKS(10));

    Log_RemoveListener(listener_h);
    VTS_ReturnToRootFocus();
    Pipe_Delete(pipe);

    return 0;
}

// loglevel 命令：设置或显示全局日志级别
static int cmd_loglevel(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    if (argc != 2) {
        MyRTOS_printf("Usage: loglevel <level>\n");
        MyRTOS_printf("Levels: 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG\n");
        MyRTOS_printf("Current level: %d\n", Log_GetGlobalLevel());
        return -1;
    }

    int level = atoi(argv[1]);
    if (level >= LOG_LEVEL_NONE && level <= LOG_LEVEL_DEBUG) {
        Log_SetGlobalLevel((LogLevel_t)level);
        MyRTOS_printf("Global log level set to %d.\n", level);
    } else {
        MyRTOS_printf("Invalid level. Please use a number between 0 and 4.\n");
        return -1;
    }
    return 0;
}

void shell_register_log_commands(shell_handle_t shell) {
    shell_register_command(shell, "logall", "切换全局日志. 用法: logall <on|off>", cmd_logall);
    shell_register_command(shell, "log", "监听指定Tag日志. 用法: log <tag>", cmd_log);
    shell_register_command(shell, "loglevel", "设置日志级别. 用法: loglevel <0-4>", cmd_loglevel);
}

#else

// Log 服务未启用
void shell_register_log_commands(shell_handle_t shell) {
    (void)shell;
}

#endif // MYRTOS_SERVICE_LOG_ENABLE
