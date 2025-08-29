//
// Created by XiaoXiu on 8/29/2025.
//

// terminal commands for MyRTOS on the GD32F4xx platform.

#include <stdio.h>

#include "MyRTOS.h"
#include "MyRTOS_Terminal.h"
#include "MyRTOS_Monitor.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Console.h"
#include <string.h>
#include <stdlib.h>

#if (MY_RTOS_USE_TERMINAL == 1)

static void cmd_ps(int argc, char **argv);
static void cmd_heap(int argc, char **argv);
static void cmd_loglevel(int argc, char **argv);
static void cmd_reboot(int argc, char **argv);
static void cmd_mode(int argc, char **argv);


/**
 * @brief 初始化并注册所有平台相关的终端命令。
 *        应在 main 函数中, MyRTOS_SystemInit 之后调用。
 */
void MyRTOS_Platform_TerminalCommandsInit(void) {
    MyRTOS_Terminal_RegisterCommand("ps", "Show task list and status", cmd_ps);
    MyRTOS_Terminal_RegisterCommand("heap", "Show heap memory stats", cmd_heap);
    MyRTOS_Terminal_RegisterCommand("loglevel", "Set log level. Usage: loglevel <sys|user> <level>", cmd_loglevel);
    MyRTOS_Terminal_RegisterCommand("reboot", "Reboot the system", cmd_reboot);
    MyRTOS_Terminal_RegisterCommand("mode", "Switch console mode. Usage: mode <log|mon>", cmd_mode);
}


//实现

static void cmd_ps(int argc, char **argv) {
    TaskHandle_t taskHandle = Task_GetNextTaskHandle(NULL);
    TaskStats_t taskStats;

    PRINT("%-16s %-3s %-10s %-8s %-12s\r\n", "Name", "ID", "State", "Prio", "Stack(H/T)");
    PRINT("----------------------------------------------------------\r\n");

    const char *const stateStr[] = {"Unused", "Ready", "Delayed", "Blocked"};

    while(taskHandle != NULL) {
        Task_GetInfo(taskHandle, &taskStats);
        char prioStr[8];
        char stackStr[12];
        snprintf(prioStr, sizeof(prioStr), "%d/%d", taskStats.basePriority, taskStats.currentPriority);
        snprintf(stackStr, sizeof(stackStr), "%u/%u", (unsigned int)taskStats.stackHighWaterMark, (unsigned int)taskStats.stackSize);
        PRINT("%-16s %-3u %-10s %-8s %-12s\r\n",
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
    PRINT("Heap Info:\r\n");
    PRINT("  Total Size:      %u bytes\r\n", (unsigned int)heapStats.totalHeapSize);
    PRINT("  Free Remaining:  %u bytes\r\n", (unsigned int)heapStats.freeBytesRemaining);
    PRINT("  Min Ever Free:   %u bytes\r\n", (unsigned int)heapStats.minimumEverFreeBytesRemaining);
}

static void cmd_loglevel(int argc, char **argv) {
    if (argc != 3) {
        PRINT("Usage: %s <sys|user> <none|error|warn|info|debug>\r\n", argv[0]);
        return;
    }

    LogModule_t module;
    if (strcmp(argv[1], "sys") == 0) module = LOG_MODULE_SYSTEM;
    else if (strcmp(argv[1], "user") == 0) module = LOG_MODULE_USER;
    else {
        PRINT("Invalid module. Use 'sys' or 'user'.\r\n");
        return;
    }

    int level;
    if (strcmp(argv[2], "none") == 0) level = SYS_LOG_LEVEL_NONE;
    else if (strcmp(argv[2], "error") == 0) level = SYS_LOG_LEVEL_ERROR;
    else if (strcmp(argv[2], "warn") == 0) level = SYS_LOG_LEVEL_WARN;
    else if (strcmp(argv[2], "info") == 0) level = SYS_LOG_LEVEL_INFO;
    else if (strcmp(argv[2], "debug") == 0) level = SYS_LOG_LEVEL_DEBUG;
    else {
        PRINT("Invalid level.\r\n");
        return;
    }

    MyRTOS_Log_SetLevel(module, level);
    PRINT("Log level for '%s' module set to '%s'.\r\n", argv[1], argv[2]);
}

static void cmd_reboot(int argc, char **argv) {
    PRINT("Rebooting system...\r\n");
    Task_Delay(MS_TO_TICKS(100));
    NVIC_SystemReset();
}

static void cmd_mode(int argc, char **argv) {
    if (argc != 2) {
        PRINT("Usage: mode <log|mon>\r\n");
        return;
    }
    if (strcmp(argv[1], "log") == 0) {
        MyRTOS_Console_SetMode(CONSOLE_MODE_LOG);
        // 'mode log' should also stop the terminal if it's running
        if (MyRTOS_Terminal_IsRunning()) {
            MyRTOS_Terminal_Stop();
        }
        PRINT("Switched to LOG mode.\r\n");
    } else if (strcmp(argv[1], "mon") == 0) {
        #if (MY_RTOS_USE_MONITOR == 1)
        MyRTOS_Monitor_Start();
        #else
        PRINT("Monitor is disabled.\r\n");
        #endif
    } else {
        PRINT("Invalid mode.\r\n");
    }
}

#endif // MY_RTOS_USE_TERMINAL