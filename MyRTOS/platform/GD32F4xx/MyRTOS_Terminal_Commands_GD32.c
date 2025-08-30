//
// Created by XiaoXiu on 8/29/2025.
// Registers all platform-specific shell commands.
//

#include "MyRTOS.h"
#include "MyRTOS_Shell.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_IO.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if (MY_RTOS_USE_SHELL == 1)

static void cmd_ps(int argc, char **argv);

static void cmd_heap(int argc, char **argv);

static void cmd_reboot(int argc, char **argv);

#if (MY_RTOS_USE_LOG == 1)
static void cmd_loglevel(int argc, char **argv);
#endif


/**
 * @brief 初始化并注册所有平台相关的终端命令。
 *        应在 main 函数中, MyRTOS_SystemInit 之后调用。
 */
void MyRTOS_Platform_TerminalCommandsInit(void) {
    // 注册本地实现的简单命令
    MyRTOS_Shell_RegisterCommand("ps", "Show task list and status", cmd_ps);
    MyRTOS_Shell_RegisterCommand("heap", "Show heap memory stats", cmd_heap);
    MyRTOS_Shell_RegisterCommand("reboot", "Reboot the system", cmd_reboot);

#if (MY_RTOS_USE_LOG == 1)
    MyRTOS_Shell_RegisterCommand("loglevel", "Set log level. Usage: loglevel <sys|user> <level>", cmd_loglevel);
#endif
}

// --- 命令函数实现 ---
static void cmd_ps(int argc, char **argv) {
    TaskHandle_t taskHandle = Task_GetNextTaskHandle(NULL);
    TaskStats_t taskStats;
    MyRTOS_printf("%-16s %-3s %-10s %-8s %-12s\r\n", "Name", "ID", "State", "Prio", "Stack(H/T)");
    MyRTOS_printf("----------------------------------------------------------\r\n");
    const char *const stateStr[] = {"Unused", "Ready", "Delayed", "Blocked"};

    while (taskHandle != NULL) {
        Task_GetInfo(taskHandle, &taskStats);
        char prioStr[8];
        char stackStr[12];
        snprintf(prioStr, sizeof(prioStr), "%d/%d", taskStats.basePriority, taskStats.currentPriority);
        snprintf(stackStr, sizeof(stackStr), "%u/%u", (unsigned int) taskStats.stackHighWaterMark,
                 (unsigned int) taskStats.stackSize);
        MyRTOS_printf("%-16s %-3u %-10s %-8s %-12s\r\n",
                      taskStats.taskName,
                      (unsigned int)taskStats.taskId,
                      stateStr[taskStats.state],
                      prioStr,
                      stackStr);
        taskHandle = Task_GetNextTaskHandle(taskHandle);
    }
}

static void cmd_heap(int argc, char **argv) {
    HeapStats_t heapStats;
    Heap_GetStats(&heapStats);
    MyRTOS_printf("Heap Info:\r\n");
    MyRTOS_printf("  Total Size:      %u bytes\r\n", (unsigned int)heapStats.totalHeapSize);
    MyRTOS_printf("  Free Remaining:  %u bytes\r\n", (unsigned int)heapStats.freeBytesRemaining);
    MyRTOS_printf("  Min Ever Free:   %u bytes\r\n", (unsigned int)heapStats.minimumEverFreeBytesRemaining);
}

#if (MY_RTOS_USE_LOG == 1)
static void cmd_loglevel(int argc, char **argv) {
    if (argc != 3) {
        MyRTOS_printf("Usage: %s <sys|user> <level>\r\n", argv[0]);
        return;
    }

    LogModule_t module;
    if (strcmp(argv[1], "sys") == 0) module = LOG_MODULE_SYSTEM;
    else if (strcmp(argv[1], "user") == 0) module = LOG_MODULE_USER;
    else {
        MyRTOS_printf("Invalid module. Use 'sys' or 'user'.\r\n");
        return;
    }

    int level;
    if (strcmp(argv[2], "none") == 0) level = SYS_LOG_LEVEL_NONE;
    else if (strcmp(argv[2], "error") == 0) level = SYS_LOG_LEVEL_ERROR;
    else if (strcmp(argv[2], "warn") == 0) level = SYS_LOG_LEVEL_WARN;
    else if (strcmp(argv[2], "info") == 0) level = SYS_LOG_LEVEL_INFO;
    else if (strcmp(argv[2], "debug") == 0) level = SYS_LOG_LEVEL_DEBUG;
    else {
        MyRTOS_printf("Invalid level.\r\n");
        return;
    }

    MyRTOS_Log_SetLevel(module, level);
    MyRTOS_printf("Log level for '%s' module set to '%s'.\r\n", argv[1], argv[2]);
}
#endif

static void cmd_reboot(int argc, char **argv) {
    MyRTOS_printf("Rebooting system...\r\n");
    Task_Delay(MS_TO_TICKS(100));
    NVIC_SystemReset();
}

#endif // MY_RTOS_USE_SHELL
