/**
 * @file  init_main.c
 * @brief Init进程 - 系统第一个用户进程，负责启动Shell
 */
#include "init.h"
#include "MyRTOS.h"
#include "MyRTOS_IO.h"

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
#include "MyRTOS_Process.h"
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif

/**
 * @brief Init进程主函数 - 系统启动后的第一个用户进程
 * @details 负责启动Shell进程并管理其生命周期
 */
static int init_process_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // 等待父任务切换VTS焦点（避免stdout管道阻塞）
    Task_Delay(MS_TO_TICKS(50));

    MyRTOS_printf("\n");
    MyRTOS_printf("=== MyRTOS Shell Environment ===\n");
    MyRTOS_printf("Starting Shell...\n");

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    char *shell_argv[] = {"shell"};
    pid_t shell_pid = Process_RunProgram("shell", 1, shell_argv, PROCESS_MODE_BOUND);

    if (shell_pid <= 0) {
        MyRTOS_printf("Failed to start Shell!\n");
        return -1;
    }

    // 获取shell进程的stdin/stdout管道
    StreamHandle_t stdin_pipe = Process_GetFdHandleByPid(shell_pid, STDIN_FILENO);
    StreamHandle_t stdout_pipe = Process_GetFdHandleByPid(shell_pid, STDOUT_FILENO);

#if MYRTOS_SERVICE_VTS_ENABLE == 1
    // 切换VTS焦点到shell进程
    if (stdin_pipe && stdout_pipe) {
        VTS_SetFocus(stdin_pipe, stdout_pipe);
    }

    // 等待shell进程退出或信号
    uint32_t received_signals = Task_WaitSignal(
        SIG_CHILD_EXIT | SIG_INTERRUPT | SIG_SUSPEND | SIG_BACKGROUND,
        MYRTOS_MAX_DELAY, SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

    // 恢复VTS焦点
    VTS_ReturnToRootFocus();

    if (received_signals & SIG_CHILD_EXIT) {
        MyRTOS_printf("\nShell process (PID %d) exited normally.\n", shell_pid);
    }
#else
    // 没有VTS，简单等待shell退出
    while (Process_GetState(shell_pid) == PROCESS_STATE_RUNNING) {
        Task_Delay(MS_TO_TICKS(100));
    }
    MyRTOS_printf("Shell process (PID %d) exited.\n", shell_pid);
#endif

#endif // MYRTOS_SERVICE_PROCESS_ENABLE

    MyRTOS_printf("init process exiting.\n");
    return 0;
}

/**
 * @brief Init程序定义
 */
const ProgramDefinition_t g_program_init = {
    .name = "init",
    .help = "Init process - 系统启动进程",
    .main_func = init_process_main,
};
