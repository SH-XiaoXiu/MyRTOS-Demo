//
// MyRTOS Boot Module Implementation
//
// 标准启动流程，根据配置宏初始化各服务
//

#include "MyRTOS_Boot.h"

#if MYRTOS_USE_BOOT_MODULE == 1

#include "MyRTOS.h"
#include <stdio.h>

// ============================================================================
//                              服务模块导入
// ============================================================================
#if MYRTOS_SERVICE_IO_ENABLE == 1
#include "MyRTOS_IO.h"
extern StreamHandle_t g_system_stdin;
extern StreamHandle_t g_system_stdout;
extern StreamHandle_t g_system_stderr;
#endif

#if MYRTOS_SERVICE_ASYNC_IO_ENABLE == 1
#include "MyRTOS_AsyncIO.h"
#endif

#if MYRTOS_SERVICE_LOG_ENABLE == 1
#include "MyRTOS_Log.h"
#endif

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1
#include "MyRTOS_Monitor.h"
#endif

#if MYRTOS_SERVICE_TIMER_ENABLE == 1
#include "MyRTOS_Timer.h"
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
#include "MyRTOS_Process.h"
#endif

// ============================================================================
//                              内部变量
// ============================================================================
static const BootConfig_t *s_boot_config = NULL;

// ============================================================================
//                              内部函数
// ============================================================================

/**
 * @brief 打印系统横幅
 */
#if MYRTOS_SERVICE_IO_ENABLE == 1
static void boot_print_banner(StreamHandle_t console) {
    if (console) {
        Stream_Printf(console, "\r\n\r\nMyRTOS (built %s %s)\r\n", __DATE__, __TIME__);
    }
}
#else
static void boot_print_banner(void) {
    // IO 服务未启用，不输出
}
#endif

/**
 * @brief 初始化内核
 */
#if MYRTOS_SERVICE_IO_ENABLE == 1
static void boot_init_kernel(StreamHandle_t console) {
    if (console) {
        Stream_Printf(console, "[Kernel]\r\n");
    }

    MyRTOS_Init();

    if (console) {
        Stream_Printf(console, "  Heap: %lu KB available\r\n",
                     MYRTOS_MEMORY_POOL_SIZE / 1024);
        Stream_Printf(console, "  Scheduler: %d priorities, %d task slots, %lu Hz tick\r\n",
                     MYRTOS_MAX_PRIORITIES,
                     MYRTOS_MAX_CONCURRENT_TASKS,
                     MYRTOS_TICK_RATE_HZ);
    }
}
#else
static void boot_init_kernel(void) {
    MyRTOS_Init();
}
#endif

/**
 * @brief 初始化服务层
 */
static void boot_init_services(const BootConfig_t *config) {
#if MYRTOS_SERVICE_IO_ENABLE == 1
    StreamHandle_t console = config->console_stream;
    if (console) {
        Stream_Printf(console, "\r\n[Services]\r\n");
    }
#endif
    (void)config; // 避免未使用警告

    // I/O 服务
#if MYRTOS_SERVICE_IO_ENABLE == 1
    StdIOService_Init();
    if (console) Stream_Printf(console, "  [OK] I/O streams\r\n");
#endif

    // 异步 I/O 服务
#if MYRTOS_SERVICE_ASYNC_IO_ENABLE == 1
    if (AsyncIOService_Init() != 0) {
#if MYRTOS_SERVICE_IO_ENABLE == 1
        if (console) Stream_Printf(console, "  [FAIL] Async I/O\r\n");
#endif
        while (1);
    }
#if MYRTOS_SERVICE_IO_ENABLE == 1
    if (console) Stream_Printf(console, "  [OK] Async I/O (queue: %d, prio: %d)\r\n",
                              MYRTOS_ASYNCIO_QUEUE_LENGTH,
                              MYRTOS_ASYNCIO_TASK_PRIORITY);
#endif
#endif

    // 日志服务
#if MYRTOS_SERVICE_LOG_ENABLE == 1
    Log_Init();
#if MYRTOS_SERVICE_IO_ENABLE == 1
    if (console) {
        const char *log_mode = MYRTOS_LOG_USE_ASYNC_OUTPUT ? "async" : "sync";
        Stream_Printf(console, "  [OK] Logger (%s mode)\r\n", log_mode);
    }
#endif
#endif

    // 进程管理服务
#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    Process_Init();
#if MYRTOS_SERVICE_IO_ENABLE == 1
    if (console) Stream_Printf(console, "  [OK] Process manager (max: %d)\r\n",
                              MYRTOS_PROCESS_MAX_INSTANCES);
#endif
#endif

    // 设置标准IO流
#if MYRTOS_SERVICE_IO_ENABLE == 1
    g_system_stdin = config->console_stream;
    g_system_stdout = config->console_stream;
    g_system_stderr = config->console_stream;
#endif

    // VTS 服务 (虚拟终端) - 需要 IO 服务
#if MYRTOS_SERVICE_VTS_ENABLE == 1 && MYRTOS_SERVICE_IO_ENABLE == 1
    StreamHandle_t vts_pipe_in = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    StreamHandle_t vts_pipe_out = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    if (!vts_pipe_in || !vts_pipe_out) {
        if (console) Stream_Printf(console, "  [FAIL] VTS (pipe allocation)\r\n");
        while (1);
    }

    VTS_Config_t v_config = {
        .physical_stream = config->console_stream,
        .root_input_stream = vts_pipe_in,
        .root_output_stream = vts_pipe_out,
        .signal_receiver_task_handle = NULL
    };

    if (VTS_Init(&v_config) != 0) {
        if (console) Stream_Printf(console, "  [FAIL] Virtual terminal\r\n");
        while (1);
    }

    // VTS 启动后接管后台日志输出流
    g_system_stdout = VTS_GetBackgroundStream();
    g_system_stderr = VTS_GetBackgroundStream();
    if (console) Stream_Printf(console, "  [OK] Virtual terminal\r\n");
#endif

    // 系统监控服务
#if MYRTOS_SERVICE_MONITOR_ENABLE == 1
    MonitorConfig_t m_config = {.get_hires_timer_value = config->get_hires_timer_value};
    Monitor_Init(&m_config);
#if MYRTOS_SERVICE_IO_ENABLE == 1
    if (console) Stream_Printf(console, "  [OK] System monitor\r\n");
#endif
#endif

    // 软件定时器服务
#if MYRTOS_SERVICE_TIMER_ENABLE == 1
    TimerService_Init(MYRTOS_MAX_PRIORITIES - 2, 2048);
#if MYRTOS_SERVICE_IO_ENABLE == 1
    if (console) Stream_Printf(console, "  [OK] Software timers\r\n");
#endif
#endif
}

/**
 * @brief 默认空闲任务
 */
static void boot_default_idle_task(void *pv) {
    (void) pv;
    for (;;) {
        // 默认什么都不做，等待调度
    }
}

// ============================================================================
//                              公共 API
// ============================================================================

void MyRTOS_Boot_Init(const BootConfig_t *config) {
    s_boot_config = config;

#if MYRTOS_SERVICE_IO_ENABLE == 1
    StreamHandle_t console = config->console_stream;

    // 打印系统横幅
    boot_print_banner(console);

    // 初始化内核
    boot_init_kernel(console);
#else
    // 打印系统横幅
    boot_print_banner();

    // 初始化内核
    boot_init_kernel();
#endif

    // 用户内核初始化后回调
    if (config->on_kernel_init) {
        config->on_kernel_init();
    }

    // 初始化服务层
    boot_init_services(config);

    // 用户服务初始化后回调
    if (config->on_services_init) {
        config->on_services_init();
    }

#if MYRTOS_SERVICE_IO_ENABLE == 1
    if (console) {
        Stream_Printf(console, "\r\n[Application]\r\n");
        Stream_Printf(console, "  Setup complete\r\n");
    }
#endif
}


void MyRTOS_Boot_Start(const BootConfig_t *config) {
#if MYRTOS_SERVICE_IO_ENABLE == 1
    StreamHandle_t console = config->console_stream;

    if (console) {
        Stream_Printf(console, "\r\nCreating system tasks...\r\n");
    }
#endif

    // 创建用户任务
    if (config->create_tasks) {
        config->create_tasks();
    }

#if MYRTOS_SERVICE_IO_ENABLE == 1
    if (console) {
        Stream_Printf(console, "System tasks created\r\n");
        Stream_Printf(console, "\r\n");
        Stream_Printf(console, "========================================\r\n");
        Stream_Printf(console, "Starting scheduler...\r\n");
        Stream_Printf(console, "Entering multitasking mode\r\n");
        Stream_Printf(console, "========================================\r\n");
        Stream_Printf(console, "\r\n");
    }
#endif

    // 获取空闲任务
    void (*idle_task)(void *) = config->idle_task;
    if (!idle_task) {
        idle_task = boot_default_idle_task;
    }

    // 启动调度器（永不返回）
    Task_StartScheduler(idle_task);

    // 永远不会执行到这里
    while (1);
}


void MyRTOS_Boot(const BootConfig_t *config) {
    MyRTOS_Boot_Init(config);
    MyRTOS_Boot_Start(config);
}

#endif // MYRTOS_USE_BOOT_MODULE
